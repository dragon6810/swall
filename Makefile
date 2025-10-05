CC = clang
LDFLAGS = -O3 -flto -march=native -mcpu=native
CFLAGS = -MMD -Wall -Werror -O3 -march=native -mcpu=native

BIN_DIR = bin
OBJ_DIR = obj
SRC_DIR = src

SRC_FILE = $(wildcard $(SRC_DIR)/*.c)
OBJ_FILE = $(addprefix $(OBJ_DIR)/, $(addsuffix .o, $(SRC_FILE)))
DEP_FILE = $(OBJ_FILE:.o=.d)
BIN_FILE = $(BIN_DIR)/swall

.PHONY: all clean mkdirs

all: mkdirs $(BIN_FILE)

clean:
	rm -rf $(BIN_DIR)
	rm -rf $(OBJ_DIR)

mkdirs:
	mkdir -p $(BIN_DIR)
	mkdir -p $(OBJ_DIR)
	mkdir -p $(OBJ_DIR)/$(SRC_DIR)

$(BIN_FILE): $(OBJ_FILE)
	$(CC) $(LDFLAGS) $(OBJ_FILE) -o $(BIN_FILE)

$(OBJ_DIR)/$(SRC_DIR)/%.c.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

-include $(DEP_FILE)