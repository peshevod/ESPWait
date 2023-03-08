/*
 * lorawan.c
 *
 *  Created on: 20 июл. 2021 г.
 *      Author: ilya_000
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "lorawan_types.h"
#include "lorawan_radio.h"
#include "lorawan_defs.h"
#include "sx1276_radio_driver.h"
#include "aes.h"
#include "shell.h"
#include "eui.h"
#include "cmd_nvs.h"
#include "channels.h"
#include "commands.h"
#include "lorax.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

//CID = LinkCheckReq     = 2
//CID = LinkADRAns       = 3
//CID = DutyCycleAns     = 4
//CID = RX2SetupAns      = 5
//CID = DevStatusAns     = 6
//CID = NewChannelAns    = 7
//CID = RXTimingSetupAns = 8
// Index in macEndDevCmdReplyLen = CID - 2
LoRa_t loRa;
RxAppData_t rxPayload;
static uint8_t number_of_devices;
extern Profile_t* devices;
uint8_t aesBuffer[AES_BLOCKSIZE];
uint32_t NetID,NwkID,NwkID_mask;
uint8_t NwkID_type;
uint8_t JoinNonce[3];
uint8_t macBuffer[MAXIMUM_BUFFER_LENGTH];
uint8_t radioBuffer[MAXIMUM_BUFFER_LENGTH];
extern GenericEui_t JoinEui;
DeviceAddress_t DevAddr={.value=0};

/****************************** VARIABLES *************************************/
const uint8_t rxWindowSize[] =  {8, 10, 14, 26, 49, 88, 60, 8};
//const uint8_t rxWindowSize[] =  {32, 40, 56, 104, 196, 352, 240, 32};

// Max Payload Size
const uint8_t maxPayloadSize[8] = {51, 51, 51, 115, 242, 242, 242, 56}; // for FSK max message size should be 64 bytes

const int8_t rxWindowOffset[] = {-33, -50, -58, -62, -66, -68, -15, -2};
//const int8_t rxWindowOffset[] = {-66, -100, -116, -124, -132, -136, -30, -4};

// Tx power possibilities by ism band
static const int8_t txPower868[] = {20, 14, 11, 8, 5, 2};
static const int8_t txPowerRU864[] = {20, 14, 12, 10, 8, 6, 4, 2, 0};

static int8_t txPower433[] = {10, 7, 4, 1, -2, -5};

// Spreading factor possibilities
static const uint8_t spreadingFactor[] = {12, 11, 10, 9, 8, 7, 7};

// Bandwidth possibilities
static const uint8_t bandwidth[] = {BW_125KHZ, BW_125KHZ, BW_125KHZ, BW_125KHZ, BW_125KHZ, BW_125KHZ, BW_250KHZ};

// Modulation possibilities
static const uint8_t modulation[] = {MODULATION_LORA, MODULATION_LORA, MODULATION_LORA, MODULATION_LORA, MODULATION_LORA, MODULATION_LORA, MODULATION_LORA, MODULATION_FSK};

static const uint8_t FskSyncWordBuff[3] = {0xC1, 0x94, 0xC1};

const char* errors[] = {
    "OK",
    "NETWORK_NOT_JOINED",
    "MAC_STATE_NOT_READY_FOR_TRANSMISSION",
    "INVALID_PARAMETER",
    "KEYS_NOT_INITIALIZED",
    "SILENT_IMMEDIATELY_ACTIVE",
    "FRAME_COUNTER_ERROR_REJOIN_NEEDED",
    "INVALID_BUFFER_LENGTH",
    "MAC_PAUSED",
    "NO_CHANNELS_FOUND",
    "INVALID_CLASS",
    "MCAST_PARAM_ERROR",
    "MCAST_MSG_ERROR",
    "DEVICE_DEVNONCE_ERROR"
};



void print_error(char* TAG, LorawanError_t err)
{
    ESP_LOGE(TAG,"LORAWAN Error %s(%d)",errors[err],err);
}

static void PrepareSessionKeys (uint8_t* sessionKey, uint8_t* joinNonce, uint8_t* networkId, uint16_t* devNonce)
{
    uint8_t index = 0;

    memset (&sessionKey[index], 0, sizeof(aesBuffer));  //appends 0-es to the buffer so that pad16 is done
    index ++; // 1 byte for 0x01 or 0x02 depending on the key type

    memcpy(&sessionKey[index], joinNonce, JA_JOIN_NONCE_SIZE);
    index = index + JA_JOIN_NONCE_SIZE;

    memcpy(&sessionKey[index], networkId, JA_NET_ID_SIZE);
    index = index + JA_NET_ID_SIZE;

//    memcpy(&sessionKey[index], &loRa.devNonce, sizeof(loRa.devNonce) );
    memcpy(&sessionKey[index], devNonce, JA_DEV_NONCE_SIZE );

}

static void DeviceComputeSessionKeys (JoinAccept_t *joinAcceptBuffer)
{
    PrepareSessionKeys(loRa.activationParameters.applicationSessionKey, joinAcceptBuffer->members.joinNonce, joinAcceptBuffer->members.networkId, &loRa.devNonce);
    loRa.activationParameters.applicationSessionKey[0] = 0x02; // used for Network Session Key
    AESEncodeLoRa(loRa.activationParameters.applicationSessionKey, loRa.activationParameters.AppKey);

    PrepareSessionKeys(loRa.activationParameters.networkSessionKey, joinAcceptBuffer->members.joinNonce, joinAcceptBuffer->members.networkId, &loRa.devNonce);
    loRa.activationParameters.networkSessionKey[0] = 0x01; // used for Network Session Key
    AESEncodeLoRa(loRa.activationParameters.networkSessionKey, loRa.activationParameters.AppKey);
}

static void NetworkComputeSessionKeys (uint8_t dn)
{
    PrepareSessionKeys(devices[dn].AppSKey, JoinNonce, (uint8_t*)(&NetID), &(devices[dn].DevNonce));
    devices[dn].AppSKey[0] = 0x02; // used for Network Session Key
    AESEncodeLoRa(devices[dn].AppSKey, loRa.activationParameters.AppKey);

    PrepareSessionKeys(devices[dn].NwkSKey, JoinNonce, (uint8_t*)(&NetID), &(devices[dn].DevNonce));
    devices[dn].NwkSKey[0] = 0x01; // used for Network Session Key
    AESEncodeLoRa(devices[dn].NwkSKey, loRa.activationParameters.AppKey);
}


static uint32_t ComputeMic ( uint8_t *key, uint8_t* buffer, uint8_t bufferLength)  // micType is 0 for join request and 1 for data packet
{
    uint32_t mic = 0;

    AESCmac(key, aesBuffer, buffer, bufferLength); //if micType is 0, bufferLength the same, but for data messages bufferLength increases with 16 because a block is added

    memcpy(&mic, aesBuffer, sizeof( mic ));

    return mic;
}


static uint32_t ExtractMic (uint8_t *buffer, uint8_t bufferLength)
{
     uint32_t mic = 0;
     memcpy (&mic, &buffer[bufferLength - 4], sizeof (mic));
     return mic;
}

void UpdateJoinSuccessState(uint8_t param)
{
    loRa.lorawanMacStatus.joining = 0;  //join was done
    loRa.macStatus.networkJoined = 1;   //network is joined
    loRa.macStatus.macState = IDLE;

    devices[param].macStatus.networkJoined=1;

    loRa.adrAckCnt = 0;  // adr ack counter becomes 0, it increments only for ADR set
    loRa.counterAdrAckDelay = 0;

    // if the link check mechanism was enabled, then its timer will begin counting
    if (loRa.macStatus.linkCheck == ENABLED)
    {
    	xTimerChangePeriod(loRa.linkCheckTimerId, loRa.periodForLinkCheck/portTICK_PERIOD_MS,0);
        xTimerStart(loRa.linkCheckTimerId,0);
    }

    if (rxPayload.RxJoinResponse != NULL)
    {
        rxPayload.RxJoinResponse(ACCEPTED); // inform the application layer that join was successful via a callback
    }
}

void SetJoinFailState(void)
{
    loRa.macStatus.networkJoined = 0;
    loRa.lorawanMacStatus.joining = 0;
    loRa.macStatus.macState = IDLE;
    if (rxPayload.RxJoinResponse != NULL)
    {
        rxPayload.RxJoinResponse(REJECTED); // inform the application layer that join failed via callback
    }
}

static uint8_t PrepareJoinAcceptFrame (uint8_t dev_number)
{
    Mhdr_t mhdr;
    uint32_t mic;
    uint8_t temp;

    devices[dev_number].bufferIndex = 0;

    memset (devices[dev_number].macBuffer, 0, sizeof(devices[dev_number].macBuffer) );  // clear the mac buffer

    mhdr.bits.mType = FRAME_TYPE_JOIN_ACCEPT;  //prepare the mac header to include mtype as frame type join request
    mhdr.bits.major = MAJOR_VERSION3;
    mhdr.bits.rfu = RESERVED_FOR_FUTURE_USE;

    devices[dev_number].macBuffer[devices[dev_number].bufferIndex++] = mhdr.value;  // add the mac header to the buffer

    memcpy(&devices[dev_number].macBuffer[devices[dev_number].bufferIndex],&JoinNonce,JA_JOIN_NONCE_SIZE);
    devices[dev_number].bufferIndex = devices[dev_number].bufferIndex + JA_JOIN_NONCE_SIZE;

    memcpy(&devices[dev_number].macBuffer[devices[dev_number].bufferIndex],(uint8_t*)(&NetID),JA_NET_ID_SIZE);
    devices[dev_number].bufferIndex = devices[dev_number].bufferIndex + JA_NET_ID_SIZE;

    memcpy(&devices[dev_number].macBuffer[devices[dev_number].bufferIndex],devices[dev_number].DevAddr.buffer,sizeof(devices[dev_number].DevAddr.buffer));
    devices[dev_number].bufferIndex = devices[dev_number].bufferIndex + sizeof(devices[dev_number].DevAddr.buffer);

    memcpy(&devices[dev_number].macBuffer[devices[dev_number].bufferIndex],&(devices[dev_number].DlSettings.value),sizeof(devices[dev_number].DlSettings.value));
    devices[dev_number].bufferIndex = devices[dev_number].bufferIndex + sizeof(devices[dev_number].DlSettings.value);

    memcpy(&devices[dev_number].macBuffer[devices[dev_number].bufferIndex],&(devices[dev_number].rxDelay),sizeof(devices[dev_number].rxDelay));
    devices[dev_number].bufferIndex = devices[dev_number].bufferIndex + sizeof(devices[dev_number].rxDelay);

    mic = ComputeMic (loRa.activationParameters.AppKey, devices[dev_number].macBuffer, devices[dev_number].bufferIndex);

    memcpy ( &devices[dev_number].macBuffer[devices[dev_number].bufferIndex], &mic, sizeof (mic));
    devices[dev_number].bufferIndex = devices[dev_number].bufferIndex + sizeof(mic);

    temp = devices[dev_number].bufferIndex-1;
    while (temp > 0)
    {
       //Decode message
       AESDecodeLoRa (&devices[dev_number].macBuffer[devices[dev_number].bufferIndex-temp], loRa.activationParameters.AppKey);
       if (temp > AES_BLOCKSIZE)
       {
           temp -= AES_BLOCKSIZE;
       }
       else
       {
           temp = 0;
       }
    }
    return devices[dev_number].bufferIndex;
}

static void AssembleEncryptionBlock (uint8_t dir, uint32_t frameCounter, uint8_t blockId, uint8_t firstByte, uint8_t multicastStatus)
{
    uint8_t bufferIndex = 0;

    memset (aesBuffer, 0, sizeof (aesBuffer)); //clear the aesBuffer

    aesBuffer[bufferIndex] = firstByte;

    bufferIndex = bufferIndex + 5;  // 4 bytes of 0x00 (done with memset at the beginning of the function)

    aesBuffer[bufferIndex++] = dir;

    if (DISABLED == multicastStatus)
    {
        memcpy (&aesBuffer[bufferIndex], &loRa.activationParameters.deviceAddress, sizeof (loRa.activationParameters.deviceAddress));
        bufferIndex = bufferIndex + sizeof (loRa.activationParameters.deviceAddress);
    }
    else
    {
        memcpy (&aesBuffer[bufferIndex], &loRa.activationParameters.mcastDeviceAddress, sizeof (loRa.activationParameters.mcastDeviceAddress));
        bufferIndex = bufferIndex + sizeof (loRa.activationParameters.mcastDeviceAddress);
    }

    memcpy (&aesBuffer[bufferIndex], &frameCounter, sizeof (frameCounter));
    bufferIndex = bufferIndex + sizeof (frameCounter) ;

    bufferIndex ++;   // 1 byte of 0x00 (done with memset at the beginning of the function)

    aesBuffer[bufferIndex] = blockId;
}

void ConfigureRadio(uint8_t dataRate, uint32_t freq)
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
}

void ConfigureRadioRx(uint8_t dataRate, uint32_t freq, Direction_t dir)
{
    ConfigureRadio(dataRate, freq);
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

void ResetParametersForConfirmedTransmission (void)
{
    loRa.macStatus.macState = IDLE;
    loRa.counterRepetitionsConfirmedUplink = 1;
//    loRa.lorawanMacStatus.ackRequiredFromNextDownlinkMessage = DISABLED;
    loRa.lorawanMacStatus.ackRequiredFromNextDownlinkMessage = ENABLED;
}

void ResetParametersForUnconfirmedTransmission (void)
{
    loRa.macStatus.macState = IDLE;
    loRa.counterRepetitionsUnconfirmedUplink = 1;
    loRa.crtMacCmdIndex = 0;
}

void LORAWAN_EnterContinuousReceive(void)
{
    RADIO_ReceiveStop();

    ConfigureRadioRx(loRa.receiveWindow2Parameters.dataRate, loRa.receiveWindow2Parameters.frequency, DIR_DOWNLINK);

    if (RADIO_ReceiveStart(CLASS_C_RX_WINDOW_SIZE) != ERR_NONE)
    {
        ResetParametersForConfirmedTransmission ();
        ResetParametersForUnconfirmedTransmission ();
        loRa.macStatus.macState = IDLE;
        if (rxPayload.RxAppData != NULL)
        {
            rxPayload.RxAppData (NULL, 0, MAC_NOT_OK);
        }
    }
}

static void EncryptFRMPayload (uint8_t* buffer, uint8_t bufferLength, uint8_t dir, uint32_t frameCounter, uint8_t* key, uint8_t macBufferIndex, uint8_t* bufferToBeEncrypted, uint8_t multicastStatus)
{
    uint8_t k = 0, i = 0, j = 0;

    k = bufferLength / AES_BLOCKSIZE;
    for (i = 1; i <= k; i++)
    {
        AssembleEncryptionBlock (dir, frameCounter, i, 0x01, multicastStatus);
        AESEncodeLoRa(aesBuffer, key);
        for (j = 0; j < AES_BLOCKSIZE; j++)
        {
            bufferToBeEncrypted[macBufferIndex++] = aesBuffer[j] ^ buffer[AES_BLOCKSIZE*(i-1) + j];
        }
    }

    if ( (bufferLength % AES_BLOCKSIZE) != 0 )
    {
        AssembleEncryptionBlock (dir, frameCounter, i, 0x01, multicastStatus);
        AESEncodeLoRa (aesBuffer, key);

        for (j = 0; j < (bufferLength % AES_BLOCKSIZE); j++)
        {
            bufferToBeEncrypted[macBufferIndex++] = aesBuffer[j] ^ buffer[(AES_BLOCKSIZE*k) + j];
        }
    }
}

static void SetReceptionNotOkState (void)
{
    if ( (loRa.macStatus.macState == RX2_OPEN) || ( (loRa.macStatus.macState == RX1_OPEN) && (loRa.rx2DelayExpired) ) )
    {
        loRa.lorawanMacStatus.ackRequiredFromNextDownlinkMessage = 0; // reset the flag
        loRa.macStatus.macState = IDLE;

        if ((loRa.deviceClass == CLASS_A) && (rxPayload.RxAppData != NULL))
        {
            loRa.lorawanMacStatus.synchronization = 0; //clear the synchronization flag, because if the user will send a packet in the callback there is no need to send an empty packet
            rxPayload.RxAppData (NULL, 0, MAC_NOT_OK);
        }
        loRa.macStatus.rxDone = 0;
    }

    if (loRa.deviceClass == CLASS_C)
    {
        loRa.macStatus.macState = CLASS_C_RX2_2_OPEN;
        LORAWAN_EnterContinuousReceive();
    }
}

//Based on the last packet received, this function checks the flags and updates the state accordingly
static void CheckFlags (Hdr_t* hdr)
{
    if (hdr->members.fCtrl.adr == ENABLED)
    {
        loRa.macStatus.adr = ENABLED;
    }

    if (hdr->members.fCtrl.fPending == ENABLED)
    {
        loRa.lorawanMacStatus.fPending = ENABLED;
    }

    if (hdr->members.fCtrl.adrAckReq == ENABLED)
    {
        loRa.lorawanMacStatus.adrAckRequest = ENABLED;
    }

    if (hdr->members.mhdr.bits.mType == FRAME_TYPE_DATA_CONFIRMED_DOWN)
    {
        loRa.lorawanMacStatus.ackRequiredFromNextUplinkMessage = ENABLED;  //next uplink packet should have the ACK bit set
    }
}

uint16_t Random (uint16_t max)
{
    return (rand () % max);
}

void ConfigureRadioTx(uint8_t dataRate, uint32_t freq, Direction_t dir)
{
    int8_t txPower;

    ConfigureRadio(dataRate, freq);

    if (ISM_EU868 == loRa.ismBand)
    {
        txPower = txPower868[loRa.txPower];
    }
    else if (ISM_RU864 == loRa.ismBand)
    {
        txPower = txPowerRU864[loRa.txPower];
    }
    else
    {
        txPower = txPower868[loRa.txPower];
    }
    RADIO_SetOutputPower (txPower);

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

static uint8_t LORAWAN_GetMaxPayloadSize (void)
{
    uint8_t result = 0;
    uint8_t macCommandsLength;

    macCommandsLength = CountfOptsLength();

    if (loRa.crtMacCmdIndex == 0)  //there are no MAC commands to either respond or interrogate
    {
        result = maxPayloadSize[loRa.currentDataRate];
    }
    else
    {
        result = maxPayloadSize[loRa.currentDataRate] - macCommandsLength ;
    }

    return result;
}

static void AssemblePacket (bool confirmed, uint8_t port, uint8_t *buffer, uint16_t bufferLength)
{
    Mhdr_t mhdr;
    uint8_t bufferIndex = 16;
    FCtrl_t fCtrl;
    uint8_t macCmdIdx = 0;

    memset (&mhdr, 0, sizeof (mhdr) );    //clear the header structure Mac header
    memset (&macBuffer[0], 0, sizeof (macBuffer) ); //clear the mac buffer
    memset (aesBuffer, 0, sizeof (aesBuffer) );  //clear the transmission buffer

    if (confirmed == 1)
    {
            mhdr.bits.mType = FRAME_TYPE_DATA_CONFIRMED_UP;
            loRa.lorawanMacStatus.ackRequiredFromNextDownlinkMessage = 1;
     }
    else
    {
        mhdr.bits.mType = FRAME_TYPE_DATA_UNCONFIRMED_UP;
    }
    mhdr.bits.major = 0;
    mhdr.bits.rfu = 0;
    macBuffer[bufferIndex++] = mhdr.value;

    memcpy (&macBuffer[bufferIndex], loRa.activationParameters.deviceAddress.buffer, sizeof (loRa.activationParameters.deviceAddress.buffer) );
    bufferIndex = bufferIndex + sizeof(loRa.activationParameters.deviceAddress.buffer);


    fCtrl.value = 0; //clear the fCtrl value

    if (loRa.macStatus.adr == ENABLED)
    {
        fCtrl.adr = ENABLED; // set the ADR bit in the packet
        if(loRa.currentDataRate > loRa.minDataRate)
        {
            fCtrl.adrAckReq = ENABLED;
            loRa.lorawanMacStatus.adrAckRequest = ENABLED; // set the flag that to remember that this bit was set
            loRa.adrAckCnt ++;   // if adr is set, each time the uplink frame counter is incremented, the device increments the adr_ack_cnt, any received downlink frame following an uplink frame resets the ADR_ACK_CNT counter

            if ( loRa.adrAckCnt == loRa.protocolParameters.adrAckLimit )
            {
                loRa.counterAdrAckDelay = 0;
                fCtrl.adrAckReq = DISABLED;
                loRa.lorawanMacStatus.adrAckRequest = DISABLED;
            }
            else
            {
                if (loRa.adrAckCnt > loRa.protocolParameters.adrAckLimit)
                {
                    // the ADRACKREQ bit is set if ADR_ACK_CNT >= ADR_ACK_LIMIT and the current data rate is greater than the device defined minimum data rate, it is cleared in other conditions
                    loRa.counterAdrAckDelay ++ ; //we have to check how many packets we sent without any response

                    // If no reply is received within the next ADR_ACK_DELAY uplinks, the end device may try to regain connectivity by switching to the next lower data rate
                    if (loRa.counterAdrAckDelay > loRa.protocolParameters.adrAckDelay)
                    {
                        loRa.counterAdrAckDelay = 0;

                        if(false == FindSmallestDataRate())
                        {
                            //Minimum data rate is reached
                            loRa.adrAckCnt = 0;
                            fCtrl.adrAckReq = DISABLED;
                            loRa.lorawanMacStatus.adrAckRequest = DISABLED;
                        }
                    }
                }
                else
                {
                    fCtrl.adrAckReq = DISABLED;
                    loRa.lorawanMacStatus.adrAckRequest = DISABLED;
                }
            }
        }
        else
        {
            loRa.lorawanMacStatus.adrAckRequest = DISABLED;
        }
    }

    if (loRa.lorawanMacStatus.ackRequiredFromNextUplinkMessage == ENABLED)
    {
        fCtrl.ack = ENABLED;
        loRa.lorawanMacStatus.ackRequiredFromNextUplinkMessage = DISABLED;
    }

    fCtrl.fPending = RESERVED_FOR_FUTURE_USE;  //fPending bit is ignored for uplink packets

    if ( (loRa.crtMacCmdIndex == 0) || (bufferLength == 0) ) // there is no MAC command in the queue or there are MAC commands to respond, but the packet does not include application payload (in this case the response to MAC commands will be included inside FRM payload)
    {
        fCtrl.fOptsLen = 0;         // fOpts field is absent
    }
    else
    {
        fCtrl.fOptsLen = CountfOptsLength();
    }
    macBuffer[bufferIndex++] = fCtrl.value;

    memcpy (&macBuffer[bufferIndex], &loRa.fCntUp.members.valueLow, sizeof (loRa.fCntUp.members.valueLow) );
    bufferIndex = bufferIndex + sizeof(loRa.fCntUp.members.valueLow);
    if ( (loRa.crtMacCmdIndex != 0) && (bufferLength != 0) ) // the response to MAC commands will be included inside FOpts
    {
        IncludeMacCommandsResponse (macBuffer, &bufferIndex, 1);
    }

   if(bufferLength!=0 || loRa.crtMacCmdIndex > 0) macBuffer[bufferIndex++] = port;     // the port field is present if the frame payload field is not empty

   if (bufferLength != 0)
   {
        memcpy (&macBuffer[bufferIndex], buffer, bufferLength);
        EncryptFRMPayload (buffer, bufferLength, 0, loRa.fCntUp.value, loRa.activationParameters.applicationSessionKey, bufferIndex, macBuffer, MCAST_DISABLED);
        bufferIndex = bufferIndex + bufferLength;
   }
   else if ( (loRa.crtMacCmdIndex > 0) ) // if answer is needed to MAC commands, include the answer here because there is no app payload
   {
       // Use networkSessionKey for port 0 data
       //Use radioBuffer as a temporary buffer. The encrypted result is found in macBuffer
       IncludeMacCommandsResponse (radioBuffer, &macCmdIdx, 0 );
       EncryptFRMPayload (radioBuffer, macCmdIdx, 0, loRa.fCntUp.value, loRa.activationParameters.networkSessionKey, bufferIndex, macBuffer, MCAST_DISABLED);
       bufferIndex = bufferIndex + macCmdIdx;
   }

   AssembleEncryptionBlock (0, loRa.fCntUp.value, bufferIndex - 16, 0x49, MCAST_DISABLED);
   memcpy (&macBuffer[0], aesBuffer, sizeof (aesBuffer));

   AESCmac (loRa.activationParameters.networkSessionKey, aesBuffer, macBuffer, bufferIndex  );

   memcpy (&macBuffer[bufferIndex], aesBuffer, 4);
   bufferIndex = bufferIndex + 4; // 4 is the size of MIC

   loRa.lastPacketLength = bufferIndex - 16;
}

LorawanError_t LORAWAN_Send (TransmissionType_t confirmed, uint8_t port,  void *buffer, uint8_t bufferLength, Direction_t dir)
{
    LorawanError_t result;

    if (loRa.macStatus.macPause == ENABLED)
    {
        return MAC_PAUSED;                               // Any further transmissions or receptions cannot occur is macPaused is enabled.
    }

    if (loRa.macStatus.silentImmediately == ENABLED)  // The server decided that any further uplink transmission is not possible from this end device.
    {
        return SILENT_IMMEDIATELY_ACTIVE;
    }

    if (loRa.macStatus.networkJoined == DISABLED)          //The network needs to be joined before sending
    {
        return NETWORK_NOT_JOINED;
    }

    if ( (port < FPORT_MIN) && (bufferLength != 0) )   //Port number should be <= 1 if there is data to send. If port number is 0, it indicates only Mac commands are inside FRM Payload
    {
        return INVALID_PARAMETER;
    }

    //validate date length using MaxPayloadSize
    if (bufferLength > LORAWAN_GetMaxPayloadSize ())
    {
        return INVALID_BUFFER_LENGTH;
    }

    if (loRa.fCntUp.value == UINT32_MAX)
    {
        // Inform application about rejoin in status
        loRa.macStatus.rejoinNeeded = 1;
        return FRAME_COUNTER_ERROR_REJOIN_NEEDED;
    }

    if ((loRa.macStatus.macState != IDLE) && (CLASS_A == loRa.deviceClass))
    {
        return MAC_STATE_NOT_READY_FOR_TRANSMISSION;
    }

    result = SelectChannelForTransmission (1, dir);
    if (result != LORA_OK)
    {
        return result;
    }
    else
    {
        if (CLASS_C == loRa.deviceClass)
        {
            RADIO_ReceiveStop();
        }

        AssemblePacket (confirmed, port, buffer, bufferLength);

        if (RADIO_Transmit (&macBuffer[16], (uint8_t)loRa.lastPacketLength) == ERR_NONE)
        {
            loRa.fCntUp.value ++;   // the uplink frame counter increments for every new transmission (it does not increment for a retransmission)

            if (CNF == confirmed)
            {
                loRa.lorawanMacStatus.ackRequiredFromNextDownlinkMessage = ENABLED;
            }
            loRa.lorawanMacStatus.synchronization = ENABLED;  //set the synchronization flag because one packet was sent (this is a guard for the the RxAppData of the user)
            loRa.macStatus.macState = TRANSMISSION_OCCURRING; // set the state of MAC to transmission occurring. No other packets can be sent afterwards
        }
        else
        {
            return MAC_STATE_NOT_READY_FOR_TRANSMISSION;
        }
    }

    return OK;
}

void UpdateRetransmissionAckTimeoutState (void)
{
    loRa.macStatus.macState = RETRANSMISSION_DELAY;
    xTimerChangePeriod(loRa.ackTimeoutTimerId, loRa.protocolParameters.ackTimeout/portTICK_PERIOD_MS, 0);
    xTimerStart(loRa.ackTimeoutTimerId, 0);
}

static uint8_t CheckMcastFlags (Hdr_t* hdr)
{

    /*
     * Multicast message conditions:
     * - ACK == 0
     * - ADRACKReq == 0
     * - FOpt == empty => FOptLen == 0 (no mac commands in fopt)
     * - FPort == 0 (no commands in payload)
     * - MType == UNCNF Data downlink
     */

    if ((0 != hdr->members.fCtrl.ack) || (0 != hdr->members.fCtrl.adrAckReq) || (FRAME_TYPE_DATA_UNCONFIRMED_DOWN != hdr->members.mhdr.bits.mType))
    {
        return FLAG_ERROR;
    }

    if (0 != hdr->members.fCtrl.fOptsLen)
    {
        return FLAG_ERROR;
    }
    else
    {
        if ( 0 == *(((uint8_t *)hdr) + 8)) // Port vlaue in case of FOpts empty
        {
            return FLAG_ERROR;
        }
    }

    if (hdr->members.fCtrl.fPending == ENABLED)
    {
        loRa.lorawanMacStatus.fPending = ENABLED;
    }

    return FLAG_OK;
}

LorawanError_t LORAWAN_RxDone (uint8_t *buffer, uint8_t bufferLength)
{
    uint32_t computedMic, extractedMic;
    Mhdr_t mhdr;
    uint8_t fPort, bufferIndex, channelIndex;
    uint8_t frmPayloadLength;
    uint8_t *packet;
    uint8_t temp;
    char TAG[]="LORAWAN_RxDone";
    uint8_t dev_number=0;

    ESP_LOGI(TAG,"Received frame type=0x%02x",(buffer[0]&0xE0)>>5);
    if (loRa.macStatus.macPause == DISABLED)
    {
        mhdr.value = buffer[0];
        if ( (mhdr.bits.mType == FRAME_TYPE_JOIN_ACCEPT) && (loRa.activationParameters.activationType == OTAA) )
        {
            ESP_LOGI(TAG,"Received Join accept frame length=%d",bufferLength);
            temp = bufferLength - 1; //MHDR not encrypted
            while (temp > 0)
            {
               //Decode message
               AESEncodeLoRa (&buffer[bufferLength - temp], loRa.activationParameters.AppKey);
               if (temp > AES_BLOCKSIZE)
               {
                   temp -= AES_BLOCKSIZE;
               }
               else
               {
                   temp = 0;
               }
            }

            //verify MIC
            computedMic = ComputeMic (loRa.activationParameters.AppKey, buffer, bufferLength - sizeof(extractedMic));
            extractedMic = ExtractMic (buffer, bufferLength);
            if (extractedMic != computedMic)
            {
                ESP_LOGE(TAG,"BAD MIC");
                if ((loRa.macStatus.macState == RX2_OPEN) || ((loRa.macStatus.macState == RX1_OPEN) && (loRa.rx2DelayExpired)))
                {
                    SetJoinFailState();
                }

                return INVALID_PARAMETER;
            }
            ESP_LOGI(TAG,"MIC OK");

            // if the join request message was received during receive window 1, receive window 2 should not open any more, so its timer will be stopped
            if (loRa.macStatus.macState == RX1_OPEN)
            {
                xTimerStop (loRa.joinAccept2TimerId,0);
            }

            JoinAccept_t *joinAccept;
            joinAccept = (JoinAccept_t*)buffer;

            loRa.activationParameters.deviceAddress.value = joinAccept->members.deviceAddress.value; //device address is saved

            UpdateReceiveDelays (joinAccept->members.rxDelay & LAST_NIBBLE); //receive delay 1 and receive delay 2 are updated according to the rxDelay field from the join accept message

            UpdateDLSettings(joinAccept->members.DLSettings.bits.rx2DataRate, joinAccept->members.DLSettings.bits.rx1DROffset);

            UpdateCfList (bufferLength, joinAccept);

            DeviceComputeSessionKeys (joinAccept); //for activation by personalization, the network and application session keys are computed

            UpdateJoinSuccessState(0);

            loRa.fCntUp.value = 0;   // uplink counter becomes 0
            loRa.fCntDown.value = 0; // downlink counter becomes 0

            return LORA_OK;
        }
        else if ( (mhdr.bits.mType == FRAME_TYPE_JOIN_REQ) && (loRa.activationParameters.activationType == OTAA) )
        {
            ESP_LOGI(TAG,"Received Join Request Frame length=%d",bufferLength);

            computedMic = ComputeMic (loRa.activationParameters.AppKey, buffer, bufferLength - sizeof(extractedMic));
            extractedMic = ExtractMic (buffer, bufferLength);
            if (extractedMic != computedMic)
            {
                ESP_LOGE(TAG,"BAD MIC");
                SetJoinFailState();
                return INVALID_PARAMETER;
            }
            ESP_LOGI(TAG,"MIC OK");
            JoinRequest_t *joinRequest;
            joinRequest=(JoinRequest_t*)buffer;
            ESP_LOGI(TAG,"Received Join EUI=%016llx My Join EUI=%016llx",joinRequest->joinEui.eui,JoinEui.eui);
            if(euicmpr(&(joinRequest->joinEui),&JoinEui))
            {
                SetJoinFailState();
                return INVALID_PARAMETER;
            }
            bool found=false;
            ESP_LOGI(TAG,"Received Device EUI=%016llx",joinRequest->devEui.eui);
            for(uint8_t j=0;j<number_of_devices;j++)
            {
                if(!euicmpr(&(joinRequest->devEui),&(devices[j].Eui)))
                {
                    dev_number=j;
                    ESP_LOGI(TAG,"Join Request from device number %d",j);
                    found=true;
                    break;
                }
            }
            if(!found)
            {
                SetJoinFailState();
                return INVALID_PARAMETER;
            }
            int32_t dt;
            set_s("RX1_OFFSET",&dt);
            ESP_LOGI(TAG,"inbase devnonce=0x%04x, received devnonce=0x%04x",devices[dev_number].DevNonce,joinRequest->devNonce);
            if(joinRequest->devNonce>devices[dev_number].DevNonce)
            {
                devices[dev_number].DevNonce=joinRequest->devNonce;
                put_DevNonce(devices[dev_number].js,devices[dev_number].DevNonce);
            }
            else
            {
                return DEVICE_DEVNONCE_ERROR;
            }
            *((uint16_t*)JoinNonce)=Random(UINT16_MAX);
            JoinNonce[2]=(uint8_t)Random(UINT8_MAX);
            //            getinc_JoinNonce(&JoinNonce);
            NetworkComputeSessionKeys(dev_number);
            devices[dev_number].DevAddr.value=get_nextDevAddr(&(DevAddr));
            devices[dev_number].DlSettings.bits.version_1dot1=0;
            devices[dev_number].DlSettings.bits.rx1DROffset=0;
            devices[dev_number].DlSettings.bits.rx2DataRate=DR0;
            devices[dev_number].rxDelay=1;
            devices[dev_number].fCntUp.value = 0;   // uplink counter becomes 0
            devices[dev_number].fCntDown.value = 0; // downlink counter becomes 0
            devices[dev_number].status.states.joining = 0;  //join was done
            devices[dev_number].status.states.joined = 1;   //network is joined
            PrepareJoinAcceptFrame (dev_number);
            loRa.macStatus.macState=IDLE;
            devices[dev_number].macStatus.macState = BEFORE_TX1;
            xTimerChangePeriod(devices[dev_number].sendJoinAccept1TimerId,loRa.protocolParameters.joinAcceptDelay1/portTICK_PERIOD_MS, 0);
            xTimerStart(devices[dev_number].sendJoinAccept1TimerId, 0);
            UpdateJoinSuccessState(dev_number);

            return LORA_OK;

        }
        else if ( (mhdr.bits.mType == FRAME_TYPE_DATA_UNCONFIRMED_DOWN) || (mhdr.bits.mType == FRAME_TYPE_DATA_CONFIRMED_DOWN) )
        {
            loRa.crtMacCmdIndex = 0;   // clear the macCommands requests list

            Hdr_t *hdr;
            hdr=(Hdr_t*)buffer;

            // CLASS C MULTICAST
            if ( (CLASS_C == loRa.deviceClass) && (hdr->members.devAddr.value == loRa.activationParameters.mcastDeviceAddress.value) && (ENABLED == loRa.macStatus.mcastEnable) )
            {

                if (FLAG_ERROR == CheckMcastFlags(hdr))
                {
                    loRa.macStatus.macState = CLASS_C_RX2_2_OPEN;
                    LORAWAN_EnterContinuousReceive();
                    return MCAST_MSG_ERROR;
                }

                AssembleEncryptionBlock (1, hdr->members.fCnt, bufferLength - sizeof (computedMic), 0x49, MCAST_ENABLED);
                memcpy (&radioBuffer[0], aesBuffer, sizeof (aesBuffer));
                memcpy (&radioBuffer[16], buffer, bufferLength-sizeof(computedMic));
                AESCmac(loRa.activationParameters.mcastNetworkSessionKey, aesBuffer, &radioBuffer[0], bufferLength - sizeof(computedMic) + sizeof (aesBuffer));

                memcpy(&computedMic, aesBuffer, sizeof(computedMic));
                extractedMic = ExtractMic (&buffer[0], bufferLength);

                if (computedMic != extractedMic)
                {
                    loRa.macStatus.macState = CLASS_C_RX2_2_OPEN;
                    LORAWAN_EnterContinuousReceive();
                    return MCAST_MSG_ERROR;
                }

                if (hdr->members.fCnt >= loRa.fMcastCntDown.members.valueLow)
                {
                    if ( (hdr->members.fCnt - loRa.fMcastCntDown.members.valueLow) > loRa.protocolParameters.maxMultiFcntGap )
                    {
                        if (rxPayload.RxAppData != NULL)
                        {
                            rxPayload.RxAppData (NULL, 0, MCAST_RE_KEYING_NEEDED);
                        }

                        loRa.macStatus.macState = CLASS_C_RX2_2_OPEN;
                        LORAWAN_EnterContinuousReceive();
                        return MCAST_MSG_ERROR;
                    }
                    else
                    {
                        loRa.fMcastCntDown.members.valueLow = hdr->members.fCnt;  //frame counter received is OK, so the value received from the server is kept in sync with the value stored in the end device
                    }
                }
                else
                {
                    if ( (0 == hdr->members.fCnt) && (0xFFFF == loRa.fMcastCntDown.members.valueLow) )
                    {
                        loRa.fMcastCntDown.members.valueLow = 0;
                        loRa.fMcastCntDown.members.valueHigh ++;
                    }
                    else
                    {
                        if (rxPayload.RxAppData != NULL)
                        {
                            rxPayload.RxAppData (NULL, 0, MCAST_RE_KEYING_NEEDED);
                        }

                        loRa.macStatus.macState = CLASS_C_RX2_2_OPEN;
                        LORAWAN_EnterContinuousReceive();
                        return MCAST_MSG_ERROR;
                    }
                }

                if (loRa.fMcastCntDown.value == UINT32_MAX)
                {
                    if (rxPayload.RxAppData != NULL)
                    {
                        rxPayload.RxAppData (NULL, 0, MCAST_RE_KEYING_NEEDED);
                    }

                    loRa.macStatus.macState = CLASS_C_RX2_2_OPEN;
                    LORAWAN_EnterContinuousReceive();
                    return FRAME_COUNTER_ERROR_REJOIN_NEEDED;
                }

                if (CLASS_C_RX2_1_OPEN == loRa.macStatus.macState)
                {
                    xTimerStop (loRa.receiveWindow1TimerId,0);
                    xTimerStop (loRa.receiveWindow2TimerId,0);
                }
                else if (RX1_OPEN == loRa.macStatus.macState)
                {
                    xTimerStop (loRa.receiveWindow2TimerId,0);
                }

                buffer = buffer + 8; // TODO: magic number

                if ( (sizeof(extractedMic) + hdr->members.fCtrl.fOptsLen + 8) != bufferLength)     //we have port and FRM Payload in the reception packet
                {
                    fPort = *(buffer++);

                    frmPayloadLength = bufferLength - 8 - sizeof (extractedMic); //frmPayloadLength includes port
                    bufferIndex = 16 + 9;

                    EncryptFRMPayload (buffer, frmPayloadLength - 1, 1, loRa.fMcastCntDown.value, loRa.activationParameters.mcastApplicationSessionKey, bufferIndex, radioBuffer, MCAST_ENABLED);
                    packet = buffer - 1;
                }
                else
                {
                    frmPayloadLength = 0;
                    packet = NULL;
                }

                loRa.macStatus.rxDone = 1;
                loRa.macStatus.macState = IDLE;

                if (rxPayload.RxAppData != NULL)
                {
                    rxPayload.RxAppData (packet, frmPayloadLength, MAC_OK);
                }

                loRa.macStatus.macState = CLASS_C_RX2_2_OPEN;
                LORAWAN_EnterContinuousReceive();
                return MAC_OK;
            }

            //  verify if the device address stored in the activation parameters is the same with the device address piggybacked in the received packet, if not ignore packet
            if (hdr->members.devAddr.value != loRa.activationParameters.deviceAddress.value)
            {
                SetReceptionNotOkState();
                if (CLASS_C == loRa.deviceClass)
                {
                    loRa.macStatus.macState = CLASS_C_RX2_2_OPEN;
                    LORAWAN_EnterContinuousReceive();
                }
                return INVALID_PARAMETER;
            }
            ESP_LOGI(TAG,"Addr OK");

            AssembleEncryptionBlock (1, hdr->members.fCnt, bufferLength - sizeof (computedMic), 0x49, MCAST_DISABLED);
            memcpy (&radioBuffer[0], aesBuffer, sizeof (aesBuffer));
            memcpy (&radioBuffer[16], buffer, bufferLength-sizeof(computedMic));
            AESCmac(loRa.activationParameters.networkSessionKey, aesBuffer, &radioBuffer[0], bufferLength - sizeof(computedMic) + sizeof (aesBuffer));

            memcpy(&computedMic, aesBuffer, sizeof(computedMic));
            extractedMic = ExtractMic (&buffer[0], bufferLength);

            // verify if the computed MIC is the same with the MIC piggybacked in the packet, if not ignore packet
            if (computedMic != extractedMic)
            {
                ESP_LOGE(TAG,"Ack BAD MIC");
                SetReceptionNotOkState();
                if (CLASS_C == loRa.deviceClass)
                {
                    loRa.macStatus.macState = CLASS_C_RX2_2_OPEN;
                    LORAWAN_EnterContinuousReceive();
                }
                return INVALID_PARAMETER;
            }
            ESP_LOGI(TAG,"Ack MIC OK");

            //  frame counter check, frame counter received should be less than last frame counter received, otherwise it  was an overflow
            if (hdr->members.fCnt >= loRa.fCntDown.members.valueLow)
            {
                if ((hdr->members.fCnt - loRa.fCntDown.members.valueLow) > loRa.protocolParameters.maxFcntGap) //if this difference is greater than the value of max_fct_gap then too many data frames have been lost then subsequesnt will be discarded
                {
                    loRa.lorawanMacStatus.ackRequiredFromNextDownlinkMessage = 0; // reset the flag
                    loRa.macStatus.macState = IDLE;
                    if (rxPayload.RxAppData != NULL)
                    {
                        loRa.lorawanMacStatus.synchronization = 0; //clear the synchronization flag, because if the user will send a packet in the callback there is no need to send an empty packet
                        rxPayload.RxAppData (NULL, 0, MAC_NOT_OK);
                    }
                    loRa.macStatus.rxDone = 0;

                    // Inform application about rejoin in status
                    loRa.macStatus.rejoinNeeded = 1;
                    if (CLASS_C == loRa.deviceClass)
                    {
                        loRa.macStatus.macState = CLASS_C_RX2_2_OPEN;
                        LORAWAN_EnterContinuousReceive();
                    }
                    return FRAME_COUNTER_ERROR_REJOIN_NEEDED;
                }
                else
                {
                    loRa.fCntDown.members.valueLow = hdr->members.fCnt;  //frame counter received is OK, so the value received from the server is kept in sync with the value stored in the end device
                }
            }
            else
            {
                if((hdr->members.fCnt == 0) && (loRa.fCntDown.members.valueLow == 0xFFFF))
                {
                    //Frame counter rolled over
                    loRa.fCntDown.members.valueLow = hdr->members.fCnt;
                    loRa.fCntDown.members.valueHigh ++;
                }
                else
                {
                    SetReceptionNotOkState();
                    if (CLASS_C == loRa.deviceClass)
                    {
                        loRa.macStatus.macState = CLASS_C_RX2_2_OPEN;
                        LORAWAN_EnterContinuousReceive();
                    }
                    //Reject packet
                    return INVALID_PARAMETER;
                }
            }

            if (loRa.fCntDown.value == UINT32_MAX)
            {

                // Inform application about rejoin in status
                loRa.macStatus.rejoinNeeded = 1;
                if (CLASS_C == loRa.deviceClass)
                {
                    loRa.macStatus.macState = CLASS_C_RX2_2_OPEN;
                    LORAWAN_EnterContinuousReceive();
                }
                return FRAME_COUNTER_ERROR_REJOIN_NEEDED;
            }
            ESP_LOGI(TAG,"fcnt OK");

            // if the downlink message was received during receive window 1, receive window 2 should not open any more, so its timer will be stopped
            if (loRa.macStatus.macState == RX1_OPEN)
            {
                xTimerStop (loRa.receiveWindow2TimerId,0);
            }

            loRa.counterRepetitionsUnconfirmedUplink = 1; // this is a guard for LORAWAN_RxTimeout, for any packet that is received, the last uplink packet should not be retransmitted

            CheckFlags (hdr);

            if (hdr->members.fCtrl.fOptsLen != 0)
            {
                buffer = MacExecuteCommands(hdr->members.MacCommands, hdr->members.fCtrl.fOptsLen);
            }
            else
            {
                buffer = buffer + 8;  // 8 bytes for size of header without mac commands
            }
            if ( (sizeof(extractedMic) + hdr->members.fCtrl.fOptsLen + 8) != bufferLength)     //we have port and FRM Payload in the reception packet
            {
                fPort = *(buffer++);

                frmPayloadLength = bufferLength - 8 - hdr->members.fCtrl.fOptsLen - sizeof (extractedMic); //frmPayloadLength includes port
                bufferIndex = 16 + 8 + hdr->members.fCtrl.fOptsLen + sizeof (fPort);

                if (fPort != 0)
                {
                    EncryptFRMPayload (buffer, frmPayloadLength - 1, 1, loRa.fCntDown.value, loRa.activationParameters.applicationSessionKey, bufferIndex, radioBuffer, MCAST_DISABLED);
                    packet = buffer - 1;
                }
                else
                {
                    // 13 = sizeof(extractedMic) + hdr->members.fCtrl.fOptsLen + 8 + sizeof (fPort);
                    if(bufferLength > (HDRS_MIC_PORT_MIN_SIZE + hdr->members.fCtrl.fOptsLen))
                    {
                        // Decrypt port 0 payload
                        EncryptFRMPayload (buffer, frmPayloadLength - 1, 1, loRa.fCntDown.value, loRa.activationParameters.networkSessionKey, bufferIndex, radioBuffer, MCAST_DISABLED);
                        buffer = MacExecuteCommands(buffer, frmPayloadLength - 1 );
                    }

                    frmPayloadLength = 0;  // we have only MAC commands, so no application payload
                    packet = NULL;
                }
            }
            else
            {
                frmPayloadLength = 0;
                packet = NULL;
                ESP_LOGI(TAG,"frmPayloadLength = 0");
            }

            loRa.counterRepetitionsUnconfirmedUplink = 1; // reset the counter

            loRa.adrAckCnt = 0; // if a packet comes and is correct after device address and MIC, the counter will start counting again from 0 (any received downlink frame following an uplink frame resets the ADR_ACK_CNT counter)
            loRa.counterAdrAckDelay = 0; // if a packet was received, the counter for adr ack limit will become 0
            loRa.lorawanMacStatus.adrAckRequest = 0; //reset the flag for ADR ACK request

            loRa.macStatus.rxDone = 1;  //packet is ready for reception for the application layer

            if ( loRa.lorawanMacStatus.ackRequiredFromNextDownlinkMessage == 1 ) //if last uplink packet was confirmed;
            {
                if  (hdr->members.fCtrl.ack == 1) // if ACK was received
                {
                    loRa.lorawanMacStatus.ackRequiredFromNextDownlinkMessage = 0; // reset the flag
                    loRa.macStatus.macState = IDLE;
                    if (rxPayload.RxAppData != NULL)
                    {
                        loRa.lorawanMacStatus.synchronization = 0; //clear the synchronization flag, because if the user will send a packet in the callback there is no need to send an empty packet
                        rxPayload.RxAppData (packet, frmPayloadLength, MAC_OK);
                    }
                    loRa.macStatus.rxDone = 0;
                    ESP_LOGI(TAG,"Ack received");
                    if ( (loRa.macStatus.automaticReply == 1) && (loRa.lorawanMacStatus.synchronization == 0) && ( (loRa.lorawanMacStatus.ackRequiredFromNextUplinkMessage == 1) || (loRa.lorawanMacStatus.fPending == ENABLED) ) )
                    {
                        if (SearchAvailableChannel (loRa.maxChannels, 1, &channelIndex) == LORA_OK)
                        {
                            LORAWAN_Send (0, 0, 0, 0, DIR_UPLINK);  // send an empty unconfirmed packet
                            loRa.lorawanMacStatus.fPending = DISABLED; //clear the fPending flag
                        }
                        else
                        {
                            //searches for the minimum channel timer and starts a software timer for it
                            StartReTxTimer();
                        }
                    }
                }
                //if ACK was required, but not received, then retransmission will happen
                else
                {
                    UpdateRetransmissionAckTimeoutState ();
                    ESP_LOGE(TAG,"Ack not received");
                }
            }
            else
            {
                loRa.macStatus.macState = IDLE;

                if (rxPayload.RxAppData != NULL)
                {
                    loRa.lorawanMacStatus.synchronization = 0; //clear the synchronization flag, because if the user will send a packet in the callback there is no need to send an empty packet
                    rxPayload.RxAppData (packet, frmPayloadLength, MAC_OK);
                }
                // if the user sent a packet via the callback, the synchronization bit will be 1 and there is no need to send an empty packet
                if ( (loRa.macStatus.automaticReply == 1) && (loRa.lorawanMacStatus.synchronization == 0) && ( (loRa.lorawanMacStatus.ackRequiredFromNextUplinkMessage == 1) || (loRa.lorawanMacStatus.fPending == ENABLED) ) )
                {
                    if (SearchAvailableChannel (loRa.maxChannels, 1, &channelIndex) == LORA_OK)
                    {
                        LORAWAN_Send (0, 0, 0, 0, DIR_UPLINK);  // send an empty unconfirmed packet
                        loRa.lorawanMacStatus.fPending = DISABLED; //clear the fPending flag
                    }
                    else
                    {
                        //searches for the minimum channel timer and starts a software timer for it
                        StartReTxTimer();
                    }
                }
            }

            if (CLASS_C == loRa.deviceClass)
            {
                loRa.macStatus.macState = CLASS_C_RX2_2_OPEN;
                LORAWAN_EnterContinuousReceive();
            }
        }
        else if ( mhdr.bits.mType == FRAME_TYPE_DATA_UNCONFIRMED_UP || mhdr.bits.mType == FRAME_TYPE_DATA_CONFIRMED_UP )
        {
            Hdr_t *hdr;
            hdr=(Hdr_t*)buffer;
    //        send_chars("type OK\r\n");
            bool found=false;
            for(uint8_t j=0;j<number_of_devices;j++)
            {
                if (hdr->members.devAddr.value == devices[j].DevAddr.value)
                {
                    dev_number=j;
                    ESP_LOGI(TAG,"Devaddr of device %d addr=0x%08x",j,devices[j].DevAddr.value);
                    xTimerChangePeriod(devices[dev_number].sendWindow1TimerId, loRa.protocolParameters.receiveDelay1/portTICK_PERIOD_MS, 0);
                    xTimerStart(devices[dev_number].sendWindow1TimerId, 0);
                    found=true;
                    break;
                }
            }
            if(!devices[dev_number].macStatus.networkJoined) ESP_LOGE(TAG,"device number %d not joined",dev_number);

            if(!found || !devices[dev_number].macStatus.networkJoined)
            {
            	ESP_LOGE(TAG,"Unknown devaddr=0x%08x",hdr->members.devAddr.value);
                xTimerStop(devices[dev_number].sendWindow1TimerId,0);
                SetReceptionNotOkState();
                loRa.macStatus.macState = IDLE;
                return INVALID_PARAMETER;
            }
            //        send_chars("Address OK\r\n");
            loRa.activationParameters.deviceAddress.value=hdr->members.devAddr.value;
            AssembleEncryptionBlock (0, hdr->members.fCnt, bufferLength - sizeof (computedMic), 0x49, MCAST_DISABLED);
            memcpy (&radioBuffer[0], aesBuffer, sizeof (aesBuffer));
            memcpy (&radioBuffer[16], buffer, bufferLength-sizeof(computedMic));
            AESCmac(devices[dev_number].NwkSKey, aesBuffer, &radioBuffer[0], bufferLength - sizeof(computedMic) + sizeof (aesBuffer));

            memcpy(&computedMic, aesBuffer, sizeof(computedMic));
            extractedMic = ExtractMic (&buffer[0], bufferLength);

            // verify if the computed MIC is the same with the MIC piggybacked in the packet, if not ignore packet
            if (computedMic != extractedMic)
            {
                ESP_LOGE(TAG,"BAD MIC");
                xTimerStop(devices[dev_number].sendWindow1TimerId,0);
                SetReceptionNotOkState();
                return INVALID_PARAMETER;
            }
            ESP_LOGI(TAG,"MIC OK");
            if (hdr->members.fCnt >= devices[dev_number].fCntUp.members.valueLow)
            {
                if ((hdr->members.fCnt - devices[dev_number].fCntUp.members.valueLow) > loRa.protocolParameters.maxFcntGap) //if this difference is greater than the value of max_fct_gap then too many data frames have been lost then subsequesnt will be discarded
                {
                    devices[dev_number].lorawanMacStatus.ackRequiredFromNextDownlinkMessage = 0; // reset the flag
                    if (rxPayload.RxAppData != NULL)
                    {
                        devices[dev_number].lorawanMacStatus.synchronization = 0; //clear the synchronization flag, because if the user will send a packet in the callback there is no need to send an empty packet
                        rxPayload.RxAppData (NULL, 0, MAC_NOT_OK);
                    }
                    devices[dev_number].macStatus.rxDone = 0;

                    // Inform application about rejoin in status
                    xTimerStop(devices[dev_number].sendWindow1TimerId,0);
                    devices[dev_number].macStatus.macState = IDLE;
                    return FRAME_COUNTER_ERROR_REJOIN_NEEDED;
                }
                else
                {
                    devices[dev_number].fCntUp.members.valueLow = hdr->members.fCnt;  //frame counter received is OK, so the value received from the server is kept in sync with the value stored in the end device
                }
            }
            else
            {
                if((hdr->members.fCnt == 0) && (devices[dev_number].fCntUp.members.valueLow == 0xFFFF))
                {
                    //Frame counter rolled over
                    devices[dev_number].fCntUp.members.valueLow = hdr->members.fCnt;
                    devices[dev_number].fCntUp.members.valueHigh ++;
                }
                else
                {
                    SetReceptionNotOkState();
                    //Reject packet
                    xTimerStop(devices[dev_number].sendWindow1TimerId,0);
                    devices[dev_number].macStatus.macState = IDLE;
                    return INVALID_PARAMETER;
                }
            }

            if (devices[dev_number].fCntUp.value == UINT32_MAX)
            {
                // Inform application about rejoin in status
                xTimerStop(devices[dev_number].sendWindow1TimerId,0);
                devices[dev_number].macStatus.macState = IDLE;
                return FRAME_COUNTER_ERROR_REJOIN_NEEDED;
            }
            devices[dev_number].macStatus.macState = BEFORE_ACK;
            loRa.macStatus.macState=IDLE;
            ESP_LOGI(TAG,"NSRxDone OK");
            return OK;
        }
        else
        {
            //if the mType is incorrect, set reception not OK state
            SetReceptionNotOkState ();
            return INVALID_PARAMETER;
        }
    }
    else
    {
        //Standalone radio reception OK, using the same callback as in LoRaWAN rx
        if ( rxPayload.RxAppData != NULL )
        {
            if ((RADIO_GetStatus() & RADIO_FLAG_RXERROR) == 0)
            {
                rxPayload.RxAppData(buffer, bufferLength, RADIO_OK);
            }
            else
            {
               rxPayload.RxAppData(buffer, bufferLength, RADIO_NOT_OK);
            }
        }
    }

    return OK;
}

