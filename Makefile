OBJ = boot.o gdt.o idt.o timer.o rtc.o pmm.o vmm.o kheap.o vbe.o keyboard.o mouse.o fs.o shell.o kernel.o

all: clean build_iso run

build_kernel:
	nasm -f elf32 boot.asm -o boot.o
	gcc -m32 -ffreestanding -O2 -c gdt.c -o gdt.o
	gcc -m32 -ffreestanding -O2 -c idt.c -o idt.o
	gcc -m32 -ffreestanding -O2 -c timer.c -o timer.o
	gcc -m32 -ffreestanding -O2 -c rtc.c -o rtc.o
	gcc -m32 -ffreestanding -O2 -c pmm.c -o pmm.o
	gcc -m32 -ffreestanding -O2 -c vmm.c -o vmm.o
	gcc -m32 -ffreestanding -O2 -c kheap.c -o kheap.o
	gcc -m32 -ffreestanding -O2 -c vbe.c -o vbe.o
	gcc -m32 -ffreestanding -O2 -c keyboard.c -o keyboard.o
	gcc -m32 -ffreestanding -O2 -c mouse.c -o mouse.o
	gcc -m32 -ffreestanding -O2 -c fs.c -o fs.o
	gcc -m32 -ffreestanding -O2 -c shell.c -o shell.o
	gcc -m32 -ffreestanding -O2 -c kernel.c -o kernel.o
	ld -m elf_i386 -T linker.ld -o myos.bin $(OBJ)

build_iso: build_kernel
	mkdir -p iso/boot/grub
	cp myos.bin iso/boot/myos.bin
	cp font.ttf iso/boot/font.ttf
	cp bg.png iso/boot/bg.png
	cp icon.png iso/boot/icon.png
	cp grub.cfg iso/boot/grub/grub.cfg
	grub-mkrescue -o b-nix.iso iso

run:
	qemu-system-i386 -m 256M -cdrom b-nix.iso -vga std

clean:
	rm -rf *.o myos.bin b-nix.iso iso/boot/myos.bin