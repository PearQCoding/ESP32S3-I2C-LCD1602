/* HD44780 LCD + PCF8574-compatible I2C backpack, ESP-IDF 6.x adaptation.
 * API/mapping based on the public-domain ESP32-HD44780 driver by maxsydney:
 * https://github.com/maxsydney/ESP32-HD44780
 * The underlying source was released into the public domain / CC0.
 *
 * PCF8574 mapping: P0=RS, P1=RW, P2=EN, P3=backlight, P4..P7=D4..D7.
 * Unlike the 2022 example, this uses driver/i2c_master.h (current API).
 * All LCD stabilization delays here are 10 ms where relevant.
 */
#include "HD44780.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stdint.h>

#define LCD_BACKLIGHT 0x08
#define LCD_ENABLE    0x04
#define LCD_RS        0x01
#define LCD_COMMAND   0x00
#define LCD_DATA      0x01

static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_lcd;
static uint8_t s_rows;
static uint8_t s_cols;

/* Sends one nibble over the PCF8574 with the enable pulse. I2C is 100 kHz,
 * so the EN pulse meets the LCD's minimum timing requirements. */
static void LCD_writeNibble(uint8_t upper_nibble, uint8_t mode)
{
    uint8_t value = (upper_nibble & 0xF0) | LCD_BACKLIGHT | mode;
    uint8_t pulse[3] = {value, (uint8_t)(value | LCD_ENABLE), value};
    ESP_ERROR_CHECK(i2c_master_transmit(s_lcd, pulse, sizeof(pulse), 1000));
}

static void LCD_writeByte(uint8_t byte, uint8_t mode)
{
    LCD_writeNibble(byte & 0xF0, mode);
    LCD_writeNibble((byte << 4) & 0xF0, mode);
}

void LCD_init(uint8_t addr, uint8_t dataPin, uint8_t clockPin,
              uint8_t cols, uint8_t rows)
{
    s_cols = cols;
    s_rows = rows;

    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = dataPin,
        .scl_io_num = clockPin,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &s_bus));

    i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(s_bus, &device_config, &s_lcd));

    vTaskDelay(pdMS_TO_TICKS(100));
    // Three wake-up nibbles then select 4-bit mode.
    LCD_writeNibble(0x30, LCD_COMMAND);
    vTaskDelay(10 / portTICK_PERIOD_MS);  // First required stabilization wait
    LCD_writeNibble(0x30, LCD_COMMAND);
    vTaskDelay(pdMS_TO_TICKS(10));
    LCD_writeNibble(0x30, LCD_COMMAND);
    vTaskDelay(pdMS_TO_TICKS(10));
    LCD_writeNibble(0x20, LCD_COMMAND);
    vTaskDelay(pdMS_TO_TICKS(10));

    LCD_writeByte(0x28, LCD_COMMAND);  // 4 bits, 2 lines, 5x8 dots
    LCD_writeByte(0x01, LCD_COMMAND);  // Clear display
    vTaskDelay(10 / portTICK_PERIOD_MS);  // Wait after initial clear
    LCD_writeByte(0x06, LCD_COMMAND);  // Increment cursor
    LCD_writeByte(0x0C, LCD_COMMAND);  // Display on, cursor off
}

void LCD_setCursor(uint8_t col, uint8_t row)
{
    static const uint8_t row_offsets[] = {0x00, 0x40, 0x14, 0x54};
    if (row >= s_rows || col >= s_cols) return;
    LCD_writeByte(0x80 | (col + row_offsets[row]), LCD_COMMAND);
}

void LCD_home(void)
{
    LCD_writeByte(0x02, LCD_COMMAND);
    vTaskDelay(10 / portTICK_PERIOD_MS);  // Home command delay
}

void LCD_clearScreen(void)
{
    LCD_writeByte(0x01, LCD_COMMAND);
    vTaskDelay(10 / portTICK_PERIOD_MS);  // Clear command delay
}

void LCD_writeChar(char c)
{
    LCD_writeByte((uint8_t)c, LCD_DATA);
}

void LCD_writeStr(const char *str)
{
    while (*str) LCD_writeChar(*str++);
}
