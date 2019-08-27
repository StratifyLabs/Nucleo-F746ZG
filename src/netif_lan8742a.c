
#include <mcu/debug.h>

#include "netif_lan8742a.h"

int netif_lan8742a_open(const devfs_handle_t * handle){
    return mcu_eth_open(handle);
}

int netif_lan8742a_ioctl(const devfs_handle_t * handle, int request, void * ctl){
    int result;
    netif_attr_t * netif_attr = ctl;
    netif_info_t * netif_info = ctl;
	 mcu_channel_t eth_register;
	 eth_attr_t attr;
	 u32 o_flags;
    switch(request){
    case I_NETIF_GETVERSION: return NETIF_VERSION;
    case I_NETIF_SETATTR:
        o_flags = netif_attr->o_flags;
		  if(o_flags & NETIF_FLAG_INIT ){
            //initialize the interface
            result = mcu_eth_setattr(handle, 0);
            if( result < 0 ){
					 mcu_debug_log_error(MCU_DEBUG_USER0, "Failed to initialize ethernet (%d, %d)", SYSFS_GET_RETURN(result), SYSFS_GET_RETURN_ERRNO(result));
                return result;
            }
            return 0;
        }

		  //restart after having been stopped
		  if( o_flags & NETIF_FLAG_SET_LINK_UP ){
			  eth_register.loc = 0; //status regsiter
			  if( mcu_eth_getregister(handle, &eth_register) < 0 ){ return SYSFS_SET_RETURN(EIO); }
			  eth_register.value &= ~1<<11; //Set power down mode
			  if( mcu_eth_setregister(handle, &eth_register) < 0 ){ return SYSFS_SET_RETURN(EIO); }

			  attr.o_flags = ETH_FLAG_STOP;
			  if( mcu_eth_setattr(handle, &attr) < 0 ){ return SYSFS_SET_RETURN(EIO); }
		  }

        if( o_flags & NETIF_FLAG_IS_LINK_UP ){
            eth_register.loc = 1; //status regsiter
            if( mcu_eth_getregister(handle, &eth_register) < 0 ){ return SYSFS_SET_RETURN(EIO); }

            mcu_debug_log_info(MCU_DEBUG_USER2, "Status register 0x%lX", eth_register.value);
            return (eth_register.value & (1<<2)) != 0;
        }

        if( o_flags & NETIF_FLAG_SET_LINK_DOWN ){
            mcu_channel_t eth_register;
            eth_register.loc = 0; //status regsiter
            if( mcu_eth_getregister(handle, &eth_register) < 0 ){ return SYSFS_SET_RETURN(EIO); }
				eth_register.value |= ~1<<11; //Set power down mode
            if( mcu_eth_setregister(handle, &eth_register) < 0 ){ return SYSFS_SET_RETURN(EIO); }

            attr.o_flags = ETH_FLAG_STOP;
            if( mcu_eth_setattr(handle, &attr) < 0 ){ return SYSFS_SET_RETURN(EIO); }
        }


        break;


    case I_NETIF_GETINFO:

        netif_info->o_flags = NETIF_FLAG_INIT |
                NETIF_FLAG_DEINIT |
                NETIF_FLAG_IS_LINK_UP |
                NETIF_FLAG_SET_LINK_DOWN |
                NETIF_FLAG_SET_LINK_UP;

        netif_info->o_events = MCU_EVENT_FLAG_DATA_READY | MCU_EVENT_FLAG_WRITE_COMPLETE;

        break;
    }



    return mcu_eth_ioctl(handle, request, ctl);

}

int netif_lan8742a_read(const devfs_handle_t * handle, devfs_async_t * async){
    return mcu_eth_read(handle, async);
}

int netif_lan8742a_write(const devfs_handle_t * handle, devfs_async_t * async){
    return mcu_eth_write(handle, async);
}

int netif_lan8742a_close(const devfs_handle_t * handle){
    return mcu_eth_close(handle);
}
