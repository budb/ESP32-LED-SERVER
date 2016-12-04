
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "led_strip/led_strip.h"
#include "esp_log.h"

#include <stdio.h>

#include "led_control.h"

#define LED_STRIP_LENGTH 4U
static struct led_color_t led_strip_buf_1[LED_STRIP_LENGTH];
static struct led_color_t led_strip_buf_2[LED_STRIP_LENGTH];

static const char *TAG = "rgb_led";

#define LED_STRIP_RMT_INTR_NUM 19

static const const uint8_t lights[360]={
  0,   0,   0,   0,   0,   1,   1,   2, 
  2,   3,   4,   5,   6,   7,   8,   9, 
 11,  12,  13,  15,  17,  18,  20,  22, 
 24,  26,  28,  30,  32,  35,  37,  39, 
 42,  44,  47,  49,  52,  55,  58,  60, 
 63,  66,  69,  72,  75,  78,  81,  85, 
 88,  91,  94,  97, 101, 104, 107, 111, 
114, 117, 121, 124, 127, 131, 134, 137, 
141, 144, 147, 150, 154, 157, 160, 163, 
167, 170, 173, 176, 179, 182, 185, 188, 
191, 194, 197, 200, 202, 205, 208, 210, 
213, 215, 217, 220, 222, 224, 226, 229, 
231, 232, 234, 236, 238, 239, 241, 242, 
244, 245, 246, 248, 249, 250, 251, 251, 
252, 253, 253, 254, 254, 255, 255, 255, 
255, 255, 255, 255, 254, 254, 253, 253, 
252, 251, 251, 250, 249, 248, 246, 245, 
244, 242, 241, 239, 238, 236, 234, 232, 
231, 229, 226, 224, 222, 220, 217, 215, 
213, 210, 208, 205, 202, 200, 197, 194, 
191, 188, 185, 182, 179, 176, 173, 170, 
167, 163, 160, 157, 154, 150, 147, 144, 
141, 137, 134, 131, 127, 124, 121, 117, 
114, 111, 107, 104, 101,  97,  94,  91, 
 88,  85,  81,  78,  75,  72,  69,  66, 
 63,  60,  58,  55,  52,  49,  47,  44, 
 42,  39,  37,  35,  32,  30,  28,  26, 
 24,  22,  20,  18,  17,  15,  13,  12, 
 11,   9,   8,   7,   6,   5,   4,   3, 
  2,   2,   1,   1,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0, 
  0,   0,   0,   0,   0,   0,   0,   0};

static struct led_strip_t led_strip = {
        .rgb_led_type = RGB_LED_TYPE_WS2812,
        .rmt_channel = RMT_CHANNEL_1,
        .rmt_interrupt_num = LED_STRIP_RMT_INTR_NUM,
        .gpio = GPIO_NUM_14,
        .led_strip_buf_1 = led_strip_buf_1,
        .led_strip_buf_2 = led_strip_buf_2,
        .led_strip_length = LED_STRIP_LENGTH
    };

static struct led_color_t led_color = {
        .red = 0,
        .green = 0,
        .blue = 0,
    };

static int rgb_red=0;
static int rgb_green=120;
static int rgb_blue=240;

bool running = true;

void check_notify(TickType_t t)
{
  uint32_t ulNotifiedValue;

  xTaskNotifyWait( 0x00,           // Don't clear any notification bits on entry. 
                   ULONG_MAX,        // Reset the notification value to 0 on exit. 
                   &ulNotifiedValue, // Notified value pass out in ulNotifiedValue. 
                   t );               // Block indefinitely. 

  if( ( ulNotifiedValue & 0x01 ) != 0 ){running = true;}
  else if( ( ulNotifiedValue & 0x02 ) != 0 ){running = false;}
}

void start_rainbow(void *pvParameters)
{
  led_strip.access_semaphore = xSemaphoreCreateBinary();
  bool led_init_ok = led_strip_init(&led_strip);
  assert(led_init_ok);
  
  while(true){
    if(running){
      for (uint32_t index = 0; index < LED_STRIP_LENGTH; index++) {
        led_strip_set_pixel_color(&led_strip, index, &led_color);
      }
      led_strip_show(&led_strip);

      led_color.red   = lights[rgb_red];
      led_color.green = lights[rgb_green];
      led_color.blue  = lights[rgb_blue];

      rgb_red += 1;
      rgb_green += 1;
      rgb_blue += 1;

      //reset to 0 after 360 degr
      if (rgb_red >= 360) rgb_red=0;
      if (rgb_green >= 360) rgb_green=0;
      if (rgb_blue >= 360) rgb_blue=0;
    
      vTaskDelay(60 / portTICK_PERIOD_MS);
      check_notify(1);
    }
    else{
      led_color.red   = 0;
      led_color.green = 0;
      led_color.blue  = 0;
      for (uint32_t index = 0; index < LED_STRIP_LENGTH; index++) {
        led_strip_set_pixel_color(&led_strip, index, &led_color);
      }
      led_strip_show(&led_strip);

      vTaskDelay(60 / portTICK_PERIOD_MS);
      assert(led_strip_clear(&led_strip));
      check_notify(portMAX_DELAY);
    }
  }
}



