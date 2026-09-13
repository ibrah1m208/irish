# C-Shell Makefile
CC = gcc
CSTD = -std=c23
DEFINES = -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700
WARNINGS = -Wall -Wextra -Werror -Wno-unused-parameter
CFLAGS = $(CSTD) $(DEFINES) $(WARNINGS) -fno-asm -g -O0

SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
BIN_DIR = .

SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TARGET = $(BIN_DIR)/shell.out

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^
	@-clear 2>/dev/null || true
	@echo "Built $(TARGET)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@
	@echo "Compiled $<"

clean:
	rm -rf $(OBJ_DIR) $(TARGET)
	@echo "Cleaned"

.PHONY: all clean help

help:
	@echo "========================================================================"
	@echo "  C-Shell Build System"
	@echo "========================================================================"
	@echo "  make all    - Compile and link ./shell.out"
	@echo "  make clean  - Remove obj/ directory and shell.out"
	@echo "  make help   - Show this help message"
	@echo "========================================================================"
	@echo "2025117012 - Mohammed Ibrahim Bin Faisal"