CC            := cc
BIN_DIR       := bin
TARGET        := $(BIN_DIR)/texas
TEST_TARGET   := $(BIN_DIR)/texas_test
ICON_GEN      := $(BIN_DIR)/gen_icon
ICON_ICNS     := $(BIN_DIR)/Texas.icns
APP_NAME      := Texas.app
APP_BUNDLE    := $(BIN_DIR)/$(APP_NAME)
INSTALL_DIR   ?= /Applications

# Linux / other Unix install layout (FreeDesktop). Defaults to a per-user
# install (no sudo). Override PREFIX for a system-wide install, e.g.
# `sudo make install PREFIX=/usr/local`. DESTDIR is honored for packaging.
PREFIX        ?= $(HOME)/.local
BINDIR        := $(DESTDIR)$(PREFIX)/bin
DATADIR       := $(DESTDIR)$(PREFIX)/share
DESKTOP_DIR   := $(DATADIR)/applications
ICON_DIR      := $(DATADIR)/icons/hicolor
ICON_SIZES    := 32 48 64 128 256 512
ICON_PNGS     := $(foreach s,$(ICON_SIZES),$(BIN_DIR)/icons/$(s)/texas.png)

UNAME_S       := $(shell uname -s)

CORE_SRCS     := game.c tetromino.c backdrop.c music.c config.c
APP_SRCS      := main.c $(CORE_SRCS)
TEST_SRCS     := texas_test.c $(CORE_SRCS)
HEADERS       := game.h tetromino.h backdrop.h music.h config.h

RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null || echo -I/opt/homebrew/include)
RAYLIB_LIBS   := $(shell pkg-config --libs raylib 2>/dev/null || echo -L/opt/homebrew/lib -lraylib)

WARN_FLAGS    := -Wall -Wextra -Wpedantic
CFLAGS        := -std=c11 $(WARN_FLAGS) $(RAYLIB_CFLAGS)
LDFLAGS       := $(RAYLIB_LIBS) -lm

.PHONY: all build release run run-windowed app linux-assets install uninstall test check clean help

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

$(BIN_DIR)/icons/%/texas.png: $(ICON_GEN)
	mkdir -p $(dir $@)
	$(ICON_GEN) $@ $*

linux-assets: $(ICON_PNGS)

ifeq ($(UNAME_S),Darwin)

install: app
	rm -rf "$(INSTALL_DIR)/$(APP_NAME)"
	ditto "$(APP_BUNDLE)" "$(INSTALL_DIR)/$(APP_NAME)"
	touch "$(INSTALL_DIR)/$(APP_NAME)"
	@echo "Installed $(INSTALL_DIR)/$(APP_NAME)"

uninstall:
	rm -rf "$(INSTALL_DIR)/$(APP_NAME)"
	@echo "Removed $(INSTALL_DIR)/$(APP_NAME)"

else

# The .desktop file is generated at install time so Exec= always matches the PREFIX in use.
install: release linux-assets
	install -Dm755 $(TARGET) "$(BINDIR)/texas"
	mkdir -p "$(DESKTOP_DIR)"
	sed 's|@BINDIR@|$(PREFIX)/bin|g' linux/texas.desktop.in > "$(DESKTOP_DIR)/texas.desktop"
	chmod 644 "$(DESKTOP_DIR)/texas.desktop"
	@for s in $(ICON_SIZES); do \
		install -Dm644 $(BIN_DIR)/icons/$$s/texas.png "$(ICON_DIR)/$${s}x$${s}/apps/texas.png"; \
	done
	@if [ -z "$(DESTDIR)" ]; then \
		gtk-update-icon-cache -q -t "$(ICON_DIR)" 2>/dev/null || true; \
		update-desktop-database -q "$(DESKTOP_DIR)" 2>/dev/null || true; \
	fi
	@echo "Installed $(BINDIR)/texas (+ desktop entry and icons under $(DATADIR))"

uninstall:
	rm -f "$(BINDIR)/texas" "$(DESKTOP_DIR)/texas.desktop"
	@for s in $(ICON_SIZES); do \
		rm -f "$(ICON_DIR)/$${s}x$${s}/apps/texas.png"; \
	done
	@if [ -z "$(DESTDIR)" ]; then \
		gtk-update-icon-cache -q -t "$(ICON_DIR)" 2>/dev/null || true; \
		update-desktop-database -q "$(DESKTOP_DIR)" 2>/dev/null || true; \
	fi
	@echo "Removed Texas from $(PREFIX)"

endif

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
ifeq ($(UNAME_S),Darwin)
	@echo "  make app           Build double-clickable macOS app bundle ($(APP_BUNDLE))"
	@echo "  make install       Build and install $(APP_NAME) into $(INSTALL_DIR)"
	@echo "  make uninstall     Remove $(APP_NAME) from $(INSTALL_DIR)"
else
	@echo "  make install       Install texas, desktop entry, and icons under PREFIX ($(PREFIX))"
	@echo "                     system-wide: sudo make install PREFIX=/usr/local"
	@echo "  make uninstall     Remove texas, desktop entry, and icons from PREFIX"
endif
	@echo "  make run           Build and run fullscreen (pass ARGS=... for flags)"
	@echo "  make run-windowed  Build and run in windowed mode"
	@echo "  make test          Build and run unit tests"
	@echo "  make check         Strict C11 syntax and warning check (-Werror)"
	@echo "  make clean         Remove build artifacts ($(BIN_DIR)/)"
