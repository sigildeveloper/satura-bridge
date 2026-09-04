#include "board.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

/* M5StickC Plus2 has NO power management chip — unlike the original
 * M5StickC Plus (AXP192) and Core2 (also AXP192), the Plus2 dropped
 * the PMIC entirely. Power hold and battery sensing are done through
 * bare GPIO/ADC instead. Verified against two independent sources
 * before writing this (per explicit instruction not to trust anything
 * carried over from an earlier conversation): M5Stack's own official
 * product manual/power-on-off sequence, and ESPHome's
 * community-maintained device page for this exact board
 * (devices.esphome.io/devices/M5Stack-M5StickC-PLUS2) — both agree:
 *
 *   - GPIO4 ("HOLD"): must be driven HIGH immediately after boot, or
 *     the board powers itself back off a moment later. Driving it LOW
 *     is the documented way to power off — never do that from here.
 *   - GPIO38: battery voltage sense, ADC1 channel 2, behind a
 *     resistor divider that halves the real voltage — the raw ADC
 *     reading has to be doubled to recover actual battery voltage.
 *     ESPHome's own example config confirms both the channel and the
 *     "multiply by 2" step explicitly.
 *
 * No documented charge-status signal was found for this board in
 * either source — it has no PMU to ask, and there's no dedicated pin
 * for it in the pinout tables. Rather than guess at an unverified
 * GPIO, board_battery_is_charging() always reports false here. */

#define PLUS2_HOLD_GPIO      GPIO_NUM_4
#define PLUS2_BATT_ADC_UNIT  ADC_UNIT_1
#define PLUS2_BATT_ADC_CHAN  ADC_CHANNEL_2 /* GPIO38 on ADC1 */

static const char *TAG = "board_stickc_plus2";
static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t cali_handle = NULL;
static bool cali_ok = false;

void board_power_hold(void) {
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << PLUS2_HOLD_GPIO,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&cfg);
    gpio_set_level(PLUS2_HOLD_GPIO, 1);
}

void board_init(void) {
    /* Also asserted separately, earlier, directly from
     * app_bootstrap.c's very first line — see the comment there for
     * why. Calling it again here is redundant but harmless
     * (idempotent), and keeps this function self-sufficient if it's
     * ever called on its own. */
    board_power_hold();

    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = PLUS2_BATT_ADC_UNIT,
    };
    if (adc_oneshot_new_unit(&unit_cfg, &adc_handle) != ESP_OK) {
        ESP_LOGW(TAG, "failed to init ADC unit for battery sense");
        adc_handle = NULL;
        return;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    if (adc_oneshot_config_channel(adc_handle, PLUS2_BATT_ADC_CHAN, &chan_cfg) != ESP_OK) {
        ESP_LOGW(TAG, "failed to configure battery ADC channel");
    }

    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id = PLUS2_BATT_ADC_UNIT,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    cali_ok = (adc_cali_create_scheme_line_fitting(&cali_cfg, &cali_handle) == ESP_OK);
    if (!cali_ok) {
        ESP_LOGW(TAG, "ADC calibration unavailable — battery voltage reading will be uncalibrated/approximate");
    }

    ESP_LOGI(TAG, "M5StickC Plus2 board ready (power hold + battery ADC)");
}

const char *board_get_name(void) { return "M5StickC Plus2"; }

int board_get_battery_voltage_mv(void) {
    if (!adc_handle) return -1;

    int raw = 0;
    if (adc_oneshot_read(adc_handle, PLUS2_BATT_ADC_CHAN, &raw) != ESP_OK) return -1;

    if (cali_ok) {
        int mv = 0;
        if (adc_cali_raw_to_voltage(cali_handle, raw, &mv) == ESP_OK) {
            return mv * 2; /* undo the divider */
        }
    }

    /* Calibration unavailable — rough linear estimate against 12dB
     * attenuation's ~3100mV full-scale over the 12-bit range. Less
     * accurate than the calibrated path but still directionally
     * usable rather than returning nothing. */
    return (int)(raw * 3100.0f / 4095.0f) * 2;
}

int board_get_battery_percent(void) {
    int mv = board_get_battery_voltage_mv();
    if (mv < 0) return -1;

    /* No official M5Stack percentage curve was found for this board
     * (unlike Core2's AXP192 GetBatteryLevel(), which is a real,
     * sourced formula) — this is a generic single-cell LiPo linear
     * approximation (3.3V empty, 4.2V full), not derived from any
     * M5Stack firmware. Treat as approximate, not authoritative. */
    float v = mv / 1000.0f;
    float pct = (v - 3.3f) * (100.0f / (4.2f - 3.3f));
    if (pct > 100.0f) pct = 100.0f;
    if (pct < 0.0f) pct = 0.0f;
    return (int)pct;
}

bool board_battery_is_charging(void) {
    return false; /* unsupported on this board — see file comment */
}

bool board_battery_charging_status_available(void) {
    return false;
}
