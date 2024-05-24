#ifndef SAP_INCLUDE_
#define SAP_INCLUDE_

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define SAP_MAX_OPT_COUNT 32
#define SAP_MAX_ARGV_COUNT 32

#include <stdio.h>

typedef enum {
    SAP_INT,
    SAP_BOOL,
    SAP_STRING,
    SAP_FLOAT,
} SapOptionType;

typedef struct {
    const char* long_name;
    char short_name;
    const char* description;
    SapOptionType type;
    int required;
    void* value;
    int parsed;
} SapOption;

typedef struct {
    SapOption* options;
    int options_count;
    const char* argv[SAP_MAX_ARGV_COUNT];
    int argv_count;
} SapParser;

SapOption* sap_get_long(SapParser const* parser, const char* name);
SapOption* sap_get_short(SapParser const* parser, char name);

int sap_parse(SapParser* parser, int argc, char* argv[]);

#ifdef SAP_IMPLEMENTATION

int _sap_parse_for_type(const char* arg, SapOptionType type, void** value) {
    switch (type) {
        case SAP_BOOL:
            if (strcmp(arg, "true")) {
                *value = (void*)1;
                return 0;
            }
            if (strcmp(arg, "false")) {
                *value = (void*)0;
                return 0;
            }

        return 1;
        case SAP_STRING:
        *value = (void*)arg;
        return 0;

        case SAP_INT: {
            char* endptr;
            long v = strtol(arg, &endptr, 0);
            if (endptr && *endptr == '\0') {
                *value = (void*)v;
                return 0;
            }

            return 1;
        }

        case SAP_FLOAT: {
            char* endptr;
            double v = strtod(arg, &endptr);
            if (endptr && *endptr == '\0') {
                *value = (void*)(uintptr_t)v;
                return 0;
            }

            return 1;
        }
    }
}

SapOption* sap_get_long(SapParser const* parser, const char* name) {
    for (int i = 0; i < parser->options_count; i++) {
        SapOption* opt = &parser->options[i];
        if (strcmp(opt->long_name, name) == 0) {
            return opt;
        }
    }

    return NULL;
}

SapOption* sap_get_short(SapParser const* parser, char name) {
    for (int i = 0; i < parser->options_count; i++) {
        SapOption* opt = &parser->options[i];
        if (opt->short_name == name) {
            return opt;
        }
    }

    return NULL;
}

int sap_parse(SapParser* parser, int argc, char* argv[]) {
    argc--;
    argv++;

    for (int i = 0; i < parser->options_count; i++) {
        parser->options[i].parsed = 0;
    }

    parser->argv_count = 0;

    for (int i = 0; i < argc; i++) {
        char* arg = argv[i];

        if (strlen(arg) == 0) {
            continue;
        }

        if (arg[0] == '-') {
            while (arg[0] == '-') {
                arg++;
            }

            int j;

            for (j = 0; j < parser->options_count; j++) {
                SapOption* raw = &parser->options[j];

                int is_short = strlen(arg) == 1 && arg[0] == raw->short_name;

                if (is_short || strcmp(raw->long_name, arg) == 0) {
                    if (raw->type == SAP_BOOL) {
                        raw->value = (void*)1;
                        raw->parsed = 1;
                        break;
                    }

                    if (++i == argc) return 1; // no value for the flag

                    arg = argv[i];

                    if (_sap_parse_for_type(arg, raw->type, &raw->value) != 0) {
                        return 2; // could not parse the value
                    }

                    raw->parsed = 1;
                    break;
                }
            }

            if (j >= parser->options_count) {
                return 3; // unrecognized option
            }

            continue;
        }

        parser->argv[parser->argv_count++] = arg;
    }

    for (int k = 0; k < parser->options_count; k++) {
        SapOption* raw = &parser->options[k];

        if (raw->type == SAP_BOOL && !raw->parsed) {
            raw->value = (void*)(uintptr_t)0;
            raw->parsed = 0;
            continue;
        }

        if (raw->required && !raw->parsed) {
            return 4; // required option not present
        }
    }

    return 0;
}

#endif // SAP_IMPLEMENTATION

#endif // SAP_INCLUDE_