/*
 * lorax.c
 *
 *  Created on: 24 июл. 2021 г.
 *      Author: ilya_000
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "lorax.h"
#include "aes.h"
#include "cmd_nvs.h"
#include "esp_log.h"
#include "esp_event.h"
#include "keys.h"
#include "MainLoop.h"
#include "sx1276_radio_driver.h"
#include "channels.h"
#include "shell.h"
#include "driver/periph_ctrl.h"
#include "request.h"
#include "storage.h"
#include "dirent.h"
#include "../../main/main.h"


NetworkServer_t networkServer;
//JoinServer_t joinServer;
uint8_t number_of_devices;
extern EndDevice_t* endDevices[MAX_NUMBER_OF_DEVICES];
NetworkSession_t *networkSessions[MAX_NUMBER_OF_DEVICES];
//uint8_t number_of_networkSessions;
uint32_t NetID,NwkID,NwkID_mask;
uint8_t NwkID_type;
ESP_EVENT_DECLARE_BASE(LORA_SESSION_TIMER_EVENTS);
ESP_EVENT_DECLARE_BASE(LORA_EVENTS);
extern volatile RadioMode_t macState;
LoRa_t loRa;
extern ChannelParams_t Channels[MAX_RU_SINGLE_BAND_CHANNELS];
extern uint8_t mode;
extern esp_event_loop_handle_t mainLoop;
uint8_t aesBuffer[AES_BLOCKSIZE];
uint8_t macBuffer[MAXIMUM_BUFFER_LENGTH];
uint8_t blockBuffer[MAXIMUM_BUFFER_LENGTH];
uint8_t radioBuffer[RADIO_BUFFER_MAX];
static Data_t data;
extern TimerHandle_t startTimerId;
extern GenericEui_t joinEui;


// Spreading factor possibilities
static const uint8_t spreadingFactor[] = {12, 11, 10, 9, 8, 7, 7};

// Bandwidth possibilities
static const uint8_t bandwidth[] = {BW_125KHZ, BW_125KHZ, BW_125KHZ, BW_125KHZ, BW_125KHZ, BW_125KHZ, BW_250KHZ};

// Modulation possibilities
static const uint8_t modulation[] = {MODULATION_LORA, MODULATION_LORA, MODULATION_LORA, MODULATION_LORA, MODULATION_LORA, MODULATION_LORA, MODULATION_LORA, MODULATION_FSK};

static const uint8_t FskSyncWordBuff[3] = {0xC1, 0x94, 0xC1};
static char TAG[]={"lorax"};

extern const int8_t txPower868[];
extern const int8_t txPowerRU864[];
extern const int8_t txPower433[];


void InitializeLorawan(void)
{
	number_of_devices=0;
	fill_devices1();
	periph_module_enable(PERIPH_AES_MODULE);
	set_s("JOINEUI",joinEui.buffer);
    ESP_LOGI(TAG,"joineui 0x%016llX",joinEui.eui);
}


void CreateAllTimers(void)
{

}

void StopAllSoftwareTimers ()
{

}

uint32_t get_nextDevAddr(DeviceAddress_t* devaddr)
{
    devaddr->value++;
    devaddr->value&=~NwkID_mask;
    devaddr->value|=NwkID;
    ESP_LOGI("get_nextDevAddr","DevAddr=0x%08x",devaddr->value);
    return devaddr->value;
}

void calculate_NwkID(uint32_t NetID)
{
    uint8_t ids[8]={6,6,9,11,12,13,15,17};
    NwkID_mask=1;
    NwkID_type=(NetID&0x00E00000)>>21;
    for(uint8_t j=0;j<ids[NwkID_type]-1;j++)
    {
        NwkID_mask<<=1;
        NwkID_mask|=0x00000001;
    }
    NwkID=(NetID&NwkID_mask)<<(23-ids[NwkID_type]);
    NwkID_mask=(NwkID_mask<<1|0x00000001)<<(23-ids[NwkID_type]);
    for(uint8_t j=0;j<NwkID_type;j++)
    {
        NwkID=(NwkID>>1)|0x00800000;
        NwkID_mask=(NwkID_mask>>1)|0x00800000;
    }
//    send_chars("NwkID=");
//    send_chars(ui32tox(NwkID,b));
//    send_chars(" NwkID_mask=");
//    send_chars(ui32tox(NwkID_mask,b));
//    send_chars("\r\n");
}

static uint32_t ExtractMic (uint8_t *buffer, uint8_t bufferLength)
{
     uint32_t mic = 0;
     memcpy (&mic, &buffer[bufferLength - 4], sizeof (mic));
     return mic;
}

static uint32_t ComputeMic ( uint8_t *key, uint8_t* buffer, uint8_t bufferLength)  // micType is 0 for join request and 1 for data packet
{
    uint32_t mic = 0;
    uint8_t aesBuffer[AES_BLOCKSIZE];

//    ESP_LOGI(TAG,"Before compute mic");
    AESCmac(key, aesBuffer, buffer, bufferLength); //if micType is 0, bufferLength the same, but for data messages bufferLength increases with 16 because a block is added
//    ESP_LOGI(TAG,"After compute mic");

    memcpy(&mic, aesBuffer, sizeof( mic ));

    return mic;
}


LorawanError_t initNetworkSession(NetworkSession_t* networkSession , EndDevice_t* endDevice, NetworkServer_t* networkServer, const GenericEui_t joinEui)
{
    //protocol parameters receive the default values
    networkSession->protocolParameters.receiveDelay1 = RECEIVE_DELAY1;
    networkSession->protocolParameters.receiveDelay2 = RECEIVE_DELAY2;
    networkSession->protocolParameters.joinAcceptDelay1 = JOIN_ACCEPT_DELAY1;
    networkSession->protocolParameters.joinAcceptDelay2 = JOIN_ACCEPT_DELAY2;
    networkSession->protocolParameters.ackTimeout = ACK_TIMEOUT;
    networkSession->protocolParameters.adrAckDelay = ADR_ACK_DELAY;
    networkSession->protocolParameters.adrAckLimit = ADR_ACK_LIMIT;
    networkSession->protocolParameters.maxFcntGap = MAX_FCNT_GAP;
    networkSession->protocolParameters.maxMultiFcntGap = MAX_MCAST_FCNT_GAP;
	networkSession->endDevice=endDevice;
	networkSession->networkServer=networkServer;
	networkSession->joinEui.eui=joinEui.eui;
    networkSession->joinNonce=getinc_JoinNonce();
    if(endDevice->version==1) networkSession->dlSettings.bits.version_1dot1=1;
    else networkSession->dlSettings.bits.version_1dot1=0;
    networkSession->dlSettings.bits.rx1DROffset=0;
    networkSession->dlSettings.bits.rx2DataRate=DR0;
    networkSession->devAddr.value=get_nextDevAddr(&(networkSession->networkServer->lastDevAddr));
    networkSession->cflist_present=0;
    memset(networkSession->cflist,0,16);
    networkSession->FCntUp.value=0;
    networkSession->NFCntDown.value = 0;
    networkSession->AFCntDown.value = 0;
    networkSession->rxDelay.delay=1;
    computeKeys(networkSession);
    networkSession->flags.value=0;

    networkSession->sendAnswerTimerId=malloc(sizeof(SessionTimer_t));
    networkSession->sendAnswerTimerId->event=0xFF;
    networkSession->sendAnswerTimerId->networkSession=(void*)networkSession;
    networkSession->sendAnswerTimerId->timer=xTimerCreate("sendAnswerTimerId",86400000,pdFALSE,networkSession->sendAnswerTimerId, LORAX_SendAnswerCallback);

    networkSession->sendMessageTimer=xTimerCreate("messageTimer", 2000 / portTICK_PERIOD_MS, pdFALSE, (void*)networkSession, messagePrepare);

    struct stat buf = {0};
    bool dir_exists=false;
    sprintf(networkSession->dir,"%s/R%016llx",MOUNT_POINT,networkSession->endDevice->devEui.eui);
    if(stat(networkSession->dir,&buf)!=-1)
    {
    	if ((buf.st_mode & S_IFMT) != S_IFDIR)
    	{
    		remove(networkSession->dir);
    	}
    	else dir_exists=true;
    }
    ESP_LOGI(TAG,"Directory %s %s",networkSession->dir,dir_exists ? "exists" : "doesnt exist");
    if(!dir_exists) mkdir(networkSession->dir, 0700);

    struct dirent* ep;
    DIR* dir=opendir(MOUNT_POINT);
	if (dir != NULL)
	{
	  while ((ep = readdir (dir))!=NULL)
	  {
		  ESP_LOGI(TAG,"Directory/File %s/%s",MOUNT_POINT,ep->d_name);
	  }
	  closedir (dir);
	}


    return LORA_OK;
}

void freeNetworkSession(NetworkSession_t** networkSession)
{
	if(networkSession==NULL || *networkSession==NULL) return;
	xTimerDelete((*networkSession)->sendAnswerTimerId->timer,0);
	free((*networkSession)->sendAnswerTimerId);
	xTimerDelete((*networkSession)->sendMessageTimer,0);
	free(*networkSession);
	*networkSession=NULL;
}

void LORAX_SendAnswerCallback ( TimerHandle_t xExpiredTimer )
{
    char TAG[]="LORAX_SendAnswerCallback";
    NetworkSession_t* networkSession;

    SessionTimer_t* sessionTimer=(SessionTimer_t*)pvTimerGetTimerID(xExpiredTimer);
    networkSession=sessionTimer->networkSession;
	if(macState==MODE_RXCONT)
	{
		RADIO_ReceiveStop();
		ESP_LOGI(TAG," Receiving stopped");
	}
	SelectChannelForTransmission(0,DIR_DOWNLINK);

	RADIO_SetWatchdogTimeout(5000);
	ESP_LOGI(TAG,"Network Session %d: Answer ready to transmit for device eui 0x%016llx",networkSession->sessionNumber, networkSession->endDevice->devEui.eui);
	if (RADIO_Transmit (networkSession->macBuffer, networkSession->bufferIndex) == ERR_NONE)
	{
		ESP_LOGI(TAG,"Answer transmission for device eui 0x%016llx length=%d initialized",networkSession->endDevice->devEui.eui,networkSession->bufferIndex);
		networkSession->macState = TRANSMISSION_OCCURRING; // set the state of MAC to transmission occurring. No other packets can be sent afterwards
		loRa.curentTransmitNetworkSession=networkSession->sessionNumber;
	}
	else
	{
		ESP_LOGE(TAG,"Transmission not ready");
		networkSession->macState = MAC_STATE_NOT_READY_FOR_TRANSMISSION;
	}
}


static void AssembleEncryptionBlock (uint8_t dir, uint32_t frameCounter, uint8_t blockId, uint8_t firstByte, DeviceAddress_t devAddr,uint16_t ConfFCnt)
{
    uint8_t bufferIndex = 0;

    memset (aesBuffer, 0, sizeof (aesBuffer)); //clear the aesBuffer

    aesBuffer[bufferIndex++] = firstByte;

    memcpy(&aesBuffer[bufferIndex],&ConfFCnt,sizeof(ConfFCnt));

    bufferIndex = bufferIndex + sizeof(ConfFCnt) +2;  // 4 bytes of 0x00 (done with memset at the beginning of the function)

    aesBuffer[bufferIndex++] = dir;

    memcpy(&aesBuffer[bufferIndex],&devAddr.value,sizeof(devAddr.value));
    bufferIndex+=sizeof(devAddr.value);

    /*if (DISABLED == multicastStatus)
    {
        memcpy (&aesBuffer[bufferIndex], &loRa.activationParameters.deviceAddress, sizeof (loRa.activationParameters.deviceAddress));
        bufferIndex = bufferIndex + sizeof (loRa.activationParameters.deviceAddress);
    }
    else
    {
        memcpy (&aesBuffer[bufferIndex], &loRa.activationParameters.mcastDeviceAddress, sizeof (loRa.activationParameters.mcastDeviceAddress));
        bufferIndex = bufferIndex + sizeof (loRa.activationParameters.mcastDeviceAddress);
    }*/

    memcpy (&aesBuffer[bufferIndex], &frameCounter, sizeof (frameCounter));
    bufferIndex = bufferIndex + sizeof (frameCounter) ;

    bufferIndex ++;   // 1 byte of 0x00 (done with memset at the beginning of the function)

    aesBuffer[bufferIndex] = blockId;
}



static void EncryptFRMPayload (uint8_t* buffer, uint8_t bufferLength, uint8_t dir, uint32_t frameCounter, uint8_t* key, uint8_t macBufferIndex, uint8_t* bufferToBeEncrypted, DeviceAddress_t devAddr)
{
    uint8_t k = 0, i = 0, j = 0;

    k = bufferLength / AES_BLOCKSIZE;
    for (i = 1; i <= k; i++)
    {
        AssembleEncryptionBlock (dir, frameCounter, i, 0x01, devAddr, 0);
        AESEncodeLoRa(aesBuffer, key);
        for (j = 0; j < AES_BLOCKSIZE; j++)
        {
            bufferToBeEncrypted[macBufferIndex++] = aesBuffer[j] ^ buffer[AES_BLOCKSIZE*(i-1) + j];
        }
    }

    if ( (bufferLength % AES_BLOCKSIZE) != 0 )
    {
        AssembleEncryptionBlock (dir, frameCounter, i, 0x01, devAddr, 0);
        AESEncodeLoRa (aesBuffer, key);

        for (j = 0; j < (bufferLength % AES_BLOCKSIZE); j++)
        {
            bufferToBeEncrypted[macBufferIndex++] = aesBuffer[j] ^ buffer[(AES_BLOCKSIZE*k) + j];
        }
    }
}



uint8_t PrepareJoinAcceptFrame (NetworkSession_t* networkSession, uint8_t *macBuffer)
{
    Mhdr_t mhdr;
    uint32_t mic;
    uint8_t temp;
    uint8_t bufferIndex = 0;
    uint8_t* micBuffer;


    memset (macBuffer, 0, 64 );  // clear the mac buffer

    mhdr.bits.mType = FRAME_TYPE_JOIN_ACCEPT;  //prepare the mac header to include mtype as frame type join request
    mhdr.bits.major = MAJOR_VERSION3;
    mhdr.bits.rfu = RESERVED_FOR_FUTURE_USE;

    macBuffer[bufferIndex++] = mhdr.value;  // add the mac header to the buffer

    memcpy(&macBuffer[bufferIndex],&networkSession->joinNonce,JA_JOIN_NONCE_SIZE);
    bufferIndex = bufferIndex + JA_JOIN_NONCE_SIZE;

    memcpy(&macBuffer[bufferIndex],&networkSession->networkServer->netID,JA_NET_ID_SIZE);
    bufferIndex = bufferIndex + JA_NET_ID_SIZE;

    memcpy(&macBuffer[bufferIndex],networkSession->devAddr.buffer,sizeof(networkSession->devAddr.buffer));
    bufferIndex = bufferIndex + sizeof(networkSession->devAddr.buffer);

    memcpy(&macBuffer[bufferIndex],&(networkSession->dlSettings.value),sizeof(networkSession->dlSettings.value));
    bufferIndex = bufferIndex + sizeof(networkSession->dlSettings.value);

    memcpy(&macBuffer[bufferIndex],&networkSession->rxDelay,sizeof(Delay_t));
    bufferIndex = bufferIndex + sizeof(Delay_t);

    if(networkSession->cflist_present)
   	{
    	memcpy(&macBuffer[bufferIndex],networkSession->cflist,sizeof(networkSession->cflist));
    	bufferIndex = bufferIndex + sizeof(networkSession->cflist);
   	}

    uint8_t i=0;
    if(networkSession->endDevice->version)
    {
    	micBuffer=malloc(bufferIndex+16);
    	micBuffer[i++]=0xFF;
    	for(uint8_t j=0;j<8;j++) micBuffer[i+j]=networkSession->joinEui.buffer[7-j];
    	i+=8;
    	memcpy(&micBuffer[i],&networkSession->endDevice->devNonce,sizeof(networkSession->endDevice->devNonce));
    	i+=2;
    	memcpy(&micBuffer[i],macBuffer,bufferIndex);
    	i+=bufferIndex;
    	mic=ComputeMic (networkSession->JSIntKey, micBuffer, i);
    	free(micBuffer);
    } else mic = ComputeMic (networkSession->endDevice->NwkKey, macBuffer, bufferIndex);

    memcpy ( &macBuffer[bufferIndex], &mic, sizeof (mic));
    bufferIndex = bufferIndex + sizeof(mic);

    temp = bufferIndex-1;
    while (temp > 0)
    {
       //Decode message
       AESDecodeLoRa (&macBuffer[bufferIndex-temp], networkSession->endDevice->NwkKey);
       if (temp > AES_BLOCKSIZE)
       {
           temp -= AES_BLOCKSIZE;
       }
       else
       {
           temp = 0;
       }
    }
    return bufferIndex;
}



uint8_t PrepareAckFrame (NetworkSession_t* networkSession, uint8_t *macBuffer)
{
    Mhdr_t mhdr;
    uint32_t mic;
    uint8_t bufferIndex = 0;


    memset (macBuffer, 0, 64 );  // clear the mac buffer

    mhdr.bits.mType = FRAME_TYPE_DATA_UNCONFIRMED_DOWN;  //prepare the mac header to include mtype as frame type join request
    if(networkSession->flags.REQUEST_CONFIRMED_ANSWER)
    {
    	mhdr.bits.mType = FRAME_TYPE_DATA_CONFIRMED_DOWN;
    	networkSession->flags.REQUEST_CONFIRMED_ANSWER=0;
    }
    mhdr.bits.major = MAJOR_VERSION3;
    mhdr.bits.rfu = RESERVED_FOR_FUTURE_USE;

    macBuffer[bufferIndex++] = mhdr.value;  // add the mac header to the buffer

    memcpy(&macBuffer[bufferIndex],&networkSession->devAddr.value,sizeof(DeviceAddress_t));
    bufferIndex = bufferIndex + sizeof(DeviceAddress_t);

    FCtrl_t fctrl;
    fctrl.value=0;
    if(networkSession->flags.REQUEST_SET_ACK)
    {
    	fctrl.bits.ack=1;
    	networkSession->flags.REQUEST_SET_ACK=0;
    }
    macBuffer[bufferIndex++]=fctrl.value;

    uint16_t fcnt=networkSession->NFCntDown.members.valueLow;
    *((uint16_t*)(&macBuffer[bufferIndex]))=fcnt;
    bufferIndex+=2;

    AssembleEncryptionBlock (DIR_DOWNLINK, networkSession->NFCntDown.value, bufferIndex, 0x49, networkSession->devAddr,networkSession->endDevice->version==0 ? 0 : networkSession->FCntUp.members.valueLow);
    memcpy (&radioBuffer[0], aesBuffer, sizeof (aesBuffer));
    memcpy (&radioBuffer[16], macBuffer, bufferIndex);
    AESCmac(networkSession->FNwkSIntKey, aesBuffer, &radioBuffer[0], bufferIndex + sizeof (aesBuffer));
    memcpy ( &macBuffer[bufferIndex], aesBuffer, sizeof (mic));
    bufferIndex = bufferIndex + sizeof(mic);
    networkSession->NFCntDown.value++;
    networkSession->AFCntDown.value=networkSession->NFCntDown.value;
    return bufferIndex;
}

LorawanError_t LORAX_RxDone (uint8_t* buffer, uint8_t bufferLength, int16_t rssi, int8_t snr)
{
    uint32_t computedMic, extractedMic;
    Mhdr_t mhdr;
    char TAG[]="LORAX_RxDone";
    NetworkSession_t* networkSession;
    uint8_t sessionNumber;
    uint32_t exp;
    uint8_t repeat;

    ESP_LOGI(TAG,"Received frame type=0x%02x",(buffer[0]&0xE0)>>5);
    for(uint8_t k=0;k<bufferLength;k++) printf(" %02X",buffer[k]);
    printf("\n");
    mhdr.value = buffer[0];
    if (  mhdr.bits.mType == FRAME_TYPE_JOIN_REQ )
    {

        JoinRequest_t* joinRequest=(JoinRequest_t*)buffer;
        ESP_LOGI(TAG,"Received Join EUI=%016llx My Join EUI=%016llx",*((uint64_t*)(&buffer[1])),joinEui.eui);
        if(euicmpr(&buffer[1],&joinEui))
        {
            ESP_LOGE(TAG,"Received Join EUI=%016llx not equal my Join EUI=%016llx",joinRequest->joinEui.eui,joinEui.eui);
            return INVALID_JOIN_EUI;
        }
        ESP_LOGI(TAG,"Received Device EUI=%016llx",joinRequest->devEui.eui);
        uint8_t j;
        EndDevice_t* endDevice=NULL;
        for(j=0;j<number_of_devices;j++)
        {
            if(!euicmpr(&(joinRequest->devEui),&(endDevices[j]->devEui)))
            {
                endDevice=endDevices[j];
                ESP_LOGI(TAG,"Join Request from device number %d",j);
                break;
            }
        }
        if(j==number_of_devices)
        {
            ESP_LOGE(TAG,"Received EndDevice EUI=%016llx not found in base of my devices (registered %d devices)",joinRequest->devEui.eui, number_of_devices);
            for(j=0;j<number_of_devices;j++) ESP_LOGI(TAG,"Device %d Eui=%016llx",j,endDevices[j]->devEui.eui);
            return UNKNOWN_DEVICE;
        }
        computedMic = ComputeMic (endDevice->NwkKey, buffer, bufferLength - sizeof(extractedMic));
        extractedMic = ExtractMic (buffer, bufferLength);
        if (extractedMic != computedMic)
        {
            ESP_LOGE(TAG,"BAD MIC");
            return INVALID_MIC;
        }
        ESP_LOGI(TAG,"MIC OK");
        if(joinRequest->devNonce > endDevice->devNonce)
        {
            sessionNumber=j;
            endDevice->devNonce=joinRequest->devNonce;
            put_DevNonce(sessionNumber, endDevice->devNonce);
        }
        else
        {
            ESP_LOGE(TAG,"Wrong devnonce: received=0x%04X, Prev=0x%04X",joinRequest->devNonce, endDevice->devNonce);
        	return DEVICE_DEVNONCE_ERROR;
        }
        if(networkSessions[sessionNumber]!=NULL)
        {
        	freeNetworkSession(&networkSessions[sessionNumber]);
        }
        networkSessions[sessionNumber]=malloc(sizeof(NetworkSession_t));
        memset(networkSessions[sessionNumber],0,sizeof(NetworkSession_t));
        ESP_LOGI(TAG,"Create new network session %d",sessionNumber);
        networkSession=networkSessions[sessionNumber];
        networkSession->sessionNumber=sessionNumber;
        initNetworkSession(networkSession , endDevice, &networkServer, joinEui);
        networkSession->macState = BEFORE_TX1;

        networkSession->sendAnswerTimerId->event=SEND_JOIN_ACCEPT_1_TIMER;
        exp=START_TIMER_VALUE - xTimerGetExpiryTime(startTimerId) + xTaskGetTickCount();
        xTimerChangePeriod(networkSession->sendAnswerTimerId->timer,(networkSession->protocolParameters.joinAcceptDelay1-20)/portTICK_PERIOD_MS-exp, 0);
        xTimerStart(networkSession->sendAnswerTimerId->timer, 0);

        networkSession->bufferIndex=PrepareJoinAcceptFrame (networkSession, networkSession->macBuffer);
        ESP_LOGI(TAG,"NetworkSession %d:Prepared JoinAcceptFrame for devEUI=%016llx length=%d",sessionNumber, networkSession->endDevice->devEui.eui, networkSession->bufferIndex);
        return LORA_OK;
    }
    else if ( mhdr.bits.mType == FRAME_TYPE_DATA_UNCONFIRMED_UP || mhdr.bits.mType == FRAME_TYPE_DATA_CONFIRMED_UP )
    {
        Hdr_t* hdr=(Hdr_t*)buffer;
//        ESP_LOGI(TAG, "FRAME_TYPE_DATA_UP hdr:%016llx",*((uint64_t*)buffer));
//        for(uint8_t i=0;i<bufferLength;i++) printf("0x%02X ",buffer[i]);
//        printf("\n");
        networkSession=NULL;
        time_t t=time(NULL);
        for(sessionNumber=0;sessionNumber<number_of_devices;sessionNumber++)
        {
            if(networkSessions[sessionNumber]==NULL) continue;
        	if (hdr->devAddr.value == networkSessions[sessionNumber]->devAddr.value)
            {
                networkSession=networkSessions[sessionNumber];
                ESP_LOGI(TAG,"Session number %d for devEUI=%016llx",sessionNumber,networkSession->endDevice->devEui.eui);
                break;
            }
        }
        if(networkSession==NULL || sessionNumber==number_of_devices)
        {
        	ESP_LOGE(TAG,"Not found session for dev address %08x",hdr->devAddr.value);
        	return NETWORK_NOT_JOINED;
        }

//        ESP_LOGI(TAG,"hdr->fcnt=%d FCntUp=%d sizeof Hdr_t=%d size FCtrl_t %d",hdr->fCnt,networkSession->FCntUp.members.valueLow,sizeof(Hdr_t),sizeof(FCtrl_t));
        repeat=0;
        if (hdr->fCnt >= networkSession->FCntUp.members.valueLow)
        {
            if ((hdr->fCnt - networkSession->FCntUp.members.valueLow) > loRa.protocolParameters.maxFcntGap) //if this difference is greater than the value of max_fct_gap then too many data frames have been lost then subsequesnt will be discarded
            {
                ESP_LOGE(TAG,"fCnt false. Rejoin needed hdr->fcnt=%d FCntUp=%d",hdr->fCnt,networkSession->FCntUp.members.valueLow);
            	return FRAME_COUNTER_ERROR_REJOIN_NEEDED;
            }
            else
            {
            	if(hdr->fCnt == networkSession->FCntUp.members.valueLow) repeat=1;
            	networkSession->FCntUp.members.valueLow = hdr->fCnt;  //frame counter received is OK, so the value received from the server is kept in sync with the value stored in the end device
            }
        }
        else
        {
            if((hdr->fCnt == 0) && (networkSession->FCntUp.members.valueLow == 0xFFFF))
            {
                //Frame counter rolled over
            	networkSession->FCntUp.members.valueLow = hdr->fCnt;
            	networkSession->FCntUp.members.valueHigh ++;
            }
            else
            {
                return LORA_INVALID_PARAMETER;
            }
        }
        if (networkSession->FCntUp.value == UINT32_MAX)
        {
            return FRAME_COUNTER_ERROR_REJOIN_NEEDED;
        }

        AssembleEncryptionBlock (DIR_UPLINK, networkSession->FCntUp.value, bufferLength - sizeof (computedMic), 0x49, networkSession->devAddr,0);
        memcpy (&radioBuffer[0], aesBuffer, sizeof (aesBuffer));
        memcpy (&radioBuffer[16], buffer, bufferLength-sizeof(computedMic));
        AESCmac(networkSession->FNwkSIntKey, aesBuffer, &radioBuffer[0], bufferLength - sizeof(computedMic) + sizeof (aesBuffer));

        memcpy(&computedMic, aesBuffer, sizeof(computedMic));
        extractedMic = ExtractMic (&buffer[0], bufferLength);

        if (computedMic != extractedMic)
        {
            ESP_LOGE(TAG,"BAD MIC");
            return LORA_INVALID_PARAMETER;
        }
        ESP_LOGI(TAG,"MIC OK");


        if(hdr->fCtrl.bits.ack) networkSession->flags.WAITING_RECEIVE_ACK=0;
        if(!(bufferLength==hdr->fCtrl.bits.fOptsLen+12))
        {
        	networkSession->port=buffer[hdr->fCtrl.bits.fOptsLen+8];
        	networkSession->payloadLength=bufferLength-hdr->fCtrl.bits.fOptsLen-9-sizeof(computedMic);
        	EncryptFRMPayload(&buffer[hdr->fCtrl.bits.fOptsLen+9], networkSession->payloadLength, DIR_UPLINK, networkSession->FCntUp.value, networkSession->port==0 ? networkSession->NwkSEncKey : networkSession->AppSKey, 0, macBuffer, networkSession->devAddr);
        	if(networkSession->port!=0)
        	{
        		networkSession->payload=macBuffer;
        		networkSession->currentState.t=t;
        		networkSession->currentState.local_snr=snr;
        		networkSession->currentState.local_rssi=rssi;
        		if(!repeat) xTaskCreatePinnedToCore(writeData,"writeData",4096,networkSession,tskIDLE_PRIORITY+2,&networkSession->app,0);
        	}
        }
        if(mhdr.bits.mType == FRAME_TYPE_DATA_UNCONFIRMED_UP && !networkSession->flags.REQUEST_SEND_ANSWER) xTimerStop(networkSession->sendAnswerTimerId->timer, 0);
        else
        {
        	networkSession->flags.REQUEST_SET_ACK=1;
        	networkSession->bufferIndex=PrepareAckFrame (networkSession, networkSession->macBuffer);
        }
        networkSession->sendAnswerTimerId->event=SEND_ACK_TIMER;
        exp=START_TIMER_VALUE - xTimerGetExpiryTime(startTimerId) + xTaskGetTickCount();
        xTimerChangePeriod(networkSession->sendAnswerTimerId->timer,(networkSession->protocolParameters.receiveDelay1-20)/portTICK_PERIOD_MS-exp, 0);
        xTimerStart(networkSession->sendAnswerTimerId->timer, 0);
    }
    return LORA_OK;
}

void ConfigureRadio(uint8_t dataRate, uint32_t freq, Direction_t dir)
{
    RADIO_SetModulation (modulation[dataRate]);
    RADIO_SetChannelFrequency (freq);
    RADIO_SetFrequencyHopPeriod (DISABLED);

    if (dataRate <= DR6)
    {
        //LoRa modulation
        RADIO_SetSpreadingFactor (spreadingFactor[dataRate]);
        RADIO_SetBandwidth (bandwidth[dataRate]);
        RADIO_SetLoRaSyncWord(loRa.syncWord);
    }
    else
    {
        //FSK modulation
        RADIO_SetFSKSyncWord(sizeof(FskSyncWordBuff) / sizeof(FskSyncWordBuff[0]), (uint8_t*)FskSyncWordBuff);
    }
    if(dir==DIR_DOWNLINK)
    {
        RADIO_SetCRC(DISABLED);
        RADIO_SetIQInverted(ENABLED);
    }
    else
    {
        RADIO_SetCRC(ENABLED);
        RADIO_SetIQInverted(DISABLED);
    }
}

void LORAX_RxTimeout(void)
{
	// Start Rx again
    uint8_t channel;
	set_s("CHANNEL",&channel);
	ConfigureRadio(loRa.currentDataRate, Channels[channel].frequency, DIR_UPLINK);
	ESP_LOGI(TAG,"Radio configured for RX");
	RADIO_SetRxWatchdogTimeout(RX_WATCHDOG_DEFAULT_TIME);
	RADIO_ReceiveStart(0);
	ESP_LOGI(TAG,"Receive start");
}

uint32_t uid;

int32_t delta_freq=-30000;

void LORAX_TRANSMIT(void)
{
    uint32_t period=3000; // 3 s
    TimerHandle_t RepeatTransmitTimerId = xTimerCreate("RepeatTransmitTimer", period / portTICK_PERIOD_MS, pdTRUE, (void*) REPEAT_TRANSMIT_TIMER,TimerCallback);
    xTimerStart(RepeatTransmitTimerId,0);
    ((uint32_t*)(&data.temperature))[1]=0;
    set_s("UID",&uid);
    ((uint32_t*)(&data.temperature))[0]=uid;
    delta_freq=-30000;
}

void LORAX_SEND_START(void)
{
	ESP_LOGI(TAG,"Transmit start");
    uint8_t channel;
	set_s("CHANNEL",&channel);
	ConfigureRadio(loRa.currentDataRate, Channels[channel].frequency+delta_freq, DIR_UPLINK);
	delta_freq+=1000;
	if(delta_freq>30000) delta_freq=-30000;
	ESP_LOGI(TAG,"Radio configured for TX");
    ((uint32_t*)(&data.temperature))[1]++;
	RADIO_Transmit(&data, sizeof(data));
}

void LORAX_Reset (IsmBand_t ismBandNew)
{
    if (loRa.macInitialized == ENABLED)
    {
        StopAllSoftwareTimers ();
    }

    loRa.syncWord = 0x34;
    RADIO_SetLoRaSyncWord(loRa.syncWord);

    loRa.macStatus.value = 0;
    loRa.linkCheckMargin = 255; // reserved
    loRa.linkCheckGwCnt = 0;
    loRa.lastTimerValue = 0;
    loRa.lastPacketLength = 0;
    loRa.fCntDown.value = 0;
    loRa.fCntUp.value = 0;
    loRa.devNonce = 0;
    loRa.prescaler = 1;
    loRa.adrAckCnt = 0;
    loRa.counterAdrAckDelay = 0;
    loRa.offset = 0;
    loRa.lastTimerValue = 0;
    loRa.curentTransmitNetworkSession=0xFF;

    // link check mechanism should be disabled
    loRa.macStatus.linkCheck = DISABLED;

    //flags all 0-es
    loRa.macStatus.value = 0;
    loRa.lorawanMacStatus.value = 0;
/*    for(uint8_t j=0;j<number_of_devices;j++)
    {
        devices[j].macStatus.value = 0;
        devices[j].lorawanMacStatus.value = 0;
        devices[j].fCntDown.value = 0;
        devices[j].fCntUp.value = 0;
        devices[j].DevNonce = 0;
    }*/

    loRa.maxRepetitionsConfirmedUplink = 7; // 7 retransmissions should occur for each confirmed frame sent until ACK is received
    loRa.maxRepetitionsUnconfirmedUplink = 0; // 0 retransmissions should occur for each unconfirmed frame sent until a response is received
    loRa.counterRepetitionsConfirmedUplink = 1;
    loRa.counterRepetitionsUnconfirmedUplink = 1;

    loRa.batteryLevel = BATTERY_LEVEL_INVALID; // the end device was not able to measure the battery level

    loRa.ismBand = ismBandNew;

    loRa.deviceClass = CLASS_A;

    // initialize default channels
    loRa.maxChannels = MAX_EU_SINGLE_BAND_CHANNELS;
    if(ISM_EU868 == ismBandNew)
    {
        RADIO_Init(&radioBuffer[16], EU868_CALIBRATION_FREQ);

        InitDefault868Channels ();

        loRa.receiveWindow2Parameters.dataRate = EU868_DEFAULT_RX_WINDOW2_DR;
        loRa.receiveWindow2Parameters.frequency = EU868_DEFAULT_RX_WINDOW2_FREQ;
    }
    else if(ISM_RU864 == ismBandNew)
    {
        RADIO_Init(&radioBuffer[16], RU864_CALIBRATION_FREQ);

        InitDefaultRU864Channels ();

        loRa.receiveWindow2Parameters.dataRate = RU864_DEFAULT_RX_WINDOW2_DR;
        loRa.receiveWindow2Parameters.frequency = RU864_DEFAULT_RX_WINDOW2_FREQ;
    }
    else
    {
        RADIO_Init(&radioBuffer[16], EU433_CALIBRATION_FREQ);

        InitDefault433Channels ();

        loRa.receiveWindow2Parameters.dataRate = EU433_DEFAULT_RX_WINDOW2_DR;
        loRa.receiveWindow2Parameters.frequency = EU433_DEFAULT_RX_WINDOW2_FREQ;
    }

    uint8_t p;
    set_s("POWER",&p);
    loRa.txPower = p;

//    loRa.currentDataRate =DR0;
    uint8_t bw;
    uint8_t sf;
    set_s("SF",&sf);
    set_s("BW",&bw);
    if(bw==0)
    {
        loRa.currentDataRate=12-sf;
    }
    else loRa.currentDataRate=6;

    UpdateMinMaxChDataRate ();

    //keys will be filled with 0
    loRa.macKeys.value = 0;  //no keys are set
    memset (&loRa.activationParameters, 0, sizeof(loRa.activationParameters));

    //protocol parameters receive the default values
    loRa.protocolParameters.receiveDelay1 = RECEIVE_DELAY1;
    loRa.protocolParameters.receiveDelay2 = RECEIVE_DELAY2;
    loRa.protocolParameters.joinAcceptDelay1 = JOIN_ACCEPT_DELAY1;
    loRa.protocolParameters.joinAcceptDelay2 = JOIN_ACCEPT_DELAY2;
    loRa.protocolParameters.ackTimeout = ACK_TIMEOUT;
    loRa.protocolParameters.adrAckDelay = ADR_ACK_DELAY;
    loRa.protocolParameters.adrAckLimit = ADR_ACK_LIMIT;
    loRa.protocolParameters.maxFcntGap = MAX_FCNT_GAP;
    loRa.protocolParameters.maxMultiFcntGap = MAX_MCAST_FCNT_GAP;

//    LORAWAN_LinkCheckConfigure (DISABLED); // disable the link check mechanism
}

void LORAX_TxDone (uint16_t timeOnAir)
{
	if(mode==MODE_SEND)
	{
		ESP_LOGI(TAG, "Time on air=%dms",timeOnAir);
		return;
	}
	if(mode==MODE_NETWORK_SERVER)
	{
		ESP_LOGI(TAG, "Time on air=%dms",timeOnAir);
		if(loRa.curentTransmitNetworkSession!=0xFF)
		{
			networkSessions[loRa.curentTransmitNetworkSession]->macState = IDLE;
			networkSessions[loRa.curentTransmitNetworkSession]->currentState.local_power = RADIO_GetOutputPower();
		    loRa.curentTransmitNetworkSession=0xFF;
		}
		esp_event_post_to(mainLoop, LORA_EVENTS, LORA_RXTIMEOUT_EVENT, NULL, 0, 0);
		return;
	}
}

