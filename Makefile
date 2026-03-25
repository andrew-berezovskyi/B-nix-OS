CC = gcc
AS = nasm
LD = ld

CFLAGS = -m32 -ffreestanding -O2 -Wall -Wextra -Iinclude
LDFLAGS = -m elf_i386 -T linker.ld

BUILD_DIR = build
ISO_DIR = iso/boot

# Списки файлів
C_SOURCES = core/kernel.c core/gdt.c core/idt.c \
            drivers/vbe.c drivers/keyboard.c drivers/mouse.c drivers/rtc.c drivers/timer.c drivers/ata.c \
            mm/pmm.c mm/vmm.c mm/kheap.c \
            fs/fs.c fs/shell.c \
            gui/desktop.c gui/render.c gui/login.c gui/wm.c
# Перетворюємо імена .c файлів на .o файли в папці build
OBJS = $(C_SOURCES:%.c=$(BUILD_DIR)/%.o) $(BUILD_DIR)/core/boot.o

all: build_iso

# Правило для компіляції асемблера
$(BUILD_DIR)/core/boot.o: core/boot.asm
	@mkdir -p $(dir $@)
	$(AS) -f elf32 $< -o $@

# Правило для компіляції C-файлів
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Збираємо ядро
myos.bin: $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

# Збираємо ISO образ
build_iso: myos.bin
	@mkdir -p $(ISO_DIR)
	cp myos.bin $(ISO_DIR)/myos.bin
	cp assets/bg.png $(ISO_DIR)/bg.png 2>/dev/null || true
	cp assets/icon.png $(ISO_DIR)/icon.png 2>/dev/null || true
	cp assets/font.ttf $(ISO_DIR)/font.ttf 2>/dev/null || true
	cp assets/folder.png $(ISO_DIR)/folder.png 2>/dev/null || true
	cp assets/file.png $(ISO_DIR)/file.png 2>/dev/null || true
	grub-mkrescue -o b-nix.iso iso

clean:
	rm -rf $(BUILD_DIR) myos.bin b-nix.iso

# Команда, яка створює пустий файл на 10 МБ (наш жорсткий диск)
disk_image:
	@if [ ! -f c_drive.img ]; then dd if=/dev/zero of=c_drive.img bs=1M count=10; fi

run: build_iso disk_image
	qemu-system-i386 -m 256M -cdrom b-nix.iso -hda c_drive.img -vga std