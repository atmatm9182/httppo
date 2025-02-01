BUILD_DIR = build
SRC_DIR = src
SOURCES = $(addprefix $(SRC_DIR)/,main.c thread_pool.c protocol.c config.c files.c hash.c)
COMPILED_OBJECTS = $(addprefix $(BUILD_DIR)/,$(notdir $(SOURCES:.c=.o)))

CFLAGS += -std=c11 -D_POSIX_C_SOURCE=200809L -g -Og -Wall -Wextra -pedantic -Wno-unused

.PHONY: clean httppo

httppo: $(BUILD_DIR)/httppo

$(BUILD_DIR)/httppo: $(COMPILED_OBJECTS)
	cc $(CFLAGS) -o $@ $(COMPILED_OBJECTS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	cc $(CFLAGS) -o $(BUILD_DIR)/$(notdir $@) -c $<

clean:
	rm -rf $(BUILD_DIR)/*
