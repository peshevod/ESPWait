/*
 * my_server.c
 *
 *  Created on: 1 авг. 2021 г.
 *      Author: ilya_000
 */

#include "esp_https_server.h"
#include "esp_event.h"
#include "wolfssl/wolfcrypt/coding.h"
#include "wolfssl/internal.h"
#include "crypto.h"
#include "users.h"
#include "my_server.h"
#include "shell.h"
#include "esp_httpd_priv.h"
#include "lorawan_defs.h"
#include "cJSON.h"
#include "lorax.h"
#include "cmd_nvs.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "esp_vfs.h"
#include "storage.h"
#include "esp_tls.h"
#include "storage.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

static const char *TAG = "my_server";
static char buffer1[MY_SERVER_AUTHORIZATION_MAX+1];
extern ChannelParams_t Channels[MAX_RU_SINGLE_BAND_CHANNELS];
extern const int8_t txPowerRU864[];
extern uint8_t number_of_devices;
extern NetworkSession_t *networkSessions[MAX_NUMBER_OF_DEVICES];


#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (1280)

#define REST_CHECK(a, str, goto_tag, ...)                                              \
    do                                                                                 \
    {                                                                                  \
        if (!(a))                                                                      \
        {                                                                              \
            ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto goto_tag;                                                             \
        }                                                                              \
    } while (0)

typedef struct rest_server_context {
    char base_path[ESP_VFS_PATH_MAX + 1];
    char scratch[SCRATCH_BUFSIZE];
} rest_server_context_t;

rest_server_context_t* rest_context;

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t set_content_type_from_file(httpd_req_t *req, const char *filepath)
{
    const char *type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html")) {
        type = "text/html";
    } else if (CHECK_FILE_EXTENSION(filepath, ".js")) {
        type = "application/javascript";
    } else if (CHECK_FILE_EXTENSION(filepath, ".css")) {
        type = "text/css";
    } else if (CHECK_FILE_EXTENSION(filepath, ".png")) {
        type = "image/png";
    } else if (CHECK_FILE_EXTENSION(filepath, ".ico")) {
        type = "image/x-icon";
    } else if (CHECK_FILE_EXTENSION(filepath, ".svg")) {
        type = "text/xml";
    }
    return httpd_resp_set_type(req, type);
}

static void print_headers(httpd_req_t *req)
{
    struct httpd_req_aux *ra = req->aux;
    const char   *hdr_ptr = ra->scratch;         /*!< Request headers are kept in scratch buffer */
    unsigned      count   = ra->req_hdrs_count;  /*!< Count set during parsing  */
    if(count==0) ESP_LOGI(TAG,"Number of header fields==0");
    else
    {
		while (count--) {
			ESP_LOGI(TAG,"%d Field [%s]",count,hdr_ptr);
			if(count)
			{
				hdr_ptr = 1 + strchr(hdr_ptr, '\0');
                while (*hdr_ptr == '\0') hdr_ptr++;
			}
		}
    }
}

/* Send HTTP response with the contents of the requested file */
static esp_err_t rest_common_get_handler(httpd_req_t *req)
{
    char filepath[FILE_PATH_MAX];
    uint8_t k=0;

	print_headers(req);

	int fd0=httpd_req_to_sockfd(req);
    struct httpd_req_aux *ra = req->aux;
    struct sock_db *sd=ra->sd;
    esp_tls_t* tls=sd->transport_ctx;
    WOLFSSL* ssl=tls->priv_ssl;
    ProtocolVersion version,chVersion;
    ESP_LOGI(TAG,"Common Get Handler Connection state=%d fd=%d is_tls=%s ver=%hhd.%hhd client hello ver=%hhd.%hhd",tls->conn_state,sd->fd, (tls->is_tls ? "TLS" : "non-TLS"),ssl->version.major,ssl->version.minor,ssl->chVersion.major,ssl->chVersion.minor);

    rest_server_context_t *rest_context = *((rest_server_context_t**)(req->user_ctx));
    strlcpy(filepath, rest_context->base_path, sizeof(filepath));
    if (req->uri[strlen(req->uri) - 1] == '/') {
        strlcat(filepath, "/index.html", sizeof(filepath));
    } else {
        strlcat(filepath, req->uri, sizeof(filepath));
    }
    int fd = open(filepath, O_RDONLY, 0);
    if (fd == -1) {
        ESP_LOGE(TAG, "Failed to open file : %s", filepath);
        /* Respond with 500 Internal Server Error */
	    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , "*");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    set_content_type_from_file(req, filepath);

    char *chunk = rest_context->scratch;
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , "*");
    ssize_t read_bytes;
    do {
        /* Read file in chunks into the scratch buffer */
        read_bytes = read(fd, chunk, SCRATCH_BUFSIZE);
        if (read_bytes == -1) {
            ESP_LOGE(TAG, "Failed to read file : %s", filepath);
        } else if (read_bytes > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, chunk, read_bytes) != ESP_OK) {
                close(fd);
                ESP_LOGE(TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (read_bytes > 0);
    /* Close file after sending complete */
    close(fd);
    ESP_LOGI(TAG, "File sending complete");
    /* Respond with an empty chunk to signal HTTP response completion */
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t proceed_request(httpd_req_t *req,char* user,char* role)
{
	httpd_resp_set_type(req, "text/html");
	char resp_text[128];
	strcpy(resp_text,"<h1>Hello ");
	strcat(resp_text,user);
	strcat(resp_text,"! Your role is ");
	strcat(resp_text,role);
	strcat(resp_text,".</h1>\n");
	httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , "*");
	httpd_resp_send(req, resp_text, HTTPD_RESP_USE_STRLEN);
	return ESP_OK;
}


static bool verifyBearer(httpd_req_t *req, char* user, char* role)
{
    size_t auth_len;
    char* bearer;
    char auth[MY_SERVER_AUTHORIZATION_MAX+1];
    int n;

    if((auth_len=httpd_req_get_hdr_value_len(req,"Authorization"))==0) return false;
	httpd_req_get_hdr_value_str(req,"Authorization",auth,MY_SERVER_AUTHORIZATION_MAX);
	auth[auth_len]=0;
	if((bearer=strstr(auth,"Bearer "))!=NULL)
	{
		bearer+=7;
		while(*bearer==' ' && *bearer!=0) bearer++;
		n=strlen(bearer);
		while(bearer[n-1]==' ' && n>0 ) n--;
		bearer[n]=0;
		ESP_LOGI(TAG,"bearer=%s",bearer);
		if(verifyToken(bearer,user,role)==ESP_OK)
		{
			ESP_LOGI(TAG,"Token verified user=%s role=%s",user,role);
			return true;
		}
		else
		{
			ESP_LOGE(TAG,"Token not verified or expired");
		}
	}
    return false;
}


static esp_err_t system_get_handler(httpd_req_t *req)
{
    char join[2048];
    char parstr[128];
    esp_chip_info_t info;


    sprintf(join,"{\"Version\":\"%s\",",esp_get_idf_version());
    esp_chip_info(&info);
    switch(info.model)
    {
	case CHIP_ESP32:
		sprintf(parstr,"\"Model\":\"ESP32\",");
		break;
	case CHIP_ESP32S2:
		sprintf(parstr,"\"Model\":\"ESP32-S2\",");
		break;
	case CHIP_ESP32S3:
		sprintf(parstr,"\"Model\":\"ESP32-S3\",");
		break;
	case CHIP_ESP32C3:
		sprintf(parstr,"\"Model\":\"ESP32-C3\",");
		break;
 	default:
		sprintf(parstr,"\"Model\":\"Unknown\",");
		break;
    }
    strcat(join,parstr);
    sprintf(parstr,"\"Cores\":\"%d\",",info.cores);
    strcat(join,parstr);
    sprintf(parstr,"\"Revision\":\"%d\"}",info.revision);
    strcat(join,parstr);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , "*");
	httpd_resp_sendstr(req, join);
    return ESP_OK;
}

static esp_err_t login_get_handler(httpd_req_t *req)
{
    size_t auth_len;
    char auth[MY_SERVER_AUTHORIZATION_MAX+1];
    char x[160];
    char errr_str[64];
    char user[USERNAME_MAX];
    char role[ROLENAME_MAX];
    char* basic;
    char* y;
    char username[USERNAME_MAX];
    char password[USERNAME_MAX];
    char firstName[USERNAME_MAX];
    char lastName[USERNAME_MAX];
    char uname[16];
    int n;
    uint8_t j;
    uint32_t m;
    uint8_t k=0;

	ESP_LOGI(TAG,"login handler");

	print_headers(req);

/*	int fd0=httpd_req_to_sockfd(req);
	for(k=0;k<MAX_SESS;k++)
	{
		if(ssl_yes[k]==fd0) break;
	}
	if(k==MAX_SESS)
	{
	    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , "*");
		httpd_resp_send_404(req);
		return ESP_FAIL;
	};*/

    if((auth_len=httpd_req_get_hdr_value_len(req,"Authorization"))==0)
    {
	    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , "*");
    	httpd_resp_set_hdr(req,"WWW-Authenticate", "Basic realm=\"Monitor\"");
    	httpd_resp_send_err(req,HTTPD_401_UNAUTHORIZED,"Unauthorized");
    	ESP_LOGI(TAG,"Send WWW-Authenticate");
    }
    else
    {
    	if(auth_len<MY_SERVER_AUTHORIZATION_MAX)
    	{
			httpd_req_get_hdr_value_str(req,"Authorization",auth,MY_SERVER_AUTHORIZATION_MAX);
			auth[auth_len]=0;
    	ESP_LOGI(TAG,"Rec Authorization header %s",auth);
			if((basic=strstr(auth,"Basic "))!=NULL)
			{
				basic+=6;
				while(*basic==' ' && *basic!=0) basic++;
				n=strlen(basic);
				while(basic[n-1]==' ' && n>0 ) n--;
				basic[n]=0;
				ESP_LOGI(TAG,"basic=%s",basic);
				m=127;
				if(Base64_Decode((const byte*)basic,n,(byte*)x,&m)==0)
				{
					if((y=strchr(x,':'))!=NULL)
					{
						*y='\x00';
						y++;
						for(j=0;j<=MAX_USERS;j++)
						{
							if(j!=0)
							{
								sprintf(uname,"USR%d",j);
								if(Read_str_params(uname,username, USERNAME_MAX)!=ESP_OK) continue;
							}
							else strcpy(username,"admin");
							sprintf(uname,"PWD%d",j);
							if(Read_str_params(uname,password,USERNAME_MAX)!=ESP_OK) continue;
							if(!strcmp(x,username) && !strcmp(y,password))
							{
								httpd_resp_set_type(req, "application/json");
								strcpy(buffer1,"{\"token\":\"");
								uint8_t l=strlen(buffer1);
								makeToken(&buffer1[l],sizeof(buffer1)-l,x,3600, j==0 ? "admin" : "user");
								strcat(buffer1,"\",\"user\":\"");
							    strcat(buffer1,j==0 ? "admin" : username);
							    strcat(buffer1,"\"");
							    if(j!=0)
							    {
							    	strcat(buffer1,",\"welcome\":\" ");
									sprintf(uname,"FirstName%d",j);
									if(Read_str_params(uname,firstName, USERNAME_MAX)!=ESP_OK) continue;
									strcat(buffer1,firstName);
									strcat(buffer1," ");
									sprintf(uname,"LastName%d",j);
									if(Read_str_params(uname,lastName, USERNAME_MAX)!=ESP_OK) continue;
									strcat(buffer1,lastName);
									strcat(buffer1,"!\"");
							    }
							    strcat(buffer1,"}\n");
								httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , "*");
								httpd_resp_send(req, buffer1, HTTPD_RESP_USE_STRLEN);
								return ESP_OK;
							}
						}
						strcpy(errr_str,"Wrong credentials ");
						strcat(errr_str,x);
						strcat(errr_str,":");
						strcat(errr_str,y);
					}
					else strcpy(errr_str,"Wrong format user:password");
				}
				else strcpy(errr_str,"Base64 decode error");
			}
			else
			{
				if(verifyBearer(req,user,role))
			    {
					proceed_request(req,user,role);
					return ESP_OK;
			    } else strcpy(errr_str,"Token not verified");
			}
    	}
    	else
    	{
    		strcpy(errr_str,"Auth header>=MY_SERVER_AUTHORIZATION_MAX");
    	}
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , "*");
    httpd_resp_send_err(req,HTTPD_403_FORBIDDEN,errr_str);
    printf(errr_str);
    return ESP_OK;
}

static esp_err_t options_handler(httpd_req_t *req)
{
	char origin[128];
	char headers[128];
	int auth_len;
	esp_err_t err0;

	print_headers(req);

	if((auth_len=httpd_req_get_hdr_value_len(req,"Origin"))!=0)
    {
		err0=httpd_req_get_hdr_value_str(req, "Origin", origin, sizeof(origin));
    	ESP_LOGI(TAG,"Origin %s",origin);
    	if(err0!=ESP_OK)
    	{
    		ESP_LOGE(TAG,"Error while reading header Origin, result=%s",esp_err_to_name(err0));
    		return err0;
    	}
    	httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , origin);
    } else httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , "*");

    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods" , "GET, POST, OPTIONS");

	if((auth_len=httpd_req_get_hdr_value_len(req,"Access-Control-Request-Headers"))!=0)
    {
    	err0=httpd_req_get_hdr_value_str(req, "Access-Control-Request-Headers", headers, sizeof(headers));
    	ESP_LOGI(TAG,"Access-Control-Request-Headers %s",headers);
    	if(err0!=ESP_OK)
    	{
    		ESP_LOGE(TAG,"Error while reading header Access-Control-Request-Headers, result=%s",esp_err_to_name(err0));
    		return err0;
    	}
    	httpd_resp_set_hdr(req, "Access-Control-Allow-Headers" , headers);
    } else httpd_resp_set_hdr(req, "Access-Control-Allow-Headers" , "*");

    httpd_resp_set_hdr(req, "Access-Control-Max-Age" , "86400");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Credentials" , "true");
	httpd_resp_sendstr(req, "200 OK");
    return ESP_OK;
};

static esp_err_t settings_get_handler(httpd_req_t *req)
{
    char user[USERNAME_MAX];
    char role[ROLENAME_MAX];
    uint8_t join_eui[8];
    char join[2048];
    uint8_t tmp8;
    char parstr[128];
    int l;



    if(!verifyBearer(req,user,role) || strcmp(role,"admin"))
    {
	    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , "*");
    	httpd_resp_send_err(req,HTTPD_401_UNAUTHORIZED,"Unauthorized");
    	return ESP_FAIL;
    }

    // JoinEUI & channel number

    set_s("JOINEUI",join_eui);
    set_s("CHANNEL",&tmp8);
    sprintf(join,"{\"JoinEUI\":\"0x%02X%02X%02X%02X%02X%02X%02X%02X\",\"CurChannel\":\"%d\",\"Channel\":[\n",join_eui[0],join_eui[1],join_eui[2],join_eui[3],join_eui[4],join_eui[5],join_eui[6],join_eui[7],tmp8);

//    ESP_LOGI(TAG,"join=%s",join);

    // Channel Plan
    for(uint8_t i=0;i<MAX_RU_SINGLE_BAND_CHANNELS;i++)
    {
    	sprintf(parstr,"\"CH%d - %dHz\",\n",i,Channels[i].frequency);
//    	ESP_LOGI(TAG,"Chanstr=%s",parstr);
    	strcat(join,parstr);
    }
    l=strlen(join);
    join[l-2]=0;
    strcat(join,"],\n");

    // Bandwidth
    set_s("BW",&tmp8);
    sprintf(parstr,"\"CurBW\":\"%d\",\"BW\":[\"125000Hz\",\"250000Hz\",\"500000Hz\"],\n",tmp8);
    strcat(join,parstr);

    // SpreadFactor
    set_s("SF",&tmp8);
    sprintf(parstr,"\"CurSF\":\"%d\",\"SF\":[\"SF7(DR5)\",\"SF8(DR4)\",\"SF9(DR3)\",\"SF10(DR2)\",\"SF11(DR1)\",\"SF12(DR0)\"],\n",tmp8-7);
    strcat(join,parstr);

    //
    set_s("CRC",&tmp8);
    sprintf(parstr,"\"CurCRC\":\"%d\",\"CRC\":[\"CRC OFF\",\"CRC ON\"],\n",tmp8);
    strcat(join,parstr);

    //
    set_s("FEC",&tmp8);
    sprintf(parstr,"\"CurFEC\":\"%d\",\"FEC\":[\"OFF\",\"4/5\",\"4/6\",\"4/7\",\"4/8\"],\n",tmp8);
    strcat(join,parstr);

    set_s("POWER",&tmp8);
    sprintf(parstr,"\"CurPower\":\"%d\",\"Power\":[",tmp8);
    strcat(join,parstr);
    for(uint8_t i=0;i<9;i++)
    {
    	sprintf(parstr,"\"%d dbm\",",txPowerRU864[i]);
    	strcat(join,parstr);
    }
    l=strlen(join);
    join[l-1]=']';
    strcat(join,",\n");

    set_s("HEADER_MODE",&tmp8);
    sprintf(parstr,"\"CurHM\":\"%d\",\"HM\":[\"Explicit\",\"Implicit\"],\n",tmp8);
    strcat(join,parstr);

    uint32_t netId;
    set_s("NETID",&netId);
    sprintf(parstr,"\"NetID\":\"0x%08X\",\n",netId);
    strcat(join,parstr);

    l=strlen(join);
    join[l-2]='}';

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , "*");
	httpd_resp_sendstr(req, join);
    return ESP_OK;
}

static esp_err_t accounts_get_handler(httpd_req_t *req)
{
    char user[USERNAME_MAX];
    char role[ROLENAME_MAX];
    char join[2048];
    char parstr[128];
    char firstName[PAR_STR_MAX_SIZE];
    char lastName[PAR_STR_MAX_SIZE];
    int l;
    char uname[16];



    if(!verifyBearer(req,user,role) || strcmp(role,"admin"))
    {
	    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , "*");
    	httpd_resp_send_err(req,HTTPD_401_UNAUTHORIZED,"Unauthorized");
    	return ESP_FAIL;
    }

    strcpy(join,"{\"Users\":[");
    for(uint8_t j=1;j<MAX_USERS;j++)
    {
		sprintf(uname,"USR%d",j);
		if(Read_str_params(uname,user, USERNAME_MAX)!=ESP_OK) continue;
		sprintf(uname,"FirstName%d",j);
		if(Read_str_params(uname,firstName, USERNAME_MAX)!=ESP_OK) firstName[0]=0;
		sprintf(uname,"LastName%d",j);
		if(Read_str_params(uname,lastName, USERNAME_MAX)!=ESP_OK) lastName[0]=0;
		sprintf(parstr,"{\"Username\":\"%s\",\"FirstName\":\"%s\",\"LastName\":\"%s\"},",user,firstName,lastName);
		strcat(join,parstr);
    }
    l=strlen(join);
    if(join[l-1]==',') join[l-1]=0;
    strcat(join,"]}");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , "*");
	httpd_resp_sendstr(req, join);
    return ESP_OK;
}

static esp_err_t devices_get_handler(httpd_req_t *req)
{
    char user[USERNAME_MAX];
    char devUser[USERNAME_MAX];
	uint8_t users[MAX_USERS];
    char role[ROLENAME_MAX];
    char join[2048];
    char parstr[128];
    char devName[PAR_STR_MAX_SIZE];
    uint8_t eui[8];
    uint8_t AppKey[16];
    uint8_t NwkKey[16];
    uint8_t version;
    int l=0;
    char uname[16];



    if(!verifyBearer(req,user,role))
    {
	    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , "*");
    	httpd_resp_send_err(req,HTTPD_401_UNAUTHORIZED,"Unauthorized");
    	return ESP_FAIL;
    }

    strcpy(join,"{\"Devices\":[");
    uint8_t usernum=get_user_number(user,role);
    for(uint8_t j=1;j<MAX_NUMBER_OF_DEVICES;j++)
    {
    	sprintf(uname,"Dev%dUsers",j);
		if(Read_eui_params(uname,users)!=ESP_OK) continue;
		if(!in_list(usernum,users)) continue;
		sprintf(uname,"Dev%dEui",j);
		if(Read_eui_params(uname,eui)!=ESP_OK) continue;
		sprintf(uname,"Dev%dAppKey",j);
		if(Read_key_params(uname,AppKey)!=ESP_OK) for(uint8_t i=0;i<16;i++) AppKey[i]=0;
		sprintf(uname,"Dev%dNwkKey",j);
		if(Read_key_params(uname,NwkKey)!=ESP_OK) for(uint8_t i=0;i<16;i++) NwkKey[i]=0;
		sprintf(uname,"Dev%dName",j);
		if(Read_str_params(uname,devName, PAR_STR_MAX_SIZE)!=ESP_OK) devName[0]=0;
		sprintf(uname,"Dev%dVersion",j);
		if(Read_u8_params(uname,&version)!=ESP_OK) version=0;
		sprintf(parstr,"{\"DevName\":\"%s\",\"DevEUI\":\"",devName);
		strcat(join,parstr);
		l=strlen(join);
		for(uint8_t i=0;i<8;i++,l+=2) sprintf(&join[l],"%02X",eui[i]);
		join[l]=0;
		if(!usernum)
		{
			sprintf(parstr,"\",\"AppKey\":\"");
			strcat(join,parstr);
			l=strlen(join);
			for(uint8_t i=0;i<16;i++,l+=2) sprintf(&join[l],"%02X",AppKey[i]);
			join[l]=0;
			sprintf(parstr,"\",\"NwkKey\":\"");
			strcat(join,parstr);
			l=strlen(join);
			for(uint8_t i=0;i<16;i++,l+=2) sprintf(&join[l],"%02X",NwkKey[i]);
			join[l]=0;
			strcat(join,"\",\"Users\":[");
			parstr[0]=0;
			for(uint8_t k=0;k<8;k++)
			{
				if(users[k])
				{
					sprintf(uname,"USR%d",users[k]);
					if(Read_str_params(uname,devUser, PAR_STR_MAX_SIZE)==ESP_OK)
					{
						if(parstr[0]) strcat(parstr,",");
						strcat(parstr,"\"");
						strcat(parstr,devUser);
						strcat(parstr,"\"");
					}
				}
			}
			strcat(parstr,"]");
			strcat(join,parstr);
		}
		sprintf(parstr,",\"Version\":\"%d\"},",version);
		strcat(join,parstr);
    }
    l=strlen(join);
    join[l-1]=0;
    strcat(join,"]}");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , "*");
	httpd_resp_sendstr(req, join);
    return ESP_OK;
}

static esp_err_t monitor_post_handler(httpd_req_t *req)
{
    char user[USERNAME_MAX];
    char role[ROLENAME_MAX];
    esp_err_t err=ESP_OK;
    char uname[USERNAME_MAX+8];
    char uname_free[USERNAME_MAX+8];

    ESP_LOGI(TAG,"POST monitor/* handler");
	print_headers(req);
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , "*");
    if(!verifyBearer(req,user,role))
    {
    	httpd_resp_send_err(req,HTTPD_401_UNAUTHORIZED,"Unauthorized");
    	return ESP_FAIL;
    }
    ESP_LOGI(TAG,"URL=%s",req->uri);
    char* sw=NULL;
    if((sw=strchr(&req->uri[9],'/'))!=NULL) *sw=0;
    sw=&req->uri[9];
    ESP_LOGI(TAG,"sw=%s",sw);
    if(!strcmp(sw,"token"))
    {
    	char* token=malloc(req->content_len+1);
    	if((err=httpd_req_recv(req, token, req->content_len))<0)
    	{
			ESP_LOGE(TAG,"Error get content len=%d %s",req->content_len, esp_err_to_name(err));
			httpd_resp_send_err(req,500,"Bad Content string");
			return ESP_FAIL;
    	}
    	token[req->content_len]=0;
    	ESP_LOGI(TAG,"Get content=%s",token);
    	uint8_t found=0;
    	uname_free[0]=0;
    	for(uint8_t i=0;i<10;i++)
    	{
    		sprintf(uname,"%s_token%d",user,i);
    		if((found=isKeyExist(uname,token))==1) break;
    		if(found==0 && uname_free[0]==0) strcpy(uname_free,uname);
    	}
    	if(found!=1)
    	{
			Write_str_params(uname_free,token);
			Commit_params();
    	}
    	free(token);
     }
    httpd_resp_send(req, NULL,0);


    return ESP_OK;
}




static esp_err_t monitor_get_handler(httpd_req_t *req)
{
    char user[USERNAME_MAX];
    char role[ROLENAME_MAX];
    char join[2048];
    esp_err_t err=ESP_OK;
    uint8_t chunk;

    ESP_LOGI(TAG,"monitor/sessions handler");
	print_headers(req);
    if(!verifyBearer(req,user,role))
    {
	    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , "*");
    	httpd_resp_send_err(req,HTTPD_401_UNAUTHORIZED,"Unauthorized");
    	return ESP_FAIL;
    }


    /*    size_t req_path_len=httpd_req_get_url_query_len(req);
   	char* req_path=malloc(req_path_len+1);
    if((err=httpd_req_get_url_query_str(req, req_path, req_path_len))!=ESP_OK)
    {
    	ESP_LOGE(TAG,"No read url len=%d err=%s",req_path_len,esp_err_to_name(err));
    	httpd_resp_send_404(req);
    	return ESP_FAIL;
    }
    ESP_LOGI(TAG,"Monitor GET uri=%s",req_path);
    if(strstr(req_path,"/monitor/sessions")!=NULL)*/
//    {
//    	free(req_path);
    	chunk=0;
    	httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , "*");
    	for(uint8_t j=0;j<number_of_devices;j++)
    	{
    		if(networkSessions[j]!=NULL)
    		{
    			if(chunk==0) strcpy(join,"{\"Sessions\":[");
    			else strcpy(join,",");
    			if((err=getJsonData(user, role, networkSessions[j],join, 2048-strlen(join)))==ESP_OK)
				{
    				if((err=httpd_resp_send_chunk(req, join, strlen(join)))==ESP_OK)
					{
    					ESP_LOGI(TAG,"SEND chunk %d:\n%s", chunk, join);
    					chunk++;
    					continue;
					} else break;

 				} else break;
    		}
    	}
    	if(err==ESP_OK)
		{
    		strcpy(join,"]}");
    		err=httpd_resp_send_chunk(req, join, strlen(join));
    		if(err==ESP_OK)
    		{
				ESP_LOGI(TAG,"SEND chunk %d:\n%s", chunk, join);
				err=httpd_resp_send_chunk(req, NULL, 0);
    		}
		}
    	else ESP_LOGE(TAG,"error=%s",esp_err_to_name(err));
//    }
//    else free(req_path);
    return err;
}

static esp_err_t settings_post_handler(httpd_req_t *req)
{
    char user[USERNAME_MAX];
    char role[ROLENAME_MAX];
    char join[2048];
    uint32_t err0;
    uint8_t channel=0, bw=0,sf=0,crc=0,fec=0,pow=0,hm=0;
    uint32_t NetID=0;
    cJSON *par;


    if(!verifyBearer(req,user,role) || strcmp(role,"admin"))
    {
	    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , "*");
    	httpd_resp_send_err(req,HTTPD_401_UNAUTHORIZED,"Unauthorized");
    	return ESP_FAIL;
    }

    err0=0;
    int data_len=req->content_len;
    if(data_len<2048)
    {
    	int err=httpd_req_recv(req, join, 2048);
    	if(err==data_len)
    	{
    		join[data_len]=0;
    		ESP_LOGI(TAG,"Received data=%s",join);
    		cJSON *json_content = cJSON_Parse(join);
    		if(json_content!=NULL)
    		{
    			par = cJSON_GetObjectItemCaseSensitive(json_content,"CurChannel");
    			if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
    			{
    				channel=atoi(par->valuestring);
    				set_raw_par("CHANNEL",&channel);
    			}
    			else err0|=1;
    			par = cJSON_GetObjectItemCaseSensitive(json_content,"CurBW");
    			if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
    			{
    				bw=atoi(par->valuestring);
    				set_raw_par("BW",&bw);
    			}
    			else err0|=2;
    			par = cJSON_GetObjectItemCaseSensitive(json_content,"CurSF");
    			if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
    			{
    				sf=atoi(par->valuestring)+7;
    				set_raw_par("SF",&sf);
    			}
    			else err0|=4;
    			par = cJSON_GetObjectItemCaseSensitive(json_content,"CurCRC");
    			if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
    			{
    				crc=atoi(par->valuestring);
    				set_raw_par("CRC",&crc);
    			}
    			else err0|=8;
    			par = cJSON_GetObjectItemCaseSensitive(json_content,"CurFEC");
    			if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
    			{
    				fec=atoi(par->valuestring);
    				set_raw_par("FEC",&fec);
    			}
    			else err0|=16;
    			par = cJSON_GetObjectItemCaseSensitive(json_content,"CurPower");
    			if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
    			{
    				pow=atoi(par->valuestring);
    				set_raw_par("POWER",&pow);
    			}
    			else err0|=32;
    			par = cJSON_GetObjectItemCaseSensitive(json_content,"CurHM");
    			if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
    			{
    				hm=atoi(par->valuestring);
    				set_raw_par("HEADER_MODE",&hm);
    			}
    			else err0|=64;
    			par = cJSON_GetObjectItemCaseSensitive(json_content,"NetID");
    			if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
    			{
    				sscanf(par->valuestring,"0x%08X",&NetID);
    				set_raw_par("NETID",&NetID);
    			}
    			else err0|=128;
    			cJSON_Delete(json_content);
    			ESP_LOGI(TAG,"Channel=%d BW=%d SF=%d CRC=%d FEC=%d Power=%d HM=%d NetID=0x%08X",channel,bw,sf,crc,fec,pow,hm,NetID);
    			if(err0==0)
    			{
    				LORAX_Reset(ISM_RU864);
    				InitializeLorawan();
    			}
    		    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , "*");
    		    httpd_resp_send(req, NULL,0);
    		    return ESP_OK;
    		}
    	}
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , "*");
	httpd_resp_send_404(req);
	return ESP_FAIL;
}

static esp_err_t accounts_post_handler(httpd_req_t *req)
{
    char user[USERNAME_MAX];
    char role[ROLENAME_MAX];
    char join[2048];
    char username[PAR_STR_MAX_SIZE];
    char uname[16];
    esp_err_t err0;
    cJSON *par;
    uint8_t j0;
    uint8_t jfree;



    if(!verifyBearer(req,user,role) || strcmp(role,"admin"))
    {
	    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , "*");
    	httpd_resp_send_err(req,HTTPD_401_UNAUTHORIZED,"Unauthorized");
    	return ESP_FAIL;
    }

    err0=0;
    int data_len=req->content_len;
    if(data_len<2048)
    {
    	int err=httpd_req_recv(req, join, 2048);
    	if(err==data_len)
    	{
    		join[data_len]=0;
    		ESP_LOGI(TAG,"Received data=%s",join);
    		cJSON *json_content = cJSON_Parse(join);
    		if(json_content!=NULL)
    		{
    			par = cJSON_GetObjectItemCaseSensitive(json_content,"Username");
    			if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
    			{
   					strcpy(username,par->valuestring);
   					j0=MAX_USERS;
   					jfree=0;
   				    for(uint8_t j=1;j<MAX_USERS;j++)
   				    {
   						sprintf(uname,"USR%d",j);
   						if((err0=Read_str_params(uname,user, USERNAME_MAX))!=ESP_OK)
   						{
   							if(err0==ESP_ERR_NVS_NOT_FOUND && jfree==0) jfree=j;
   							continue;
   						}
   						if(!strcmp(user,username))
   						{
   							j0=j;
   							break;
   						}
   				    }
   				    ESP_LOGI(TAG,"Username=%s j0=%d jfree=%d",username,j0,jfree);
   				    if(j0!=MAX_USERS || jfree!=0)
   				    {
						par = cJSON_GetObjectItemCaseSensitive(json_content,"Delete");
						if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
						{
							if(!strcmp(par->valuestring,"true"))
							{
								deleteAccount(j0);
								ESP_LOGI(TAG,"Account %s deleted",username);
							}

						}
						else
						{
							if(j0==MAX_USERS && jfree!=0) j0=jfree;
							sprintf(uname,"USR%d",j0);
							Write_str_params(uname,username);

							par = cJSON_GetObjectItemCaseSensitive(json_content,"FirstName");
							if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
							{
								sprintf(uname,"FirstName%d",j0);
								Write_str_params(uname,par->valuestring);
							}
							par = cJSON_GetObjectItemCaseSensitive(json_content,"LastName");
							if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
							{
								sprintf(uname,"LastName%d",j0);
								Write_str_params(uname,par->valuestring);
							}
							par = cJSON_GetObjectItemCaseSensitive(json_content,"Password");
							if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
							{
								sprintf(uname,"PWD%d",j0);
								Write_str_params(uname,par->valuestring);
							}
							Commit_params();
						}
		    		    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , "*");
		    		    httpd_resp_send(req, NULL,0);
		    		    cJSON_Delete(json_content);
		    		    return ESP_OK;
   				    }
    			}
    			cJSON_Delete(json_content);
    		}
    	}
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , "*");
	httpd_resp_send_404(req);
	return ESP_FAIL;
}

static esp_err_t devices_post_handler(httpd_req_t *req)
{
    char user[USERNAME_MAX];
    char role[ROLENAME_MAX];
    char join[2048];
	GenericEui_t eui;
	GenericEui_t devEui;
    uint8_t AppKey[16];
    uint8_t NwkKey[16];
    char uname[16];
    esp_err_t err0;
    cJSON *par;
    cJSON *item;
    uint8_t j0;
    uint8_t jfree;
    uint8_t version;
    char devUser[USERNAME_MAX];
    uint8_t users[8];




    if(!verifyBearer(req,user,role) || strcmp(role,"admin"))
    {
	    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , "*");
    	httpd_resp_send_err(req,HTTPD_401_UNAUTHORIZED,"Unauthorized");
    	return ESP_FAIL;
    }

    err0=0;
    int data_len=req->content_len;
    if(data_len<2048)
    {
    	int err=httpd_req_recv(req, join, 2048);
    	if(err==data_len)
    	{
    		join[data_len]=0;
    		ESP_LOGI(TAG,"Received data=%s",join);
    		cJSON *json_content = cJSON_Parse(join);
    		if(json_content!=NULL)
    		{
    			par = cJSON_GetObjectItemCaseSensitive(json_content,"DevEUI");
    			if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
    			{
                    sscanf(par->valuestring,"%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX",(uint8_t*)(&eui.buffer[0]),(uint8_t*)(&eui.buffer[1]),(uint8_t*)(&eui.buffer[2]),(uint8_t*)(&eui.buffer[3]),(uint8_t*)(&eui.buffer[4]),(uint8_t*)(&eui.buffer[5]),(uint8_t*)(&eui.buffer[6]),(uint8_t*)(&eui.buffer[7]));
   					j0=MAX_NUMBER_OF_DEVICES;
   					jfree=0;
   				    for(uint8_t j=1;j<MAX_NUMBER_OF_DEVICES;j++)
   				    {
   						sprintf(uname,"Dev%dEui",j);
   						if((err0=Read_eui_params(uname,devEui.buffer))!=ESP_OK)
   						{
   							if(err0==ESP_ERR_NVS_NOT_FOUND && jfree==0) jfree=j;
   							continue;
   						}
   						if(!euicmp(&eui,&devEui))
   						{
   							j0=j;
   							break;
   						}
   				    }
   				    ESP_LOGI(TAG,"Eui=0x%16llX j0=%d jfree=%d",*((uint64_t*)(eui.buffer)),j0,jfree);
   				    if(j0!=MAX_NUMBER_OF_DEVICES || jfree!=0)
   				    {
						par = cJSON_GetObjectItemCaseSensitive(json_content,"Delete");
						if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
						{
							if(!strcmp(par->valuestring,"true"))
							{
								deleteDevice(j0);
								ESP_LOGI(TAG,"Device 0x%16llX deleted",*((uint64_t*)(eui.buffer)));
							}

						}
						else
						{
							if(j0==MAX_NUMBER_OF_DEVICES && jfree!=0) j0=jfree;
							sprintf(uname,"Dev%dEui",j0);
							Write_eui_params(uname,eui.buffer);

							par = cJSON_GetObjectItemCaseSensitive(json_content,"AppKey");
							if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
							{
								for(uint i=0;i<16;i++) sscanf(&((char*)par->valuestring)[2*i],"%02hhX",&AppKey[i]);
								sprintf(uname,"Dev%dAppKey",j0);
								Write_key_params(uname,AppKey);
							}
							par = cJSON_GetObjectItemCaseSensitive(json_content,"NwkKey");
							if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
							{
								for(uint i=0;i<16;i++) sscanf(&((char*)par->valuestring)[2*i],"%02hhX",&NwkKey[i]);
								sprintf(uname,"Dev%dNwkKey",j0);
								Write_key_params(uname,NwkKey);
							}
							par = cJSON_GetObjectItemCaseSensitive(json_content,"DevName");
							if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
							{
								sprintf(uname,"Dev%dName",j0);
								Write_str_params(uname,par->valuestring);
							}
							par = cJSON_GetObjectItemCaseSensitive(json_content,"Version");
							if(par!=NULL && cJSON_IsString(par) && par->valuestring!=NULL )
							{
								sscanf(par->valuestring,"%hhd",&version);
								sprintf(uname,"Dev%dVersion",j0);
								Write_u8_params(uname,version);
							}
							par = cJSON_GetObjectItemCaseSensitive(json_content,"Users");
							if(par!=NULL && cJSON_IsArray(par))
							{
								bzero(users,8);
								uint8_t k1=0;
								cJSON_ArrayForEach(item,par)
								{
									if(item!=NULL && cJSON_IsString(item) && item->valuestring!=NULL)
									{
										for(uint8_t k=1;k<MAX_USERS;k++)
										{
											sprintf(uname,"USR%d",k);
					   						if( (err0=Read_str_params(uname,devUser, USERNAME_MAX))==ESP_OK && !strcmp(item->valuestring,devUser) && k1<8 ) users[k1++]=k;
										}
									}
								}
								sprintf(uname,"Dev%dUsers",j0);
								Write_eui_params(uname,users);
							}
							Commit_params();
						}
						fill_devices1();
		    		    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , "*");
		    		    httpd_resp_send(req, NULL,0);
		    		    cJSON_Delete(json_content);
		    		    return ESP_OK;
   				    }
    			}
		        cJSON_Delete(json_content);
    		}
    	}
    }
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin" , "*");
	httpd_resp_send_404(req);
	return ESP_FAIL;
}




/*
 * static const httpd_uri_t monitor = {
    .uri       = "/monitor",
    .method    = HTTP_GET,
    .handler   = monitor_get_handler
};
*/
static const httpd_uri_t login = {
    .uri       = "/login",
    .method    = HTTP_GET,
    .handler   = login_get_handler,
    .user_ctx = NULL
};

static const httpd_uri_t systemx = {
    .uri       = "/system",
    .method    = HTTP_GET,
    .handler   = system_get_handler,
    .user_ctx = NULL
};

static const httpd_uri_t settings = {
    .uri       = "/settings",
    .method    = HTTP_GET,
    .handler   = settings_get_handler,
    .user_ctx = NULL
};

static const httpd_uri_t devices = {
    .uri       = "/devices",
    .method    = HTTP_GET,
    .handler   = devices_get_handler,
    .user_ctx = NULL
};

static const httpd_uri_t devices2 = {
    .uri       = "/devices",
    .method    = HTTP_POST,
    .handler   = devices_post_handler,
    .user_ctx = NULL
};

static const httpd_uri_t accounts = {
    .uri       = "/accounts",
    .method    = HTTP_GET,
    .handler   = accounts_get_handler,
    .user_ctx = NULL
};

static const httpd_uri_t accounts2 = {
    .uri       = "/accounts",
    .method    = HTTP_POST,
    .handler   = accounts_post_handler,
    .user_ctx = NULL
};

static const httpd_uri_t settings2 = {
    .uri       = "/settings",
    .method    = HTTP_POST,
    .handler   = settings_post_handler,
    .user_ctx = NULL
};

static const httpd_uri_t options = {
    .uri       = "/*",
    .method    = HTTP_OPTIONS,
    .handler   = options_handler,
    .user_ctx = NULL
};

/*static esp_err_t root_get_handler(httpd_req_t *req)
{
	ESP_LOGI("MyServer","root handler");
	httpd_resp_set_type(req, "text/html");
	httpd_resp_send(req, "<h1>Hello Secure World!</h1>", HTTPD_RESP_USE_STRLEN);
	return ESP_OK;
}*/

static httpd_uri_t common_get_uri = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = rest_common_get_handler,
    .user_ctx = &rest_context
};

static httpd_uri_t monitor_get_uri = {
    .uri = "/monitor/sessions",
    .method = HTTP_GET,
    .handler = monitor_get_handler,
    .user_ctx = &rest_context
};

static httpd_uri_t monitor_post_uri = {
    .uri = "/monitor/*",
    .method = HTTP_POST,
    .handler = monitor_post_handler,
    .user_ctx = &rest_context
};


httpd_handle_t start_my_server(void)
{
	httpd_handle_t my_server = NULL;
    rest_context = calloc(1, sizeof(rest_server_context_t));
    REST_CHECK(rest_context, "No memory for rest context", err_start);
    strlcpy(rest_context->base_path, MOUNT_POINT, sizeof(rest_context->base_path));
    strlcat(rest_context->base_path,"/server", sizeof(rest_context->base_path));

    // Start the httpd server
    ESP_LOGI(TAG, "Starting My Server");

    httpd_ssl_config_t conf = HTTPD_SSL_CONFIG_DEFAULT();
    conf.httpd.stack_size=6144;
    conf.httpd.uri_match_fn=httpd_uri_match_wildcard;
    conf.httpd.max_uri_handlers=16;
    conf.port_insecure = 0xffff;
    conf.httpd.core_id = 0;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size=6144;
    config.uri_match_fn=httpd_uri_match_wildcard;
    config.max_uri_handlers=16;

    extern const unsigned char cacert_pem_start[] asm("_binary_mm304_asuscomm_com_der_start");
    extern const unsigned char cacert_pem_end[]   asm("_binary_mm304_asuscomm_com_der_end");
    conf.cacert_pem = cacert_pem_start;
    conf.cacert_len = cacert_pem_end - cacert_pem_start;

    extern const unsigned char prvtkey_pem_start[] asm("_binary_mm304_asuscomm_com_key_start");
    extern const unsigned char prvtkey_pem_end[]   asm("_binary_mm304_asuscomm_com_key_end");
    conf.prvtkey_pem = prvtkey_pem_start;
    conf.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;

    esp_err_t ret = httpd_ssl_start(&my_server, &conf);
//    esp_err_t ret = httpd_start(&my_server, &config);
    if (ESP_OK == ret) {
   // Set URI handlers
		ESP_LOGI(TAG, "Registering URI handlers");
		httpd_register_uri_handler(my_server, &login);
		httpd_register_uri_handler(my_server, &options);
		httpd_register_uri_handler(my_server, &settings);
		httpd_register_uri_handler(my_server, &accounts);
		httpd_register_uri_handler(my_server, &devices);
		httpd_register_uri_handler(my_server, &accounts2);
		httpd_register_uri_handler(my_server, &settings2);
		httpd_register_uri_handler(my_server, &devices2);
		httpd_register_uri_handler(my_server, &systemx);
		httpd_register_uri_handler(my_server, &monitor_get_uri);
		httpd_register_uri_handler(my_server, &monitor_post_uri);
		httpd_register_uri_handler(my_server, &common_get_uri);
    }
    else
	{
    	ESP_LOGI(TAG, "Error starting my_server!");
    	return NULL;
	}
    return my_server;
err_start:
        free(rest_context);
        return NULL;
}

void stop_my_server(httpd_handle_t my_server)
{
    // Stop the httpd server
	    httpd_ssl_stop(my_server);
//	    httpd_stop(my_server);
        free(rest_context);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* my_server = (httpd_handle_t*) arg;
    if (*my_server) {
        stop_my_server(*my_server);
        *my_server = NULL;
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* my_server = (httpd_handle_t*) arg;
    if (*my_server == NULL) {
        start_my_server();
    }
}
