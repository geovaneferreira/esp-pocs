/* Hello World Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_spi_flash.h"
#include "esp_log.h"

static const char *TAG = "ExampleUART";
static const char *TAGRX = "UART RX";


#define ECHO_TEST_TXD 17
#define ECHO_TEST_RXD 16
#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)

#define BUF_SIZE (1024)
#define UART_PORT_NUM 2

static void configure_uart(const uart_port_t uart_num)
{
    ESP_LOGI(TAG, "Configured Uart !");
    // const uart_port_t uart_num = uart_port;
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
     int intr_alloc_flags = 0;
    // Configure UART parameters
    
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));
}

static void uart_task(void *arg)
{
    // Configure a temporary buffer for the incoming data
    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
    uint8_t *datacp = (uint8_t *) malloc(BUF_SIZE*2);
    int newdata = 0;
    int lenpk = 0;
    while (1) {
        // Read data from the UART
        int len = uart_read_bytes(UART_PORT_NUM, data, BUF_SIZE, 20 / portTICK_RATE_MS);
        if(len > 1){
            printf("recebido %d\n", len);
            for(int i=0;i<len;i++){
                datacp[i+lenpk] = data[i];
            }
            lenpk += len;
            vTaskDelay(10);
            newdata = 1;
        } else if(newdata == 1 && len == 0){
            //Log to console
            ESP_LOG_BUFFER_CHAR(TAGRX, datacp, lenpk);
            // Write data back to the UART
            uart_write_bytes(UART_PORT_NUM, (const char *) datacp, lenpk);
            lenpk = 0;
            newdata = 0;
        }
        vTaskDelay(1);
    }
}

void app_main(void)
{
    printf("Hello world!\n");

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is ESP32 chip with %d CPU cores, WiFi%s%s, ",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    configure_uart(UART_PORT_NUM);

    xTaskCreate(uart_task, "uart_echo_task", 2048, NULL, 10, NULL);
}
