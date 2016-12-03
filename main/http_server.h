#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

extern const unsigned char index_html_start[] asm("_binary_index_html_start");
extern const unsigned char index_html_end[]   asm("_binary_index_html_end");

void http_server_netconn_serve(struct netconn *conn);
void http_server_task(void *pvParameters);
#endif 
