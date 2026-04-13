#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "Самостійна_робота";

#define BLINK_GPIO 2
#define BUTTON_GPIO 0 
#define MY_GROUP_NUMBER 4 

void configure_led(void) {
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

void blink_led(int state) {
    gpio_set_level(BLINK_GPIO, state);
}

void app_main(void)
{
    configure_led(); 

    gpio_reset_pin(BUTTON_GPIO);
    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_GPIO, GPIO_PULLUP_ONLY);

    int digit = 0;
    int led_state = 0;

    while (1) {
        ESP_LOGI(TAG, "Поточна цифра: %d", digit);
        
        led_state = !led_state;
        blink_led(led_state);
        
        if (gpio_get_level(BUTTON_GPIO) == 0) {
            ESP_LOGW(TAG, "!!! Кнопка натиснута !!!");
            ESP_LOGW(TAG, "Мій номер у списку: %d", MY_GROUP_NUMBER);
            vTaskDelay(pdMS_TO_TICKS(500)); 
        }

        digit++;
        if (digit > 9) {
            digit = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}