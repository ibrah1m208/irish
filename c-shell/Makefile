# C-Shell Makefile
CC = gcc
CSTD = -std=c23
DEFINES = -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700
WARNINGS = -Wall -Wextra -Werror -Wno-unused-parameter
CFLAGS = $(CSTD) $(DEFINES) $(WARNINGS) -fno-asm -g -O0

# Source and include directories
SRC_DIR = src
INC_DIR = include
OBJ_DIR = obj
BIN_DIR = .

# Source files
SOURCES = $(SRC_DIR)/main.c
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TARGET = $(BIN_DIR)/shell.out

# Default target
all: $(TARGET)

# Build executable
$(TARGET): $(OBJECTS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "Built $(TARGET)"

# Compile object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -c $< -o $@
	@echo "Compiled $<"

# # Clean build artifacts
# clean:
# 	rm -rf $(OBJ_DIR) $(TARGET)
# 	@echo "Cleaned"

clean:
	@if [ -d "$(OBJ_DIR)" ] && [ -e "$(TARGET)" ]; then \
		rm -rf "$(OBJ_DIR)" "$(TARGET)"; \
	fi
	@echo "Cleaned"

# Phony targets
.PHONY: all clean

# Help target
help:
	@echo "C-Shell Build System"
	@echo "Targets:"
	@echo "  make all    - Build shell.out (default)"
	@echo "  make clean  - Remove build artifacts"
	@echo "  make help   - Display this message"