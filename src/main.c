#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// NOTE: ORDER MATTERS HERE!
#define BASE_IMPLEMENTATION
#include "base.h"

#define ARENA_H_IMPLEMENTATION
#include "arena.h"
#include "config.h"
#include "files.h"
#include "protocol.h"
#include "thread_pool.h"
#include "util.h"

#define HTML_INDEX_FILE "index.html"
#define HTTPPO_FILES_CAP 23

#define TCP_BACKLOG_SIZE 256

// shared state
static HttppoFiles files;

// thread-local state
static thread_local Arena arena;
static thread_local string_builder sb;
static thread_local bool is_state_init = false;

void* server_worker(void* socket) {
    if (!is_state_init) {
        sb = sb_new(1024);
        is_state_init = true;
    }

    int sock = (int)(uintptr_t)socket;

    char msg_buf[1024];

    int nread = recv(sock, msg_buf, sizeof(msg_buf), 0);
    if (nread == -1) {
        fprintf(stderr, "ERROR: could not read from socket %d: %s\n", sock, strerror(errno));
        exit(1);
    }

    assert(nread != 1024);
    msg_buf[nread] = '\0';

    HttpRequest* req = http_req_parse(sv_make(msg_buf, nread), &arena);
    assert(req);

    http_req_print(req);

    HttppoFile* file = NULL;
    if (strcmp(req->headers.path, "/") == 0) {
        file = httppo_files_get(&files, HTML_INDEX_FILE);
    } else {
        file = httppo_files_get(&files, req->headers.path + 1);
    }

    const char* res_body = file ? file->contents : NULL;
    HttpStatusCode status_code = file ? STATUS_OK : STATUS_NOT_FOUND;
    HttpResponse res = http_res_new(status_code, res_body, ht_make(NULL, NULL, 0));
    http_res_encode_sb(&res, &sb);
    const char* res_str = sb_to_cstr(&sb);
    assert(send(sock, res_str, strlen(res_str), 0) != -1);

    if (close(sock) == -1) {
        fprintf(stderr, "ERROR: could not close socket %d: %s\n", sock, strerror(errno));
        exit(1);
    }

    arena_free(&arena);
    sb_clear(&sb);
    http_res_free(&res);
    http_req_free(req);

    return NULL;
}

static int server_sock = -1;

void server(ThreadPool* thread_pool, const char* port) {
    struct addrinfo hints = {0};
    struct addrinfo* server_addr;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int status = getaddrinfo(NULL, port, &hints, &server_addr);
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        exit(1);
    }

    server_sock = socket(server_addr->ai_family, server_addr->ai_socktype, 0);
    if (server_sock == -1) {
        die("call to `socket` failed");
    }

    int opt_value = 1;
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt_value, sizeof(opt_value)) == -1) {
        die("failed to set socket options");
    }

    if (bind(server_sock, server_addr->ai_addr, server_addr->ai_addrlen) == -1) {
        die("call to `bind` failed");
    }

    if (listen(server_sock, TCP_BACKLOG_SIZE) == -1) {
        die("call to `listen` failed");
    }

    struct sockaddr_storage client_addr;
    socklen_t client_addr_size = sizeof(client_addr);

    while (true) {
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_addr_size);
        if (client_sock == -1) {
            goto fail;
        }

        threadpool_schedule(thread_pool, server_worker, (void*)(uintptr_t)client_sock);
    }

fail:
    close(server_sock);
    die("could not accept the connection");
}

static void init_state(void) {
    files = httppo_files_new(HTTPPO_FILES_CAP);
}

int main(int argc, char* argv[]) {
    HttppoConfig config = httppo_config_parse(argc, argv);

    ThreadPool thread_pool = threadpool_init(config.threads);

    char port[6];
    sprintf(port, "%d", config.port);

    init_state();

    server(&thread_pool, port);
    threadpool_free(&thread_pool);

    return 0;
}
