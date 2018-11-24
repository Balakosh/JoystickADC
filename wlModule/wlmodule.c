/*
 * nrf24l01.c
 *
 *  Created on: 24.11.2018
 *      Author: Akeman
 */

#include <stdint.h>
#include <stdbool.h>

#include <ti/sysbios/knl/Task.h>
#include <driverlib/ssi.h>
#include <ti/drivers/GPIO.h>

#include <EK_TM4C1294XL.h>
#include "wlmodule.h"

void initWlModule(void)
{
    GPIO_write(NRF24L01_CE, GPIO_CFG_OUT_LOW);
    wl_module_set_channel(WLMODULE_CHANNEL);
}

void wl_module_config_register(uint8_t reg, uint8_t value)
{
    SSIDataPut(WLMODULE_SPI_BASE, WRITE_REGISTER | (REGISTER_MASK & reg));
    SSIDataPut(WLMODULE_SPI_BASE, value);
}

void wl_module_set_channel(uint8_t channel)
{
    wl_module_config_register(RF_CH, channel);
}

uint32_t wl_module_read_channel(void)
{
    uint32_t buffer;

    SSIDataPut(WLMODULE_SPI_BASE, READ_REGISTER | (REGISTER_MASK & RF_CH));
    SSIDataGet(WLMODULE_SPI_BASE, &buffer);

    return buffer;
}

void wlModuleIRQHandler(void)
{

}

void wlModuleTaskFnx(void)
{
    Task_sleep(1000);

    initWlModule();

    Task_sleep(500);

    uint32_t readChannel = wl_module_read_channel();

    while (1)
    {
        Task_sleep(1);
    }
}
