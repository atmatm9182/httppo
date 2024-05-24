#include "config.h"

#include <limits.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#define SAP_IMPLEMENTATION
#include "sap.h"

#define MAX_PORT_NUMBER 65535

static SapOption opts[] = {
    {"threads", 't', "number of threads to use", SAP_INT, 0},
    {"port", 'p', "port number", SAP_INT, 0},
};

int httppo_config_parse(int argc, char** argv, HttppoConfig* config) {
    memset(config, 0, sizeof(HttppoConfig));

    SapParser parser = {.options = opts, .options_count = sizeof(opts) / sizeof(opts[0])};
    int status = sap_parse(&parser, argc, argv);
    if (status != 0) {
        return status;
    }

    SapOption* topt = sap_get_short(&parser, 't');
    config->threads = (uintptr_t)topt->value;
    int nproc = sysconf(_SC_NPROCESSORS_CONF);

    if (!topt->parsed || config->threads > nproc) {
        config->threads = nproc;
    }

    SapOption* popt = sap_get_short(&parser, 'p');
    config->port = (uintptr_t)popt->value;

    if (!popt->parsed) {
        config->port = HTTPPO_DEFAULT_PORTI;
    }

    if (config->port > MAX_PORT_NUMBER) {
        return 1;  // the port number is invalid
    }

    return 0;
}
