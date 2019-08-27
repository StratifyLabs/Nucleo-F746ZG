
#include <sos/dev/netif.h>
#include "lwip_config.h"

#define NETWORK_INTERFACE_COUNT 1

extern const sysfs_t sysfs_list[];

struct netif network_interface_list[NETWORK_INTERFACE_COUNT];
lwip_api_state_t lwip_state;

#define MAX_PACKET_SIZE 2048

u8 lwip_packet_buffer[MAX_PACKET_SIZE];
const lwip_api_config_t lwip_config = {
	.device_config = {
		.devfs = &sysfs_list[1],
		.name = "eth0",
		.state = (sysfs_shared_state_t*)&lwip_state
	},
	.hw_addr[0] = 0,
	.hw_addr[1] = 0x80,
	.hw_addr[2] = 0xE1,
	.hw_addr[3] = 0x00,
	.hw_addr[4] = 0x00,
	.hw_addr[5] = 0x00,
	.mtu = 1500,
	.host_name = "Nucleo-F746ZG",
	.network_interface_list = network_interface_list,
	.network_interface_count = NETWORK_INTERFACE_COUNT,
	.max_packet_size = MAX_PACKET_SIZE,
	.packet_buffer = lwip_packet_buffer
};

LWIP_DECLARE_SOCKET_API(lwip, &lwip_config, &lwip_state);
