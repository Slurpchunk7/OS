CC      = aarch64-none-elf-gcc
CXX     = aarch64-none-elf-g++
AS      = aarch64-none-elf-as
LD      = aarch64-none-elf-ld

CFLAGS  = -O1 -ffreestanding -nostdlib -mcpu=cortex-a72 -march=armv8-a
CXXFLAGS= -O1 -ffreestanding -nostdlib -mcpu=cortex-a72 -march=armv8-a
ASFLAGS = -mcpu=cortex-a72 -march=armv8-a

LDFLAGS = -T linker.ld

ASM     = src/_start.asm
C_SRC   = src/main.cpp src/ramfb.c

OBJ     = _start.o main.o ramfb.o
TARGET  = kernel.elf

all: $(TARGET)

$(TARGET): $(OBJ)
	$(LD) $(LDFLAGS) $(OBJ) -o $(TARGET)

_start.o: $(ASM)
	$(AS) $(ASFLAGS) $(ASM) -o _start.o

main.o: src/main.cpp
	$(CXX) $(CXXFLAGS) -c src/main.cpp -o main.o

ramfb.o: src/ramfb.c
	$(CC) $(CFLAGS) -c src/ramfb.c -o ramfb.o

run: $(TARGET)
	qemu-system-aarch64 \
		-machine virt \
		-m 128M \
		-cpu cortex-a72 \
		-vga none \
		-device ramfb \
		-kernel $(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)