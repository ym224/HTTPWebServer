#ifndef PROCESS_REQUEST_H
#define PROCESS_REQUEST_H


void get_content_type(char *file_ext, char *content_type);
int check_file_access(char *file_path, char *response);
void process_head(Request * request, char * response, char * resource_path, int * is_closed);
void process_get(Request * request, char * response, char * resource_path, int * is_closed);
void process_post(Request * request, char * response, char * resource_path, int * is_closed);
void process_http_request(Request * request, char * response, char * resource_path, int * is_closed);
void append_date_headers(Request *request, char *response);
#endif
