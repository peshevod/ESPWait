/*
 * channels.c
 *
 *  Created on: 21 июл. 2021 г.
 *      Author: ilya_000
 */

#include <stdint.h>

#include "lorawan_types.h"
#include "lorawan_defs.h"
#include "channels.h"
#include "freertos/timers.h"
#include "shell.h"
#include "lorax.h"
#include "sx1276_radio_driver.h"
#include "commands.h"
#include "crypto.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

// Channels by ism band
ChannelParams_t Channels[MAX_RU_SINGLE_BAND_CHANNELS];

static ChannelParams_t DefaultChannels868[] = {
LC0_868,
LC1_868,
LC2_868,
LC3_868,
LC4_868,
LC5_868,
};

static ChannelParams_t DefaultChannelsRU864[] = {
LC0_RU864,
LC1_RU864,
LC2_RU864,
LC3_RU864,
LC4_RU864,
LC5_RU864,
LC6_RU864,
LC7_RU864,
LC8_RU864,
LC9_RU864,
LC10_RU864,
LC11_RU864,
LC12_RU864,
LC13_RU864,
LC14_RU864,
LC15_RU864,
LC16_RU864,
};

static ChannelParams_t DefaultChannels433[] = {
LC0_433,
LC1_433,
LC2_433,
};

extern LoRa_t loRa;

LorawanError_t ValidateFrequency (uint32_t frequencyNew)
{
    LorawanError_t result = LORA_OK;

    if(ISM_EU868 == loRa.ismBand || ISM_RU864 == loRa.ismBand)
    {
        if ( (frequencyNew > FREQ_870000KHZ) || (frequencyNew < FREQ_863000KHZ) )
        {
            result = LORA_INVALID_PARAMETER ;
        }
    }
    else
    {
        if ( (frequencyNew > FREQ_434790KHZ) || (frequencyNew < FREQ_433050KHZ) )
        {
            result = LORA_INVALID_PARAMETER ;
        }
    }

    return result;
}

LorawanError_t ValidateDataRange (uint8_t dataRangeNew)
{
    LorawanError_t result = LORA_OK;
    uint8_t dataRateMax, dataRateMin;

    dataRateMin = dataRangeNew & LAST_NIBBLE;
    dataRateMax = (dataRangeNew & FIRST_NIBBLE) >> SHIFT4;

    if ( (ValidateDataRate (dataRateMax) != LORA_OK) || (ValidateDataRate (dataRateMin) != LORA_OK ) || (dataRateMax < dataRateMin) )
    {
        result = LORA_INVALID_PARAMETER;
    }
    return result;
}

LorawanError_t ValidateChannelMask (uint16_t channelMask)
{
   uint8_t i = 0;

   if(channelMask != 0x0000U)
   {
        for (i = 0; i < MAX_EU_SINGLE_BAND_CHANNELS; i++)
        {
            if ( ( (channelMask & BIT0) == BIT0) && ( (Channels[i].parametersDefined & (FREQUENCY_DEFINED | DATA_RANGE_DEFINED | DUTY_CYCLE_DEFINED) ) != (FREQUENCY_DEFINED | DATA_RANGE_DEFINED | DUTY_CYCLE_DEFINED) ) )  // if the channel mask sent enables a yet undefined channel, the command is discarded and the device state is not changed
            {
                return LORA_INVALID_PARAMETER;
            }
            else
            {
                channelMask = channelMask >> SHIFT1;
            }
        }

      return LORA_OK;
   }
   else
   {
       //ChMask set to 0x0000 in ADR may be used as a DoS attack so receiving this results in an error
       return LORA_INVALID_PARAMETER;
   }
}

LorawanError_t ValidateChannelMaskCntl (uint8_t channelMaskCntl)
{
   LorawanError_t result = LORA_OK;

   if ( (channelMaskCntl != 0) && (channelMaskCntl != 6) )
   {
       result = LORA_INVALID_PARAMETER;
   }

  return result;
}

LorawanError_t ValidateChannelId (uint8_t channelId, bool allowedForDefaultChannels)  //if allowedForDefaultChannels is 1, all the channels can be modified, if it is 0 channels 0, 1, 2 and 16, 17, and 18 (dual band) cannot be modified
{
    LorawanError_t result = LORA_OK;

    if ( (channelId >= MAX_RU_SINGLE_BAND_CHANNELS) ||  ( (allowedForDefaultChannels == WITHOUT_DEFAULT_CHANNELS) && (channelId < 3) ) )
    {
        result = LORA_INVALID_PARAMETER ;
    }

    return result;
}



void UpdateChannelIdStatus (uint8_t channelId, bool statusNew)
{
    uint8_t i;

    Channels[channelId].status = statusNew;
    if(Channels[channelId].status == DISABLED)
    {
        //Clear the dutycycle timer of the channel
        Channels[channelId].channelTimer = 0;
    }

    for (i = 0; i < loRa.maxChannels; i++)
    {
        if ( (Channels[i].dataRange.min < loRa.minDataRate) && (Channels[i].status == ENABLED) )
        {
            loRa.minDataRate = Channels[i].dataRange.min;
        }
        if ( (Channels[i].dataRange.max > loRa.maxDataRate) && (Channels[i].status == ENABLED) )
        {
            loRa.maxDataRate = Channels[i].dataRange.max;
        }
    }

    if (loRa.currentDataRate > loRa.maxDataRate)
    {
        loRa.currentDataRate = loRa.maxDataRate;
    }

    if (loRa.currentDataRate < loRa.minDataRate)
    {
        loRa.currentDataRate = loRa.minDataRate;
    }
}

LorawanError_t LORAWAN_SetChannelIdStatus (uint8_t channelId, bool statusNew)
{
    LorawanError_t result = LORA_OK;


    if (ValidateChannelId (channelId, ALL_CHANNELS) != LORA_OK)
    {
        result = LORA_INVALID_PARAMETER;
    }

    else
    {
        if ( (Channels[channelId].parametersDefined & (FREQUENCY_DEFINED | DATA_RANGE_DEFINED | DUTY_CYCLE_DEFINED) ) == (FREQUENCY_DEFINED | DATA_RANGE_DEFINED | DUTY_CYCLE_DEFINED) )
        {
            UpdateChannelIdStatus (channelId, statusNew);
        }
        else
        {
            result = LORA_INVALID_PARAMETER;
        }
    }

    return result;
}

void EnableChannels1 (uint16_t channelMask, uint8_t channelMaskCntl, uint8_t channelIndexMin,  uint8_t channelIndexMax)
{
   uint8_t i;

   if (channelMaskCntl == 6)
   {
       for ( i = channelIndexMin; i < channelIndexMax; i++ )
       {
           UpdateChannelIdStatus (i, ENABLED);
       }
   }
   else if (channelMaskCntl == 0)
   {
       for ( i = channelIndexMin; i < channelIndexMax; i++ )
       {
           if ( ( channelMask & BIT0 ) == BIT0)
           {
               UpdateChannelIdStatus (i, ENABLED);
           }
           else
           {
               UpdateChannelIdStatus (i, DISABLED);
           }
           channelMask = channelMask >> SHIFT1;
       }
   }
}

void EnableChannels (uint16_t channelMask, uint8_t channelMaskCntl)
{
    EnableChannels1 (channelMask, channelMaskCntl, 0, MAX_EU_SINGLE_BAND_CHANNELS);
}

void UpdateFrequency (uint8_t channelId, uint32_t frequencyNew )
{
    Channels[channelId].frequency = frequencyNew;
    Channels[channelId].parametersDefined |= FREQUENCY_DEFINED;
}

void UpdateDutyCycle (uint8_t channelId, uint16_t dutyCycleNew)
{
    Channels[channelId].dutyCycle = dutyCycleNew;
    Channels[channelId].parametersDefined |= DUTY_CYCLE_DEFINED;
}

void UpdateDataRange (uint8_t channelId, uint8_t dataRangeNew)
{
    uint8_t i;
    // after updating the data range of a channel we need to check if the minimum dataRange has changed or not.
    // The user cannot set the current data rate outside the range of the data range
    loRa.minDataRate = DR7;
    loRa.maxDataRate = DR0;

    Channels[channelId].dataRange.value = dataRangeNew;
    Channels[channelId].parametersDefined |= DATA_RANGE_DEFINED;
    for (i=0; i < loRa.maxChannels; i++)
    {
        if ( (Channels[i].dataRange.min < loRa.minDataRate) && (Channels[i].status == ENABLED) )
        {
            loRa.minDataRate = Channels[i].dataRange.min;
        }
        if ( (Channels[i].dataRange.max > loRa.maxDataRate) && (Channels[i].status == ENABLED) )
        {
            loRa.maxDataRate = Channels[i].dataRange.max;
        }
    }

    if (loRa.currentDataRate > loRa.maxDataRate)
    {
        loRa.currentDataRate = loRa.maxDataRate;
    }

    if (loRa.currentDataRate < loRa.minDataRate)
    {
        loRa.currentDataRate = loRa.minDataRate;
    }
}

// This function checks which value can be assigned to the current data rate.
bool FindSmallestDataRate (void)
{
    uint8_t  i = 0, dataRate;
    bool found = false;

    if ((loRa.currentDataRate > loRa.minDataRate) && false)
    {
        dataRate = loRa.currentDataRate - 1;

        while ( (found == false) && (dataRate >= loRa.minDataRate) )
        {
            for ( i = 0; i < loRa.maxChannels; i++ )
            {
                if ( (dataRate >= Channels[i].dataRange.min) && (dataRate <= Channels[i].dataRange.max ) && ( Channels[i].status == ENABLED ) )
                {
                    found = true;
                    break;
                }
            }
            if ( (found == false) &&  (dataRate > loRa.minDataRate) ) // if no channels were found after one search, then the device will switch to the next lower data rate possible
            {
                dataRate = dataRate - 1;
            }
        }

        if (found == true)
        {
            loRa.currentDataRate = dataRate;
        }
    }

    return found;
}

void UpdateCfList (uint8_t bufferLength, JoinAccept_t *joinAccept)
{
    uint8_t i;
    uint32_t frequency;
    uint8_t channelIndex;

    if ( (bufferLength == SIZE_JOIN_ACCEPT_WITH_CFLIST) )
    {
        // 3 is the minimum channel index for single band
        channelIndex = 3;

        for (i = 0; i < NUMBER_CFLIST_FREQUENCIES; i++ )
        {
            frequency = 0;
            memcpy (&frequency, joinAccept->members.cfList + 3*i, 3);
            frequency *= 100;
            if (frequency != 0)
            {
                if (ValidateFrequency (frequency) == LORA_OK)
                {
                    Channels[i+channelIndex].frequency = frequency;
                    Channels[i+channelIndex].dataRange.max = DR5;
                    Channels[i+channelIndex].dataRange.min = DR0;
                    Channels[i+channelIndex].dutyCycle = DUTY_CYCLE_DEFAULT_NEW_CHANNEL;
                    Channels[i+channelIndex].parametersDefined = 0xFF; //all parameters defined
                    LORAWAN_SetChannelIdStatus(i+channelIndex, ENABLED);
                    loRa.macStatus.channelsModified = ENABLED; // a new channel was added, so the flag is set to inform the user
                }
            }
            else
            {
                LORAWAN_SetChannelIdStatus(i+channelIndex, DISABLED);
            }
         }

         loRa.macStatus.channelsModified = ENABLED;
    }
}

void UpdateMinMaxChDataRate (void)
{
    uint8_t i;
    // after updating the data range of a channel we need to check if the minimum dataRange has changed or not.
    // The user cannot set the current data rate outside the range of the data range
    loRa.minDataRate = DR7;
    loRa.maxDataRate = DR0;

    for (i = 0; i < loRa.maxChannels; i++)
    {
        if ( (Channels[i].dataRange.min < loRa.minDataRate) && (Channels[i].status == ENABLED) )
        {
            loRa.minDataRate = Channels[i].dataRange.min;
        }
        if ( (Channels[i].dataRange.max > loRa.maxDataRate) && (Channels[i].status == ENABLED) )
        {
            loRa.maxDataRate = Channels[i].dataRange.max;
        }
    }
}

void StartReTxTimer(void)
{
    uint8_t i;
    uint32_t minim = UINT32_MAX;

    for (i = 0; i <= loRa.maxChannels; i++)
    {
        if ( (Channels[i].status == ENABLED) && (Channels[i].channelTimer != 0) && (Channels[i].channelTimer <= minim) && (loRa.currentDataRate >= Channels[i].dataRange.min) && (loRa.currentDataRate <= Channels[i].dataRange.max) )
        {
            minim = Channels[i].channelTimer;
        }
    }
    loRa.macStatus.macState = RETRANSMISSION_DELAY;
    xTimerChangePeriod (loRa.automaticReplyTimerId, minim/portTICK_PERIOD_MS, 0);
    xTimerStart (loRa.automaticReplyTimerId, 0);
}

LorawanError_t SearchAvailableChannel (uint8_t maxChannels, bool transmissionType, uint8_t* channelIndex)
{
    uint8_t randomNumberCopy, randomNumber, i;
    LorawanError_t result = LORA_OK;

    set_s("CHANNEL",&i);
    if(i!=0xFF)
    {
        *channelIndex=i;
        return LORA_OK;
    }

    randomNumber = Random (maxChannels) + 1; //this is a guard so that randomNumber is not 0 and the search will happen
    randomNumberCopy = randomNumber;

    while (randomNumber)
    {
        for (i=0; (i < maxChannels) && (randomNumber != 0) ; i++)
        {
            if ( ( Channels[i].status == ENABLED ) && ( Channels[i].channelTimer == 0 ) && ( loRa.currentDataRate >= Channels[i].dataRange.min ) && ( loRa.currentDataRate <= Channels[i].dataRange.max ) )
            {
                if (transmissionType == 0) // if transmissionType is join request, then check also for join request channels
                {
                    if ( Channels[i].joinRequestChannel == 1 )
                    {
                        randomNumber --;
                    }
                }
                else
                {
                    randomNumber --;
                }
            }
        }
        // if after one search in all the vector no valid channel was found, exit the loop and return an error
        if ( randomNumber == randomNumberCopy )
        {
            result = NO_CHANNELS_FOUND;
            break;
        }
    }

    if ( i != 0)
    {
        *channelIndex = i - 1;
    }
    else
    {
        result = NO_CHANNELS_FOUND;
    }
    return result;
}

LorawanError_t SelectChannelForTransmission (bool transmissionType, Direction_t dir)  // transmission type is 0 means join request, transmission type is 1 means data message mode
{
    LorawanError_t result = LORA_OK;
    uint8_t channelIndex;
    char TAG[]="SelectChannelForTransmission";

    result = SearchAvailableChannel (MAX_RU_SINGLE_BAND_CHANNELS, transmissionType, &channelIndex);

    if (result == LORA_OK)
    {
        loRa.lastUsedChannelIndex = channelIndex;
        loRa.receiveWindow1Parameters.frequency = Channels[channelIndex].frequency;
        loRa.receiveWindow1Parameters.dataRate = loRa.currentDataRate;

        ConfigureRadio(loRa.receiveWindow1Parameters.dataRate, loRa.receiveWindow1Parameters.frequency, dir);
    }
    else ESP_LOGE(TAG,"Error SearchAvailableChannel result=%d",result);
    return result;
}

void InitDefaultRU864Channels(void)
{
    memset (Channels, 0, sizeof(Channels) );
    memcpy (Channels, DefaultChannelsRU864, sizeof(DefaultChannelsRU864) );
}

void InitDefault868Channels (void)
{
    uint8_t i;

    memset (Channels, 0, sizeof(Channels) );
    memcpy (Channels, DefaultChannels868, sizeof(DefaultChannels868) );
    for (i = 3; i < MAX_EU_SINGLE_BAND_CHANNELS; i++)
    {
        // for undefined channels the duty cycle should be a very big value, and the data range a not-valid value
        //duty cycle 0 means no duty cycle limitation, the bigger the duty cycle value, the greater the limitation
        Channels[i].dutyCycle = UINT16_MAX;
        Channels[i].dataRange.value = UINT8_MAX;
    }
}

void InitDefault433Channels (void)
{
    uint8_t i;

    memset (Channels, 0, sizeof(Channels) );
    memcpy (Channels, DefaultChannels433, sizeof(DefaultChannels433) );
    for (i = 3; i < MAX_EU_SINGLE_BAND_CHANNELS; i++)
    {
        // for undefined channels the duty cycle should be a very big value, and the data range a not-valid value
        //duty cycle 0 means no duty cycle limitation, the bigger the duty cycle value, the greater the limitation
        Channels[i].dutyCycle = UINT16_MAX;
        Channels[i].dataRange.value = UINT8_MAX;
    }
}





