TARGETS += $(BUILD_DIR)/main

SRC_DIR = src
BUILD_DIR = build

OBJS = $(BUILD_DIR)/freesky.o $(BUILD_DIR)/main.o

CFLAGS += \
	-std=c99 \
	-W \
	-Wall \
	-Wunused-parameter \
	-Wunknown-pragmas \
	-Wsign-compare \
	-D_GNU_SOURCE

all: $(TARGETS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/main: $(OBJS)
	gcc -o $@ $^ -lpthread

clean:
	rm -rf $(BUILD_DIR)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(BUILD_DIR) 
	gcc $(CFLAGS) -c -o $@ $<
