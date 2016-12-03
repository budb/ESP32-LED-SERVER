#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "soc/dport_reg.h"
#include "soc/soc.h"
#include "soc/ledc_reg.h"
#include "soc/gpio_reg.h"
#include "driver/gpio.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "lwip/opt.h"
#include "lwip/arch.h"
#include "lwip/api.h"

#include "led_control.h"
#include "http_server.h"

static const char *TAG = "http_server";

static const char header[] = "HTTP/1.1 200 OK\r\nContent-type: text/html\r\n\r\n";

static char error[] = "HTTP/1.1 501 Not Implemented\r\n\r\n";

TaskHandle_t led_task_handle = NULL;


void http_server_netconn_serve(struct netconn *conn){
	struct netbuf *inbuf;
  	char *buf;
  	u16_t buflen;
  	err_t err;

  	char *reqString = NULL;


	const unsigned int index_html_bytes = index_html_end - index_html_start;  


  	// Read the data from the port, blocking if nothing yet there. 
  	// We assume the request (the part we care about) is in one netbuf 
  	err = netconn_recv(conn, &inbuf);
  

  	if (err == ERR_OK) 
  	{
    	netbuf_data(inbuf, (void**)&buf, &buflen);
    	buflen = ( buflen > 768 ) ? 768 : buflen;

    	reqString = malloc(sizeof(char)*buflen);

    	strncpy( reqString, buf, buflen );
    	ESP_LOGI(TAG, "request %s", reqString);

    if (strncmp( reqString, "GET / HTTP/1.1", strlen("GET / HTTP/1.1")) == 0 )
    {
     	ESP_LOGI(TAG, "GET ");
      	netconn_write(conn, header, sizeof(header)-1, NETCONN_NOCOPY);
      	netconn_write(conn, index_html_start, index_html_bytes, NETCONN_NOCOPY);
    }
    if (strncmp( reqString, "GET /favicon.ico", strlen("GET /favicon.ico")) == 0 )
    {
      	ESP_LOGI(TAG, "GET /favicon.ico");
      	netconn_write(conn, error, sizeof(error)-1, NETCONN_NOCOPY);
    }
    if (strncmp( reqString, "POST /mode", strlen("POST /mode")) == 0 )
    {
      	ESP_LOGI(TAG, "POST /mode");
      	netconn_write(conn, header, sizeof(header)-1, NETCONN_NOCOPY);
      	if(led_task_handle == NULL){

        //task control mutex
        led_mutex = xSemaphoreCreateMutex();

        ESP_LOGI(TAG, "start led task");
        xTaskCreate(&start_rainbow, "start_rainbow", 2048, NULL, 5, &led_task_handle);
      	}
      	else if(led_task_handle != NULL){
        ESP_LOGI(TAG, "delete task");

        if( xSemaphoreTake(led_mutex, ( TickType_t ) 20 ) == pdTRUE )
        {
            ESP_LOGI(TAG, "stop task");
        }
        xSemaphoreGive(led_mutex);
        led_task_handle = NULL;
      }
    }
    else{
      ESP_LOGI(TAG, "wrong request");
    }
  }
  else{
      ESP_LOGI(TAG, "error");
  }

  // Close the connection (server closes in HTTP) 
  netconn_close(conn);
  
  // Delete the buffer (netconn_recv gives us ownership,
  // so we have to make sure to deallocate the buffer) 
  netbuf_delete(inbuf);
}

void http_server_task(void *pvParameters){
	ESP_LOGI(TAG, "Start Task");
struct netconn *conn, *newconn;
  err_t err;
  
  // Create a new TCP connection handle 
  conn = netconn_new(NETCONN_TCP);

  // Bind to port 80 (HTTP) with default IP address 
  netconn_bind(conn, NULL, 80);
  
  // Put the connection into LISTEN state 
  netconn_listen(conn);
  
  do {
    err = netconn_accept(conn, &newconn);
    if (err == ERR_OK) {
      http_server_netconn_serve(newconn);
      netconn_delete(newconn);
    }
  } while(err == ERR_OK);

  netconn_close(conn);
}
