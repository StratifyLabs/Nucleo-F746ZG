/*

Copyright 2011-2019 Stratify Labs, Inc

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

	 http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

 */


#include <sys/lock.h>
#include <fcntl.h>
#include <errno.h>
#include <mcu/mcu.h>
#include <mcu/debug.h>
#include <mcu/periph.h>
#include <device/sys.h>
#include <device/uartfifo.h>
#include <device/usbfifo.h>
#include <device/fifo.h>
#include <device/cfifo.h>
#include <device/sys.h>
#include <sos/link.h>
#include <sos/fs/sysfs.h>
#include <sos/fs/appfs.h>
#include <sos/fs/devfs.h>
#include <sos/fs/sffs.h>
#include <sos/sos.h>

#include "config.h"
#include "sl_config.h"
#include "link_config.h"
#include "netif_lan8742a.h"

//--------------------------------------------Device Filesystem-------------------------------------------------


/*
 * Defaults configurations
 *
 * This provides the default pin assignments and settings for peripherals. If
 * the defaults are not provided, the application must specify them.
 *
 * Defaults should be added for peripherals that are dedicated for use on the
 * board. For example, if a UART has an external connection and label on the
 * board, the BSP should provide the default configuration.
 *
 *
 *
 */

//USART6
char uart5_fifo_buffer[64];
fifo_config_t uart5_fifo_config = {
	.size = 64,
	.buffer = uart5_fifo_buffer
};

#define UART_DMA_FLAGS STM32_DMA_FLAG_IS_MEMORY_SINGLE | \
	STM32_DMA_FLAG_IS_PERIPH_SINGLE | \
	STM32_DMA_FLAG_IS_PERIPH_BYTE | \
	STM32_DMA_FLAG_IS_MEMORY_BYTE

stm32_uart_dma_config_t uart5_dma_config = {
	.uart_config = {
		.attr = {
			.o_flags = UART_FLAG_SET_LINE_CODING_DEFAULT,
			.freq = 9600,
			.width = 8,
			.pin_assignment = {
				.tx = { 6, 14 }, //PF14
				.rx = { 6, 9 }, //PF9
				.rts = { 0xff, 0xff },
				.cts = { 0xff, 0xff }
			}
		},
		.fifo_config = &uart5_fifo_config
	},
	.dma_config = {
		.rx = {
			.dma_number = STM32_DMA2,
			.stream_number = 2,
			.channel_number = 5,
			.priority = STM32_DMA_PRIORITY_LOW,
			.o_flags = STM32_DMA_FLAG_IS_PERIPH_TO_MEMORY | UART_DMA_FLAGS | STM32_DMA_FLAG_IS_CIRCULAR
		},
		.tx = {
			.dma_number = STM32_DMA2,
			.stream_number = 6,
			.channel_number = 5,
			.priority = STM32_DMA_PRIORITY_LOW,
			.o_flags = STM32_DMA_FLAG_IS_MEMORY_TO_PERIPH | UART_DMA_FLAGS | STM32_DMA_FLAG_IS_NORMAL
		}
	}
};


FIFO_DECLARE_CONFIG_STATE(stdio_in, SOS_BOARD_STDIO_BUFFER_SIZE);
FIFO_DECLARE_CONFIG_STATE(stdio_out, SOS_BOARD_STDIO_BUFFER_SIZE);
CFIFO_DECLARE_CONFIG_STATE_4(board_fifo, 256);

u8 eth_tx_buffer[STM32_ETH_DMA_BUFFER_SIZE];
u8 eth_rx_buffer[STM32_ETH_DMA_BUFFER_SIZE];

const stm32_eth_dma_config_t eth0_config = {
	.eth_config = {
		.attr = {
			.o_flags = ETH_FLAG_SET_INTERFACE |
			ETH_FLAG_START |
			ETH_FLAG_IS_RMII |
			ETH_FLAG_IS_AUTONEGOTIATION_ENABLED,
			.pin_assignment = {
				.rmii = {
					.clk = {0, 1}, //PA1
					.txd0 = {6, 13}, //PG13
					.txd1 = {1, 13}, //PB13
					.tx_en = {6, 11}, //PG11
					.rxd0 = {2, 4}, //PC4
					.rxd1 = {2, 5}, //PC5
					.crs_dv = {0, 7}, //PA7
					.rx_er = {0xff, 0xff}, //??
					.unused[0] = {0xff, 0xff},
					.unused[1] = {0xff, 0xff},
					.unused[2] = {0xff, 0xff},
					.unused[3] = {0xff, 0xff},
					.unused[4] = {0xff, 0xff},
					.unused[5] = {0xff, 0xff},
					.unused[6] = {0xff, 0xff},
					.unused[7] = {0xff, 0xff}
				},
				.mdio = {0, 2}, //PA2
				.mdc = {2, 1} //PC1
			},
			.mac_address[0] = 0x00,
			.mac_address[1] = 0x80,
			.mac_address[2] = 0xe1,
			.mac_address[3] = 0x00,
			.mac_address[4] = 0x00,
			.mac_address[5] = 0x00,
			.mac_address[6] = 0x00, //unused
			.mac_address[7] = 0x00, //unused
			.phy_address = 0 //address of PHY CHIP
		}
	},
	.tx_buffer = eth_tx_buffer,
	.rx_buffer = eth_rx_buffer
};

const rtc_config_t rtc_config = {
	.attr = {
		.o_flags = RTC_FLAG_ENABLE,
	}
};

//this is custom for the STM32 SPI DMA -- it is compatible with the drive_sdspi implementation
typedef struct {
	mcu_pin_t cs;
	u32 spi_config_size;
	stm32_spi_dma_config_t spi;
} drive_sdspi_stm32_dma_config_t;

//#define SOS_BOARD_SPI1_SCK_PIN {0, 5} //PA5
//#define SOS_BOARD_SPI1_MISO_PIN {0, 6} //PA6
//#define SOS_BOARD_SPI1_MOSI_PIN {1, 5} //PB5
//#define SOS_BOARD_SPI1_CS_PIN {0xff, 0xff} //Not used

#define SOS_BOARD_SPI1_RX_DMA STM32_DMA2
#define SOS_BOARD_SPI1_RX_DMA_STREAM 0
#define SOS_BOARD_SPI1_RX_DMA_CHANNEL 3
#define SOS_BOARD_SPI1_TX_DMA STM32_DMA2
#define SOS_BOARD_SPI1_TX_DMA_STREAM 3
#define SOS_BOARD_SPI1_TX_DMA_CHANNEL 3

#define SPI_DMA_FLAGS STM32_DMA_FLAG_IS_NORMAL | \
	STM32_DMA_FLAG_IS_MEMORY_SINGLE | \
	STM32_DMA_FLAG_IS_PERIPH_SINGLE | \
	STM32_DMA_FLAG_IS_PERIPH_BYTE | \
	STM32_DMA_FLAG_IS_MEMORY_BYTE

/* This is the list of devices that will show up in the /dev folder.
 */
const devfs_device_t devfs_list[] = {
	//System devices
	DEVFS_DEVICE("trace", ffifo, 0, &board_trace_config, &board_trace_state, 0666, SOS_USER_ROOT, S_IFCHR),
	DEVFS_DEVICE("fifo", cfifo, 0, &board_fifo_config, &board_fifo_state, 0666, SOS_USER_ROOT, S_IFCHR),
	DEVFS_DEVICE("stdio-out", fifo, 0, &stdio_out_config, &stdio_out_state, 0666, SOS_USER_ROOT, S_IFCHR),
	DEVFS_DEVICE("stdio-in", fifo, 0, &stdio_in_config, &stdio_in_state, 0666, SOS_USER_ROOT, S_IFCHR),
	DEVFS_DEVICE("link-phy-usb", usbfifo, 0, &sos_link_transport_usb_fifo_cfg, &sos_link_transport_usb_fifo_state, 0666, SOS_USER_ROOT, S_IFCHR),
	DEVFS_DEVICE("sys", sys, 0, 0, 0, 0666, SOS_USER_ROOT, S_IFCHR),

	//MCU peripherals
	DEVFS_DEVICE("core", mcu_core, 0, 0, 0, 0666, SOS_USER_ROOT, S_IFCHR),
	DEVFS_DEVICE("core0", mcu_core, 0, 0, 0, 0666, SOS_USER_ROOT, S_IFCHR),

	DEVFS_DEVICE("eth0", netif_lan8742a, 0, &eth0_config, 0, 0666, SOS_USER_ROOT, S_IFCHR),

	DEVFS_DEVICE("pio0", mcu_pio, 0, 0, 0, 0666, SOS_USER_ROOT, S_IFCHR), //GPIOA
	DEVFS_DEVICE("pio1", mcu_pio, 1, 0, 0, 0666, SOS_USER_ROOT, S_IFCHR), //GPIOB
	DEVFS_DEVICE("pio2", mcu_pio, 2, 0, 0, 0666, SOS_USER_ROOT, S_IFCHR), //GPIOC
	DEVFS_DEVICE("pio3", mcu_pio, 3, 0, 0, 0666, SOS_USER_ROOT, S_IFCHR), //GPIOD
	DEVFS_DEVICE("pio4", mcu_pio, 4, 0, 0, 0666, SOS_USER_ROOT, S_IFCHR), //GPIOE
	DEVFS_DEVICE("pio5", mcu_pio, 5, 0, 0, 0666, SOS_USER_ROOT, S_IFCHR), //GPIOF
	DEVFS_DEVICE("pio6", mcu_pio, 6, 0, 0, 0666, SOS_USER_ROOT, S_IFCHR), //GPIOG
	DEVFS_DEVICE("pio7", mcu_pio, 7, 0, 0, 0666, SOS_USER_ROOT, S_IFCHR), //GPIOH

	DEVFS_DEVICE("rtc", mcu_rtc, 0, &rtc_config, 0, 0666, SOS_USER_ROOT, S_IFCHR),

	DEVFS_DEVICE("spi0", mcu_spi, 0, 0, 0, 0666, SOS_USER_ROOT, S_IFCHR),
	DEVFS_DEVICE("spi1", mcu_spi, 1, 0, 0, 0666, SOS_USER_ROOT, S_IFCHR),
	DEVFS_DEVICE("spi2", mcu_spi, 2, 0, 0, 0666, SOS_USER_ROOT, S_IFCHR),
	DEVFS_DEVICE("spi3", mcu_spi, 3, 0, 0, 0666, SOS_USER_ROOT, S_IFCHR),

	DEVFS_DEVICE("tmr0", mcu_tmr, 0, 0, 0, 0666, SOS_USER_ROOT, S_IFCHR), //TIM1
	DEVFS_DEVICE("tmr1", mcu_tmr, 1, 0, 0, 0666, SOS_USER_ROOT, S_IFCHR), //TIM2
	DEVFS_DEVICE("tmr2", mcu_tmr, 2, 0, 0, 0666, SOS_USER_ROOT, S_IFCHR),
	DEVFS_DEVICE("tmr3", mcu_tmr, 3, 0, 0, 0666, SOS_USER_ROOT, S_IFCHR),
	DEVFS_DEVICE("tmr4", mcu_tmr, 4, 0, 0, 0666, SOS_USER_ROOT, S_IFCHR),
	DEVFS_DEVICE("tmr5", mcu_tmr, 5, 0, 0, 0666, SOS_USER_ROOT, S_IFCHR),
	DEVFS_DEVICE("tmr6", mcu_tmr, 6, 0, 0, 0666, SOS_USER_ROOT, S_IFCHR),
	DEVFS_DEVICE("tmr7", mcu_tmr, 7, 0, 0, 0666, SOS_USER_ROOT, S_IFCHR), //TIM8
	//Does this chip have more timers?

	DEVFS_DEVICE("uart5", mcu_uart_dma, 5, &uart5_dma_config, 0, 0666, SOS_USER_ROOT, S_IFCHR),
	DEVFS_TERMINATOR
};



