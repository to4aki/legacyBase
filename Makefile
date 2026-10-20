ASM = nasm
CC = gcc
LD = ld
OBJCOPY = objcopy

BOOT_BIN = boot.bin

STAGE2_OBJ = stage2.o
IRQ_OBJ = irq.o
APP_OBJ = app.o

STAGE2_ELF = stage2.elf
STAGE2_BIN = stage2.bin

IMAGE = app.img

STAGE2_SIZE = 32768

CFLAGS = \
	-m32 \
	-ffreestanding \
	-fno-pie \
	-fno-pic \
	-fno-stack-protector \
	-O2 \
	-Wall \
	-Wextra

LDFLAGS = \
	-m \
	elf_i386 \
	-T \
	linker.ld


all: $(IMAGE)


$(BOOT_BIN): boot.asm
	$(ASM) \
		-f bin \
		boot.asm \
		-o $(BOOT_BIN)


$(STAGE2_OBJ): stage2.asm
	$(ASM) \
		-f elf32 \
		stage2.asm \
		-o $(STAGE2_OBJ)


$(IRQ_OBJ): irq.asm
	$(ASM) \
		-f elf32 \
		irq.asm \
		-o $(IRQ_OBJ)


$(APP_OBJ): app.c
	$(CC) \
		$(CFLAGS) \
		-c \
		app.c \
		-o $(APP_OBJ)


$(STAGE2_ELF): \
	$(STAGE2_OBJ) \
	$(IRQ_OBJ) \
	$(APP_OBJ) \
	linker.ld

	$(LD) \
		$(LDFLAGS) \
		-o $(STAGE2_ELF) \
		$(STAGE2_OBJ) \
		$(IRQ_OBJ) \
		$(APP_OBJ)


$(STAGE2_BIN): $(STAGE2_ELF)
	$(OBJCOPY) \
		-O binary \
		$(STAGE2_ELF) \
		$(STAGE2_BIN)

	@size=$$(stat -c%s $(STAGE2_BIN)); \
	if [ $$size -gt $(STAGE2_SIZE) ]; then \
		echo "ERROR: stage2.bin too large: $$size bytes"; \
		exit 1; \
	fi

	truncate \
		-s $(STAGE2_SIZE) \
		$(STAGE2_BIN)


$(IMAGE): $(BOOT_BIN) $(STAGE2_BIN)
	cat \
		$(BOOT_BIN) \
		$(STAGE2_BIN) \
		> $(IMAGE)


run: $(IMAGE)
	qemu-system-i386 \
		-drive format=raw,file=$(IMAGE),if=ide


usb: $(IMAGE)
	@echo "Rufus image ready: $(IMAGE)"
	@ls -lh $(IMAGE)


clean:
	rm -f \
		$(BOOT_BIN) \
		$(STAGE2_OBJ) \
		$(IRQ_OBJ) \
		$(APP_OBJ) \
		$(STAGE2_ELF) \
		$(STAGE2_BIN) \
		$(IMAGE)


.PHONY: all run usb clean


