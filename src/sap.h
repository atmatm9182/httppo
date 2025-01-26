#ifndef SAP_INCLUDE_
#define SAP_INCLUDE_

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "base.h"

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
    size_t options_count;
    const char* argv[SAP_MAX_ARGV_COUNT];
    size_t argv_count;
    char err[128];
} SapParser;

SapOption* sap_get_long(SapParser const* parser, const char* name);
SapOption* sap_get_short(SapParser const* parser, char name);

char* sap_generate_help_message(SapParser const* parser);

int sap_parse(SapParser* parser, int argc, char* argv[]);

#ifdef SAP_IMPLEMENTATION
#ifndef BASE_IMPLEMENTATION
#error \
    "You need to define both 'BASE_IMPLEMENTATION' and 'SAP_IMPLEMENTATION' to use the SAP implementation."
#endif  // BASE_IMPLEMENTATION

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

    default: {
        fprintf(stderr, "Unknown SAP option type '%d'\n", type);
        exit(1);
    }
    }
}

SapOption* sap_get_long(SapParser const* parser, const char* name) {
    for (size_t i = 0; i < parser->options_count; i++) {
        SapOption* opt = &parser->options[i];
        if (strcmp(opt->long_name, name) == 0) {
            return opt;
        }
    }

    return NULL;
}

SapOption* sap_get_short(SapParser const* parser, char name) {
    for (size_t i = 0; i < parser->options_count; i++) {
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

    for (size_t i = 0; i < parser->options_count; i++) {
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

            size_t j;

            for (j = 0; j < parser->options_count; j++) {
                SapOption* raw = &parser->options[j];

                int is_short = strlen(arg) == 1 && arg[0] == raw->short_name;

                if (is_short || strcmp(raw->long_name, arg) == 0) {
                    if (raw->type == SAP_BOOL) {
                        raw->value = (void*)1;
                        raw->parsed = 1;
                        break;
                    }

                    if (++i == argc) {
                        sprintf(parser->err, "no value provided for flag '%s'", raw->long_name);
                        return 1;
                    }

                    arg = argv[i];

                    if (_sap_parse_for_type(arg, raw->type, &raw->value) != 0) {
                        sprintf(parser->err, "wrong type of value for flag '%s'", raw->long_name);
                        return 1;
                    }

                    raw->parsed = 1;
                    break;
                }
            }

            if (j >= parser->options_count) {
                sprintf(parser->err, "unrecognized flag '%s'", arg);
                return 1;
            }

            continue;
        }

        parser->argv[parser->argv_count++] = arg;
    }

    for (size_t k = 0; k < parser->options_count; k++) {
        SapOption* raw = &parser->options[k];

        if (raw->type == SAP_BOOL && !raw->parsed) {
            raw->value = (void*)(uintptr_t)0;
            raw->parsed = 0;
            continue;
        }

        if (raw->required && !raw->parsed) {
            sprintf(parser->err, "required option '%s' is not present", raw->long_name);
            return 1;
        }
    }

    return 0;
}

char* sap_generate_help_message(SapParser const* parser) {
    size_t max_len = 0;
    for (size_t i = 0; i < parser->options_count; i++) {
        size_t opt_len = strlen(parser->options[i].long_name);
        if (opt_len > max_len) {
            max_len = opt_len;
        }
    }

    string_builder sb = sb_new(1024);
    for (size_t i = 0; i < parser->options_count; i++) {
        SapOption* opt = &parser->options[i];
        sb_sprintf(&sb, "  %c, %s ", opt->short_name, opt->long_name);

        size_t count = max_len - strlen(opt->long_name);
        for (size_t j = 0; j < count; j++) {
            sb_push(&sb, ' ');
        }

        sb_sprintf(&sb, "- %s", opt->description);
        if (opt->required) {
            sb_sprintf(&sb, " (required)");
        }
        sb_push(&sb, '\n');
    }

    return sb_to_cstr(&sb);
}

#endif  // SAP_IMPLEMENTATION

#endif  // SAP_INCLUDE_
