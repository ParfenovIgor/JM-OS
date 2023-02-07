SRCS_ASM := $(wildcard kernel/asm/*.s)
OBJS_ASM := $(patsubst %.s, build/%.o, $(SRCS_ASM))

SRCS_C := $(wildcard kernel/source/*.c)
OBJS_C := $(patsubst %.c, build/%.o, $(SRCS_C))

CFLAGS=-std=gnu99 -ffreestanding -O2 -Wall -Wextra -Ikernel/header
LDFLAGS=-Tkernel/link.ld -ffreestanding -O2 -nostdlib -lgcc
ASFLAGS=-felf

.PHONY: all kernel initrd

all: kernel initrd grub qemu

kernel: make_asm make_c $(OBJS_ASM) $(OBJS_C) link

make_asm:
	mkdir -p build/kernel/asm	

make_c:
	mkdir -p build/kernel/source

build/%.o: %.s
	nasm $(ASFLAGS) $< -o $@

build/%.o: %.c
	i686-elf-gcc -c $< -o $@ $(CFLAGS)

link:
	i686-elf-gcc $(LDFLAGS) -o build/kernel/kernel $(OBJS_ASM) $(OBJS_C)

initrd:
	mkdir -p build/ramdisk/source
	g++ ramdisk/source/make_initrd.c -o build/ramdisk/source/make_initrd
	./build/ramdisk/source/make_initrd build/ramdisk/initrd $(wildcard ramdisk/data/*)

grub:
	grub-file --is-x86-multiboot build/kernel/kernel || (echo "GRUB test fail"; exit 1)
	mkdir -p build/isodir/boot/grub
	cp build/kernel/kernel build/isodir/boot/kernel
	cp build/ramdisk/initrd build/isodir/boot/initrd
	cp kernel/grub/grub.cfg build/isodir/boot/grub/grub.cfg
	grub-mkrescue -o build/OS.iso build/isodir

qemu:
	qemu-system-i386 -cdrom build/OS.iso

clean:
	rm -rf build

