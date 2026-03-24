OBJ = boot.o gdt.o idt.o timer.o rtc.o pmm.o vmm.o kheap.o vbe.o keyboard.o mouse.o shell.o kernel.o

all: clean build_iso run

build_kernel:
	nasm -f elf32 boot.asm -o boot.o
	gcc -m32 -ffreestanding -c gdt.c -o gdt.o
	gcc -m32 -ffreestanding -c idt.c -o idt.o
	gcc -m32 -ffreestanding -c timer.c -o timer.o
	gcc -m32 -ffreestanding -c rtc.c -o rtc.o
	gcc -m32 -ffreestanding -c pmm.c -o pmm.o
	gcc -m32 -ffreestanding -c vmm.c -o vmm.o
	gcc -m32 -ffreestanding -c kheap.c -o kheap.o
	gcc -m32 -ffreestanding -c vbe.c -o vbe.o
	gcc -m32 -ffreestanding -c keyboard.c -o keyboard.o
	gcc -m32 -ffreestanding -c mouse.c -o mouse.o
	gcc -m32 -ffreestanding -c shell.c -o shell.o
	gcc -m32 -ffreestanding -c kernel.c -o kernel.o
	ld -m elf_i386 -T linker.ld -o myos.bin $(OBJ)

build_iso: build_kernel
	mkdir -p iso/boot/grub
	cp myos.bin iso/boot/myos.bin
	grub-mkrescue -o b-nix.iso iso

run:
	qemu-system-i386 -m 256M -cdrom b-nix.iso -vga std

clean:
	rm -rf *.o myos.bin b-nix.iso iso/boot/myos.bin