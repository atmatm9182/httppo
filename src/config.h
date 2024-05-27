#pragma once

#define HTTPPO_DEFAULT_PORT "6969"
#define HTTPPO_DEFAULT_PORTI 6969

typedef struct {
    int threads;
    int port;
} HttppoConfig;

HttppoConfig httppo_config_parse(int argc, char** argv);
