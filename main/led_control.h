#ifndef LED_CONTROL_H
#define LED_CONTROL_H

SemaphoreHandle_t led_mutex;

void start_rainbow(void *pvParameters);

#endif 