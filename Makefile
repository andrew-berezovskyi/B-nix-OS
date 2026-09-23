CROSS ?=
CC := $(CROSS)gcc
AS := nasm
LD := $(CROSS)ld
QEMU ?= qemu-system-i386

CPPFLAGS := -Iinclude
COMMON_CFLAGS := -m32 -ffreestanding -fno-pie -fno-stack-protector -fno-builtin \
                 -fno-asynchronous-unwind-tables -fno-unwind-tables -Wall -Wextra
CFLAGS := $(COMMON_CFLAGS) $(CPPFLAGS) -O2
LDFLAGS := -m elf_i386 -T linker.ld
APP_CFLAGS := $(COMMON_CFLAGS) -Iapps -O2
APP_LDFLAGS := -m elf_i386 -e _start -Ttext 0x02000000

BUILD_DIR := build
ISO_DIR := iso/boot

C_SOURCES := core/kernel.c core/gdt.c core/idt.c core/exceptions.c core/syscall.c core/sys_api.c \
             drivers/serial.c drivers/vbe.c drivers/keyboard.c drivers/mouse.c drivers/rtc.c \
             drivers/timer.c drivers/ata.c mm/pmm.c mm/vmm.c mm/kheap.c fs/fs.c fs/shell.c \
             gui/desktop.c gui/render.c gui/login.c gui/wm.c
ASM_SOURCES := core/boot.asm core/isr.asm
OBJS := $(C_SOURCES:%.c=$(BUILD_DIR)/%.o) $(ASM_SOURCES:%.asm=$(BUILD_DIR)/%.o)
APP_OBJS := $(BUILD_DIR)/apps/calc.o $(BUILD_DIR)/apps/libc.o

.PHONY: all clean build_iso disk_image run run-headless check

all: build_iso

$(BUILD_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(AS) -f elf32 $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/apps/%.o: apps/%.c
	@mkdir -p $(dir $@)
	$(CC) $(APP_CFLAGS) -c $< -o $@

calc.bin: $(APP_OBJS)
	$(LD) $(APP_LDFLAGS) -o $@ $(APP_OBJS)

myos.bin: $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

build_iso: myos.bin calc.bin
	@mkdir -p $(ISO_DIR)/grub
	cp grub.cfg $(ISO_DIR)/grub/grub.cfg
	cp myos.bin calc.bin assets/font.ttf assets/bg.png assets/icon.png assets/folder.png assets/file.png $(ISO_DIR)/
	grub-mkrescue -o b-nix.iso iso

check: myos.bin
	grub-file --is-x86-multiboot myos.bin

disk_image:
	@if [ ! -f c_drive.img ]; then dd if=/dev/zero of=c_drive.img bs=1M count=10; fi

run: build_iso disk_image
	$(QEMU) -m 256M -cdrom b-nix.iso -drive file=c_drive.img,format=raw,if=ide -vga std -serial stdio

run-headless: build_iso disk_image
	$(QEMU) -m 256M -cdrom b-nix.iso -drive file=c_drive.img,format=raw,if=ide -vga std \
		-display none -serial stdio -no-reboot -no-shutdown

clean:
	rm -rf $(BUILD_DIR) iso myos.bin b-nix.iso calc.bin
