#include "config.h"

#include <limits.h>
#include <stdint.h>
#include <unistd.h>

#define SAP_IMPLEMENTATION
#include "sap.h"

#define MAX_PORT_NUMBER 65535

#define DIE(s, ...) do { \
        fprintf(stderr, (s), __VA_ARGS__); \
        exit(1); \
    } while (0)

static SapOption opts[] = {
    {"threads", 't', "number of threads to use", SAP_INT, 0},
    {"port", 'p', "port number", SAP_INT, 0},
};

HttppoConfig httppo_config_parse(int argc, char** argv) {
    HttppoConfig config;

    SapParser parser = {.options = opts, .options_count = sizeof(opts) / sizeof(opts[0])};
    int status = sap_parse(&parser, argc, argv);
    if (status != 0) {
        DIE("argument parser error: %s", parser.err);
    }

    SapOption* topt = sap_get_short(&parser, 't');
    uintptr_t nthreads = (uintptr_t)topt->value;
    if (nthreads == 0) {
        DIE("cannot start the server with %lu threads", nthreads);
    }
    config.threads = nthreads;
    int nproc = sysconf(_SC_NPROCESSORS_CONF);

    if (!topt->parsed || config.threads > nproc) {
        config.threads = nproc;
    }

    SapOption* popt = sap_get_short(&parser, 'p');
    config.port = (uintptr_t)popt->value;

    if (!popt->parsed) {
        config.port = HTTPPO_DEFAULT_PORTI;
    }

    if (config.port > MAX_PORT_NUMBER) {
        DIE("the port number '%d' is not valid", config.port);
    }

    return config;
}
