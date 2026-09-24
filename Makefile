# ============================================================
# Makefile for the RISC-V simulator
#
# Targets:
#   make sim          build the simulator itself
#   make run-c         compile test_program/main.c and run it
#   make run-asm       assemble test_program/main.s and run it
#   make objdump-c      show the real instructions main.c compiled to
#   make objdump-asm    show the real instructions main.s assembled to
#   make clean         remove build output
#
# make run-c / make run-asm require the RISC-V toolchain:
#   sudo apt install gcc-riscv64-unknown-elf
# ============================================================

CC       = gcc
RVCC     = riscv64-unknown-elf-gcc
OBJCOPY  = riscv64-unknown-elf-objcopy
OBJDUMP  = riscv64-unknown-elf-objdump

# RV32I only, matching the simulator. No standard library, no
# startup files, so ONLY the instructions in your source file get
# used, nothing pulled in from outside. Linked starting at address
# 0, matching how the simulator loads a raw binary.
RVFLAGS  = -march=rv32i -mabi=ilp32 -nostdlib -nostartfiles -Wl,-Ttext=0x0 -O1

.PHONY: sim run-c run-asm objdump-c objdump-asm clean

sim: src/riscv_sim.c
	$(CC) -Wall -Wextra -o sim src/riscv_sim.c

run-c: sim test_program/main.c
	$(RVCC) $(RVFLAGS) -o test_program/main.elf test_program/main.c
	$(OBJCOPY) -O binary test_program/main.elf test_program/program.bin
	./sim test_program/program.bin

run-asm: sim test_program/main.s
	$(RVCC) $(RVFLAGS) -o test_program/main.elf test_program/main.s
	$(OBJCOPY) -O binary test_program/main.elf test_program/program.bin
	./sim test_program/program.bin

# Handy for checking exactly what instructions your C/asm turned
# into, useful if make run-c prints "UNKNOWN instruction".
objdump-c: test_program/main.c
	$(RVCC) $(RVFLAGS) -o test_program/main.elf test_program/main.c
	$(OBJDUMP) -d test_program/main.elf

objdump-asm: test_program/main.s
	$(RVCC) $(RVFLAGS) -o test_program/main.elf test_program/main.s
	$(OBJDUMP) -d test_program/main.elf

clean:
	rm -f sim test_program/*.elf test_program/*.bin
