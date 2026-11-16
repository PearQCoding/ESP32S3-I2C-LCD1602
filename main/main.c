/*
 * SPIFFS File Storage and I2C LCD1602 Display
 *
 * Writes "Hello" and "World" to a SPIFFS file, reads the
 * stored data, and displays it on an I2C LCD1602.
 *
 * ESP32-S3 | ESP-IDF v6.1-beta1
 * ECEN 621 - Embedded Computing System Design
 * North Carolina A&T State University
 */

#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "HD44780.h"

#define TAG       "SPIFFS_LCD"
#define FILE_PATH "/spiffs/message.txt"

// I2C LCD1602 configuration
#define LCD_ADDR  0x27
#define SDA_PIN   14
#define SCL_PIN   13
#define LCD_COLS  16
#define LCD_ROWS  2

void app_main(void)
{
    // 1. Mount the SPIFFS storage partition.
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true,
    };

    esp_err_t err = esp_vfs_spiffs_register(&conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(err));
        return;
    }

    // 2. Write both words to separate lines of the SPIFFS file.
    FILE *file = fopen(FILE_PATH, "w");
    if (file == NULL) {
        ESP_LOGE(TAG, "Could not open file for writing");
        goto cleanup;
    }

    if (fprintf(file, "Hello\nWorld\n") < 0) {
        ESP_LOGE(TAG, "Writing to SPIFFS failed");
        fclose(file);
        goto cleanup;
    }
    if (fclose(file) != 0) {
        ESP_LOGE(TAG, "Could not close the written file");
        goto cleanup;
    }

    // 3. Reopen the file and read both lines from SPIFFS.
    char first_line[17] = {0};
    char second_line[17] = {0};
    file = fopen(FILE_PATH, "r");
    if (file == NULL) {
        ESP_LOGE(TAG, "Could not open file for reading");
        goto cleanup;
    }

    if (fgets(first_line, sizeof(first_line), file) == NULL ||
        fgets(second_line, sizeof(second_line), file) == NULL) {
        ESP_LOGE(TAG, "Could not read both lines");
        fclose(file);
        goto cleanup;
    }
    fclose(file);

    // Remove the newline characters before writing to the LCD.
    first_line[strcspn(first_line, "\r\n")] = '\0';
    second_line[strcspn(second_line, "\r\n")] = '\0';

    printf("Contents of %s:\n", FILE_PATH);
    printf("Line 1: %s\n", first_line);
    printf("Line 2: %s\n", second_line);

    // 4. Display the values read from the SPIFFS file on the LCD.
    LCD_init(LCD_ADDR, SDA_PIN, SCL_PIN, LCD_COLS, LCD_ROWS);
    LCD_clearScreen();
    LCD_setCursor(0, 0);
    LCD_writeStr(first_line);
    LCD_setCursor(0, 1);
    LCD_writeStr(second_line);

    ESP_LOGI(TAG, "SPIFFS data is now displayed on the LCD");

cleanup:
    esp_vfs_spiffs_unregister("storage");
}
