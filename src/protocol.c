#include "protocol.h"

#include <assert.h>
#include <stdio.h>

static void http_req_headers_print(HttpRequestHeaders const* headers) {
    printf("method: %s, path: %s, version: %s\n", headers->method, headers->path,
           headers->http_version);
    printf("headers:\n");
    HT_ITER(&headers->headers, { printf("%s:%s\n", (const char*)kv.key, (const char*)kv.value); });
}

void http_req_print(HttpRequest const* req) {
    http_req_headers_print(&req->headers);
    printf("body: %s\n", req->body);
}

static void http_req_headers_free(HttpRequestHeaders* headers) {
    free((void*)headers->method);
    free((void*)headers->path);
    free((void*)headers->http_version);

    HT_ITER(&headers->headers, {
        free(kv.key);
        free(kv.value);
    });

    ht_destroy(&headers->headers);
}

void http_req_free(HttpRequest* req) {
    http_req_headers_free(&req->headers);
    free((void*)req->body);
    free(req);
}

HttpResponse http_res_new(HttpStatusCode status_code, const char* body, hash_table headers) {
    return (HttpResponse){
        .body = body, .status_code = status_code, .headers = headers, .http_version = "HTTP/1.1"};
}

static int http_res_headers_encode(HttpResponse const* res, char* buf) {
    int written = 0;
    HT_ITER(&res->headers, {
        written += sprintf(buf + written, "%s: %s\r\n", (const char*)kv.key, (const char*)kv.value);
    });

    written += sprintf(buf + written, "\r\n");

    return written;
}

char* http_res_encode(HttpResponse const* res) {
    size_t body_len = res->body ? strlen(res->body) : 0;
    size_t size = body_len + 30 + res->headers.len * 30;

    char* buf = malloc(size * sizeof(char) + 1);
    int num_written = sprintf(buf, "%s %d %s\r\n", res->http_version, res->status_code,
                              status_str(res->status_code));
    assert(num_written != -1);
    num_written += http_res_headers_encode(res, buf + num_written);

    if (res->body) {
        sprintf(buf + num_written, "%s", res->body);
    }

    return buf;
}

void http_res_free(HttpResponse* res) {
    ht_destroy(&res->headers);
    if (res->body) {
        free((void*)res->body);
    }
}

uint64_t hash_func(void const* ptr) {
    const char* str = (const char*)ptr;
    uint64_t hash = 5381;
    char c;

    while ((c = *str++)) hash = ((hash << 5) + hash) + c;

    return hash;
}

static bool eq_func(void const* lhs, void const* rhs) {
    return strcmp((const char*)lhs, (const char*)rhs) == 0;
}

thread_local HttpRequestParseError http_req_parse_error;

static HttpRequestHeaders http_req_headers_parse(string_view string) {
    HttpRequestHeaders result;

    result.headers = ht_make(hash_func, eq_func, 10);
    ssize_t split_idx = sv_find_sub_cstr(string, "\r\n");
    if (split_idx == -1) goto fail;

    size_t i = split_idx + 2;

    string_view first_line = sv_slice(string, 0, split_idx);
    split_idx = sv_find(first_line, ' ');
    if (split_idx == -1) goto fail;

    result.method = sv_dup(sv_slice(first_line, 0, split_idx));
    first_line = sv_slice_end(first_line, split_idx + 1);

    split_idx = sv_find(first_line, ' ');
    if (split_idx == -1) goto fail;

    result.path = sv_dup(sv_slice(first_line, 0, split_idx));
    first_line = sv_slice_end(first_line, split_idx + 1);

    result.http_version = sv_dup(first_line);

    size_t orig_len = string.size;
    string = sv_slice_end(string, i);

    while (i < orig_len) {
        ssize_t line_end = sv_find_sub_cstr(string, "\r\n");
        size_t line_len = line_end == -1 ? string.size : (size_t)line_end;

        string_view line = sv_slice(string, 0, line_len);
        ssize_t kv_sep_idx = sv_find(line, ':');
        if (kv_sep_idx == -1) goto fail;

        string_view key = sv_slice(line, 0, kv_sep_idx);
        string_view value = sv_slice_end(line, kv_sep_idx + 1);

        ht_add(&result.headers, (void*)sv_dup(key), (void*)sv_dup(value));

        i += line_len + 2;
        string = sv_slice_end(string, line_end + 2);
    }

    return result;

fail:
    http_req_parse_error = HTTP_ERR_MALFORMED_HEADERS;
    return (HttpRequestHeaders){0};
}

HttpRequest* http_req_parse(string_view string) {
    ssize_t split_idx = sv_find_sub_cstr(string, "\r\n\r\n");
    if (split_idx == -1) {
        http_req_parse_error = HTTP_ERR_MALFORMED_BODY;
        return NULL;
    }

    string_view header_string = sv_slice(string, 0, split_idx);
    HttpRequestHeaders headers = http_req_headers_parse(header_string);
    if (http_req_parse_error != HTTP_ERR_NONE) {
        return NULL;
    }

    HttpRequest* result = malloc(sizeof(HttpRequest));
    result->headers = headers;

    string_view body_sv =
        sv_slice_end(string, split_idx + 4);  // NOTE: always add 4 to skip the double \r\n
    result->body = sv_dup(body_sv);

    return result;
}
