PROJECT_NAME = httppo

BUILD_DIR = build
SOURCES = src/main.c src/thread_pool.c src/protocol.c
SOURCES_OBJS = $(SOURCES:.c=.o)
COMPILED_OBJS = $(addprefix $(BUILD_DIR)/,$(notdir $(SOURCES_OBJS)))

CFLAGS +=  -std=c11 -D_POSIX_C_SOURCE=200809L -g -Og -Wall -Wextra -pedantic

.PHONY: clean

%.o: %.c
	cc $(CFLAGS) -o $(BUILD_DIR)/$(notdir $@) -c $<

all: builddir $(SOURCES_OBJS)
	cc $(CFLAGS) -o $(BUILD_DIR)/$(PROJECT_NAME) $(COMPILED_OBJS)

clean:
	rm $(BUILD_DIR)/*

builddir:
	mkdir -p $(BUILD_DIR)
