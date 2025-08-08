CC = arm-none-eabi-gcc
CFLAGS = -mcpu=cortex-m4 -mthumb -Wall -I../freertos-kernel/include -I../freertos-kernel/portable -I../src/freertos_config
LDFLAGS = -T ../linker/cortex-m.ld -nostartfiles

SRC = ../src/main.c ../src/startup.s ../freertos-kernel/tasks.c
OBJ = $(SRC:.c=.o)

TARGET = blinky.elf

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.s
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)