/*
 * nrf24l01.h
 *
 *  Created on: 24.11.2018
 *      Author: Akeman
 */

#ifndef WLMODULE_WLMODULE_H_
#define WLMODULE_WLMODULE_H_

#define WLMODULE nrf24l01
#define WLMODULE_CHANNEL 5
#define WLMODULE_SPI_BASE 0x4000B000

#define READ_REGISTER 0
#define WRITE_REGISTER 0b00100000
#define REGISTER_MASK 0x1F
#define READ_RX_PAYLOAD 0b1100001
#define WRITE_TX_PAYLOAD 0b10100000
#define FLUSH_TX 0b11100001
#define FLUSH_RX 0b11100010
#define REUSE_TX_PL 0b11100011
#define ACTIVATE 0b01010000
#define READ_RX_PL_WID 0b01100000
#define WRITE_ACK_PAYLOAD 0b10101000
#define WRITE_TX_PAYLOAD_NOACK 0b1011000
#define NOP 0b11111111

// Register address map
#define RF_CH 0x05

void wl_module_set_channel(uint8_t channel);
void wlModuleTaskFnx(void);

#endif /* WLMODULE_WLMODULE_H_ */
