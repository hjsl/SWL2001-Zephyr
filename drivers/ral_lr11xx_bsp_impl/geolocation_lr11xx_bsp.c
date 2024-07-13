#include <lr11xx_system_types.h>
#include <lr11xx_gnss_types.h>
#include <zephyr/device.h>
#include <lr11xx_hal_context.h>

void geolocation_bsp_gnss_prescan_actions( void )
{
    /* Switch LNA on. */
    const struct device *lr11xx_dev = DEVICE_DT_GET(DT_NODELABEL(lr11xx));
	const struct lr11xx_hal_context_cfg_t *lr11xx_cfg = lr11xx_dev->config;

    gpio_pin_set_dt(&lr11xx_cfg->lna_en, 1);
}

void geolocation_bsp_gnss_postscan_actions( void )
{
    /* Switch LNA off. */
    const struct device *lr11xx_dev = DEVICE_DT_GET(DT_NODELABEL(lr11xx));
	const struct lr11xx_hal_context_cfg_t *lr11xx_cfg = lr11xx_dev->config;

    gpio_pin_set_dt(&lr11xx_cfg->lna_en, 0);
}

void geolocation_bsp_wifi_prescan_actions( void ) {}

void geolocation_bsp_wifi_postscan_actions( void ) {}

lr11xx_system_lfclk_cfg_t geolocation_bsp_get_lr11xx_lf_clock_cfg( void )
{
    const struct device *lr11xx_dev = DEVICE_DT_GET(DT_NODELABEL(lr11xx));
	const struct lr11xx_hal_context_cfg_t *lr11xx_cfg = lr11xx_dev->config;

    return lr11xx_cfg->lf_clck_cfg.lf_clk_cfg;
}

void geolocation_bsp_get_lr11xx_reg_mode( const void* context, lr11xx_system_reg_mode_t* reg_mode )
{
    const struct device *lr11xx_dev = (const struct device *)context;
	const struct lr11xx_hal_context_cfg_t *lr11xx_cfg = lr11xx_dev->config;

    *reg_mode = lr11xx_cfg->reg_mode;
}

void geolocation_bsp_gnss_get_consumption(
    lr11xx_gnss_instantaneous_power_consumption_ua_t* instantaneous_power_consumption_ua )
{
    /* These values are for an EVK board in DC-DC mode with Xtal 32kHz and a TCXO 32MHz. */
    instantaneous_power_consumption_ua->board_voltage_mv = 3300;
    instantaneous_power_consumption_ua->init_ua = 3150;
    instantaneous_power_consumption_ua->phase1_gps_capture_ua = 11900;
    instantaneous_power_consumption_ua->phase1_gps_process_ua = 3340;
    instantaneous_power_consumption_ua->multiscan_gps_capture_ua = 10700;
    instantaneous_power_consumption_ua->multiscan_gps_process_ua = 4180;
    instantaneous_power_consumption_ua->phase1_beidou_capture_ua = 13500;
    instantaneous_power_consumption_ua->phase1_beidou_process_ua = 3190;
    instantaneous_power_consumption_ua->multiscan_beidou_capture_ua = 12600;
    instantaneous_power_consumption_ua->multiscan_beidou_process_ua = 3430;
    instantaneous_power_consumption_ua->sleep_32k_ua = 1210;
    instantaneous_power_consumption_ua->demod_sleep_32m_ua = 2530;
}

