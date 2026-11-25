CC            := cc
BIN_DIR       := bin
TARGET        := $(BIN_DIR)/texas
TEST_TARGET   := $(BIN_DIR)/texas_test

CORE_SRCS     := game.c tetromino.c backdrop.c music.c
APP_SRCS      := main.c $(CORE_SRCS)
TEST_SRCS     := texas_test.c $(CORE_SRCS)
HEADERS       := game.h tetromino.h backdrop.h music.h

RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null || echo -I/opt/homebrew/include)
RAYLIB_LIBS   := $(shell pkg-config --libs raylib 2>/dev/null || echo -L/opt/homebrew/lib -lraylib)

WARN_FLAGS    := -Wall -Wextra -Wpedantic
CFLAGS        := -std=c11 $(WARN_FLAGS) $(RAYLIB_CFLAGS)
LDFLAGS       := $(RAYLIB_LIBS) -lm

.PHONY: all build release run run-windowed test check clean help

all: build

build: $(BIN_DIR)
	$(CC) $(CFLAGS) -g -O0 $(APP_SRCS) -o $(TARGET) $(LDFLAGS)

release: $(BIN_DIR)
	$(CC) $(CFLAGS) -O2 $(APP_SRCS) -o $(TARGET) $(LDFLAGS)

run: build
	./$(TARGET) $(ARGS)

run-windowed: build
	./$(TARGET) --windowed

test: $(BIN_DIR)
	$(CC) $(CFLAGS) -g -O0 $(TEST_SRCS) -o $(TEST_TARGET) $(LDFLAGS)
	./$(TEST_TARGET)

check:
	$(CC) $(CFLAGS) -Werror -fsyntax-only $(APP_SRCS) texas_test.c

clean:
	rm -rf $(BIN_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

help:
	@echo "Available targets:"
	@echo "  make               Build debug binary ($(TARGET))"
	@echo "  make release       Build optimized binary ($(TARGET))"
	@echo "  make run           Build and run fullscreen (pass ARGS=... for flags)"
	@echo "  make run-windowed  Build and run in windowed mode"
	@echo "  make test          Build and run unit tests"
	@echo "  make check         Strict C11 syntax and warning check (-Werror)"
	@echo "  make clean         Remove build artifacts ($(BIN_DIR)/)"
