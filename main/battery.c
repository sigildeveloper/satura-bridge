#include "config.h"
#include "battery.h"

#if ENABLE_M5STACK_BATTERY

#include "driver/i2c_master.h"
#include "esp_log.h"

/* M5Stack Core2 (original — AXP192 PMIC). Registers and the battery
 * percentage formula are taken directly from M5Stack's own official
 * AXP192.cpp (github.com/m5stack/M5Core2/blob/master/src/AXP192.cpp):
 *   - 0x78/0x79: battery voltage, 12-bit ADC, 1.1 mV/LSB
 *   - 0x00 bit 2 (0x04): charging status
 * GetBatteryLevel()'s own linear voltage->percent mapping is reused
 * as-is rather than re-derived, since it's the same formula M5Stack's
 * official firmware itself uses to report "the" battery percentage —
 * matching it means this reads the same number the stock M5Stack
 * launcher would show.
 *
 * Internal PMIC bus: SDA=GPIO21, SCL=GPIO22 (M5Stack's own "Wire1"),
 * I2C address 0x34 — confirmed independently by multiple third-party
 * AXP192 drivers (ESPHome, TinyGo), not just the official source. */

#define AXP192_I2C_ADDR 0x34
#define AXP192_SDA_GPIO 21
#define AXP192_SCL_GPIO 22

static const char *TAG = "battery";
static i2c_master_bus_handle_t bus = NULL;
static i2c_master_dev_handle_t dev = NULL;

static bool axp192_read(uint8_t reg, uint8_t *out, size_t len) {
    if (!dev) return false;
    return i2c_master_transmit_receive(dev, &reg, 1, out, len, 100) == ESP_OK;
}

void battery_init(void) {
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = AXP192_SDA_GPIO,
        .scl_io_num = AXP192_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    if (i2c_new_master_bus(&bus_cfg, &bus) != ESP_OK) {
        ESP_LOGW(TAG, "failed to init I2C bus for AXP192");
        return;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP192_I2C_ADDR,
        .scl_speed_hz = 400000,
    };
    if (i2c_master_bus_add_device(bus, &dev_cfg, &dev) != ESP_OK) {
        ESP_LOGW(TAG, "failed to add AXP192 as I2C device");
        dev = NULL;
        return;
    }

    ESP_LOGI(TAG, "AXP192 battery monitor ready");
}

int battery_get_percent(void) {
    uint8_t buf[2];
    if (!axp192_read(0x78, buf, 2)) return -1;

    uint16_t raw12 = ((uint16_t)buf[0] << 4) | (buf[1] & 0x0F);
    float voltage = raw12 * 1.1f / 1000.0f; /* volts */

    /* M5Stack's own AXP192::GetBatteryLevel() formula, unmodified. */
    float pct = (voltage < 3.248088f) ? 0.0f : (voltage - 3.120712f) * 100.0f;
    if (pct > 100.0f) pct = 100.0f;
    if (pct < 0.0f) pct = 0.0f;
    return (int)pct;
}

bool battery_is_charging(void) {
    uint8_t status;
    if (!axp192_read(0x00, &status, 1)) return false;
    return (status & 0x04) != 0;
}

#else /* !ENABLE_M5STACK_BATTERY */

void battery_init(void) {}
int battery_get_percent(void) { return -1; }
bool battery_is_charging(void) { return false; }

#endif
