CC            := cc
BIN_DIR       := bin
TARGET        := $(BIN_DIR)/texas
TEST_TARGET   := $(BIN_DIR)/texas_test
ICON_GEN      := $(BIN_DIR)/gen_icon
ICON_ICNS     := $(BIN_DIR)/Texas.icns
APP_NAME      := Texas.app
APP_BUNDLE    := $(BIN_DIR)/$(APP_NAME)
INSTALL_DIR   ?= /Applications

CORE_SRCS     := game.c tetromino.c backdrop.c music.c config.c
APP_SRCS      := main.c $(CORE_SRCS)
TEST_SRCS     := texas_test.c $(CORE_SRCS)
HEADERS       := game.h tetromino.h backdrop.h music.h config.h

RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null || echo -I/opt/homebrew/include)
RAYLIB_LIBS   := $(shell pkg-config --libs raylib 2>/dev/null || echo -L/opt/homebrew/lib -lraylib)

WARN_FLAGS    := -Wall -Wextra -Wpedantic
CFLAGS        := -std=c11 $(WARN_FLAGS) $(RAYLIB_CFLAGS)
LDFLAGS       := $(RAYLIB_LIBS) -lm

.PHONY: all build release run run-windowed app install uninstall test check clean help

all: build

build: $(BIN_DIR)
	$(CC) $(CFLAGS) -g -O0 $(APP_SRCS) -o $(TARGET) $(LDFLAGS)

release: $(BIN_DIR)
	$(CC) $(CFLAGS) -O2 $(APP_SRCS) -o $(TARGET) $(LDFLAGS)

run: build
	./$(TARGET) $(ARGS)

run-windowed: build
	./$(TARGET) --windowed

$(ICON_GEN): tools/gen_icon.c | $(BIN_DIR)
	$(CC) -std=c11 $(WARN_FLAGS) -O2 tools/gen_icon.c -o $(ICON_GEN) -lm

$(ICON_ICNS): $(ICON_GEN) macos/make_icon.sh
	sh macos/make_icon.sh $(ICON_GEN) $(ICON_ICNS) $(BIN_DIR)

app: release $(ICON_ICNS) macos/Info.plist macos/bundle_app.sh
	sh macos/bundle_app.sh $(TARGET) $(APP_BUNDLE) $(ICON_ICNS) macos/Info.plist
	@echo "Built $(APP_BUNDLE)"

install: app
	rm -rf "$(INSTALL_DIR)/$(APP_NAME)"
	ditto "$(APP_BUNDLE)" "$(INSTALL_DIR)/$(APP_NAME)"
	touch "$(INSTALL_DIR)/$(APP_NAME)"
	@echo "Installed $(INSTALL_DIR)/$(APP_NAME)"

uninstall:
	rm -rf "$(INSTALL_DIR)/$(APP_NAME)"
	@echo "Removed $(INSTALL_DIR)/$(APP_NAME)"

test: $(BIN_DIR)
	$(CC) $(CFLAGS) -g -O0 $(TEST_SRCS) -o $(TEST_TARGET) $(LDFLAGS)
	./$(TEST_TARGET)

check:
	$(CC) $(CFLAGS) -Werror -fsyntax-only $(APP_SRCS) texas_test.c
	$(CC) -std=c11 $(WARN_FLAGS) -Werror -fsyntax-only tools/gen_icon.c

clean:
	rm -rf $(BIN_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

help:
	@echo "Available targets:"
	@echo "  make               Build debug binary ($(TARGET))"
	@echo "  make release       Build optimized binary ($(TARGET))"
	@echo "  make app           Build double-clickable macOS app bundle ($(APP_BUNDLE))"
	@echo "  make install       Build and install $(APP_NAME) into $(INSTALL_DIR)"
	@echo "  make uninstall     Remove $(APP_NAME) from $(INSTALL_DIR)"
	@echo "  make run           Build and run fullscreen (pass ARGS=... for flags)"
	@echo "  make run-windowed  Build and run in windowed mode"
	@echo "  make test          Build and run unit tests"
	@echo "  make check         Strict C11 syntax and warning check (-Werror)"
	@echo "  make clean         Remove build artifacts ($(BIN_DIR)/)"
