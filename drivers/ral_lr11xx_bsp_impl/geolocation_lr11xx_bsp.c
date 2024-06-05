#include <lr11xx_system_types.h>
#include <lr11xx_gnss_types.h>

void geolocation_bsp_gnss_prescan_actions( void ) {}

void geolocation_bsp_gnss_postscan_actions( void ) {}

void geolocation_bsp_wifi_prescan_actions( void ) {}

void geolocation_bsp_wifi_postscan_actions( void ) {}

lr11xx_system_lfclk_cfg_t geolocation_bsp_get_lr11xx_lf_clock_cfg( void ) {}

void geolocation_bsp_get_lr11xx_reg_mode( const void* context, lr11xx_system_reg_mode_t* reg_mode ) {}

void geolocation_bsp_gnss_get_consumption(
    lr11xx_gnss_instantaneous_power_consumption_ua_t* instantaneous_power_consumption_ua )
{
}

