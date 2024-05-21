#include <assert.h>
#include <netdb.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

// NOTE: ORDER MATTERS HERE!
#define BASE_IMPLEMENTATION
#include "base.h"
#include "protocol.h"
#include "thread_pool.h"

#define PORT "6969"

#define HTML_INDEX_FILE "index.html"

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

    const char* body = NULL;
    if (strcmp(req->headers.path, "/") == 0) {
        body = base_read_whole_file(HTML_INDEX_FILE);
    } else {
        body = base_read_whole_file(req->headers.path + 1);
    }

    HttpStatusCode status_code = body ? STATUS_OK : STATUS_NOT_FOUND;

    HttpResponse res = http_res_new(status_code, body, ht_make(NULL, NULL, 0));
    const char* res_str = http_res_encode(&res);
    assert(send(sock, res_str, strlen(res_str), 0) != -1);

    printf("sending response: %s\n", res_str);

    close(sock);
    free((void*)res_str);
    http_res_free(&res);
    http_req_free(req);

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

int main(int argc, char* argv[]) {
    int nprocs = sysconf(_SC_NPROCESSORS_CONF);
    ThreadPool thread_pool = threadpool_init(nprocs);

    server(&thread_pool);
    threadpool_free(&thread_pool);

    return 0;
}
