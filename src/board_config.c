/*

Copyright 2011-2018 Stratify Labs, Inc

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

#include <string.h>
#include <fcntl.h>
#include <sos/sos.h>
#include <mcu/debug.h>
#include <cortexm/task.h>
#include <sos/link/types.h>

#include "lwip_config.h"
#include "board_config.h"

#define MANAGE_APPLICATION 1

extern void SystemClock_Config();

#define TRACE_COUNT 8
#define TRACE_FRAME_SIZE sizeof(link_trace_event_t)
#define TRACE_BUFFER_SIZE (sizeof(link_trace_event_t)*TRACE_COUNT)
static char trace_buffer[TRACE_FRAME_SIZE*TRACE_COUNT];
const ffifo_config_t board_trace_config = {
	.frame_count = TRACE_COUNT,
	.frame_size = sizeof(link_trace_event_t),
	.buffer = trace_buffer
};
ffifo_state_t board_trace_state;


void board_trace_event(void * event){
	link_trace_event_t * trace_event = event;
	devfs_async_t async;
	const devfs_device_t * trace_dev = &(devfs_list[0]);


	//write the event to the fifo
	memset(&async, 0, sizeof(devfs_async_t));
	async.tid = task_get_current();
	async.buf = event;
	async.nbyte = trace_event->header.size;
	async.flags = O_RDWR;
	trace_dev->driver.write(&(trace_dev->handle), &async);

	if( trace_event->header.id == LINK_NOTIFY_ID_POSIX_TRACE_EVENT ){

		//this will reset the device if the application crashes
		if( trace_event->posix_trace_event.posix_event_id == LINK_POSIX_TRACE_FATAL ){
			//mcu_core_reset(0,0);
		}
	}

}

void start_rtc(){
	int fd = open("/dev/rtc", O_RDWR);
	int result;
	if( fd < 0 ){
		mcu_debug_log_warning(MCU_DEBUG_USER0, "failed to open rtc");
		return;
	}

	if( (result = ioctl(fd, I_RTC_SETATTR)) < 0 ){
		mcu_debug_log_warning(MCU_DEBUG_USER0, "failed to set rtc attributes (%d, %d)", result, errno);
		return;
	}

	close(fd);
}

void board_event_handler(int event, void * args){
	switch(event){
		case MCU_BOARD_CONFIG_EVENT_ROOT_TASK_INIT:
			//no debugging allowed here
			break;

		case MCU_BOARD_CONFIG_EVENT_FATAL:
			mcu_debug_log_error(MCU_DEBUG_SYS, "Fatal Error %s\n", (const char*)args);
			break;

		case MCU_BOARD_CONFIG_EVENT_ROOT_FATAL:
			//start the bootloader on a fatal event
			if( args != 0 ){
				mcu_debug_log_error(MCU_DEBUG_SYS, "Fatal Error %s\n", (const char*)args);
			} else {
				mcu_debug_log_error(MCU_DEBUG_SYS, "Fatal Error unknown\n");
			}

			mcu_core_invokebootloader(0, 0);
			break;

		case MCU_BOARD_CONFIG_EVENT_ROOT_INITIALIZE_CLOCK:
			SystemClock_Config();
			break;

		case MCU_BOARD_CONFIG_EVENT_START_INIT:
			break;

		case MCU_BOARD_CONFIG_EVENT_START_LINK:
			mcu_debug_log_info(MCU_DEBUG_USER0, "Start Link");
			sos_led_startup();

			start_rtc();

			//start LWIP
			mcu_debug_log_info(MCU_DEBUG_USER0, "Start LWIP");

			usleep(500*1000);
			lwip_api.startup(&lwip_api);


			break;

		case MCU_BOARD_CONFIG_EVENT_START_FILESYSTEM:
			break;
	}
}
