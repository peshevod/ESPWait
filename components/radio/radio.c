#include "esp_log.h"
#include "S2LP_Config.h"
#include "radio.h"
#include "nvs_flash.h"
#include "cmd_nvs.h"
#include "shell.h"

static const char* TAG = "Radio";
extern uint8_t bypass_ldo;
extern volatile uint8_t irqf;
extern RTC_SLOW_ATTR uint8_t cw, pn9;

SRadioInit xRadioInit = {
   BASE_FREQUENCY,
   MODULATION_SELECT,
   DATARATE,
   FREQ_DEVIATION,
   BANDWIDTH
};
 
 
PktBasicInit xBasicInit={
   PREAMBLE_LENGTH,
   SYNC_LENGTH,
   SYNC_WORD,
   VARIABLE_LENGTH,
   EXTENDED_LENGTH_FIELD,
   CRC_MODE,
   EN_ADDRESS,
   EN_FEC,
   EN_WHITENING
};
 
 
SGpioInit xGpioIRQ={
   S2LP_GPIO_0,
   S2LP_GPIO_MODE_DIGITAL_OUTPUT_LP,
   S2LP_GPIO_DIG_OUT_IRQ
};
 
SGpioInit xGpioTxState={
   S2LP_GPIO_1,
   S2LP_GPIO_MODE_DIGITAL_OUTPUT_HP,
   S2LP_GPIO_DIG_OUT_TX_STATE
};

SAfcInit xSAfcInit={
    S_ENABLE,  /*!< AFC enable */
//    S_DISABLE,  /*!< AFC disable */
    S_ENABLE,  /*!< Freeze the parameters on SYNC word detect */
    AFC_MODE_LOOP_CLOSED_ON_2ND_CONV_STAGE,/*!< Specify the AFC mode. @ref SAfcMode */
    0x80,            /*!< Fast period duration */
    2,            /*!< Gain used during fast loop */
    5           /*!< Gain used during slow loop */
};

void radio_init(uint8_t packetlen)
{
    uint8_t tmp,tmp1;
    uint32_t tmp32;
    
    set_s("F", &xRadioInit.lFrequencyBase);
    set_s("M", &xRadioInit.xModulationSelect);
    set_s("R", &xRadioInit.lDatarate);
    set_s("W", &xRadioInit.lBandwidth);
    set_s("D", &xRadioInit.lFreqDev);

    ESP_LOGI(TAG,"nvs FMRWD done");

    S2LPRadioSetXtalFrequency(XTAL_FREQ);

    if(xRadioInit.xModulationSelect==0x70)
    {
    	cw=1;
        ESP_LOGI(TAG,"Constant Wave selected");
    }

    set_s("E", &tmp32);
    xBasicInit.xPreambleLength=(uint16_t)tmp32;
    if(xBasicInit.xPreambleLength==0)
    {
    	pn9=1;
        ESP_LOGI(TAG,"PN9 source selected");
    }

    /* S2LP IRQ config */
    S2LPGpioInit(&xGpioIRQ);
    ESP_LOGI(TAG,"GpioIRQ done");
    
    /* TX state output on gpio1 */
    S2LPGpioInit(&xGpioTxState);
    ESP_LOGI(TAG,"TX state on gpio1 done");

    /* S2LP Radio config */
    S2LPRadioInit(&xRadioInit);
    set_s("S", &tmp32);
    S2LPRadioSetChannelSpace(tmp32);
    set_s("C", &tmp);
    S2LPRadioSetChannel(tmp);
    
    if(cw)
    {
        tmp=0x77;
        S2LPSpiWriteRegisters(MOD2_ADDR, 1, &tmp);
    }
    if(pn9)
    {
        S2LPSpiReadRegisters(PCKTCTRL1_ADDR, 1, &tmp);
        tmp|=TXSOURCE_REGMASK;
        S2LPSpiWriteRegisters(PCKTCTRL1_ADDR, 1, &tmp);
    }

    if(!cw && !pn9)
    {
    	/* S2LP Packet config */
        set_s("G", &tmp);
    	xBasicInit.xCrcMode=tmp;
    	S2LPPktBasicInit(&xBasicInit);
   
    	/* S2LP IRQs enable */
    	S2LPGpioIrqDeInit(NULL);
   
    	/* payload length config */
    	S2LPPktBasicSetPayloadLength(packetlen);
   
    	/* IRQ registers blanking */
    	S2LPGpioIrqClearStatus();
    }
    
    S2LPSpiReadRegisters(0x78, 1, &tmp);
    set_s("L", &tmp1);
    if(tmp1) tmp&=0xFB;else tmp|=0x04;
    S2LPSpiWriteRegisters(0x78, 1, &tmp);
   

}

void radio_tx_init(uint8_t packetlen)
{
    uint8_t tmp;
    int32_t power;
    radio_init(packetlen);
    set_s("P", &power);
   
    /* S2LP Radio set power */
    if(power>14)
    {
        /* Maximum POWER*/
        S2LPSpiReadRegisters(PM_CONF0_ADDR, 1, &tmp);
        tmp|=SET_SMPS_LVL_REGMASK;
        S2LPSpiWriteRegisters(PM_CONF0_ADDR, 1, &tmp);
        S2LPRadioSetMaxPALevel(S_ENABLE);
    }
    else
    {
        S2LPRadioSetMaxPALevel(S_DISABLE);
        S2LPRadioSetPALeveldBm(7,power);
        S2LPRadioSetPALeveldBm(6,(power+30)*7/8-30);
        S2LPRadioSetPALeveldBm(5,(power+30)*6/8-30);
        S2LPRadioSetPALeveldBm(4,(power+30)*5/8-30);
        S2LPRadioSetPALeveldBm(3,(power+30)*4/8-30);
        S2LPRadioSetPALeveldBm(2,(power+30)*3/8-30);
        S2LPRadioSetPALeveldBm(1,(power+30)*2/8-30);
        S2LPRadioSetPALeveldBm(0,(power+30)*1/8-30);
        S2LPRadioSetManualRampingMode(S_ENABLE);
        S2LPRadioSetPALevelMaxIndex(7);
        S2LPSpiReadRegisters(PA_POWER0_ADDR, 1, &tmp);
        tmp&=0xE7;
        tmp|=0x18;
        g_xStatus = S2LPSpiWriteRegisters(PA_POWER0_ADDR, 1, &tmp);
    }
   
    if(!cw && !pn9)
    {
    	/* S2LP IRQs enable */
    	S2LPGpioIrqConfig(IRQ_TX_DATA_SENT , S_ENABLE);
    
    	/* IRQ registers blanking */
    	S2LPGpioIrqClearStatus();
    };
};

void radio_rx_init(uint8_t packetlen)
{
    uint8_t tmp;
    radio_init(packetlen);
    
    S2LPSpiReadRegisters(PM_CONF0_ADDR, 1, &tmp);
    tmp&=~SET_SMPS_LVL_REGMASK;
    tmp|=0x30;  //1.4v
    S2LPSpiWriteRegisters(PM_CONF0_ADDR, 1, &tmp);
    
    SRssiInit xSRssiInit = {
      .cRssiFlt = 14,
      .xRssiMode = RSSI_STATIC_MODE,
      .cRssiThreshdBm = -85,
    };
   
    set_s("V", &xSRssiInit.cRssiThreshdBm);

    S2LPRadioRssiInit(&xSRssiInit);
    S2LPRadioAfcInit(&xSAfcInit);
    
    /* use SLEEP_A mode (default) */
    S2LPTimerSleepB(S_DISABLE);
    S2LPTimerSetRxTimerUs(30000);
    S2LpSetTimerFastRxTermTimerUs(FAST_RX_TIMER);
    S2LPTimerSetWakeUpTimerUs(WAKEUP_TIMER);
    S2LpTimerFastRxTermTimer(S_ENABLE);    
    S2LPTimerLdcrMode(S_ENABLE);


    S2LPGpioIrqConfig(IRQ_RX_DATA_READY,S_ENABLE);

    /* IRQ registers blanking */
    S2LPGpioIrqClearStatus();
}


