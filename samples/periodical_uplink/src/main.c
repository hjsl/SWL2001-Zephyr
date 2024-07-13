/** @file main.c
 *
 * @brief Periodical uplink example
 */

#include <smtc_modem_api.h>
#include <smtc_modem_hal_init.h>
#include <smtc_modem_utilities.h>
#include <smtc_modem_geolocation_api.h>

/* include hal and ralf so that initialization can be done */
#include <ralf_lr11xx.h>
#include <smtc_modem_hal.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>

#include <stdio.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main);


#define STACK_ID 0
#define PERIODICAL_UPLINK_DELAY_S 60
#define DELAY_FIRST_MSG_AFTER_JOIN 60

static uint8_t rx_payload[SMTC_MODEM_MAX_LORAWAN_PAYLOAD_LENGTH] = { 0 };
static uint8_t rx_payload_size = 0;
static smtc_modem_dl_metadata_t rx_metadata = { 0 };
static uint8_t rx_remaining = 0;

static void modem_event_callback(void);
static int get_battery_level_callback(uint32_t *value);
static int get_temperature_callback(int32_t *value);
static int get_voltage_callback(uint32_t *value);
static void user_lbm_irq_callback(void);

/**
 * @brief Duration between uplinks
 */
#define APP_TX_DUTYCYCLE 60

/**
 * @brief Port to send uplinks on
 */
#define KEEP_ALIVE_PORT (2)
#define KEEP_ALIVE_PERIOD_S (3600 / 2)
#define KEEP_ALIVE_SIZE (4)
uint8_t keep_alive_payload[KEEP_ALIVE_SIZE] = { 0 };

#define GEOLOCATION_GNSS_SCAN_PERIOD_S (2 * 60)
#define GEOLOCATION_WIFI_SCAN_PERIOD_S (3 * 60)

/* ---------------- LoRaWAN Configurations ---------------- */

/* Stack id value (multistack modem is not yet available, so use 0) */
#define STACK_ID 0

K_SEM_DEFINE(main_sleep_sem, 0, 1);

static uint8_t dev_eui[8] = { 0 };
static uint8_t join_eui[8] = { 0 };
static uint8_t app_key[16] = { 0 };

static struct smtc_modem_hal_cb hal_callbacks = {
	.get_battery_level = get_battery_level_callback,
	.get_temperature = get_temperature_callback,
	.get_voltage = get_voltage_callback,
	.user_lbm_irq = user_lbm_irq_callback,
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

static void user_lbm_irq_callback(void)
{
	// LOG_WRN("%s", __func__);
	k_sem_give(&main_sleep_sem);
}

static void modem_event_callback(void)
{
	smtc_modem_return_code_t rc;
	smtc_modem_event_t current_event;
	uint8_t event_pending_count;
	uint8_t stack_id = STACK_ID;
	smtc_modem_gnss_event_data_scan_done_t gnss_scan_done_data;
	smtc_modem_gnss_event_data_terminated_t gnss_terminated_data;
	smtc_modem_wifi_event_data_scan_done_t wifi_scan_done_data;
	smtc_modem_wifi_event_data_terminated_t wifi_terminated_data;
	smtc_modem_almanac_demodulation_event_data_almanac_update_t almanac_demodulation_data;
	uint32_t gps_time_s;
	uint32_t gps_fractional_s;

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
					break;
				}

				rc = smtc_modem_set_joineui(stack_id, join_eui);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to set joineui, err: %d", rc);
					break;
				}

				rc = smtc_modem_set_nwkkey(stack_id, app_key);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to set nwkkey, err: %d", rc);
					break;
				}

				rc = smtc_modem_set_region(stack_id, SMTC_MODEM_REGION_EU_868);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to set region, err: %d", rc);
					break;
				}

				rc = smtc_modem_join_network(stack_id);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to start join, err: %d", rc);
					break;
				}

				rc = smtc_modem_almanac_demodulation_set_constellations(stack_id, SMTC_MODEM_GNSS_CONSTELLATION_GPS_BEIDOU);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to set almanac demodulation constellations, err: %d", rc);
					break;
				}

				// rc = smtc_modem_almanac_demodulation_start(stack_id);
				// if (rc != SMTC_MODEM_RC_OK) {
				// 	LOG_ERR("Failed to start almanac demodulation, err: %d", rc);
				// 	break;
				// }

     			rc = smtc_modem_store_and_forward_flash_clear_data( stack_id );
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to clear flash data, err: %d", rc);
					break;
				}
            
				rc = smtc_modem_store_and_forward_set_state( stack_id, SMTC_MODEM_STORE_AND_FORWARD_ENABLE );
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to enable store and forward, err: %d", rc);
					break;
				}
       
				rc = smtc_modem_gnss_send_mode(stack_id, SMTC_MODEM_SEND_MODE_STORE_AND_FORWARD);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to set gnss send mode, err: %d", rc);
					break;
				}

				rc = smtc_modem_wifi_send_mode(stack_id, SMTC_MODEM_SEND_MODE_STORE_AND_FORWARD);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to set wifi send mode, err: %d", rc);
					break;
				}

				rc = smtc_modem_gnss_set_constellations(stack_id, SMTC_MODEM_GNSS_CONSTELLATION_GPS_BEIDOU);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to set gnss scan constellations, err: %d", rc);
					break;
				}

				// rc = smtc_modem_alarm_start_timer(60);
				// if (rc != SMTC_MODEM_RC_OK) {
				// 	LOG_ERR("Failed to start timer, err: %d", rc);
				// 	break;
				// }

				// rc = smtc_modem_gnss_scan(stack_id, SMTC_MODEM_GNSS_MODE_STATIC, 0);
				// if (rc != SMTC_MODEM_RC_OK) {
				// 	LOG_ERR("Failed to start gnss scan, err: %d", rc);
				// 	break;
				// }

				// rc = smtc_modem_wifi_scan(stack_id, 0);
				// if (rc != SMTC_MODEM_RC_OK) {
				// 	LOG_ERR("Failed to start wifi scan, err: %d", rc);
				// 	break;
				// }

				break;

			case SMTC_MODEM_EVENT_ALARM:
				LOG_INF("Event received: ALARM");

				rc = smtc_modem_request_uplink(stack_id, KEEP_ALIVE_PORT, false, keep_alive_payload, KEEP_ALIVE_SIZE);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to request uplink, err: %d", rc);
					break;
				}

				rc = smtc_modem_alarm_start_timer(60);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to start timer, err: %d", rc);
					break;
				}

				break;

			case SMTC_MODEM_EVENT_JOINED:
				LOG_INF("Event received: JOINED");

				// send_uplink_counter_on_port(101);

				// rc = smtc_modem_set_nb_trans(stack_id, custom_nb_trans);
				// if (rc != SMTC_MODEM_RC_OK) {
				// 	LOG_ERR("Failed to set number of transmission, err: %d", rc);
				// 	break;
				// }

				// rc = smtc_modem_alarm_start_timer(KEEP_ALIVE_PERIOD_S);
				// if (rc != SMTC_MODEM_RC_OK) {
				// 	LOG_ERR("Failed to start timer, err: %d", rc);
				// 	break;
				// }

				rc = smtc_modem_start_alcsync_service(stack_id);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to start alcsync service, err: %d", rc);
					break;
				}

				rc = smtc_modem_trigger_alcsync_request(stack_id);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to trigger alcsync request, err: %d", rc);
					break;
				}

				rc = smtc_modem_almanac_start(stack_id);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to start almanac update service, err: %d", rc);
					break;
				}

				rc = smtc_modem_trig_lorawan_mac_request(stack_id, SMTC_MODEM_LORAWAN_MAC_REQ_DEVICE_TIME);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to request device time, err: %d", rc);
					break;
				}

				break;

			case SMTC_MODEM_EVENT_TXDONE:
				LOG_INF("Event received: TXDONE");

				break;

			case SMTC_MODEM_EVENT_DOWNDATA:
				LOG_INF("Event received: DOWNDATA");

				rc = smtc_modem_get_downlink_data(rx_payload,
						&rx_payload_size, &rx_metadata, &rx_remaining);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Faild to get downlink data, err: %d", rc);
					break;
				}

				LOG_INF("Data received on port %u", rx_metadata.fport);
				LOG_HEXDUMP_INF(rx_payload, rx_payload_size, "Received payload:");

				break;

			case SMTC_MODEM_EVENT_JOINFAIL:
				LOG_INF("Event received: JOINFAIL");
				break;

			case SMTC_MODEM_EVENT_GNSS_ALMANAC_DEMOD_UPDATE:
				LOG_INF("Event received: GNSS_ALMANAC_DEMOD_UPDATE");

				rc = smtc_modem_almanac_demodulation_get_event_data_almanac_update(stack_id, &almanac_demodulation_data);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to get almanac demodulation data, err: %d", rc);
					break;
				}

				LOG_INF("progress: GPS %u%%, beidou %u%%", almanac_demodulation_data.update_progress_gps, almanac_demodulation_data.update_progress_beidou);

				/* Store progress in keep alive payload. */
				keep_alive_payload[0] = almanac_demodulation_data.update_progress_gps;
				keep_alive_payload[1] = almanac_demodulation_data.update_progress_beidou;
 
				break;

			case SMTC_MODEM_EVENT_GNSS_SCAN_DONE:
				LOG_INF("Event received: GNSS_SCAN_DONE");

				rc = smtc_modem_gnss_get_event_data_scan_done(stack_id, &gnss_scan_done_data);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to get gnss scan done data, err: %d", rc);
					break;
				}

				for (int i = 0; i < gnss_scan_done_data.nb_scans_valid; i++) {
					LOG_INF("Scan group: %u, %u SVs detected", i, gnss_scan_done_data.scans[i].nb_svs);
					for (int j = 0; j < gnss_scan_done_data.scans[i].nb_svs; j++) {
						/* Correct SV id. */
						uint8_t sv_id = gnss_scan_done_data.scans[i].info_svs[j].satellite_id;
						char sv_id_str[5] = { 0 };
						if (sv_id < 64) {
							/* GPS */
							snprintf(sv_id_str, sizeof(sv_id_str), "G%02u", sv_id + 1);
						} else {
							/* Beidou */
							snprintf(sv_id_str, sizeof(sv_id_str), "C%02u", sv_id - 63);
						}

						LOG_INF("SV id: %s snr: %d", sv_id_str, gnss_scan_done_data.scans[i].info_svs[j].cnr);
					}
				}

				if (gnss_scan_done_data.indoor_detected) {
					rc = smtc_modem_wifi_scan(stack_id, 0);
					if (rc != SMTC_MODEM_RC_OK) {
						LOG_ERR("Failed to start wifi scan, err: %d", rc);
						break;
					}
				}

				break;

			case SMTC_MODEM_EVENT_GNSS_TERMINATED:
				LOG_INF("Event received: GNSS_TERMINATED");

				rc = smtc_modem_gnss_get_event_data_terminated(stack_id, &gnss_terminated_data);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to get gnss terminated data, err: %d", rc);
					break;
				}

				rc = smtc_modem_gnss_scan(stack_id, SMTC_MODEM_GNSS_MODE_STATIC, GEOLOCATION_GNSS_SCAN_PERIOD_S);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to start gnss scan, err: %d", rc);
					break;
				}

				break;

			case SMTC_MODEM_EVENT_WIFI_SCAN_DONE:
				LOG_INF("Event received: WIFI_SCAN_DONE");
				
				rc = smtc_modem_wifi_get_event_data_scan_done(stack_id, &wifi_scan_done_data);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to get wifi scan done data, err: %d", rc);
					break;
				}

				LOG_INF("Got %u networks", wifi_scan_done_data.nbr_results);

				break;

			case SMTC_MODEM_EVENT_WIFI_TERMINATED:
				LOG_INF("Event received: WIFI_TERMINATED");
				
				rc = smtc_modem_wifi_get_event_data_terminated(stack_id, &wifi_terminated_data);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to get wifi terminated data, err: %d", rc);
					break;
				}

				rc = smtc_modem_wifi_scan(stack_id, GEOLOCATION_WIFI_SCAN_PERIOD_S);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to start gnss scan, err: %d", rc);
					break;
				}

				break;

			case SMTC_MODEM_EVENT_ALCSYNC_TIME:
				LOG_INF("Event received: ALCSYNC_TIME");

				rc = smtc_modem_get_alcsync_time(stack_id, &gps_time_s);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to get alcsync time, err: %d", rc);
					break;
				}

				LOG_INF("Alcsync time: %u", gps_time_s);

				break;

			case SMTC_MODEM_EVENT_LORAWAN_MAC_TIME:
				LOG_INF("Event received: LORAWAN_MAC_TIME");

				rc = smtc_modem_get_lorawan_mac_time(stack_id, &gps_time_s, &gps_fractional_s);
				if (rc != SMTC_MODEM_RC_OK) {
					LOG_ERR("Failed to get lorawan mac time, err: %d", rc);
					break;
				}

				LOG_INF("Lorawan mac time: %u.%u", gps_time_s, gps_fractional_s);

				

				break;

			default:
				LOG_INF("Event received");
				break;
		}
	} while (event_pending_count > 0);
}


int main(void)
{
	hex2bin(CONFIG_LORAWAN_DEV_EUI, 16, dev_eui, sizeof(dev_eui));
	hex2bin(CONFIG_LORAWAN_JOIN_EUI, 16, join_eui, sizeof(join_eui));
	hex2bin(CONFIG_LORAWAN_APP_KEY, 32, app_key, sizeof(app_key));

	const struct device *dev = DEVICE_DT_GET(DT_NODELABEL(lr11xx));
	smtc_modem_hal_init(dev, &hal_callbacks);
	smtc_modem_set_radio_context(dev);

	smtc_modem_init(&modem_event_callback);

	/* Enter main loop:
	 * The fist call to smtc_modem_run_engine will trigger the reset callback.
	 */

	while (true) {
		/* Execute modem runtime, this function must be called again in sleep_time_ms
		 * milliseconds or sooner. */
		uint32_t sleep_time_ms = smtc_modem_run_engine();

		LOG_INF("Sleeping for %d ms", sleep_time_ms);
		k_sem_take(&main_sleep_sem, K_MSEC(sleep_time_ms));
	}

	return 0;
}