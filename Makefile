CC      := cc
CFLAGS  := -O2 -std=c17 -Wall -Wextra -pedantic -Isrc -MMD -MP 
LDFLAGS := -lncurses -ltinfo

ifeq ($(DEBUG),1)
	CFLAGS += -g3 -fstack-protector -fsanitize=address,leak,undefined -DDEBUG
else
	CFLAGS += -s
endif

SOURCE_DIR  := src
BUILD_DIR   := build
OBJECTS_DIR := $(BUILD_DIR)/$(SOURCE_DIR)
BIN_PATH    ?= /usr/local/bin

SOURCES := $(shell find $(SOURCE_DIR) -type f -name '*.c')
OBJECTS := $(patsubst $(SOURCE_DIR)/%.c, $(OBJECTS_DIR)/%.o, $(SOURCES))
BINARY  := $(BUILD_DIR)/sagit

OBJECTS      := $(patsubst $(SOURCE_DIR)/%.c, $(OBJECTS_DIR)/%.o, $(SOURCES))
DEPENDENCIES := $(patsubst %.o, %.d, $(OBJECTS))

.PHONY: all
all: build

.PHONY: build
build: $(BINARY)

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: install
install: build
	install -D -m 755 $(BINARY) $(BIN_PATH)/$(BINARY)

.PHONY: uninstall
uninstall: build
	rm $(BIN_PATH)/$(BINARY)

$(BINARY): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJECTS_DIR)/%.o: $(SOURCE_DIR)/%.c Makefile
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -o $@ -c $<

-include $(DEPENDENCIES)
