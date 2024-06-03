/** @file main.c
 *
 * @brief LoRaWAN time sync example
 *
 * @par
 * COPYRIGHT NOTICE: (c) 2022 Irnas. All rights reserved.
 */

#include <smtc_modem_api.h>

#include <smtc_app.h>
#include <smtc_modem_api_str.h>

/* include hal and ralf so that initialization can be done */
#include <ralf_lr11xx.h>
#include <smtc_modem_hal.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main);

/* ---------------- Function declarations ---------------- */

static void on_modem_reset(uint16_t reset_count);
static void on_modem_network_joined(void);
static void on_modem_time_sync(smtc_modem_event_time_status_t status);

/* ---------------- SAMPLE CONFIGURATION ---------------- */

/**
 * @brief Application time synchronization service - see @ref smtc_modem_time_sync_service_t
 */
#define APP_MODEM_TIME_SYNC_SERVICE SMTC_MODEM_TIME_MAC_SYNC

/**
 * @brief Application time synchronization interval in second
 */
#define APP_MODEM_TIME_SYNC_INTERVAL_IN_S 900

/* ---------------- LoRaWAN Configurations ---------------- */

/* Stack id value (multistack modem is not yet available, so use 0) */
#define STACK_ID 0

/*LoRaWAN configuration */
static struct smtc_app_lorawan_cfg lorawan_cfg = {
        .use_chip_eui_as_dev_eui = false,
        .dev_eui = {0xb5, 0x09, 0xb4, 0x53, 0xfa, 0x12, 0x58, 0x79},
        .join_eui = {0xe0, 0x96, 0xb0, 0x1d, 0xa5, 0xbf, 0x49, 0x4a},
        .app_key =  {0xda, 0x87, 0xec, 0x9c, 0x3e, 0xf7, 0x43, 0x52, 0x49, 0x2d, 0x67, 0x08,
  0x2f, 0x2e, 0xa2, 0xe6},
	.class = SMTC_MODEM_CLASS_A,
	.region = SMTC_MODEM_REGION_EU_868,
};

static struct smtc_app_event_callbacks event_callbacks = {
	.reset = on_modem_reset,
	.joined = on_modem_network_joined,
	.time_updated_alc_sync = on_modem_time_sync,

};

/* lr11xx radio context and its use in the ralf layer */
const ralf_t modem_radio = RALF_LR11XX_INSTANTIATE(DEVICE_DT_GET(DT_NODELABEL(lr11xx)));

/* ---------------- Main ---------------- */

K_SEM_DEFINE(main_sleep_sem, 0, 1);

void main(void)
{
	/* configure LoRaWAN modem */

	/* Init the modem and use provided event callbacks.
	 * Please note that the reset callback will be called immediately after the first call to
	 * smtc_modem_run_engine because of reset detection.
	 */
	smtc_app_init(&modem_radio, &event_callbacks, NULL);
	smtc_app_display_versions();

	/* Enter main loop:
	 * The fist call to smtc_modem_run_engine will trigger the reset callback.
	 */

	while (1) {
		/* Execute modem runtime, this function must be called again in sleep_time_ms
		 * milliseconds or sooner. */
		uint32_t sleep_time_ms = smtc_modem_run_engine();

		LOG_INF("Sleeping for %d ms", sleep_time_ms);
		k_sleep(K_MSEC(sleep_time_ms));
	}
}

/**
 * @brief Reset event callback
 *
 * @param [in] reset_count reset counter from the modem
 */
static void on_modem_reset(uint16_t reset_count)
{
	/* configure lorawan parameters after reset and start join sequence */
	smtc_app_configure_lorawan_params(STACK_ID, &lorawan_cfg);
	smtc_modem_join_network(STACK_ID);
}

/**
 * @brief Network Joined event callback
 */
static void on_modem_network_joined(void)
{
	uint32_t time;
	smtc_app_get_utc_time(&time);
	LOG_INF("Joined. Modem time is: %d", time);

	/* configure time sync service */
	smtc_modem_return_code_t ret =
		smtc_modem_time_set_sync_interval_s(APP_MODEM_TIME_SYNC_INTERVAL_IN_S);
	if (ret != SMTC_MODEM_RC_OK) {
		LOG_ERR("Unable to initialize time sync, err: %d", ret);
		/* NOTE: if ret == SMTC_MODEM_RC_FAIL, you might have forgotten to set
		 * CONFIG_LORA_BASICS_MODEM_FILE_UPLOAD
		 */
	}
	smtc_modem_time_start_sync_service(STACK_ID, APP_MODEM_TIME_SYNC_SERVICE);
}

void on_modem_time_sync(smtc_modem_event_time_status_t status)
{
	LOG_INF("Time sync event: %s (%d)", smtc_modem_event_time_status_to_str(status), status);
	if (status == SMTC_MODEM_EVENT_TIME_VALID) {
		uint32_t time;
		smtc_app_get_utc_time(&time);
		LOG_INF("Valid time sync event. Modem time is: %d", time);
	}
}
