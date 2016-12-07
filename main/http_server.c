#include <string.h>
#include <stdlib.h>     /* atoi */
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

static const char header[] = "HTTP/1.1 200 OK\r\n\
                              Content-type: text/html; charset=UTF-8\r\n\
                              Content-Encoding: UTF-8\r\n\
                              Connection: close\r\n\r\n";

static char error[] = "HTTP/1.1 501 Not Implemented\r\n\r\n";

TaskHandle_t led_task_handle = NULL;

enum reqtype {GET, POST};

struct http_request {
    enum reqtype type;
    char document[16];
    u16_t request_length;
    u16_t content_length;
    char content[128];
};

/*
  parses and handles the http request
*/
struct http_request *parse_request(struct netconn *conn, struct netbuf * buf){
  struct http_request *request;
  request=(struct http_request*)malloc(sizeof(struct http_request));

  //set request size
  request->request_length = netbuf_len ( buf ); 

  //raw request data
  char data[request->request_length+1];
  netbuf_copy (buf, data, request->request_length);

  //check type
  if (strncmp(data, "GET", strlen("GET")) == 0){
    request->type=GET;
    ESP_LOGI(TAG, "GET");

    //set document path
    char * delimiter=strchr(data+4,' ');
    memcpy(request->document, data+4, delimiter-(data+4));
    request->document[delimiter-(data+4)]='\0';
    ESP_LOGI(TAG, "Document: %s|", request->document);

  }
  else if (strncmp(data, "POST", strlen("POST")) == 0){
    request->type=POST;
    ESP_LOGI(TAG, "POST");

    //set document path
    char * delimiter=strchr(data+5,' ');
    memcpy(request->document, data+5, delimiter-(data+5));
    request->document[delimiter-(data+5)]='\0';
    ESP_LOGI(TAG, "Document: %s|", request->document);
    
    //check content length
    char key[]="Content-Length: "; 
    char *clen_Str =strstr(data, key);
    if(clen_Str!=NULL){
      request->content_length = atoi (clen_Str+16);
      ESP_LOGI(TAG, "Content-Length: %d|", request->content_length);
    }

    //set content
    strcpy(request->content, data+(request->request_length-request->content_length));
    request->content[request->content_length]='\0';
    ESP_LOGI(TAG, "Content: %s|", request->content);

  }

  ESP_LOGI(TAG, "Receive:\r\n%s|", data);

  //cleanup and return
  netbuf_delete (buf);

  return request;
}


/*

*/
void http_server_netconn_serve(struct netconn *conn){

	const unsigned int index_html_bytes = index_html_end - index_html_start;  

  struct netbuf  * buf;
  struct netbuf  * nextBuf;
  struct http_request request;

  netconn_set_recvtimeout(conn, 2);

  if ( netconn_recv ( conn, &buf ) == ERR_OK )
  {    
    //appending request to ensure post req are detecte4d correctly
    if ( netconn_recv ( conn, &nextBuf) == ERR_OK ){
      ESP_LOGI(TAG, "append next buffer\r\n");
      netbuf_chain (buf, nextBuf);
    }

    //parse the request
    request = *parse_request(conn, buf);
  }

  if(request.type==GET){
    ESP_LOGI(TAG, "GET RETURN");
    if (strncmp(request.document, "/favicon.ico", strlen("/favicon.ico")) == 0){
      goto return_error;
    }
    else if (strncmp(request.document, "/", strlen("/")) == 0){
      goto return_page;
    }
    goto return_error;
  }
  else if (request.type==POST){
    ESP_LOGI(TAG, "POST RETURN");
    if (strncmp(request.document, "/", strlen("/")) == 0){
      ESP_LOGI(TAG, "Directory /");
      if (strncmp(request.content, "power=on", strlen("power=on")) == 0){
        ESP_LOGI(TAG, "Led on");
        if(led_task_handle==NULL){
          ESP_LOGI(TAG, "start led task");
          xTaskCreate(&start_rainbow, "start_rainbow", 2048, NULL, 5, &led_task_handle);
        }
        xTaskNotify( led_task_handle, 0x01, eSetValueWithOverwrite );
      }
      else if (strncmp(request.content, "power=off", strlen("power=off")) == 0){
        ESP_LOGI(TAG, "Led off");
        xTaskNotify( led_task_handle, 0x02, eSetValueWithOverwrite );
      }
      goto return_page;
    }
  }

return_page:
  netconn_write(conn, header, sizeof(header)-1, NETCONN_NOCOPY);
  netconn_write(conn, index_html_start, index_html_bytes-1, NETCONN_NOCOPY);
  netconn_close(conn);
return_error:
  netconn_write(conn, error, sizeof(error)-1, NETCONN_NOCOPY);
  netconn_close(conn);
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
