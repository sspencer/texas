BIN_DIR := bin
TARGET  := $(BIN_DIR)/texas
PKG     := ./cmd/texas

GO      ?= go
GOFLAGS ?=

.DEFAULT_GOAL := build
.PHONY: build release run run-windowed test race cover bench fuzz vet lint fmt tidy check clean help

build: ## Build a debug binary into bin/
	$(GO) build $(GOFLAGS) -o $(TARGET) $(PKG)

release: ## Build a stripped, reproducible binary into bin/
	$(GO) build $(GOFLAGS) -trimpath -ldflags="-s -w" -o $(TARGET) $(PKG)

run: ## Run fullscreen (pass ARGS=... for flags)
	$(GO) run $(PKG) $(ARGS)

run-windowed: ## Run in a window
	$(GO) run $(PKG) --windowed

test: ## Run unit tests
	$(GO) test ./...

race: ## Run unit tests with the race detector
	$(GO) test -race ./...

cover: ## Write an HTML coverage report to coverage.html
	$(GO) test -coverprofile=coverage.out ./...
	$(GO) tool cover -html=coverage.out -o coverage.html

bench: ## Run benchmarks
	$(GO) test -run='^$$' -bench=. -benchmem ./...

fuzz: ## Fuzz the game and rotation logic for 30s each
	$(GO) test -run='^$$' -fuzz=FuzzStep -fuzztime=30s ./internal/game
	$(GO) test -run='^$$' -fuzz=FuzzRotate -fuzztime=30s ./internal/tetromino

vet: ## Run go vet
	$(GO) vet ./...

lint: ## Run golangci-lint
	golangci-lint run

fmt: ## Format code
	golangci-lint fmt

tidy: ## Tidy go.mod and go.sum
	$(GO) mod tidy

check: vet lint race ## Run everything CI runs

clean: ## Remove build artifacts
	rm -rf $(BIN_DIR) coverage.out coverage.html

help: ## Show this help
	@grep -E '^[a-zA-Z_-]+:.*?## ' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "  %-14s %s\n", $$1, $$2}'
