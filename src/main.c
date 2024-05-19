#include <assert.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "thread_pool.h"

#define BASE_IMPLEMENTATION
#include "base.h"

#define PORT "6969"

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

const char* status_str(HttpStatusCode status_code) {
    switch (status_code) {
        case STATUS_OK:
            return "OK";
        case STATUS_NOT_FOUND:
            return "Not found";
        case STATUS_BAD_REQUEST:
            return "Bad request";
    }
}

void http_req_headers_print(HttpRequestHeaders const* headers) {
    printf("method: %s, path: %s, version: %s\n", headers->method, headers->path,
           headers->http_version);
    printf("headers:\n");
    HT_ITER(&headers->headers, { printf("%s:%s\n", (const char*)kv.key, (const char*)kv.value); });
}

void http_req_print(HttpRequest const* req) {
    http_req_headers_print(&req->headers);
    printf("body: %s\n", req->body);
}

void http_req_headers_free(HttpRequestHeaders* headers) {
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

int http_res_headers_encode(HttpResponse const* res, char* buf) {
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
}

// TODO: use a better hash function
uint64_t hash_func(void const* ptr) {
    const char* str = (const char*)ptr;
    uint64_t sum = 0;
    while (*str) {
        sum += *str;
        str++;
    }
    return sum;
}

bool eq_func(void const* lhs, void const* rhs) {
    return strcmp((const char*)lhs, (const char*)rhs) == 0;
}

HttpRequestHeadersParseError http_req_headers_parse_error;

HttpRequestHeaders http_req_headers_parse(string_view string) {
    HttpRequestHeaders result;

    result.headers = ht_make(hash_func, eq_func, 10);
    ssize_t split_idx = sv_find_sub_cstr(string, "\r\n");
    assert(split_idx != -1);

    size_t i = split_idx + 2;

    string_view first_line = sv_slice(string, 0, split_idx);
    split_idx = sv_find(first_line, ' ');
    assert(split_idx != -1);

    result.method = sv_dup(sv_slice(first_line, 0, split_idx));
    first_line = sv_slice_end(first_line, split_idx + 1);

    split_idx = sv_find(first_line, ' ');
    assert(split_idx != -1);

    result.path = sv_dup(sv_slice(first_line, 0, split_idx));
    first_line = sv_slice_end(first_line, split_idx + 1);

    result.http_version = sv_dup(first_line);

    size_t orig_len = string.size;
    string = sv_slice_end(string, i);

    while (i < orig_len) {
        size_t line_end = sv_find_sub_cstr(string, "\r\n");
        size_t line_len = line_end == -1 ? string.size : line_end;

        string_view line = sv_slice(string, 0, line_len);
        size_t kv_sep_idx = sv_find(line, ':');
        assert(kv_sep_idx != -1);

        string_view key = sv_slice(line, 0, kv_sep_idx);
        string_view value = sv_slice_end(line, kv_sep_idx + 1);

        ht_add(&result.headers, (void*)sv_dup(key), (void*)sv_dup(value));

        i += line_len + 2;
        string = sv_slice_end(string, line_end + 2);
    }

    return result;
}

HttpRequest* http_req_parse(string_view string) {
    ssize_t split_idx = sv_find_sub_cstr(string, "\r\n\r\n");
    assert(split_idx != -1);

    string_view header_string = sv_slice(string, 0, split_idx);
    HttpRequestHeaders headers = http_req_headers_parse(header_string);
    if (http_req_headers_parse_error != HRHPE_NONE) {
        return NULL;
    }

    HttpRequest* result = malloc(sizeof(HttpRequest));
    result->headers = headers;

    string_view body_sv =
        sv_slice_end(string, split_idx + 4);  // NOTE: always add 4 to skip the double \r\n
    result->body = sv_dup(body_sv);

    return result;
}

void* server_worker(void* socket) {
    int sock = (int)(uintptr_t)socket;

    char msg_buf[1024];
    int nread = recv(sock, msg_buf, sizeof(msg_buf), 0);
    assert(nread != -1 && nread != 1024);
    msg_buf[nread] = '\0';

    printf("got message from client: %s\n", msg_buf);

    HttpRequest* req = http_req_parse(sv_make(msg_buf, nread));
    assert(req);

    http_req_print(req);
    http_req_free(req);

    HttpResponse res = http_res_new(STATUS_OK, NULL, ht_make(NULL, NULL, 0));
    const char* res_str = http_res_encode(&res);
    assert(send(sock, res_str, strlen(res_str), 0) != -1);

    printf("sending response: %s\n", res_str);

    close(sock);
    free((void*)res_str);
    http_res_free(&res);

    return NULL;
}

void server(ThreadPool* thread_pool) {
    struct addrinfo hints = {0};
    struct addrinfo* server_addr;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int status = getaddrinfo(NULL, PORT, &hints, &server_addr);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        exit(1);
    }

    int sock = socket(server_addr->ai_family, server_addr->ai_socktype, 0);
    assert(sock != -1);
    assert(bind(sock, server_addr->ai_addr, server_addr->ai_addrlen) != -1);
    assert(listen(sock, 5) != -1);

    struct sockaddr_storage client_addr;
    socklen_t client_addr_size = sizeof(client_addr);

    while (true) {
        int client_sock = accept(sock, (struct sockaddr*)&client_addr, &client_addr_size);
        assert(client_sock != -1);

        printf("accepted\n");

        threadpool_schedule(thread_pool, server_worker, (void*)(uintptr_t)client_sock);
    }

    close(sock);
    freeaddrinfo(server_addr);
}

void client() {
    struct addrinfo hints = {0};
    struct addrinfo* client_addr;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int status = getaddrinfo(NULL, PORT, &hints, &client_addr);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        exit(1);
    }

    int sock = socket(client_addr->ai_family, client_addr->ai_socktype, 0);
    assert(sock != -1);

    assert(connect(sock, client_addr->ai_addr, client_addr->ai_addrlen) != -1);

    const char msg[] = "hello!";
    int nsent = send(sock, msg, sizeof(msg), 0);
    assert(nsent != -1);

    close(sock);
    freeaddrinfo(client_addr);
}

int main(int argc, char* argv[]) {
    ThreadPool thread_pool = threadpool_init(4);

    if (argc != 2) {
        fprintf(stderr, "wrong number of arguments\n");
        return 1;
    }

    if (strcmp(argv[1], "server") == 0) {
        server(&thread_pool);
        threadpool_free(&thread_pool);
    } else if (strcmp(argv[1], "client") == 0) {
        client();
    } else {
        fprintf(stderr, "nah\n");
        return 1;
    }

    return 0;
}
