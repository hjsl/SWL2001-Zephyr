/** @file main.c
 *
 * @brief LoRaWAN class b sample
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
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main);

/* ---------------- Function declarations ---------------- */

static void on_modem_reset(uint16_t reset_count);
static void on_modem_network_joined(void);
static void on_modem_alarm(void);
static void on_modem_down_data(int8_t rssi, int8_t snr,
			       smtc_modem_event_downdata_window_t rx_window, uint8_t port,
			       const uint8_t *payload, uint8_t size);
static void on_modem_time_sync(smtc_modem_event_time_status_t status);
static void on_class_b_ping_slot_info(smtc_modem_event_class_b_ping_slot_status_t status);
static void on_class_b_status(smtc_modem_event_class_b_status_t status);

/* ---------------- SAMPLE CONFIGURATION ---------------- */

/**
 * @brief LoRaWAN class B ping slot periodicity
 */
#define LORAWAN_CLASS_B_PING_SLOT SMTC_MODEM_CLASS_B_PINGSLOT_16_S

/**
 * @brief Application time synchronization service - see @ref smtc_modem_time_sync_service_t
 */
#define APP_MODEM_TIME_SYNC_SERVICE SMTC_MODEM_TIME_MAC_SYNC

/**
 * @brief Application time synchronization interval in second
 */
#define APP_MODEM_TIME_SYNC_INTERVAL_IN_S 900

/**
 * @brief Interval between 2 time synchronization once time is acquired
 */
#define TIME_SYNC_APP_INTERVAL_S (86400)

/**
 * @brief Invalid delay for time sync
 */
#define TIME_SYNC_APP_INVALID_DELAY_S (3 * TIME_SYNC_APP_INTERVAL_S)

/* ---------------- LoRaWAN ---------------- */

/* Stack id value (multistack modem is not yet available, so use 0) */
#define STACK_ID 0

/*LoRaWAN configuration */
static struct smtc_app_lorawan_cfg lorawan_cfg = {
        .use_chip_eui_as_dev_eui = false,
        .dev_eui = {0xb5, 0x09, 0xb4, 0x53, 0xfa, 0x12, 0x58, 0x79},
        .join_eui = {0xe0, 0x96, 0xb0, 0x1d, 0xa5, 0xbf, 0x49, 0x4a},
        .app_key =  {0xda, 0x87, 0xec, 0x9c, 0x3e, 0xf7, 0x43, 0x52, 0x49, 0x2d, 0x67, 0x08,
  0x2f, 0x2e, 0xa2, 0xe6},
	.class = SMTC_MODEM_CLASS_A, /* Start as class A - class B is enabled after joining */
	.region = SMTC_MODEM_REGION_EU_868,
};

static struct smtc_app_event_callbacks event_callbacks = {
	.joined = on_modem_network_joined,
	.down_data = on_modem_down_data,
	.reset = on_modem_reset,
	.time_updated_alc_sync = on_modem_time_sync,
	.class_b_ping_slot_info = on_class_b_ping_slot_info,
	.class_b_status = on_class_b_status,

};

/* lr11xx radio context and its use in the ralf layer */
static const ralf_t modem_radio = RALF_LR11XX_INSTANTIATE(DEVICE_DT_GET(DT_NODELABEL(lr11xx)));

/* Context data */
static bool is_time_synced = false;
static bool is_ping_slot_set = false;

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
	 * We set our LoRaWAN configration then and start the join process - see reset().
	 */

	while (1) {
		/* Execute modem runtime, this function must be recalled in sleep_time_ms or sooner
		 */
		uint32_t sleep_time_ms = smtc_modem_run_engine();

		LOG_INF("Sleeping for %d ms", sleep_time_ms);
		k_sem_take(&main_sleep_sem, K_MSEC(sleep_time_ms));
	}
}

/**
 * @brief Called when a modem reset is detected
 *
 * @param[in] reset_count Number of resets (stored persistently)
 */
void on_modem_reset(uint16_t reset_count)
{
	LOG_INF("EVENT: RESET");
	LOG_INF("Count: %d", reset_count);
	smtc_app_configure_lorawan_params(STACK_ID, &lorawan_cfg);

	smtc_modem_join_network(STACK_ID);
}

/**
 * @brief Called when modem joins the network
 *
 */
void on_modem_network_joined(void)
{
	LOG_WRN("JOINED!");

	        smtc_modem_adr_set_profile(STACK_ID, SMTC_MODEM_ADR_PROFILE_NETWORK_CONTROLLED, NULL);

	/* Configure class B parameters */
	smtc_modem_class_b_set_ping_slot_periodicity(STACK_ID, LORAWAN_CLASS_B_PING_SLOT);
	smtc_modem_lorawan_class_b_request_ping_slot_info(STACK_ID);

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

//	smtc_modem_time_start_sync_service(STACK_ID, SMTC_MODEM_TIME_MAC_SYNC);
//	smtc_modem_time_set_sync_interval_s(TIME_SYNC_APP_INTERVAL_S);
//	smtc_modem_time_set_sync_invalid_delay_s(TIME_SYNC_APP_INVALID_DELAY_S);
}

/**
 * @brief Time synchronization event callback
 */
void on_modem_time_sync(smtc_modem_event_time_status_t status)
{
	switch (status) {
	case SMTC_MODEM_EVENT_TIME_VALID: {
		uint32_t utc_time;
		smtc_app_get_utc_time(&utc_time);
		LOG_WRN("Current utc time: %d", utc_time);

		is_time_synced = true;

		/* if both time is synced and the ping slot was set, reconfigure to class B */
		if (is_ping_slot_set == true) {
			LOG_WRN("Switching to class B");
			smtc_modem_set_class(STACK_ID, SMTC_MODEM_CLASS_B);
		}

		break;
	}
	default: {
		LOG_WRN("Invalid or unsynced time received");
	}
	}
}

/**
 * @brief Class B ping slot status event callback
 */
void on_class_b_ping_slot_info(smtc_modem_event_class_b_ping_slot_status_t status)
{
	switch (status) {
	case SMTC_MODEM_EVENT_CLASS_B_PING_SLOT_ANSWERED: {
		is_ping_slot_set = true;

		/* if both time is synced and the ping slot was set, reconfigure to class B */
		if (is_time_synced == true) {
			LOG_WRN("Switching to class B");
			smtc_modem_set_class(STACK_ID, SMTC_MODEM_CLASS_B);
		}

		break;
	}
	case SMTC_MODEM_EVENT_CLASS_B_PING_SLOT_NOT_ANSWERED: {
		LOG_INF("Ping slot not answered, trying again");
		smtc_modem_lorawan_class_b_request_ping_slot_info(STACK_ID);
		break;
	}
	}
}

/**
 * @brief Class B status event callback
 *
 * Only after this is called with SMTC_MODEM_EVENT_CLASS_B_READY is class B truly operational
 * and downlinks can be received every LORAWAN_CLASS_B_PING_SLOT without sending any uplinks.
 */
void on_class_b_status(smtc_modem_event_class_b_status_t status)
{
	LOG_INF("Class B status event: %d", status);
}

/**
 * @brief Called when dowlink data has been received
 *
 */
void on_modem_down_data(int8_t rssi, int8_t snr, smtc_modem_event_downdata_window_t rx_window,
			uint8_t port, const uint8_t *payload, uint8_t size)
{
	LOG_INF("EVENT: DOWNDATA");

	LOG_INF("RSSI: %d", rssi);
	LOG_INF("SNR: %d", snr);
	LOG_INF("RX window: %s (%d)", smtc_modem_event_downdata_window_to_str(rx_window),
		rx_window);
	LOG_INF("PORT: %d", port);
	LOG_INF("Payload len: %d", size);
	LOG_HEXDUMP_INF(payload, size, "Payload buffer:");

	k_sem_give(&main_sleep_sem);

	/* NOTE: copy the payload to some other buffer if further use is required */
}
