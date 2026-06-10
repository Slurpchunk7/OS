CC      = aarch64-none-elf-gcc
CXX     = aarch64-none-elf-g++
AS      = aarch64-none-elf-as
LD      = aarch64-none-elf-ld

CFLAGS   = -O1 -ffreestanding -nostdlib -mcpu=cortex-a72 -march=armv8-a
CXXFLAGS = -O1 -ffreestanding -nostdlib -mcpu=cortex-a72 -march=armv8-a -fno-exceptions -fno-rtti
ASFLAGS  = -mcpu=cortex-a72 -march=armv8-a

LDFLAGS = -T linker.ld

TARGET = kernel.elf

ASM = src/_start.asm

C_FILES   := $(shell find src -type f -name "*.c")
CPP_FILES := $(shell find src -type f -name "*.cpp")

C_OBJS   := $(patsubst src/%.c,build/%.o,$(C_FILES))
CPP_OBJS := $(patsubst src/%.cpp,build/%.o,$(CPP_FILES))
ASM_OBJ  := build/_start.o

OBJ := $(ASM_OBJ) $(C_OBJS) $(CPP_OBJS)

all: $(TARGET)

$(TARGET): $(OBJ)
	$(LD) $(LDFLAGS) $(OBJ) -o $@

$(ASM_OBJ): $(ASM)
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

run: $(TARGET)
	qemu-system-aarch64 \
		-machine virt \
		-m 128M \
		-cpu cortex-a72 \
		-vga none \
		-device ramfb \
		-kernel $(TARGET) \
		-device virtio-keyboard-pci \
		-serial stdio \
		-device virtio-mouse-pci \
		-drive id=hd0,file=disk.img,format=raw,if=none \
		-device virtio-blk-pci,drive=hd0

clean:
	rm -rf build $(TARGET)

.PHONY: all run clean