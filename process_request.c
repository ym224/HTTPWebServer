#define BUF_SIZE 4096
#define MAX_FILE_BUF_SIZE 100000

#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "parse.h"
#include "process_request.h"
#include "log.h"

const char* HTTP_VERSION = "HTTP/1.1 ";
const char* STATUS_200 = "200 OK\r\n";
const char* STATUS_204 = "204 NO CONTENT\r\n";
const char* STATUS_400 = "400 BAD REQUEST\r\n";
const char* STATUS_404 = "404 NOT FOUND\r\n";
const char* STATUS_405 = "405 METHOD NOT ALLOWED\r\n";
const char* STATUS_411 = "411 Length REQUIRED\r\n";
const char* STATUS_415 = "415 UNSUPPORTED MEDIA TYPE\r\n";
const char* STATUS_500 = "500 INTERNAL SERVER ERROR\r\n";
const char* STATUS_501 = "501 NOT IMPLEMENTED\r\n";
const char* STATUS_505 = "505 HTTP VERSION NOT SUPPORTED\r\n";

void get_content_type(char *file_ext, char *content_type) {
    if (strstr(file_ext, ".html")) {
        strcpy(content_type, "text/html");
    }
    else if (strstr(file_ext, ".css")) {
        strcpy(content_type, "text/css");
    }
    else if (strstr(file_ext, ".png")) {
        strcpy(content_type, "image/png");
    }
    else if (strstr(file_ext, ".jpeg")) {
        strcpy(content_type, "image/jpeg");
    }
    else if (strstr(file_ext, ".gif")) {
        strcpy(content_type, "image/gif");
    }
    else {
        strcpy(content_type, "application/octet-stream");
    }
}

int check_file_access(char *file_path, char *response) {
    if (access(file_path, F_OK) == -1) {
        //log_write("cannot access file at %s\n", file_path);
        strcat(response, HTTP_VERSION);
        strcat(response, STATUS_404);
        strcat(response, "\r\n");
        return -1;
    }
    // open uri w readyonly
    int file = open(file_path, O_RDONLY);
    if (file == -1) {
        //log_write("can't open file at %s\n", file_path);
        strcat(response, HTTP_VERSION);
        strcat(response, STATUS_500);
        strcat(response, "\r\n");
    }
    return file;
}

void process_head(Request * request, char * response, char * resource_path, int * is_closed){
    char file_path[BUF_SIZE], content_type[BUF_SIZE];
    size_t content_length;

    // get request uri to get location of file
    strcat(file_path, resource_path);
    strcat(file_path, request->http_uri);

    int file = check_file_access(file_path, response);
    if (file < 0) {
        return;
    }

    char nbytes[MAX_FILE_BUF_SIZE];
    // get content length from reading file
    content_length = read(file, nbytes, sizeof(nbytes));

    // get content type based on uri
    get_content_type(request->http_uri, content_type);

    memset(response, '\0', sizeof(response));
    // construct response
    strcat(response, HTTP_VERSION);
    strcat(response, STATUS_200);

    sprintf(response, "%sserver: Liso/1.0\r\n", response);
    sprintf(response, "%scontent-length: %ld\r\n", response, content_length);
    sprintf(response, "%scontent-type: %s\r\n", response, content_type);
    sprintf(response, "%sconnection: keep-alive\r\n", response);
    append_date_headers(request, response);

    strcat(response, "\r\n");

    memset(file_path, 0, BUF_SIZE);
    memset(nbytes, 0, MAX_FILE_BUF_SIZE);
    memset(content_type, 0, BUF_SIZE);
}

// processing get and sending response separately because not sure how to reallocate more memory to response ;-((((
int process_get(Request * request, int clientFd, char * resource_path, int * is_closed){
    char file_path[BUF_SIZE], content_type[BUF_SIZE];
    size_t content_length;
    struct stat sb;

    memset(file_path, 0, sizeof(file_path));
    // get request uri to get location of file
    strcat(file_path, resource_path);
    strcat(file_path, request->http_uri);

    char * error_response[RESPONSE_DEFAULT_SIZE];
    int file = check_file_access(file_path, error_response);
    if (file < 0) {
        return send_client_response(clientFd, error_response);
    }

    FILE *f = fopen(file_path, "rb");
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *body = malloc(fsize + 1);

    // get content type based on uri
    get_content_type(request->http_uri, content_type);

    fread(body, fsize, 1, f);
    fclose(f);

    // get content type based on uri
    get_content_type(request->http_uri, content_type);

    char * response = malloc(2*fsize);

    // construct response
    memset(response, '\0', sizeof(response));

    strcat(response, HTTP_VERSION);
    strcat(response, STATUS_200);
    sprintf(response, "%sserver: Liso/1.0\r\n", response);
    sprintf(response, "%scontent-length: %ld\r\n", response, fsize);
    sprintf(response, "%scontent-type: %s\r\n", response, content_type);
    sprintf(response, "%sconnection: keep-alive\r\n", response);
    append_date_headers(request, response);
    strcat(response, "\r\n");
    strcat(response, body);


    int success = send_client_response(clientFd, response);
    memset(file_path, 0, BUF_SIZE);
    memset(content_type, 0, BUF_SIZE);
    if (body) {
        free(body);
    }
    return success;
}

void process_post(Request * request, char * response, char * resource_path, int * is_closed){
    char file_path[BUF_SIZE];

    // get request uri to get location of file
    strcat(file_path, resource_path);
    strcat(file_path, request->http_uri);

    int file = check_file_access(file_path, response);
    if (file < 0) {
        return;
    }

    char * content_length;
    // get content-length from header
    for (int i=0; i< request->header_count; i++){
        if (strcmp(request->headers[i].header_name, "Content-Length") || strcmp(request->headers[i].header_name, "content-length")){
            content_length = request->headers[i].header_value;
        }
    }

    if (content_length == NULL) {
        // return 411
        strcat(response, HTTP_VERSION);
        strcat(response, STATUS_411);
        sprintf(response, "%sconnection: close\r\n", response);
        strcat(response, "\r\n");
        return;
    }

    memset(response, '\0', sizeof(response));
    strcat(response, HTTP_VERSION);
    strcat(response, STATUS_200);
    sprintf(response, "%sserver: Liso/1.0\r\n", response);
    sprintf(response, "%scontent-length: %s\r\n", response, content_length);
    sprintf(response, "%sconnection: keep-alive\r\n", response);
    append_date_headers(request, response);
    strcat(response, "\r\n");

    memset(file_path, 0, BUF_SIZE);
}

int process_http_request(Request * request, int clientFd, char * resource_path, int * is_closed) {
    char * response = malloc(RESPONSE_DEFAULT_SIZE);
    if (request == NULL) {
        log_write("Bad Request. Request cannot be parsed!\n");
        *is_closed = 1;
        strcat(response, HTTP_VERSION);
        strcat(response, STATUS_400);
        sprintf(response, "%sconnection: close\r\n", response);
        strcat(response, "\r\n");
        return send_client_response(clientFd, response);
    }

    if (!strcmp(request->http_version, HTTP_VERSION)) {
        log_write("HTTP Version %s not supported.\n", request->http_version);
        *is_closed = 1;
        strcat(response, HTTP_VERSION);
        strcat(response, STATUS_505);
        sprintf(response, "%sconnection: close\r\n", response);
        strcat(response, "\r\n");
        return send_client_response(clientFd, response);
    }

    if (strcmp(request->http_method, "HEAD") == 0) {
        //log_write("Processing a HEAD request\n");
        process_head(request, response, resource_path, is_closed);
    }
    else if (strcmp(request->http_method, "GET") == 0) {
        //log_write("Processing a GET request\n");
        return process_get(request, clientFd, resource_path, is_closed);
    }
    else if (strcmp(request->http_method, "POST") == 0) {
        //log_write("Processing a POST request\n");
        process_post(request, response, resource_path, is_closed);
    }
    else {
        log_write("Requested http method %s is not implemented.\n",  request->http_method);
        *is_closed = 1;
        strcat(response, HTTP_VERSION);
        strcat(response, STATUS_501);
        sprintf(response, "%sserver: Liso/1.0\r\n", response);
        sprintf(response, "%sconnection: close\r\n", response);
        strcat(response, "\r\n");
    }
    return send_client_response(clientFd, response);
}

int send_client_response(int clientFd, char * response){
    if (send(clientFd, response, strlen(response), 0) < 0) {
        if (response) {
            free(response);
        }
        log_write("Error sending to client.\n");
        return -1;
    }
    if (response) {
        free(response);
    }
    return 0;
}

void append_date_headers(Request * request, char * response){
    struct tm tm;
    struct stat sbuf;
    time_t now;
    char tbuf[100], dbuf[100];
    stat(request->http_uri, &sbuf);
    tm = *gmtime(&sbuf.st_mtime); //st_mtime: time of last data modification.

    strftime(tbuf, 1000, "%a, %d %b %Y %H:%M:%S %Z", &tm);
    sprintf(response, "%slast-modified: %s\r\n", response, tbuf);

    now = time(0);
    tm = *gmtime(&now);
    strftime(dbuf, 100, "%a, %d %b %Y %H:%M:%S %Z", &tm);
    sprintf(response, "%sdate: %s\r\n", response, dbuf);
}

