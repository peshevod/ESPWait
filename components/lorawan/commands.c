/*
 * commands.c
 *
 *  Created on: 21 июл. 2021 г.
 *      Author: ilya_000
 */

#include "commands.h"
#include "channels.h"
#include "esp_log.h"
#include "sx1276_radio_driver.h"

extern LoRa_t loRa;
static const uint8_t macEndDevCmdReplyLen[] = {1, 2, 1, 2, 3, 2, 1};


uint8_t CountfOptsLength (void)
{
    uint8_t i, macCommandLength=0;

    for (i = 0; i < loRa.crtMacCmdIndex; i++)
    {
        if(loRa.macCommands[i].receivedCid != INVALID_VALUE)
        {
            if((macCommandLength + macEndDevCmdReplyLen[loRa.macCommands[i].receivedCid - 2]) <= MAX_FOPTS_LEN)
            {
                macCommandLength += macEndDevCmdReplyLen[loRa.macCommands[i].receivedCid - 2];
            }
            else
            {
                break;
            }
        }
    }

    return macCommandLength;
}

void UpdateReceiveWindow2Parameters (uint32_t frequency, uint8_t dataRate)
{
    loRa.receiveWindow2Parameters.dataRate = dataRate;
    loRa.receiveWindow2Parameters.frequency = frequency;
}

void UpdateReceiveDelays (uint8_t delay)
{
    loRa.protocolParameters.receiveDelay1 = 1000 * delay ;
    if (delay == 0)
    {
        loRa.protocolParameters.receiveDelay1 = 1000;
    }

    loRa.protocolParameters.receiveDelay2 = loRa.protocolParameters.receiveDelay1 + 1000;
}

void UpdateDLSettings(uint8_t dlRx2Dr, uint8_t dlRx1DrOffset)
{
    if (dlRx2Dr <= DR7)
    {
        loRa.receiveWindow2Parameters.dataRate = dlRx2Dr;
    }

    if (dlRx1DrOffset <= 5)
    {
        // update the offset between the uplink data rate and the downlink data rate used to communicate with the end device on the first reception slot
        loRa.offset = dlRx1DrOffset;
    }
}


LorawanError_t ValidateDataRate (uint8_t dataRate)
{
    LorawanError_t result = OK;

    if ( dataRate > DR7 )
    {
        result = INVALID_PARAMETER;
    }

    return result;
}

LorawanError_t ValidateTxPower (uint8_t txPowerNew)
{
    LorawanError_t result = OK;

    if ((ISM_EU868 == loRa.ismBand) && ((TXPOWER_MIN == txPowerNew) || (txPowerNew > TXPOWER_MAX)))
    {
        result = INVALID_PARAMETER;
    }
    else if((ISM_RU864 == loRa.ismBand) && ((TXPOWERRU864_MIN == txPowerNew) || (txPowerNew > TXPOWERRU864_MAX)))
    {
        result = INVALID_PARAMETER;
    }
    return result;
}

void UpdateCurrentDataRate (uint8_t valueNew)
{
    loRa.currentDataRate = valueNew;
}

void UpdateTxPower (uint8_t txPowerNew)
{
    loRa.txPower = txPowerNew;
}

static LorawanError_t ValidateRxOffset (uint8_t rxOffset)
{
    LorawanError_t result =  OK;

    if (rxOffset > 5)
    {
        result = INVALID_PARAMETER;
    }

    return result;
}

static uint8_t* ExecuteNewChannel (uint8_t *ptr)
{
    uint8_t channelIndex;
    DataRange_t drRange;
    uint32_t frequency = 0;

    channelIndex = *(ptr++);

    frequency = (*((uint32_t*)ptr)) & 0x00FFFFFF;
    frequency = frequency * 100;
    ptr = ptr + 3;  // 3 bytes for frequecy

    drRange.value = *(ptr++);

    if (ValidateChannelId (channelIndex, WITHOUT_DEFAULT_CHANNELS) == LORA_OK)
    {
        if ( (ValidateFrequency (frequency) == LORA_OK) || (frequency == 0) )
        {
            loRa.macCommands[loRa.crtMacCmdIndex].channelFrequencyAck = 1;
        }

        if (ValidateDataRange (drRange.value) == LORA_OK)
        {
            loRa.macCommands[loRa.crtMacCmdIndex].dataRateRangeAck = 1;
        }
    }

    if ( (loRa.macCommands[loRa.crtMacCmdIndex].channelFrequencyAck == 1) && (loRa.macCommands[loRa.crtMacCmdIndex].dataRateRangeAck == 1) )
    {
        if (loRa.lastUsedChannelIndex < 16)
        {
            if (frequency != 0)
            {
                UpdateFrequency (channelIndex, frequency);
                UpdateDataRange (channelIndex, drRange.value);
                UpdateDutyCycle (channelIndex, DUTY_CYCLE_DEFAULT);
                UpdateChannelIdStatus (channelIndex, ENABLED);
            }
            else
            {
                LORAWAN_SetChannelIdStatus (channelIndex, DISABLED);  // according to the spec, a frequency value of 0 disables the channel
            }
        }
        else
        {
            if (frequency != 0)
            {
                UpdateFrequency (channelIndex + 16, frequency);
                UpdateDataRange (channelIndex + 16, drRange.value);
                UpdateDutyCycle (channelIndex + 16, DUTY_CYCLE_DEFAULT);
                UpdateChannelIdStatus (channelIndex + 16, ENABLED);
            }
            else
            {
                LORAWAN_SetChannelIdStatus (channelIndex + 16, DISABLED);  // according to the spec, a frequency value of 0 disables the channel
            }
        }

        loRa.macStatus.channelsModified = 1; // a new channel was added, so the flag is set to inform the user
    }
    return ptr;
}

static uint8_t* ExecuteLinkCheck (uint8_t *ptr)
{
   loRa.linkCheckMargin = *(ptr++);
   loRa.linkCheckGwCnt = *(ptr++);
   return ptr;
}

static uint8_t* ExecuteRxTimingSetup (uint8_t *ptr)
{
   uint8_t delay;

   delay = (*ptr) & LAST_NIBBLE;
   ptr++;

   UpdateReceiveDelays (delay);
   loRa.macStatus.rxTimingSetup = ENABLED;

   return ptr;
}

static uint8_t* ExecuteDutyCycle (uint8_t *ptr)
{
    uint8_t maxDCycle;

    maxDCycle = *(ptr++);
    if (maxDCycle < 15)
    {
        loRa.prescaler = 1 << maxDCycle; // Execute the 2^maxDCycle here
        loRa.macStatus.prescalerModified = ENABLED;
    }

    if (maxDCycle == 255)
    {
        loRa.macStatus.silentImmediately = ENABLED;
    }

    return ptr;
}

static uint8_t* ExecuteLinkAdr (uint8_t *ptr)
{
    uint8_t txPower, dataRate;
    uint16_t channelMask;

    txPower = *(ptr) & LAST_NIBBLE;
    dataRate = ( *(ptr) & FIRST_NIBBLE ) >> SHIFT4;
    ptr++;
    channelMask = (*((uint16_t*)ptr));
    ptr = ptr + sizeof (channelMask);
    Redundancy_t *redundancy;
    redundancy = (Redundancy_t*)(ptr++);

    if (ENABLED == loRa.macStatus.adr)
    {
        if ( (ValidateChannelMaskCntl(redundancy->chMaskCntl) == LORA_OK) && (ValidateChannelMask(channelMask) == LORA_OK) )  // If the ChMask field value is one of values meaning RFU, the end-device should reject the command and unset the Channel mask ACK bit in its response.
        {
            loRa.macCommands[loRa.crtMacCmdIndex].channelMaskAck = 1;
        }

        if ( (ValidateDataRate (dataRate) == LORA_OK) &&  (dataRate >= loRa.minDataRate) && (dataRate <= loRa.maxDataRate) )
        {
            loRa.macCommands[loRa.crtMacCmdIndex].dataRateAck = 1;
        }

        if (ValidateTxPower (txPower) == LORA_OK)
        {
            loRa.macCommands[loRa.crtMacCmdIndex].powerAck = 1;
        }

        if ( (loRa.macCommands[loRa.crtMacCmdIndex].powerAck == 1) && (loRa.macCommands[loRa.crtMacCmdIndex].dataRateAck == 1) && (loRa.macCommands[loRa.crtMacCmdIndex].channelMaskAck == 1) )
        {
            EnableChannels (channelMask, redundancy->chMaskCntl);

            UpdateTxPower (txPower);
            loRa.macStatus.txPowerModified = ENABLED; // the current tx power was modified, so the user is informed about the change via this flag
            UpdateCurrentDataRate (dataRate);

            if (redundancy->nbRep == 0)
            {
                loRa.maxRepetitionsUnconfirmedUplink = 0;
            }
            else
            {
                loRa.maxRepetitionsUnconfirmedUplink = redundancy->nbRep - 1;
            }
            loRa.macStatus.nbRepModified = 1;
        }
    }
    else
    {
        loRa.macCommands[loRa.crtMacCmdIndex].channelMaskAck = 0;
        loRa.macCommands[loRa.crtMacCmdIndex].dataRateAck = 0;
        loRa.macCommands[loRa.crtMacCmdIndex].powerAck = 0;
    }

    return ptr;
}

static uint8_t* ExecuteDevStatus (uint8_t *ptr)
{
    return ptr;
}

static uint8_t* ExecuteRxParamSetupReq (uint8_t *ptr)
{
    DlSettings_t dlSettings;
    uint32_t frequency = 0;

    //In the status field (response) we have to include the following: channle ACK, RX2 data rate ACK, RX1DoffsetACK

    dlSettings.value = *(ptr++);

    frequency = (*((uint32_t*)ptr)) & 0x00FFFFFF;
    frequency = frequency * 100;
    ptr = ptr + 3; //3 bytes for frequency

    if (ValidateFrequency (frequency) == LORA_OK)
    {
        loRa.macCommands[loRa.crtMacCmdIndex].channelAck = 1;
    }

    if (ValidateDataRate (dlSettings.bits.rx2DataRate) == LORA_OK)
    {
        loRa.macCommands[loRa.crtMacCmdIndex].dataRateReceiveWindowAck = 1;
    }

    if (ValidateRxOffset (dlSettings.bits.rx1DROffset) == LORA_OK)
    {
        loRa.macCommands[loRa.crtMacCmdIndex].rx1DROffestAck = 1;
    }

    if ( (loRa.macCommands[loRa.crtMacCmdIndex].dataRateReceiveWindowAck == 1) && (loRa.macCommands[loRa.crtMacCmdIndex].channelAck == 1) && (loRa.macCommands[loRa.crtMacCmdIndex].rx1DROffestAck == 1))
    {
        loRa.offset = dlSettings.bits.rx1DROffset;
        UpdateReceiveWindow2Parameters (frequency, dlSettings.bits.rx2DataRate);
        loRa.macStatus.secondReceiveWindowModified = 1;
    }

    return ptr;
}

uint8_t* MacExecuteCommands (uint8_t *buffer, uint8_t fOptsLen)
{
    bool done = false;
    uint8_t *ptr;
    ptr = buffer;
    while ( (ptr < ( buffer + fOptsLen )) && (done == false) )
    {
        //Clean structure before using it
        loRa.macCommands[loRa.crtMacCmdIndex].channelMaskAck = 0;
        loRa.macCommands[loRa.crtMacCmdIndex].dataRateAck = 0;
        loRa.macCommands[loRa.crtMacCmdIndex].powerAck = 0;
        loRa.macCommands[loRa.crtMacCmdIndex].channelAck = 0;
        loRa.macCommands[loRa.crtMacCmdIndex].dataRateReceiveWindowAck = 0;
        loRa.macCommands[loRa.crtMacCmdIndex].rx1DROffestAck = 0;
        loRa.macCommands[loRa.crtMacCmdIndex].dataRateRangeAck = 0;
        loRa.macCommands[loRa.crtMacCmdIndex].channelFrequencyAck = 0;

        //Reply has the same value as request
        loRa.macCommands[loRa.crtMacCmdIndex].receivedCid = *ptr;

        switch (*ptr ++)
        {
            case LINK_CHECK_CID:
            {
                ptr = ExecuteLinkCheck (ptr );
                // No reply to server is needed
                loRa.macCommands[loRa.crtMacCmdIndex].receivedCid = INVALID_VALUE;
            } break;

            case LINK_ADR_CID:
            {
                ptr = ExecuteLinkAdr (ptr );
            } break;

            case DUTY_CYCLE_CID:
            {
                ptr = ExecuteDutyCycle(ptr);
            } break;

            case RX2_SETUP_CID:
            {
                ptr = ExecuteRxParamSetupReq (ptr);
            } break;

            case DEV_STATUS_CID:
            {
                 ptr = ExecuteDevStatus (ptr);
            } break;

            case NEW_CHANNEL_CID:
            {
                ptr = ExecuteNewChannel (ptr);

            } break;

            case RX_TIMING_SETUP_CID:
            {
                ptr = ExecuteRxTimingSetup (ptr);
            } break;

            default:
            {
                done = true;  // Unknown MAC commands cannot be skipped and the first unknown MAC command terminates the processing of the MAC command sequence.
                ptr = buffer + fOptsLen;
                loRa.macCommands[loRa.crtMacCmdIndex].receivedCid = INVALID_VALUE;
            } break;
        }

        if((loRa.macCommands[loRa.crtMacCmdIndex].receivedCid != INVALID_VALUE) &&
           (loRa.crtMacCmdIndex < MAX_NB_CMD_TO_PROCESS))
        {
            loRa.crtMacCmdIndex ++;
        }
    }
    return ptr;
}

void IncludeMacCommandsResponse (uint8_t* macBuffer, uint8_t* pBufferIndex, uint8_t bIncludeInFopts )
{
    uint8_t i = 0;
    uint8_t bufferIndex = *pBufferIndex;

    for(i = 0; i < loRa.crtMacCmdIndex ; i++)
    {
        if((bIncludeInFopts) && (loRa.macCommands[i].receivedCid != INVALID_VALUE))
        {
            if((bufferIndex - (*pBufferIndex) + macEndDevCmdReplyLen[loRa.macCommands[i].receivedCid - 2]) > MAX_FOPTS_LEN)
            {
                break;
            }
        }
        switch (loRa.macCommands[i].receivedCid)
        {
            case LINK_ADR_CID:
            {
                macBuffer[bufferIndex++] = LINK_ADR_CID;
                macBuffer[bufferIndex] = 0x00;
                if (loRa.macCommands[i].channelMaskAck == 1)
                {
                    macBuffer[bufferIndex] |= CHANNEL_MASK_ACK;
                }

                if (loRa.macCommands[i].dataRateAck == 1)
                {
                    macBuffer[bufferIndex] |= DATA_RATE_ACK;
                }

                if (loRa.macCommands[i].powerAck == 1)
                {
                    macBuffer[bufferIndex] |= POWER_ACK;
                }
                bufferIndex ++;
            }
            break;

            case RX2_SETUP_CID:
            {
                macBuffer[bufferIndex++] = RX2_SETUP_CID;
                macBuffer[bufferIndex] = 0x00;
                if (loRa.macCommands[i].channelAck == 1)
                {
                    macBuffer[bufferIndex] |= CHANNEL_MASK_ACK;
                }

                if (loRa.macCommands[i].dataRateReceiveWindowAck == 1)
                {
                    macBuffer[bufferIndex] |= DATA_RATE_ACK;
                }

                if (loRa.macCommands[i].rx1DROffestAck == 1)
                {
                    macBuffer[bufferIndex] |= RX1_DR_OFFSET_ACK;
                }

                bufferIndex ++;
            }
            break;

            case DEV_STATUS_CID:
            {
                macBuffer[bufferIndex++] = DEV_STATUS_CID;
                macBuffer[bufferIndex++] = loRa.batteryLevel;
                if ((RADIO_GetPacketSnr() < -32) || (RADIO_GetPacketSnr() > 31))
                {
                    macBuffer[bufferIndex++] = 0x20;  //if the value returned by the radio is out of range, send the minimum (-32)
                }
                else
                {
                    macBuffer[bufferIndex++] = ((uint8_t)RADIO_GetPacketSnr() & 0x3F);  //bits 7 and 6 are RFU, bits 5-0 are the margin level response;
                }
            }
            break;

            case NEW_CHANNEL_CID:
            {
                macBuffer[bufferIndex++] = NEW_CHANNEL_CID;
                macBuffer[bufferIndex] = 0x00;
                if (loRa.macCommands[i].channelFrequencyAck == 1)
                {
                    macBuffer[bufferIndex] |= CHANNEL_MASK_ACK;
                }

                if (loRa.macCommands[i].dataRateRangeAck == 1)
                {
                    macBuffer[bufferIndex] |= DATA_RATE_ACK;
                }
                bufferIndex ++;
            }
            break;

            case LINK_CHECK_CID:
            {
                loRa.linkCheckMargin = 255; // reserved
                loRa.linkCheckGwCnt = 0;
                macBuffer[bufferIndex++] = loRa.macCommands[i].receivedCid;
            }
            break;

            case RX_TIMING_SETUP_CID: //Fallthrough
            case DUTY_CYCLE_CID:      //Fallthrough
            {
                macBuffer[bufferIndex++] = loRa.macCommands[i].receivedCid;
            }
            break;

            default:
                //CID = 0xFF
                break;
        }
    }

    *pBufferIndex = bufferIndex;
}



