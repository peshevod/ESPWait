#include <stdio.h>
#include <lorawan_defs.h>
#include "power.h"
#include "sx1276_radio_driver.h"

// Tx power possibilities by ism band
const int8_t txPower868[] = {20, 14, 11, 8, 5, 2};
const int8_t txPowerRU864[] = {20, 14, 12, 10, 8, 6, 4, 2, 0};
const int8_t txPower433[] = {10, 7, 4, 1, -2, -5};
extern LoRa_t loRa;

static LorawanError_t ValidateTxPower (uint8_t txPowerNew)
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

static void UpdateTxPower (uint8_t txPowerNew)
{
    loRa.txPower = txPowerNew;
}

LorawanError_t LORAX_SetTxPower (uint8_t txPowerNew)
{
   LorawanError_t result = OK;
   int8_t txPower;

    if (ValidateTxPower (txPowerNew) == LORA_OK)
    {
        UpdateTxPower (txPowerNew);
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
    }
    else
    {
        result = INVALID_PARAMETER;
    }
    return result;
}

uint8_t LORAX_GetTxPower (void)
{
    return loRa.txPower;
}

uint8_t LORAX_GetTxPowerDbm (void)
{
    return loRa.txPower;
}



