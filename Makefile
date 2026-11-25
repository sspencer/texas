ODIN      := odin
BIN_DIR   := bin
TARGET    := $(BIN_DIR)/texas

.PHONY: all build release run run-windowed test check clean help

all: build

build: $(BIN_DIR)
	$(ODIN) build . -debug -out:$(TARGET)

release: $(BIN_DIR)
	$(ODIN) build . -o:speed -out:$(TARGET)

run:
	$(ODIN) run . -- $(ARGS)

run-windowed:
	$(ODIN) run . -- --windowed

test:
	$(ODIN) test .

check:
	$(ODIN) check . -vet

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
	@echo "  make test          Run unit tests"
	@echo "  make check         Type-check and vet source files"
	@echo "  make clean         Remove build artifacts ($(BIN_DIR)/)"
