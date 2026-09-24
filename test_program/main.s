# test_program/main.s
# -------------------------------------------------------------
# Edit this file, then run:  make run-asm
#
# Only instructions the simulator actually implements will work:
# add, sub, addi, lw, sw, beq, jal, jalr, lui.
#
# The entry point must be named "_start", the linker looks for
# this symbol by default when there's no startup code.
# -------------------------------------------------------------

.global _start
_start:
    addi sp, x0, 2000      # set up a stack pointer by hand (no startup code to do it)

    addi x10, x0, 10       # a = 10
    addi x11, x0, 20       # b = 20
    add  x12, x10, x11     # c = a + b
    sw   x12, 0(x0)        # store result to mem[0], change the address if you like

    .word 0                 # halt
