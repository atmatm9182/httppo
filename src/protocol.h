#pragma once

#include "base.h"

typedef struct {
    const char* method;
    const char* path;
    const char* http_version;
    hash_table headers;
} HttpRequestHeaders;

typedef struct {
    HttpRequestHeaders headers;
    const char* body;
} HttpRequest;

typedef enum {
    STATUS_OK = 200,
    STATUS_BAD_REQUEST = 400,
    STATUS_NOT_FOUND = 404,
} HttpStatusCode;

typedef struct {
    const char* http_version;
    const char* body;
    HttpStatusCode status_code;
    hash_table headers;
} HttpResponse;

typedef enum { HRHPE_NONE } HttpRequestHeadersParseError;

static inline const char* status_str(HttpStatusCode status_code) {
    switch (status_code) {
        case STATUS_OK:
            return "OK";
        case STATUS_NOT_FOUND:
            return "Not found";
        case STATUS_BAD_REQUEST:
            return "Bad request";
        default:
            return NULL;
    }
}

HttpRequest* http_req_parse(string_view sv);
void http_req_print(HttpRequest const* req);
void http_req_free(HttpRequest* req);

HttpResponse http_res_new(HttpStatusCode status_code, const char* body, hash_table headers);
char* http_res_encode(HttpResponse const* res);
void http_res_free(HttpResponse* res);
