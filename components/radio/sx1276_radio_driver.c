/*
 * SX1276_radio_driver.c
 *
 *  Created on: 18 июл. 2021 г.
 *      Author: ilya_000
 */

#include <stdlib.h>
#include <string.h>
#include "sx1276_radio_driver.h"

#include "radio_registers_SX1276.h"
#include "sx1276_hal.h"
#include "shell.h"
#include "esp_event.h"
#include "lorawan_defs.h"
#include "MainLoop.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"



#define TIME_ON_AIR_LOAD_VALUE              ((uint32_t)20000)
#define WATCHDOG_DEFAULT_TIME               ((uint32_t)15000)


// These enums need to be set according to the definition in the radio
// datasheet


static RadioConfiguration_t RadioConfiguration;
static RadioConfiguration_t* sRadioConfiguration;

static void RADIO_WriteMode(RadioMode_t newMode, RadioModulation_t newModulation, uint8_t blocking);
static void RADIO_Reset(void);
static void RADIO_WriteFrequency(uint32_t frequency);
static void RADIO_WriteFSKFrequencyDeviation(uint32_t frequencyDeviation);
static void RADIO_WriteFSKBitRate(uint32_t bitRate);
static void RADIO_WritePower(int8_t power);
static void RADIO_WriteConfiguration(uint16_t symbolTimeout);
static void RADIO_RxDone(void);
static void RADIO_FSKPayloadReady(void);
static void RADIO_RxTimeout(void);
static void RADIO_TxDone(void);
static void RADIO_FSKPacketSent(void);
static void RADIO_UnhandledInterrupt(RadioModulation_t modulation);
static void RADIO_FHSSChangeChannel(void);

uint8_t b[128];
uint8_t trace;
uint8_t rssi_reg, rssi_off;
extern uint8_t mode;
uint8_t spread_factor;
TimerHandle_t rectimer;
uint32_t rectimer_value, delta;
TickType_t ticks, ticks_old;
static char TAG[]="SX1276_driver";
extern esp_event_loop_handle_t mainLoop;
ESP_EVENT_DECLARE_BASE(LORA_EVENTS);
RadioMode_t currentMode;
TimerHandle_t startTimerId;
extern const int8_t txPowerRU864[];


// This function repurposes DIO5 for ModeReady functionality
static void RADIO_WriteMode(RadioMode_t newMode, RadioModulation_t newModulation, uint8_t blocking)
{
    uint8_t opMode;
    uint8_t dioMapping;
    RadioModulation_t currentModulation;
    RadioMode_t currentMode;
//    ESP_LOGI(TAG,"Enter in RADIO_WriteMode newModulation=%s, newMode=%d",MODULATION_FSK == newModulation ? "MODULATION_FSK":"MODULATION_LORA",newMode);

    if ((MODULATION_FSK == newModulation) &&
        ((MODE_RXSINGLE == newMode) || (MODE_CAD == newMode)))
    {
        // Unavaliable modes for FSK. Just return.
        return;
    }

    // Sanity enforcement on parameters
    newMode &= 0x07;
    newModulation &= 0x01;

    opMode = RADIO_RegisterRead(REG_OPMODE);

    if ((opMode & 0x80) != 0)
    {
        currentModulation = MODULATION_LORA;
    }
    else
    {
        currentModulation = MODULATION_FSK;
    }

    currentMode = opMode & 0x07;

    // If we need to change modulation, we need to do this in sleep mode.
    // Otherwise, we can go straight to changing the current mode to newMode.
    if (newModulation != currentModulation)
    {
        // Go to sleep
        if (MODE_SLEEP != currentMode)
        {
            // Clear mode bits, effectively going to sleep
            RADIO_RegisterWrite(REG_OPMODE, opMode & (~0x07));
            currentMode = MODE_SLEEP;
        }
        // Change modulation
        if (MODULATION_FSK == newModulation)
        {
            // Clear MSB and sleep bits to make it stay in sleep
            opMode = opMode & (~0x87);
        }
        else
        {
            // LoRa mode. Set MSB and clear sleep bits to make it stay in sleep
            opMode = 0x80 | (opMode & (~0x87));
        }
        RADIO_RegisterWrite(REG_OPMODE, opMode);
    }

    // From here on currentModulation is no longer current, we will use
    // newModulation instead as it reflects the chip configuration.
    // opMode reflects the actual configuration of the chip.

    if (newMode != currentMode)
    {
        // If we need to block until the mode switch is ready, configure the
        // DIO5 pin to relay this information.
        if ((MODE_SLEEP != newMode) && (1 == blocking))
        {
            dioMapping = RADIO_RegisterRead(REG_DIOMAPPING2);
            if (MODULATION_FSK == newModulation)
            {
                // FSK mode
                dioMapping |= 0x30;     // DIO5 = 11 means ModeReady in FSK mode
            }
            else
            {
                // LoRa mode
                dioMapping &= ~0x30;    // DIO5 = 00 means ModeReady in LoRa mode
            }
            RADIO_RegisterWrite(REG_DIOMAPPING2, dioMapping);
        }

        // Do the actual mode switch.
        opMode &= ~0x07;                // Clear old mode bits
        opMode |= newMode;              // Set new mode bits
        RADIO_RegisterWrite(REG_OPMODE, opMode);

        // If required and possible, wait for switch to complete
        if (1 == blocking)
        {
            if (MODE_SLEEP != newMode)
            {
                while (HALDIO5PinValue() == 0)
                    ;
            }
            else
            {
                vTaskDelay(1);
            }
        }
		esp_event_post_to(mainLoop, LORA_EVENTS, LORA_CHANGE_MAC_STATE_EVENT, &newMode, sizeof(newMode), 0);
//		ESP_LOGI(TAG,"Exit from RADIO_WriteMode");
    }
}


static void RADIO_Reset(void)
{
    HALResetPinMakeOutput();
    HALResetPinOutputValue(0);
    HALResetPinMakeInput();
    vTaskDelay(10);
    //Added these two lines to make sure this pin is not left in floating state during sleep
    HALResetPinMakeOutput();
    HALResetPinOutputValue(1);
}

// The math in this function needs adjusting for FXOSC != 32MHz
static void RADIO_WriteFrequency(uint32_t frequency)
{
    uint32_t num, num_mod;

//    ESP_LOGI(TAG,"Enter in RADIO_WriteFrequency freq=%d",frequency);
    // Frf = (Fxosc * num) / 2^19
    // We take advantage of the fact that 32MHz = 15625Hz * 2^11
    // This simplifies our formula to Frf = (15625Hz * num) / 2^8
    // Thus, num = (Frf * 2^8) / 15625Hz

    // First, do the division, since Frf * 2^8 does not fit in 32 bits
    num = frequency / 15625;
    num_mod = frequency % 15625;

    // Now do multiplication as well, both for the quotient as well as for
    // the remainder
    num <<= SHIFT8;
    num_mod <<= SHIFT8;

    // Try to correct for the remainder. After the multiplication we can still
    // recover some accuracy
    num_mod = num_mod / 15625;
    num += num_mod;

    // Now variable num holds the representation of the frequency that needs to
    // be loaded into the radio chip
    RADIO_RegisterWrite(REG_FRFMSB, (num >> SHIFT16) & 0xFF);
    RADIO_RegisterWrite(REG_FRFMID, (num >> SHIFT8) & 0xFF);
    RADIO_RegisterWrite(REG_FRFLSB, num & 0xFF);
//    ESP_LOGI(TAG,"Exit from RADIO_WriteFrequency");
}

// The math in this function needs adjusting for FXOSC != 32MHz
// This function needs to be called with the radio configured in FSK mode
static void RADIO_WriteFSKFrequencyDeviation(uint32_t frequencyDeviation)
{
    uint32_t num;

    // Fdev = (Fxosc * num) / 2^19
    // We take advantage of the fact that 32MHz = 15625Hz * 2^11
    // This simplifies our formula to Fdev = (15625Hz * num) / 2^8
    // Thus, num = (Fdev * 2^8) / 15625Hz

    num = frequencyDeviation;
    num <<= SHIFT8;     // Multiply by 2^8
    num /= 15625;       // divide by 15625

    // Now variable num holds the representation of the frequency deviation that
    // needs to be loaded into the radio chip
    RADIO_RegisterWrite(REG_FSK_FDEVMSB, (num >> SHIFT8) & 0xFF);
    RADIO_RegisterWrite(REG_FSK_FDEVLSB, num & 0xFF);
}

// The math in this function needs adjusting for FXOSC != 32MHz
// This function needs to be called with the radio configured in FSK mode.
// BitrateFrac is always 0
static void RADIO_WriteFSKBitRate(uint32_t bitRate)
{
    uint32_t num;

    num = 32000000;
    num /= bitRate;

    // Now variable num holds the representation of the bitrate that
    // needs to be loaded into the radio chip
    RADIO_RegisterWrite(REG_FSK_BITRATEMSB, (num >> SHIFT8) & 0xFF);
    RADIO_RegisterWrite(REG_FSK_BITRATELSB, num & 0xFF);
    RADIO_RegisterWrite(REG_FSK_BITRATEFRAC, 0x00);
}

int8_t RADIO_GetMaxPower(void)
{
    if (RadioConfiguration.paBoost == 0)
    {
        return 15;
    }
    else
    {
        return 20;
    }
}

static void RADIO_WritePower(int8_t power)
{
    uint8_t paDac;
    uint8_t ocp;

    if (RadioConfiguration.paBoost == 0)
    {
        // RFO pin used for RF output
        if (power < -3)
        {
            power = -3;
        }
        if (power > 15)
        {
            power = 15;
        }

        paDac = RADIO_RegisterRead(REG_PADAC);
        paDac &= ~(0x07);
        paDac |= 0x04;
        RADIO_RegisterWrite(REG_PADAC, paDac);

        if (power < 0)
        {
            // MaxPower = 2
            // Pout = 10.8 + MaxPower*0.6 - 15 + OutPower
            // Pout = -3 + OutPower
            power += 3;
            RADIO_RegisterWrite(REG_PACONFIG, 0x20 | power);
        }
        else
        {
            // MaxPower = 7
            // Pout = 10.8 + MaxPower*0.6 - 15 + OutPower
            // Pout = OutPower
            RADIO_RegisterWrite(REG_PACONFIG, 0x70 | power);
        }
    }
    else
    {
        // PA_BOOST pin used for RF output

        // Lower limit
        if (power < 2)
        {
            power = 2;
        }

        // Upper limit
        if (power >= 20)
        {
            power = 20;
        }
        else if (power > 17)
        {
            power = 17;
        }

        ocp = RADIO_RegisterRead(REG_OCP);
        paDac = RADIO_RegisterRead(REG_PADAC);
        paDac &= ~(0x07);
        if (power == 20)
        {
            paDac |= 0x07;
            power = 15;
            ocp &= ~(0x20);
        }
        else
        {
            paDac |= 0x04;
            power -= 2;
            ocp |= 0x20;
        }

        RADIO_RegisterWrite(REG_PADAC, paDac);
        RADIO_RegisterWrite(REG_PACONFIG, 0x80 | power);
        RADIO_RegisterWrite(REG_OCP, ocp);
    }
}

void RADIO_Init(uint8_t *radioBuffer, uint32_t frequency)
{
    uint8_t tmp8;
    uint32_t tmp32;
//    ESP_LOGI(TAG,"Enter in RADIO_Init cal.freq=%d",frequency);
    RadioConfiguration.frequency = frequency;
    ESP_LOGI(TAG,"Calibration F=%" PRIi32"\n",RadioConfiguration.frequency);
    set_s("DEVIATION",&RadioConfiguration.frequencyDeviation); // = 25000;
    set_s("FSK_BITRATE",&RadioConfiguration.bitRate); // = 50000;
    set_s("MODULATION",&tmp8);
    if(tmp8) RadioConfiguration.modulation = MODULATION_FSK;
    else RadioConfiguration.modulation = MODULATION_LORA;
    set_s("BW",&tmp8);
//    ESP_LOGI(TAG,"BW=%d",tmp8);
    switch(tmp8)
    {
        case 0:
            RadioConfiguration.bandWidth = BW_125KHZ;
            break;
        case 1:
            RadioConfiguration.bandWidth = BW_250KHZ;
            break;
        case 2:
            RadioConfiguration.bandWidth = BW_500KHZ;
            break;
        default:
            RadioConfiguration.bandWidth = BW_125KHZ;
            break;
    };
//    ESP_LOGI(TAG,"Bandwidth=%d",RadioConfiguration.bandWidth);
    set_s("POWER",&tmp8);
    RadioConfiguration.outputPower=txPowerRU864[tmp8];  // = 1;
    set_s("FEC",&RadioConfiguration.errorCodingRate); // = CR_4_5;
    set_s("HEADER_MODE",&RadioConfiguration.implicitHeaderMode); // = 0;
    set_s("PREAMBLE_LEN",&tmp32);
    RadioConfiguration.preambleLen=(uint16_t)tmp32; // = 8;
    set_s("SF",&spread_factor);
    RadioConfiguration.dataRate=spread_factor; // = SF_12;
    set_s("CRC",&RadioConfiguration.crcOn); // = 1;
    set_s("BOOST",&RadioConfiguration.paBoost); // = 0;
    set_s("IQ_INVERTED",&RadioConfiguration.iqInverted); // = 0;
    set_s("SYNCWORDLEN",&RadioConfiguration.syncWordLen); // = 3;
    set_s("SYNCWORD",&tmp32);
    RadioConfiguration.syncWord[0] = (tmp32>>24)&0xFF; //0xC1;
    RadioConfiguration.syncWord[1] = (tmp32>>16)&0xFF; //0x94;
    RadioConfiguration.syncWord[2] = (tmp32>>8)&0xFF; //0xC1;
    RadioConfiguration.syncWord[3] = tmp32&0xFF; //0;
    set_s("LORA_SYNCWORD",&RadioConfiguration.syncWordLoRa);
    RadioConfiguration.flags = 0;
    RadioConfiguration.dataBufferLen = 0;
    RadioConfiguration.dataBuffer = radioBuffer;
    RadioConfiguration.frequencyHopPeriod = 0;
    RadioConfiguration.watchdogTimerTimeout = WATCHDOG_DEFAULT_TIME;
    RadioConfiguration.rxWatchdogTimerTimeout = RX_WATCHDOG_DEFAULT_TIME;
    set_s("MODULATION",&tmp8);
    if(tmp8) RadioConfiguration.fskDataShaping = tmp8-1; //FSK_SHAPING_GAUSS_BT_0_5;
    set_s("FSK_BW",&RadioConfiguration.rxBw); // = FSKBW_50_0KHZ;
    set_s("FSK_AFC_BW",&RadioConfiguration.afcBw); // = FSKBW_83_3KHZ;
    RadioConfiguration.fhssNextFrequency = NULL;

    rectimer_value=86400000;
    rectimer=xTimerCreate("rectimer", rectimer_value / portTICK_PERIOD_MS,pdFALSE,NULL,NULL);
    xTimerStart(rectimer,0);
    delta=0;
    ticks=0;
    ticks_old=0;



    // Make sure we do not allocate multiple software timers just because the
    // radio's initialization function is called multiple times.
    if (0 == RadioConfiguration.initialized)
    {
        // This behaviour depends on the compiler's behaviour regarding
        // uninitialized variables. It should be configured to set them to 0.

        RadioConfiguration.timeOnAirTimerId = xTimerCreate("timeOnAirTimer", 86400000 / portTICK_PERIOD_MS, pdFALSE, NULL, NULL);
        RadioConfiguration.fskRxWindowTimerId = xTimerCreate("fskRxWindowTimer", 86400000 / portTICK_PERIOD_MS, pdFALSE, (void*) FSK_RX_WINDOW_TIMER, TimerCallback);
        RadioConfiguration.watchdogTimerId = xTimerCreate("watchdogTimer", 86400000 / portTICK_PERIOD_MS, pdFALSE, (void*) WATCHDOG_TIMER,TimerCallback);
        startTimerId=xTimerCreate("startTimer", START_TIMER_VALUE, pdFALSE, NULL, NULL);
        RadioConfiguration.initialized = 1;
    }
    else
    {
        xTimerStop(RadioConfiguration.timeOnAirTimerId,0);
        xTimerStop(RadioConfiguration.fskRxWindowTimerId,0);
        xTimerStop(RadioConfiguration.watchdogTimerId,0);
        xTimerStop(startTimerId,0);
    }

    RADIO_Reset();

    // Perform image and RSSI calibration. This also puts the radio in FSK mode.
    // In order to perform image and RSSI calibration, we need the radio in
    // FSK mode. To do this, we first put it in sleep mode.
//    ESP_LOGI(TAG,"Write mode");
    RADIO_WriteMode(MODE_STANDBY, MODULATION_FSK, 1);

    // Set frequency to do calibration at the configured frequency
//    ESP_LOGI(TAG,"Write freq");
    RADIO_WriteFrequency(RadioConfiguration.frequency);

//    ESP_LOGI(TAG,"Begin calibration");
    // Do not do autocalibration at runtime, start calibration now, Temp
    // threshold for monitoring 10 deg. C, Temperature monitoring enabled
    RADIO_RegisterWrite(REG_FSK_IMAGECAL, 0x42);

    // Wait for calibration to complete
    while ((RADIO_RegisterRead(REG_FSK_IMAGECAL) & 0x20) != 0)
        ;

    // High frequency LNA current adjustment, 150% LNA current (Boost on)
    RADIO_RegisterWrite(REG_LNA, 0x23);

    // Triggering event: PreambleDetect does AfcAutoOn, AgcAutoOn
    RADIO_RegisterWrite(REG_FSK_RXCONFIG, 0x1E);

    // Preamble detector on, 2 bytes trigger an interrupt, Chip errors tolerated
    // over the preamble size
    RADIO_RegisterWrite(REG_FSK_PREAMBLEDETECT, 0xAA);

    // Transmission starts as soon as there is a byte in the FIFO. FifoLevel
    // interrupt is generated whenever there are at least 16 bytes in FIFO.
    RADIO_RegisterWrite(REG_FSK_FIFOTHRESH, 0x8F);

    // Set FSK max payload length to 255 bytes
    RADIO_RegisterWrite(REG_FSK_PAYLOADLENGTH, 0xFF);

    // Packet mode
    RADIO_RegisterWrite(REG_FSK_PACKETCONFIG2, 1 << SHIFT6);

//    ESP_LOGI(TAG,"End of calibration");
    // Go to LoRa mode for this register to be set
    RADIO_WriteMode(MODE_SLEEP, MODULATION_LORA, 1);


    // Set LoRa max payload length
    RADIO_RegisterWrite(REG_LORA_PAYLOADMAXLENGTH, 0xFF);

    RadioConfiguration.regVersion = RADIO_RegisterRead(REG_VERSION);
//    ESP_LOGI(TAG,"Exit from RADIO_Init");
}

void RADIO_SetLoRaSyncWord(uint8_t syncWord)
{
    // Change LoRa syncword
    RadioConfiguration.syncWordLoRa = syncWord;
}

uint8_t RADIO_GetLoRaSyncWord(void)
{
    return RadioConfiguration.syncWordLoRa;
}

static void RADIO_WriteConfiguration(uint16_t symbolTimeout)
{
    uint32_t tempValue;
    uint8_t regValue;
    uint8_t i;

    // Load configuration from RadioConfiguration_t structure into radio
//    ESP_LOGI(TAG,"Enter in Write Configuration Modulation=%s ",MODULATION_LORA == RadioConfiguration.modulation ? "LORA " : "FSK ");
//    ESP_LOGI(TAG,"Write mode");
    RADIO_WriteMode(MODE_SLEEP, RadioConfiguration.modulation, 0);
//    ESP_LOGI(TAG,"Write freq");
    RADIO_WriteFrequency(RadioConfiguration.frequency);
    printf("F=%" PRIu32" ",RadioConfiguration.frequency);
//    ESP_LOGI(TAG,"Write power");
    RADIO_WritePower(RadioConfiguration.outputPower);
    printf("P=%d ",RadioConfiguration.outputPower);

    if (MODULATION_LORA == RadioConfiguration.modulation)
    {
//        ESP_LOGI(TAG,"Write syncword");
        RADIO_RegisterWrite(REG_LORA_SYNCWORD, RadioConfiguration.syncWordLoRa);

//        ESP_LOGI(TAG,"Write bandwith %d,fec %d,header %d",RadioConfiguration.bandWidth,RadioConfiguration.errorCodingRate,RadioConfiguration.implicitHeaderMode);
        RADIO_RegisterWrite(REG_LORA_MODEMCONFIG1,
                (RadioConfiguration.bandWidth << SHIFT4) |
                (RadioConfiguration.errorCodingRate << SHIFT1) |
                (RadioConfiguration.implicitHeaderMode & 0x01));

//        ESP_LOGI(TAG,"Write datarate,crc,symboltimeout");
        RADIO_RegisterWrite(REG_LORA_MODEMCONFIG2,
                (RadioConfiguration.dataRate << SHIFT4) |
                ((RadioConfiguration.crcOn & 0x01) << SHIFT2) |
                ((symbolTimeout & 0x0300) >> SHIFT8));
        printf("SF=%d CRC=%d ",RadioConfiguration.dataRate,RadioConfiguration.crcOn);

        // Handle frequency hopping, if necessary
        if (0 != RadioConfiguration.frequencyHopPeriod)
        {
            tempValue = RadioConfiguration.frequencyHopPeriod;
            // Multiply by BW/1000 (since period is in ms)
            switch (RadioConfiguration.bandWidth)
            {
                case BW_125KHZ:
                    tempValue *= 125;
                    break;
                case BW_250KHZ:
                    tempValue *= 250;
                    break;
                case BW_500KHZ:
                    tempValue *= 500;
                    break;
                default:
                    // Disable frequency hopping
                    tempValue = 0;
                    break;
            }
            // Divide by 2^SF
            tempValue >>= RadioConfiguration.dataRate;
        }
        else
        {
            tempValue = 0;
        }
//        ESP_LOGI(TAG,"Write hopperiod");
        RADIO_RegisterWrite(REG_LORA_HOPPERIOD, (uint8_t)tempValue);

//        ESP_LOGI(TAG,"Write symboltimeoutlsb");
        RADIO_RegisterWrite(REG_LORA_SYMBTIMEOUTLSB, (symbolTimeout & 0xFF));
        printf("Symbol_Timeout=%d ",symbolTimeout);

        // If the symbol time is > 16ms, LowDataRateOptimize needs to be set
        // This long symbol time only happens for SF12&BW125, SF12&BW250
        // and SF11&BW125 and the following if statement checks for these
        // conditions
        regValue = RADIO_RegisterRead(REG_LORA_MODEMCONFIG3);
        if (
            (
             (SF_12 == RadioConfiguration.dataRate) &&
             ((BW_125KHZ == RadioConfiguration.bandWidth) || (BW_250KHZ == RadioConfiguration.bandWidth))
            ) ||
            (
             (SF_11 == RadioConfiguration.dataRate) &&
             (BW_125KHZ == RadioConfiguration.bandWidth)
            )
           )
        {
            regValue |= 1 << SHIFT3;     // Set LowDataRateOptimize
        }
        else
        {
            regValue &= ~(1 << SHIFT3);    // Clear LowDataRateOptimize
        }
        regValue |= 1 << SHIFT2;         // LNA gain set by internal AGC loop
//        ESP_LOGI(TAG,"Write optimize values");
        RADIO_RegisterWrite(REG_LORA_MODEMCONFIG3, regValue);

        regValue = RADIO_RegisterRead(REG_LORA_DETECTOPTIMIZE);
        regValue &= ~(0x07);        // Clear DetectOptimize bits
        regValue |= 0x03;           // Set value for SF7 - SF12
        RADIO_RegisterWrite(REG_LORA_DETECTOPTIMIZE, regValue);

        // Also set DetectionThreshold value for SF7 - SF12
//        ESP_LOGI(TAG,"Write detectionthreshold");
        RADIO_RegisterWrite(REG_LORA_DETECTIONTHRESHOLD, 0x0A);

        // Errata settings to mitigate spurious reception of a LoRa Signal
//        ESP_LOGI(TAG,"begin Write values to mitigate spurious reception of a LoRa Signal");
        if (0x12 == RadioConfiguration.regVersion)
        {
            // Chip already is in sleep mode. For these BWs we don't need to
            // offset Frf
            if ( (BW_125KHZ == RadioConfiguration.bandWidth) ||
                 (BW_250KHZ == RadioConfiguration.bandWidth) )
            {
                regValue = RADIO_RegisterRead(0x31);
                regValue &= ~0x80;                                  // Clear bit 7
                RADIO_RegisterWrite(0x31, regValue);
                RADIO_RegisterWrite(0x2F, 0x40);
                RADIO_RegisterWrite(0x30, 0x00);
            }

            if (BW_500KHZ == RadioConfiguration.bandWidth)
            {
                regValue = RADIO_RegisterRead(0x31);
                regValue |= 0x80;                                   // Set bit 7
                RADIO_RegisterWrite(0x31, regValue);
            }
        }
//        ESP_LOGI(TAG,"end Write values to mitigate spurious reception of a LoRa Signal");

        regValue = RADIO_RegisterRead(REG_LORA_INVERTIQ);
//        regValue &= ~(1 << SHIFT6);                                        // Clear InvertIQ bit
//        if(mode==MODE_NETWORK_SERVER) regValue &= ~(1 << SHIFT0); else regValue |= (1 << SHIFT0);
//        if(mode==MODE_DEVICE) regValue |= (1 << SHIFT6); else regValue &= ~(1 << SHIFT6);    // Set InvertIQ bit if needed

//        ESP_LOGI(TAG,"Write iqinvert");
        if(RadioConfiguration.iqInverted & 0x01)
        {
            regValue &= ~(1 << SHIFT0);
            regValue |= (1 << SHIFT6);
        }
        else
        {
            regValue |= (1 << SHIFT0);
            regValue &= ~(1 << SHIFT6);
        }    // Set InvertIQ bit if needed
        RADIO_RegisterWrite(REG_LORA_INVERTIQ, regValue);
        printf("IQInv=%d",RadioConfiguration.iqInverted);

        regValue = REG_LORA_INVERTIQ2_VALUE_OFF & (~((RadioConfiguration.iqInverted & 0x01) << SHIFT2));
        RADIO_RegisterWrite(REG_LORA_INVERTIQ2, regValue);

//        ESP_LOGI(TAG,"Write preamblelen");
        RADIO_RegisterWrite(REG_LORA_PREAMBLEMSB, RadioConfiguration.preambleLen >> SHIFT8);
        RADIO_RegisterWrite(REG_LORA_PREAMBLELSB, RadioConfiguration.preambleLen & 0xFF);

//        ESP_LOGI(TAG,"Write fifoaddr");
        RADIO_RegisterWrite(REG_LORA_FIFOADDRPTR, 0x00);
        RADIO_RegisterWrite(REG_LORA_FIFOTXBASEADDR, 0x00);
        RADIO_RegisterWrite(REG_LORA_FIFORXBASEADDR, 0x00);

        // Errata sensitivity increase for 500kHz BW
        if (0x12 == RadioConfiguration.regVersion)
        {
//        	ESP_LOGI(TAG,"Write values to sensitivity increase for 500kHz BW");
            if ( (BW_500KHZ == RadioConfiguration.bandWidth) &&
                 (RadioConfiguration.frequency >= FREQ_862000KHZ) &&
                 (RadioConfiguration.frequency <= FREQ_1020000KHZ)
               )
            {
                RADIO_RegisterWrite(0x36, 0x02);
                RADIO_RegisterWrite(0x3a, 0x64);
            }
            else if ( (BW_500KHZ == RadioConfiguration.bandWidth) &&
                      (RadioConfiguration.frequency >= FREQ_410000KHZ) &&
                      (RadioConfiguration.frequency <= FREQ_525000KHZ)
                    )
            {
                RADIO_RegisterWrite(0x36, 0x02);
                RADIO_RegisterWrite(0x3a, 0x7F);
            }
            else
            {
                RADIO_RegisterWrite(0x36, 0x03);
            }

            // LoRa Inverted Polarity 500kHz fix (May 26, 2015 document)
//        	ESP_LOGI(TAG,"Write values to LoRa Inverted Polarity 500kHz fix (May 26, 2015 document)");
            if ((BW_500KHZ == RadioConfiguration.bandWidth) && (1 == RadioConfiguration.iqInverted))
            {
                RADIO_RegisterWrite(0x3A, 0x65);     // Freq to time drift
                RADIO_RegisterWrite(REG_LORA_INVERTIQ2, 25);       // Freq to time invert = 0d25
            }
            else
            {
                RADIO_RegisterWrite(0x3A, 0x65);     // Freq to time drift
                RADIO_RegisterWrite(REG_LORA_INVERTIQ2, 29);       // Freq to time invert = 0d29 (default)
            }
        }

        // Clear all interrupts (just in case)
//        ESP_LOGI(TAG,"Clear all interrupts");
        RADIO_RegisterWrite(REG_LORA_IRQFLAGS, 0xFF);
    }
    else
    {
        // FSK modulation
        RADIO_WriteFSKFrequencyDeviation(RadioConfiguration.frequencyDeviation);
        RADIO_WriteFSKBitRate(RadioConfiguration.bitRate);

        RADIO_RegisterWrite(REG_FSK_PREAMBLEMSB, RadioConfiguration.preambleLen >> SHIFT8);
        RADIO_RegisterWrite(REG_FSK_PREAMBLELSB, RadioConfiguration.preambleLen & 0xFF);

        // Configure PaRamp
        regValue = RADIO_RegisterRead(REG_PARAMP);
        regValue &= ~0x60;    // Clear shaping bits
        regValue |= RadioConfiguration.fskDataShaping << SHIFT5;
        RADIO_RegisterWrite(REG_PARAMP, regValue);

        // Variable length packets, whitening, keep FIFO when CRC fails
        // no address filtering, CCITT CRC and whitening
        regValue = 0xC8;
        if (RadioConfiguration.crcOn)
        {
            regValue |= 0x10;   // Enable CRC
        }
        RADIO_RegisterWrite(REG_FSK_PACKETCONFIG1, regValue);

        // Syncword value
        for (i = 0; i < RadioConfiguration.syncWordLen; i++)
        {
            // Take advantage of the fact that the SYNCVALUE registers are
            // placed at sequential addresses
            RADIO_RegisterWrite(REG_FSK_SYNCVALUE1 + i, RadioConfiguration.syncWord[i]);
        }

        // Enable sync word generation/detection if needed, Syncword size = syncWordLen + 1 bytes
        if (RadioConfiguration.syncWordLen != 0)
        {
            RADIO_RegisterWrite(REG_FSK_SYNCCONFIG, 0x10 | (RadioConfiguration.syncWordLen - 1));
        }
        else
        {
            RADIO_RegisterWrite(REG_FSK_SYNCCONFIG, 0x00);
        }

        // Clear all FSK interrupts (just in case)
        RADIO_RegisterWrite(REG_FSK_IRQFLAGS1, 0xFF);
        RADIO_RegisterWrite(REG_FSK_IRQFLAGS2, 0xFF);

    }
    printf("\n");
//    ESP_LOGI(TAG,"Exit from RADIO_WriteConfiguration");
}

RadioError_t RADIO_TransmitCW(void)
{
    if ((RadioConfiguration.flags & (RADIO_FLAG_TRANSMITTING | RADIO_FLAG_RECEIVING)) != 0)
    {
        return ERR_RADIO_BUSY;
    }

    RadioConfiguration.modulation = MODULATION_LORA;

    // Since we're interested in a transmission, rxWindowSize is irrelevant.
    // Setting it to 4 is a valid option.
    RADIO_WriteConfiguration(4);

    RadioConfiguration.flags |= RADIO_FLAG_TRANSMITTING;
    RadioConfiguration.flags &= ~RADIO_FLAG_TIMEOUT;

    RADIO_RegisterWrite(0x3D, 0xA1);
    RADIO_RegisterWrite(0x36, 0x01);
    RADIO_RegisterWrite(0x1E, 0x08);
    RADIO_RegisterWrite(0x01, 0x8B);

    RadioMode_t cwmode=MODE_CW;
	esp_event_post_to(mainLoop, LORA_EVENTS, LORA_CHANGE_MAC_STATE_EVENT, &cwmode, sizeof(cwmode), 0);
	return ERR_NONE;
}

RadioError_t RADIO_StopCW(void)
{

    RADIO_WriteMode(MODE_STANDBY, RadioConfiguration.modulation, 0);
    vTaskDelay(100/portTICK_PERIOD_MS);
    RADIO_WriteMode(MODE_SLEEP, RadioConfiguration.modulation, 0);
    vTaskDelay(100/portTICK_PERIOD_MS);
    return ERR_NONE;
}

RadioError_t RADIO_Transmit(uint8_t *buffer, uint8_t bufferLen)
{
    uint8_t regValue;

    if ((RadioConfiguration.flags & RADIO_FLAG_RXDATA) != 0)
    {
        return ERR_BUFFER_LOCKED;
    }

    if ((RadioConfiguration.flags & (RADIO_FLAG_TRANSMITTING | RADIO_FLAG_RECEIVING)) != 0)
    {
        return ERR_RADIO_BUSY;
    }

    if ((MODULATION_FSK == RadioConfiguration.modulation) && (bufferLen > 64))
    {
        return ERR_DATA_SIZE;
    }

    if(RADIO_GetPABoost()) V1_SetLow();
    else V1_SetHigh();

    xTimerStop(RadioConfiguration.timeOnAirTimerId,0);

    // Since we're interested in a transmission, rxWindowSize is irrelevant.
    // Setting it to 4 is a valid option.
    RADIO_WriteConfiguration(4);


    if (MODULATION_LORA == RadioConfiguration.modulation)
    {
        RADIO_RegisterWrite(REG_LORA_PAYLOADLENGTH, bufferLen);

        // Configure PaRamp
        regValue = RADIO_RegisterRead(REG_PARAMP);
        regValue &= ~0x0F;    // Clear lower 4 bits
        regValue |= 0x08;     // 50us PA Ramp-up time
        RADIO_RegisterWrite(REG_PARAMP, regValue);

        // DIO0 = 01 means TxDone in LoRa mode.
        // DIO2 = 00 means FHSSChangeChannel
        RADIO_RegisterWrite(REG_DIOMAPPING1, 0x40);
        RADIO_RegisterWrite(REG_DIOMAPPING2, 0x00);

        RADIO_WriteMode(MODE_STANDBY, RadioConfiguration.modulation, 1);
    }
    else
    {
        // DIO0 = 00 means PacketSent in FSK Tx mode
        RADIO_RegisterWrite(REG_DIOMAPPING1, 0x00);
        RADIO_RegisterWrite(REG_DIOMAPPING2, 0x00);
    }

    if (MODULATION_FSK == RadioConfiguration.modulation)
    {
        // FSK requires the length to be sent to the FIFO
        RADIO_RegisterWrite(REG_FIFO, bufferLen);
    }


    HALSPIWriteFIFO(buffer, bufferLen);

    RadioConfiguration.flags |= RADIO_FLAG_TRANSMITTING;
    RadioConfiguration.flags &= ~(RADIO_FLAG_TIMEOUT | RADIO_FLAG_RXERROR);

    // Non blocking switch. We don't really care when it starts transmitting.
    // If accurate timing of the time on air is required, the simplest way to
    // achieve it is to change this to a blocking mode switch.
    RADIO_WriteMode(MODE_TX, RadioConfiguration.modulation, 0);
    ESP_LOGI(TAG,"Transmit start ");

    // Set timeout to some very large value since the timer counts down.
    // Leaving the callback uninitialized it will assume the default value of
    // NULL which in turns means no callback.
    xTimerChangePeriod(RadioConfiguration.timeOnAirTimerId,TIME_ON_AIR_LOAD_VALUE/portTICK_PERIOD_MS,0);
    xTimerStart(RadioConfiguration.timeOnAirTimerId,0);

    if (0 != RadioConfiguration.watchdogTimerTimeout)
    {
    	xTimerChangePeriod(RadioConfiguration.watchdogTimerId, RadioConfiguration.watchdogTimerTimeout/portTICK_PERIOD_MS,0);
        xTimerStart(RadioConfiguration.watchdogTimerId,0);
    }
    return ERR_NONE;
}

// rxWindowSize parameter is in symbols for LoRa and ms for FSK
RadioError_t RADIO_ReceiveStart(uint16_t rxWindowSize)
{
//    ESP_LOGI(TAG,"Request to receive");
	if ((RadioConfiguration.flags & RADIO_FLAG_RXDATA) != 0)
    {
        return ERR_BUFFER_LOCKED;
    }

    if ((RadioConfiguration.flags & (RADIO_FLAG_TRANSMITTING | RADIO_FLAG_RECEIVING)) != 0)
    {
        return ERR_RADIO_BUSY;
    }

    if (0 == rxWindowSize)
    {
        RADIO_WriteConfiguration(4);
    }
    else
    {
        RADIO_WriteConfiguration(rxWindowSize);
    }

    V1_SetHigh();
    if (MODULATION_LORA == RadioConfiguration.modulation)
    {
        // All LoRa packets are received with explicit header, so this register
        // is not used. However, a value of 0 is not allowed.
        RADIO_RegisterWrite(REG_LORA_PAYLOADLENGTH, 0x01);

        // DIO0 = 00 means RxDone in LoRa mode
        // DIO1 = 00 means RxTimeout in LoRa mode
        // DIO2 = 00 means FHSSChangeChannel
        // Other DIOs are unused.
        RADIO_RegisterWrite(REG_DIOMAPPING1, 0x00);
        RADIO_RegisterWrite(REG_DIOMAPPING2, 0x00);
    }
    else
    {
        RADIO_RegisterWrite(REG_FSK_RXBW, RadioConfiguration.rxBw);
        RADIO_RegisterWrite(REG_FSK_AFCBW, RadioConfiguration.afcBw);

        // DIO0 = 00 means PayloadReady in FSK Rx mode
        RADIO_RegisterWrite(REG_DIOMAPPING1, 0x00);
        RADIO_RegisterWrite(REG_DIOMAPPING2, 0x00);
    }

    RadioConfiguration.flags |= RADIO_FLAG_RECEIVING;
    RadioConfiguration.flags &= ~(RADIO_FLAG_TIMEOUT | RADIO_FLAG_RXERROR);

    // Will use non blocking switches to RadioSetMode. We don't really care
    // when it starts receiving.
    if (0 == rxWindowSize)
    {
        RADIO_WriteMode(MODE_RXCONT, RadioConfiguration.modulation, 0);
    }
    else
    {
        if (MODULATION_LORA == RadioConfiguration.modulation)
        {
            RADIO_WriteMode(MODE_RXSINGLE, MODULATION_LORA, 0);
        }
        else
        {
            RADIO_WriteMode(MODE_RXCONT, MODULATION_FSK, 0);
            xTimerChangePeriod(RadioConfiguration.fskRxWindowTimerId, rxWindowSize/portTICK_PERIOD_MS, 0);
            xTimerStart(RadioConfiguration.fskRxWindowTimerId,0);
        }
    }
    ESP_LOGI(TAG,"Receiving start... symbolTimeout=%d",rxWindowSize);

    if (0 != RadioConfiguration.watchdogTimerTimeout)
    {
    	xTimerChangePeriod(RadioConfiguration.watchdogTimerId, RadioConfiguration.rxWatchdogTimerTimeout/portTICK_PERIOD_MS,0);
        xTimerStart(RadioConfiguration.watchdogTimerId,0);
    }
    return ERR_NONE;
}


void RADIO_ReceiveStop(void)
{
    if (RADIO_FLAG_RECEIVING == RadioConfiguration.flags)
    {
        RADIO_WriteMode(MODE_SLEEP, RadioConfiguration.modulation, 0);
        RadioConfiguration.flags &= ~RADIO_FLAG_RECEIVING;
        RadioConfiguration.flags &= ~RADIO_FLAG_RXDATA;
    }
}


static void RADIO_RxDone(void)
{
    uint8_t i, irqFlags;
    irqFlags = RADIO_RegisterRead(REG_LORA_IRQFLAGS);
    // Clear RxDone interrupt (also CRC error and ValidHeader interrupts, if
    // they exist)
    RADIO_RegisterWrite(REG_LORA_IRQFLAGS, (1<<SHIFT6) | (1<<SHIFT5) | (1<<SHIFT4));
    if (((1<<SHIFT6) | (1<<SHIFT4)) == (irqFlags & ((1<<SHIFT6) | (1<<SHIFT4))))
    {
        // Make sure the watchdog won't trigger MAC functions erroneously.
        xTimerStop(RadioConfiguration.watchdogTimerId,0);

        // Read CRC info from received packet header
        i = RADIO_RegisterRead(REG_LORA_HOPCHANNEL);

        uint8_t snr = RADIO_RegisterRead(REG_LORA_PKTSNRVALUE);
        if(snr & 0x80)
        {
        	RadioConfiguration.SNR=((~snr+1) & 0xFF )>>2;
        	RadioConfiguration.SNR=-RadioConfiguration.SNR;
        } else RadioConfiguration.SNR=(snr&0xFF)>>2;
        RadioConfiguration.RSSI=RADIO_RegisterRead(REG_LORA_PKTRSSIVALUE)-157;
        if(RadioConfiguration.SNR<0) RadioConfiguration.RSSI+=RadioConfiguration.SNR/4;

        if(xTimerIsTimerActive(rectimer)==pdTRUE)
        {
            ticks=rectimer_value/portTICK_PERIOD_MS-xTimerGetExpiryTime(rectimer);
            delta=(ticks-ticks_old)*portTICK_PERIOD_MS;
            ticks_old=ticks;
            printf(" Received! delta=%" PRIi32" snr=%" PRIi16 ,delta,RadioConfiguration.SNR);
        }
        if ((0 == RadioConfiguration.crcOn) || ((0 == (irqFlags & (1<<SHIFT5))) && (0 != (i & (1<<SHIFT6)))))
        {
            // ValidHeader and RxDone are set from the initial if condition.
            // We get here either if CRC doesn't need to be set (crcOn == 0) OR
            // if it is present in the header and it checked out.

            // Radio did not go to standby automatically. Will need to be set
            // later on.

            RadioConfiguration.dataBufferLen = RADIO_RegisterRead(REG_LORA_RXNBBYTES);
            RADIO_RegisterWrite(REG_LORA_FIFOADDRPTR, 0x00);

            HALSPIReadFIFO(RadioConfiguration.dataBuffer, RadioConfiguration.dataBufferLen);

            RadioConfiguration.flags |= RADIO_FLAG_RXDATA;
            printf(" CRC OK");
        }
        else
        {
            // CRC required and CRC error found.
            RadioConfiguration.flags |= RADIO_FLAG_RXERROR;
            printf(" CRC Error!");
        }
        printf("\n");
        RADIO_WriteMode(MODE_SLEEP, RadioConfiguration.modulation, 0);
		RadioConfiguration.flags &= ~RADIO_FLAG_RXDATA;
//		esp_event_post_to(mainLoop, LORA_EVENTS, LORA_RXDONE_EVENT, &RadioConfiguration.dataBufferLen, sizeof(uint8_t)+sizeof(uint8_t*), 0);
		sRadioConfiguration=&RadioConfiguration;
		esp_event_post_to(mainLoop, LORA_EVENTS, LORA_RXDONE_EVENT, &sRadioConfiguration, sizeof(RadioConfiguration_t**), 0);
        RadioConfiguration.flags &= ~RADIO_FLAG_RECEIVING;
//        LORAWAN_RxDone(RadioConfiguration.dataBuffer, RadioConfiguration.dataBufferLen);
        ESP_LOGI(TAG,"RADIO_RxDone dataBufferLen=%d",RadioConfiguration.dataBufferLen);
    }
    else
    {
    	currentMode=RADIO_RegisterRead(REG_OPMODE)&0x07;
		esp_event_post_to(mainLoop, LORA_EVENTS, LORA_CHANGE_MAC_STATE_EVENT, &currentMode, sizeof(currentMode), 0);
    }
}

static void RADIO_FSKPayloadReady(void)
{
    uint8_t irqFlags;

    irqFlags = RADIO_RegisterRead(REG_FSK_IRQFLAGS2);
    if ((1<<SHIFT2) == (irqFlags & (1<<SHIFT2)))
    {
        // Clearing of the PayloadReady (and CrcOk) interrupt is done when the
        // FIFO is empty

        // Make sure the watchdog won't trigger MAC functions erroneously.
        xTimerStop(RadioConfiguration.watchdogTimerId,0);
        xTimerStop(RadioConfiguration.fskRxWindowTimerId,0);

        HALSPIReadFSKFIFO(RadioConfiguration.dataBuffer, &RadioConfiguration.dataBufferLen);

        rssi_reg=RADIO_RegisterRead(REG_FSK_RSSIVALUE);
        rssi_off=RADIO_RegisterRead(REG_FSK_RSSICONFIG);
        RADIO_WriteMode(MODE_SLEEP, RadioConfiguration.modulation, 0);
        RadioConfiguration.flags &= ~RADIO_FLAG_RECEIVING;

        if (1 == RadioConfiguration.crcOn)
        {
            if ((1<<SHIFT1) == (irqFlags & (1<<SHIFT1)))
            {
                RadioConfiguration.flags |= RADIO_FLAG_RXDATA;
            }
            else
            {
                RadioConfiguration.flags &= ~RADIO_FLAG_RXDATA;
            }
        }
        else
        {
            RadioConfiguration.flags |= RADIO_FLAG_RXDATA;
        }

        if ((RadioConfiguration.flags & RADIO_FLAG_RXDATA) != 0)
        {
            esp_event_post_to(mainLoop, LORA_EVENTS, LORA_RXDONE_EVENT, &RadioConfiguration.dataBufferLen, sizeof(uint8_t)+sizeof(uint8_t*), 0);
            RadioConfiguration.flags &= ~RADIO_FLAG_RXDATA;
            //            LORAWAN_RxDone(RadioConfiguration.dataBuffer, RadioConfiguration.dataBufferLen);
        }
        else
        {
            esp_event_post_to(mainLoop, LORA_EVENTS, LORA_RXTIMEOUT_EVENT, NULL, 0,0);
//            LORAWAN_RxTimeout();
        }
    }
    else
    {
    	RadioMode_t currentMode=RADIO_RegisterRead(REG_OPMODE)&0x07;
		esp_event_post_to(mainLoop, LORA_EVENTS, LORA_CHANGE_MAC_STATE_EVENT, &currentMode, sizeof(currentMode), 0);
    }
}

static void RADIO_RxTimeout(void)
{
    // Make sure the watchdog won't trigger MAC functions erroneously.
    xTimerStop(RadioConfiguration.watchdogTimerId,0);
    RADIO_RegisterWrite(REG_LORA_IRQFLAGS, 1<<SHIFT7);
    // Radio went to STANDBY. Set sleep.
    RADIO_WriteMode(MODE_SLEEP, RadioConfiguration.modulation, 0);
    RadioConfiguration.flags &= ~RADIO_FLAG_RECEIVING;

    printf(" RXTimeout");
    esp_event_post_to(mainLoop, LORA_EVENTS, LORA_RXTIMEOUT_EVENT, NULL, 0, 0);
//            LORAWAN_RxTimeout();
}

static void RADIO_TxDone(void)
{
    uint32_t timeOnAir;
    // Make sure the watchdog won't trigger MAC functions erroneously.
    V1_SetHigh();
    xTimerStop(RadioConfiguration.watchdogTimerId,0);
    xTimerStop(RadioConfiguration.timeOnAirTimerId,0);
    RADIO_RegisterWrite(REG_LORA_IRQFLAGS, 1<<SHIFT3);
    RADIO_WriteMode(MODE_SLEEP, RadioConfiguration.modulation, 0);
    RadioConfiguration.flags &= ~RADIO_FLAG_TRANSMITTING;
    if ((RadioConfiguration.flags & RADIO_FLAG_TIMEOUT) == 0)
    {
        uint32_t exp=(xTimerGetExpiryTime(RadioConfiguration.timeOnAirTimerId) - xTaskGetTickCount())*portTICK_PERIOD_MS;
    	timeOnAir = TIME_ON_AIR_LOAD_VALUE - exp;
        xTimerStop(RadioConfiguration.timeOnAirTimerId,0);
        ESP_LOGI(TAG,"loaded %" PRIu32", exp %" PRIu32" timeOnAir %" PRIu32,TIME_ON_AIR_LOAD_VALUE,exp,timeOnAir);
        esp_event_post_to(mainLoop, LORA_EVENTS, LORA_TXDONE_EVENT, &timeOnAir, sizeof(timeOnAir), 0);
    };
}

static void RADIO_FSKPacketSent(void)
{
    uint8_t irqFlags;
    uint32_t timeOnAir;

    V1_SetHigh();
    irqFlags = RADIO_RegisterRead(REG_FSK_IRQFLAGS2);
    if ((1<<SHIFT3) == (irqFlags & (1<<SHIFT3)))
    {
        RADIO_WriteMode(MODE_SLEEP, RadioConfiguration.modulation, 0);
        RadioConfiguration.flags &= ~RADIO_FLAG_TRANSMITTING;
        // Make sure the watchdog won't trigger MAC functions erroneously.
        xTimerStop(RadioConfiguration.watchdogTimerId,0);
        // Clearing of the PacketSent interrupt is done on exiting Tx
        if ((RadioConfiguration.flags & RADIO_FLAG_TIMEOUT) == 0)
        {
            timeOnAir = TIME_ON_AIR_LOAD_VALUE - xTimerGetExpiryTime(RadioConfiguration.timeOnAirTimerId)*portTICK_PERIOD_MS;
            xTimerStop(RadioConfiguration.timeOnAirTimerId,0);
            esp_event_post_to(mainLoop, LORA_EVENTS, LORA_TXDONE_EVENT, &timeOnAir, sizeof(timeOnAir), 0);
    //        LORAWAN_TxDone((uint16_t)timeOnAir);
        }
    }
    else
    {
    	RadioMode_t currentMode=RADIO_RegisterRead(REG_OPMODE)&0x07;
		esp_event_post_to(mainLoop, LORA_EVENTS, LORA_CHANGE_MAC_STATE_EVENT, &currentMode, sizeof(currentMode), 0);
    }
}


uint8_t RADIO_GetStatus(void)
{
    return RadioConfiguration.flags;
}

RadioError_t RADIO_GetData(uint8_t *data, uint16_t *dataLen)
{
    if ((RadioConfiguration.flags & RADIO_FLAG_RXDATA) == 0)
    {
        return ERR_NO_DATA;
    }

    memcpy(data, RadioConfiguration.dataBuffer, RadioConfiguration.dataBufferLen);
    *dataLen = RadioConfiguration.dataBufferLen;

    return ERR_NONE;
}

void RADIO_ReleaseData(void)
{
    RadioConfiguration.flags &= ~RADIO_FLAG_RXDATA;
}

static void RADIO_UnhandledInterrupt(RadioModulation_t modulation)
{
    // Clear all IRQ flags to recover gracefully.
    // Since we already know the radio modulation settings we can go access
    // those registers directly.
    if (MODULATION_LORA == modulation)
    {
        RADIO_RegisterWrite(REG_LORA_IRQFLAGS, 0xFF);
    }
    else
    {
        // Although just some of the bits can be cleared, try to clear
        // everything
        RADIO_RegisterWrite(REG_FSK_IRQFLAGS1, 0xFF);
        RADIO_RegisterWrite(REG_FSK_IRQFLAGS2, 0xFF);
    }
}


static void RADIO_FHSSChangeChannel(void)
{
    uint8_t irqFlags;
    irqFlags = RADIO_RegisterRead(REG_LORA_IRQFLAGS);

    if (NULL != RadioConfiguration.frequencyHopPeriod)
    {
        if (NULL != RadioConfiguration.fhssNextFrequency)
        {
            uint32_t nextfreq=RadioConfiguration.fhssNextFrequency();
            RADIO_WriteFrequency(nextfreq);
        }
    }

    // Clear FHSSChangeChannel interrupt
    RADIO_RegisterWrite(REG_LORA_IRQFLAGS, 1<<SHIFT1);
}


void RADIO_DIO0(void)
{
    // Check radio configuration (modulation and DIO0 settings).
    uint8_t dioMapping;
    uint8_t opMode;
    xTimerStart(startTimerId,0);
    ESP_LOGI(TAG, "startTimer started");
    dioMapping = (RADIO_RegisterRead(REG_DIOMAPPING1) & 0xC0) >> SHIFT6;
    opMode = RADIO_RegisterRead(REG_OPMODE);

    if ((opMode & 0x80) != 0)
    {
        // LoRa modulation
        switch (dioMapping)
        {
            case 0x00:
            	RADIO_RxDone();
                break;
            case 0x01:
                xTimerStop(startTimerId,0);
                RADIO_TxDone();
                break;
            default:
                xTimerStop(startTimerId,0);
                RADIO_UnhandledInterrupt(MODULATION_LORA);
                break;
        }
    }
    else
    {
        // FSK modulation
        xTimerStop(startTimerId,0);
        switch (dioMapping)
        {
            case 0x00:
                // Check if the radio state is Tx or Rx
                opMode &= 0x07;
                if (MODE_TX == opMode)
                {
                    // PacketSent
                    RADIO_FSKPacketSent();
                }
                else if (MODE_RXCONT == opMode)
                {
                    // PayloadReady
                    RADIO_FSKPayloadReady();
                }
                else
                {
                    RADIO_UnhandledInterrupt(MODULATION_FSK);
                }
                break;
            default:
                RADIO_UnhandledInterrupt(MODULATION_FSK);
                break;
        }
    }
}


void RADIO_DIO1(void)
{
    // Check radio configuration (modulation and DIO1 settings).
    uint8_t dioMapping;
    dioMapping = (RADIO_RegisterRead(REG_DIOMAPPING1) & 0x30) >> SHIFT4;

    if ((RADIO_RegisterRead(REG_OPMODE) & 0x80) != 0)
    {
        // LoRa modulation
        switch (dioMapping)
        {
            case 0x00:
                RADIO_RxTimeout();
                break;
            case 0x01:
                RADIO_FHSSChangeChannel();
                break;
            default:
                RADIO_UnhandledInterrupt(MODULATION_LORA);
                break;
        }
    }
    else
    {
        // FSK modulation
        switch (dioMapping)
        {
            case 0x00:
                // RADIO_FSKFifoLevel();
                break;
            default:
                // RADIO_UnhandledInterrupt(MODULATION_FSK);
                break;
        }
    }
}

void RADIO_DIO2(void)
{
    // Check radio configuration (modulation and DIO2 settings).
    uint8_t dioMapping;
    dioMapping = (RADIO_RegisterRead(REG_DIOMAPPING1) & 0x0C) >> SHIFT2;

    if ((RADIO_RegisterRead(REG_OPMODE) & 0x80) != 0)
    {
        // LoRa modulation
        switch (dioMapping)
        {
            // Intentional fall-through
            case 0x00:
            case 0x01:
            case 0x02:
                RADIO_FHSSChangeChannel();
                break;
            default:
                RADIO_UnhandledInterrupt(MODULATION_LORA);
                break;
        }
    }
    else
    {
        // FSK modulation
    }
}

void RADIO_DIO3(void)
{
    // Check radio configuration (modulation and DIO3 settings).
    uint8_t dioMapping;
    dioMapping = RADIO_RegisterRead(REG_DIOMAPPING1) & 0x03;

    if ((RADIO_RegisterRead(REG_OPMODE) & 0x80) != 0)
    {
        // LoRa modulation
        switch (dioMapping)
        {
            default:
                RADIO_UnhandledInterrupt(MODULATION_LORA);
                break;
        }
    }
    else
    {
        // FSK modulation
        // RADIO_UnhandledInterrupt(MODULATION_FSK);
    }
}

void RADIO_DIO4(void)
{
    // Check radio configuration (modulation and DIO4 settings).
    uint8_t dioMapping;
    dioMapping = (RADIO_RegisterRead(REG_DIOMAPPING2) & 0xC0) >> SHIFT6;

    if ((RADIO_RegisterRead(REG_OPMODE) & 0x80) != 0)
    {
        // LoRa modulation
        switch (dioMapping)
        {
            default:
                RADIO_UnhandledInterrupt(MODULATION_LORA);
                break;
        }
    }
    else
    {
        // FSK modulation
        // RADIO_UnhandledInterrupt(MODULATION_FSK);
    }
}

void RADIO_DIO5(void)
{
    // Check radio configuration (modulation and DIO5 settings).
    uint8_t dioMapping;
    dioMapping = (RADIO_RegisterRead(REG_DIOMAPPING2) & 0x30) >> SHIFT4;

    if ((RADIO_RegisterRead(REG_OPMODE) & 0x80) != 0)
    {
        // LoRa modulation
        switch (dioMapping)
        {
            default:
                RADIO_UnhandledInterrupt(MODULATION_LORA);
                break;
        }
    }
    else
    {
        // FSK modulation
        // RADIO_UnhandledInterrupt(MODULATION_FSK);
    }
}

uint16_t RADIO_ReadRandom(void)
{
    uint8_t i;
    uint16_t retVal;
    retVal = 0;
    // Mask all interrupts, do many measurements of RSSI
    RADIO_WriteMode(MODE_SLEEP, MODULATION_LORA, 1);
    RADIO_RegisterWrite(REG_LORA_IRQFLAGSMASK, 0xFF);
    RADIO_WriteMode(MODE_RXCONT, MODULATION_LORA, 1);
    for (i = 0; i < 16; i++)
    {
        vTaskDelay(1);
        retVal <<= SHIFT1;
        retVal |= RADIO_RegisterRead(REG_LORA_RSSIWIDEBAND) & 0x01;
    }

    // Return radio to sleep
    RADIO_WriteMode(MODE_SLEEP, MODULATION_LORA, 1);
    // Clear interrupts in case any have been generated
    RADIO_RegisterWrite(REG_LORA_IRQFLAGS, 0xFF);
    // Unmask all interrupts
    RADIO_RegisterWrite(REG_LORA_IRQFLAGSMASK, 0x00);
    return retVal;
}

void RADIO_RxFSKTimeout(void)
{
    uint8_t irqFlags;
    irqFlags = RADIO_RegisterRead(REG_FSK_IRQFLAGS1);
    if (0 == (irqFlags & (1<<SHIFT0)))
    {
        // SyncAddressMatch is not set. Set radio to sleep.
        RADIO_WriteMode(MODE_SLEEP, RadioConfiguration.modulation, 0);
        // Make sure the watchdog won't trigger MAC functions erroneously.
        xTimerStop(RadioConfiguration.watchdogTimerId,0);
        RadioConfiguration.flags &= ~RADIO_FLAG_RECEIVING;
        esp_event_post_to(mainLoop, LORA_EVENTS, LORA_RXTIMEOUT_EVENT, NULL, 0, 0);
    //            LORAWAN_RxTimeout();
    }
}


void RADIO_WatchdogTimeout(void)
{

    V1_SetHigh();
    RADIO_WriteMode(MODE_STANDBY, RadioConfiguration.modulation, 1);
    RADIO_WriteMode(MODE_SLEEP, RadioConfiguration.modulation, 0);
    RadioConfiguration.flags |= RADIO_FLAG_TIMEOUT;
    if ((RadioConfiguration.flags & RADIO_FLAG_RECEIVING) != 0)
    {
        RadioConfiguration.flags &= ~RADIO_FLAG_RECEIVING;
        ESP_LOGI(TAG,"RX Watchdog Timeout");
        esp_event_post_to(mainLoop, LORA_EVENTS, LORA_RXTIMEOUT_EVENT, NULL, 0, 0);
    //            LORAWAN_RxTimeout();
    }
    else if ((RadioConfiguration.flags & RADIO_FLAG_TRANSMITTING) != 0)
    {
        RadioConfiguration.flags &= ~RADIO_FLAG_TRANSMITTING;
        // This will tell the MAC that the channel has been used a lot. Since
        // this time-out occured we cannot know for sure that the radio did not
        // transmit this whole time, so this is the safest way to go (block the
        // channel for a really long time from now on).
        esp_event_post_to(mainLoop, LORA_EVENTS, LORA_TXDONE_EVENT, &RadioConfiguration.watchdogTimerTimeout, sizeof(RadioConfiguration.watchdogTimerTimeout), 0);
//        LORAWAN_TxDone(RadioConfiguration.watchdogTimerTimeout);
    }
}

int8_t RADIO_GetPacketSnr(void)
{
    return RadioConfiguration.SNR;
}

void RADIO_SetSpreadingFactor(RadioDataRate_t spreadingFactor)
{
    RadioConfiguration.dataRate = spreadingFactor;
}

RadioDataRate_t RADIO_GetSpreadingFactor(void)
{
    return RadioConfiguration.dataRate;
}

RadioError_t RADIO_SetChannelFrequency(uint32_t frequency)
{
    if ( ((frequency >= FREQ_137000KHZ) && (frequency <= FREQ_175000KHZ))||
         ((frequency >= FREQ_410000KHZ) && (frequency <= FREQ_525000KHZ)) ||
         ((frequency >= FREQ_862000KHZ) && (frequency <= FREQ_1020000KHZ)) )
    {
        RadioConfiguration.frequency = frequency;
        return ERR_NONE;
    }
    else
    {
        return ERR_OUT_OF_RANGE;
    }
}

uint32_t RADIO_GetChannelFrequency(void)
{
    return RadioConfiguration.frequency;
}

void RADIO_SetOutputPower(int8_t power)
{
    RadioConfiguration.outputPower = power;
}

uint8_t RADIO_GetOutputPower(void)
{
    return RadioConfiguration.outputPower;
}

void RADIO_SetCRC(uint8_t crc)
{
    RadioConfiguration.crcOn = crc;
}

uint8_t RADIO_GetCRC(void)
{
    return RadioConfiguration.crcOn;
}

void RADIO_SetIQInverted(uint8_t iqInverted)
{
    RadioConfiguration.iqInverted = iqInverted;
}

uint8_t RADIO_GetIQInverted(void)
{
    return RadioConfiguration.iqInverted;
}


void RADIO_SetBandwidth(RadioLoRaBandWidth_t bandwidth)
{
    RadioConfiguration.bandWidth = bandwidth;
}

RadioLoRaBandWidth_t RADIO_GetBandwidth(void)
{
    return RadioConfiguration.bandWidth;
}

void RADIO_SetPABoost(uint8_t paBoost)
{
    RadioConfiguration.paBoost = paBoost;
}

uint8_t RADIO_GetPABoost(void)
{
    return RadioConfiguration.paBoost;
}

void RADIO_SetModulation(RadioModulation_t modulation)
{
    RadioConfiguration.modulation = modulation;
}

RadioModulation_t RADIO_GetModulation(void)
{
    return RadioConfiguration.modulation;
}

void RADIO_SetChannelFrequencyDeviation(uint32_t frequencyDeviation)
{
    RadioConfiguration.frequencyDeviation = frequencyDeviation;
}

uint32_t RADIO_GetChannelFrequencyDeviation(void)
{
    return RadioConfiguration.frequencyDeviation;
}

void RADIO_SetPreambleLen(uint16_t preambleLen)
{
    RadioConfiguration.preambleLen = preambleLen;
}

uint16_t RADIO_GetPreambleLen(void)
{
    return RadioConfiguration.preambleLen;
}

void RADIO_SetFHSSChangeCallback(uint32_t (*fhssNextFrequency)(void))
{
    RadioConfiguration.fhssNextFrequency = fhssNextFrequency;
}

void RADIO_SetFrequencyHopPeriod(uint16_t frequencyHopPeriod)
{
    RadioConfiguration.frequencyHopPeriod = frequencyHopPeriod;
}

uint8_t RADIO_GetFrequencyHopPeriod(void)
{
    return RadioConfiguration.frequencyHopPeriod;
}


void RADIO_SetErrorCodingRate(RadioErrorCodingRate_t errorCodingRate)
{
    RadioConfiguration.errorCodingRate = errorCodingRate;
}

RadioErrorCodingRate_t RADIO_GetErrorCodingRate(void)
{
    return RadioConfiguration.errorCodingRate;
}


void RADIO_SetWatchdogTimeout(uint32_t timeout)
{
    RadioConfiguration.watchdogTimerTimeout = timeout;
}

void RADIO_SetRxWatchdogTimeout(uint32_t timeout)
{
    RadioConfiguration.rxWatchdogTimerTimeout = timeout;
}

uint32_t RADIO_GetWatchdogTimeout(void)
{
    return RadioConfiguration.watchdogTimerTimeout;
}


void RADIO_SetFSKBitRate(uint32_t bitRate)
{
    RadioConfiguration.bitRate = bitRate;
}

uint32_t RADIO_GetFSKBitRate(void)
{
    return RadioConfiguration.bitRate;
}


void RADIO_SetFSKDataShaping(RadioFSKShaping_t fskDataShaping)
{
    RadioConfiguration.fskDataShaping = fskDataShaping;
}

RadioFSKShaping_t RADIO_GetFSKDataShaping(void)
{
    return RadioConfiguration.fskDataShaping;
}

void RADIO_SetFSKRxBw(RadioFSKBandWidth_t bw)
{
    RadioConfiguration.rxBw = bw;
}

RadioFSKBandWidth_t RADIO_GetFSKRxBw(void)
{
    return RadioConfiguration.rxBw;
}

void RADIO_SetFSKAFCBw(RadioFSKBandWidth_t bw)
{
    RadioConfiguration.afcBw = bw;
}

RadioFSKBandWidth_t RADIO_GetFSKAFCBw(void)
{
    return RadioConfiguration.afcBw;
}

void RADIO_SetFSKSyncWord(uint8_t syncWordLen, uint8_t* syncWord)
{
    if (syncWordLen > 8)
    {
        syncWordLen = 8;
    }
    memcpy(RadioConfiguration.syncWord, syncWord, syncWordLen);
    RadioConfiguration.syncWordLen = syncWordLen;
}

uint8_t RADIO_GetFSKSyncWord(uint8_t* syncWord)
{
    memcpy(syncWord, RadioConfiguration.syncWord, RadioConfiguration.syncWordLen);
    return RadioConfiguration.syncWordLen;
}


/**
 End of File
*/
