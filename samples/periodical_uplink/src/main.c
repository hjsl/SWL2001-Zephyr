/** @file main.c
 *
 * @brief Periodical uplink example
 */

#include <smtc_modem_api.h>
#include <smtc_modem_hal_init.h>
#include <smtc_modem_utilities.h>

/* include hal and ralf so that initialization can be done */
#include <ralf_lr11xx.h>
#include <smtc_modem_hal.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main);

#define STACK_ID 0
#define PERIODICAL_UPLINK_DELAY_S 60
#define DELAY_FIRST_MSG_AFTER_JOIN 60

static uint8_t rx_payload[SMTC_MODEM_MAX_LORAWAN_PAYLOAD_LENGTH] = { 0 };
static uint8_t rx_payload_size = 0;
static smtc_modem_dl_metadata_t rx_metadata = { 0 };
static uint8_t rx_remaining = 0;

static uint32_t uplink_counter = 0;

static void modem_event_callback(void);
static void send_uplink_counter_on_port(uint8_t port);
static int get_battery_level_callback(uint32_t *value);
static int get_temperature_callback(int32_t *value);
static int get_voltage_callback(uint32_t *value);





/**
 * @brief Duration between uplinks
 */
#define APP_TX_DUTYCYCLE 60

/**
 * @brief Port to send uplinks on
 */
#define LORAWAN_APP_PORT 2

/**
 * @brief Should uplinks be confirmed
 */
#define LORAWAN_CONFIRMED_MSG_ON false

/* ---------------- LoRaWAN Configurations ---------------- */

/* Stack id value (multistack modem is not yet available, so use 0) */
#define STACK_ID 0

// /*LoRaWAN configuration */
// static struct smtc_app_lorawan_cfg lorawan_cfg = {
// 	.use_chip_eui_as_dev_eui = false,

/* Home */
static const uint8_t dev_eui[] = {0xB5, 0x09, 0xB4, 0x53, 0xFA, 0x12, 0x58, 0x79};
static const uint8_t join_eui[] = {0xE0, 0x96, 0xB0, 0x1D, 0xA5, 0xBF, 0x49, 0x4A};
static const uint8_t app_key[] = {0xDA, 0x87, 0xEC, 0x9C, 0x3E, 0xF7, 0x43, 0x52, 0x49, 0x2D, 0x67, 0x08, 0x2F, 0x2E, 0xA2, 0xE6};
// 	.dev_eui = {0xb5, 0x09, 0xb4, 0x53, 0xfa, 0x12, 0x58, 0x79},
// 	.join_eui = {0xe0, 0x96, 0xb0, 0x1d, 0xa5, 0xbf, 0x49, 0x4a},
// 	.app_key =  {0xda, 0x87, 0xec, 0x9c, 0x3e, 0xf7, 0x43, 0x52, 0x49, 0x2d, 0x67, 0x08,
//   0x2f, 0x2e, 0xa2, 0xe6},
// 	.class = SMTC_MODEM_CLASS_A,
// 	.region = SMTC_MODEM_REGION_EU_868,
// };

static struct smtc_modem_hal_cb hal_callbacks = {
	.get_battery_level = get_battery_level_callback,
	.get_temperature = get_temperature_callback,
	.get_voltage = get_voltage_callback
};





static int get_battery_level_callback(uint32_t *value)
{
	*value = 0;
	return 0;
}

static int get_temperature_callback(int32_t *value)
{
	*value = 0;
	return 0;
}

static int get_voltage_callback(uint32_t *value)
{
	*value = 0;
	return 0;
}

static void modem_event_callback(void)
{
	smtc_modem_return_code_t rc;
	smtc_modem_event_t current_event;
	uint8_t event_pending_count;
	uint8_t stack_id = STACK_ID;
	


	LOG_INF("modem_event_callback");

	do {
		rc = smtc_modem_get_event(&current_event, &event_pending_count);
		if (rc != SMTC_MODEM_RC_OK) {
			LOG_ERR("Failed to get modem event, err: %d", rc);
			return;
		}

		switch (current_event.event_type) {
			case SMTC_MODEM_EVENT_RESET:
				LOG_INF("Event received: RESET");

				rc = smtc_modem_set_deveui(stack_id, dev_eui);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to set deveui, err: %d", rc);
					return;
				}

				rc = smtc_modem_set_joineui(stack_id, join_eui);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to set joineui, err: %d", rc);
					return;
				}

				rc = smtc_modem_set_nwkkey(stack_id, app_key);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to set nwkkey, err: %d", rc);
					return;
				}

				rc = smtc_modem_set_region(stack_id, SMTC_MODEM_REGION_EU_868);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to set region, err: %d", rc);
					return;
				}

				rc = smtc_modem_join_network(stack_id);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to start join, err: %d", rc);
					return;
				}

				break;

			case SMTC_MODEM_EVENT_JOINED:
				LOG_INF("Event received: JOINED");
				break;

			case SMTC_MODEM_EVENT_TXDONE:
				LOG_INF("Event received: TXDONE");
				break;

			default:
				LOG_INF("Event received");
				break;
		}
	} while (event_pending_count > 0);

}




K_SEM_DEFINE(main_sleep_sem, 0, 1);

int main(void)
{
	uint32_t sleep_time_ms = 0;

	smtc_modem_hal_init(DEVICE_DT_GET(DT_NODELABEL(lr11xx)), &hal_callbacks);

	smtc_modem_init(&modem_event_callback);

	/* Enter main loop:
	 * The fist call to smtc_modem_run_engine will trigger the reset callback.
	 */

	while (1) {
		/* Execute modem runtime, this function must be called again in sleep_time_ms
		 * milliseconds or sooner. */
		sleep_time_ms = smtc_modem_run_engine();

		LOG_INF("Sleeping for %d ms", sleep_time_ms);
		k_sleep(K_MSEC(sleep_time_ms));
	}

	return 0;
}