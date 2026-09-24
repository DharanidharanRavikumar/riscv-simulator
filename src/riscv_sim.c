/* ===================================================================
   A basic RISC-V (RV32I) simulator in C.
   Supports: add, sub, addi, lw, sw, beq, jal   (enough to run a real
   small program, and enough structure to add more instructions)

   Build:
       gcc -o riscv_sim riscv_sim.c
   Run:
       ./riscv_sim
   =================================================================== */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* ---------- CPU state ---------- */

#define MEM_SIZE   4096      /* 4 KB of simulated memory            */
#define NUM_REGS   32

typedef struct {
    int32_t  regs[NUM_REGS]; /* x0 .. x31                           */
    uint32_t pc;              /* program counter                    */
    uint8_t  mem[MEM_SIZE];  /* simulated RAM, byte-addressable     */
    int      halted;
} cpu_t;

/* ---------- Field extraction helpers -----------------------------
   These pull register numbers and immediates out of a 32-bit
   instruction word, using the bit positions from the RISC-V spec.
   ------------------------------------------------------------------ */

static inline uint32_t opcode(uint32_t insn) { return insn & 0x7F; }
static inline uint32_t rd(uint32_t insn)     { return (insn >> 7)  & 0x1F; }
static inline uint32_t funct3(uint32_t insn) { return (insn >> 12) & 0x7; }
static inline uint32_t rs1(uint32_t insn)    { return (insn >> 15) & 0x1F; }
static inline uint32_t rs2(uint32_t insn)    { return (insn >> 20) & 0x1F; }
static inline uint32_t funct7(uint32_t insn) { return (insn >> 25) & 0x7F; }

/* I-type immediate: bits [31:20], sign-extended */
static inline int32_t imm_i(uint32_t insn) {
    return ((int32_t)insn) >> 20;
}

/* S-type immediate: bits [31:25] and [11:7], sign-extended */
static inline int32_t imm_s(uint32_t insn) {
    int32_t hi = ((int32_t)insn) >> 25;          /* bits 31..25, sign-extended */
    uint32_t lo = (insn >> 7) & 0x1F;             /* bits 11..7 */
    return (hi << 5) | (int32_t)lo;
}

/* B-type immediate: scattered bits, sign-extended, always even */
static inline int32_t imm_b(uint32_t insn) {
    uint32_t bit11   = (insn >> 7)  & 0x1;
    uint32_t bits4_1  = (insn >> 8)  & 0xF;
    uint32_t bits10_5 = (insn >> 25) & 0x3F;
    uint32_t bit12   = (insn >> 31) & 0x1;
    int32_t imm = (bit12 << 12) | (bit11 << 11) | (bits10_5 << 5) | (bits4_1 << 1);
    if (bit12) imm |= 0xFFFFE000;   /* sign-extend */
    return imm;
}

/* J-type immediate: scattered bits, sign-extended, always even (used by jal) */
static inline int32_t imm_j(uint32_t insn) {
    uint32_t bit20     = (insn >> 31) & 0x1;
    uint32_t bits10_1   = (insn >> 21) & 0x3FF;
    uint32_t bit11     = (insn >> 20) & 0x1;
    uint32_t bits19_12  = (insn >> 12) & 0xFF;
    int32_t imm = (bit20 << 20) | (bits19_12 << 12) | (bit11 << 11) | (bits10_1 << 1);
    if (bit20) imm |= 0xFFE00000;   /* sign-extend */
    return imm;
}

static inline int32_t imm_u(uint32_t insn) {
    return (int32_t)(insn & 0xFFFFF000);   /* keep bits 31-12, zero out bits 11-0 */
}

/* ---------- MATCH / MASK constants --------------------------------
   Same idea as riscv/encoding.h in Spike. Each instruction has fixed
   "identity" bits (opcode, funct3, funct7) and variable "operand"
   bits (registers, immediates). MASK marks which bits are identity;
   MATCH is what those bits must equal.
   ------------------------------------------------------------------ */

#define MASK_ADD   0xFE00707F
#define MATCH_ADD  0x00000033

#define MASK_SUB   0xFE00707F
#define MATCH_SUB  0x40000033

#define MASK_ADDI  0x0000707F
#define MATCH_ADDI 0x00000013

#define MASK_LW    0x0000707F
#define MATCH_LW   0x00002003

#define MASK_SW    0x0000707F
#define MATCH_SW   0x00002023

#define MASK_BEQ   0x0000707F
#define MATCH_BEQ  0x00000063

#define MASK_JAL   0x0000007F
#define MATCH_JAL  0x0000006F

#define MASK_JALR  0x0000707F   /* correct, same as addi's mask, funct3 + opcode */
#define MATCH_JALR 0x00000067   /* jalr's own opcode is different from addi's */

#define MASK_LUI   0x0000007F
#define MATCH_LUI  0x00000037

/* ---------- Register read/write, with x0 hard-wired to 0 ---------- */

static int32_t reg_read(cpu_t *cpu, uint32_t r) {
    return (r == 0) ? 0 : cpu->regs[r];
}

static void reg_write(cpu_t *cpu, uint32_t r, int32_t value) {
    if (r != 0) cpu->regs[r] = value;   /* x0 always reads as 0 */
}

/* ---------- Execute one instruction -------------------------------
   This is the equivalent of Spike's riscv/insns/ files (add.h, sub.h,
   and so on), just all in one function instead of one file each.
   ------------------------------------------------------------------ */

static void execute(cpu_t *cpu, uint32_t insn) {
    uint32_t next_pc = cpu->pc + 4;   /* default: move to next instruction */

    if ((insn & MASK_ADD) == MATCH_ADD) {
        reg_write(cpu, rd(insn), reg_read(cpu, rs1(insn)) + reg_read(cpu, rs2(insn)));
        printf("  add  x%d, x%d, x%d\n", rd(insn), rs1(insn), rs2(insn));

    } else if ((insn & MASK_SUB) == MATCH_SUB) {
        reg_write(cpu, rd(insn), reg_read(cpu, rs1(insn)) - reg_read(cpu, rs2(insn)));
        printf("  sub  x%d, x%d, x%d\n", rd(insn), rs1(insn), rs2(insn));

    } else if ((insn & MASK_ADDI) == MATCH_ADDI) {
        reg_write(cpu, rd(insn), reg_read(cpu, rs1(insn)) + imm_i(insn));
        printf("  addi x%d, x%d, %d\n", rd(insn), rs1(insn), imm_i(insn));

    } else if ((insn & MASK_LW) == MATCH_LW) {
        int32_t addr = reg_read(cpu, rs1(insn)) + imm_i(insn);
        int32_t value;
        memcpy(&value, &cpu->mem[addr], 4);   /* little-endian load */
        reg_write(cpu, rd(insn), value);
        printf("  lw   x%d, %d(x%d)\n", rd(insn), imm_i(insn), rs1(insn));

    } else if ((insn & MASK_SW) == MATCH_SW) {
        int32_t addr = reg_read(cpu, rs1(insn)) + imm_s(insn);
        int32_t value = reg_read(cpu, rs2(insn));
        memcpy(&cpu->mem[addr], &value, 4);   /* little-endian store */
        printf("  sw   x%d, %d(x%d)\n", rs2(insn), imm_s(insn), rs1(insn));

    } else if ((insn & MASK_BEQ) == MATCH_BEQ) {
        printf("  beq  x%d, x%d, %d\n", rs1(insn), rs2(insn), imm_b(insn));
        if (reg_read(cpu, rs1(insn)) == reg_read(cpu, rs2(insn))) {
            next_pc = cpu->pc + imm_b(insn);
        }

    } else if ((insn & MASK_JAL) == MATCH_JAL) {
        reg_write(cpu, rd(insn), cpu->pc + 4);   /* save return address */
        printf("  jal  x%d, %d\n", rd(insn), imm_j(insn));
        next_pc = cpu->pc + imm_j(insn);

    } else if ((insn & MASK_JALR) == MATCH_JALR) {
        reg_write(cpu, rd(insn), cpu->pc + 4);   /* save return address */
        printf("  jalr x%d, x%d, %d\n", rd(insn), rs1(insn), imm_i(insn));
        next_pc = (reg_read(cpu, rs1(insn)) + imm_i(insn)) & ~1;   /* clear lowest bit */

    }  else if ((insn & MASK_LUI) == MATCH_LUI) {
    reg_write(cpu, rd(insn), imm_u(insn));
    printf("  lui  x%d, 0x%x\n", rd(insn), (unsigned)imm_u(insn) >> 12);


    } else if (insn == 0) {
        printf("  (zero instruction, halting)\n");
        cpu->halted = 1;
        return;

    } else {
        printf("  UNKNOWN instruction 0x%08x at pc=0x%03x\n", insn, cpu->pc);
        cpu->halted = 1;
        return;
    }

    cpu->pc = next_pc;
}

/* ---------- Fetch one instruction from memory ---------------------
   RISC-V is little-endian, so byte 0 is the least significant byte.
   ------------------------------------------------------------------ */

static uint32_t fetch(cpu_t *cpu) {
    uint32_t b0 = cpu->mem[cpu->pc + 0];
    uint32_t b1 = cpu->mem[cpu->pc + 1];
    uint32_t b2 = cpu->mem[cpu->pc + 2];
    uint32_t b3 = cpu->mem[cpu->pc + 3];
    return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

/* ---------- Print all registers ------------------------------------ */

static void dump_regs(cpu_t *cpu) {
    printf("\n---- Register dump ----\n");
    for (int i = 0; i < NUM_REGS; i++) {
        printf("x%-2d = %-10d", i, cpu->regs[i]);
        if (i % 4 == 3) printf("\n");
    }
    printf("pc  = 0x%03x\n", cpu->pc);
}

/* ---------- A tiny test program -------------------------------------
   Equivalent to this C:

       int a = 5;
       int b = 3;
       int c = a + b;     // 8
       int d = a - b;     // 2
       mem[200] = c;      // sw, store c at address 200
       int e = *(int*)&mem[200];  // lw, read it back into x5

   Written by hand as machine code (RV32I). You worked out MATCH/MASK
   values by hand earlier, so try decoding a couple of these words
   yourself (pull out opcode, funct3, funct7, rd, rs1, rs2) and compare
   with the printed trace.
   ------------------------------------------------------------------ */

static void load_program(cpu_t *cpu) {
    uint32_t program[] = {
        0x00500093,   /* addi x1, x0, 5      -> x1 = 5                */
        0x00300113,   /* addi x2, x0, 3      -> x2 = 3                */
        0x002081B3,   /* add  x3, x1, x2     -> x3 = x1 + x2 = 8      */
        0x40208233,   /* sub  x4, x1, x2     -> x4 = x1 - x2 = 2      */
        0x0C302823,   /* sw   x3, 208(x0)    -> mem[208] = x3 = 8     */
        0x0D002283,   /* lw   x5, 208(x0)    -> x5 = mem[208] = 8     */
        0x00000013,   /* addi x0, x0, 0      -> NOP                  */
        0x00000000    /* zero word           -> simulator halts      */
    };

    memcpy(cpu->mem, program, sizeof(program));
}
static void load_calltest_program(cpu_t *cpu) {
    uint32_t program[] = {
        0x00a00513,   /* 0x00  addi x10, x0, 10                        */
        0x00500593,   /* 0x04  addi x11, x0, 5                         */
        0x010000ef,   /* 0x08  jal  x1, 16   -> jumps to 0x18          */
        0x00a02023,   /* 0x0c  sw   x10, 0(x0)  -> mem[0] = x10        */
        0x00000000,   /* 0x10  zero word -> halt                       */
        0x00000013,   /* 0x14  nop (filler, unreachable)               */
        0x00b50533,   /* 0x18  add  x10, x10, x11                      */
        0x00008067    /* 0x1c  jalr x0, 0(x1)  -> return to 0x0c       */
    };
    memcpy(cpu->mem, program, sizeof(program));
}

static void load_luitest_program(cpu_t *cpu) {
    uint32_t program[] = {
        0x000102b7,   /* 0x00  lui  x5, 0x10                    */
        0x00528293,   /* 0x04  addi x5, x5, 5                   */
        0x00000000    /* 0x08  halt                              */
    };
    memcpy(cpu->mem, program, sizeof(program));
}

/* ---------- Main loop ----------------------------------------------- */

int main(int argc, char *argv[]) {
    cpu_t cpu;
    memset(&cpu, 0, sizeof(cpu));

    if (argc > 1 && strcmp(argv[1], "calltest") == 0) {
        load_calltest_program(&cpu);
    } else if (argc > 1 && strcmp(argv[1], "luitest") == 0) {
        load_luitest_program(&cpu);
    } else if (argc > 1) {
        FILE *f = fopen(argv[1], "rb");
        if (!f) {
            printf("Could not open file: %s\n", argv[1]);
            return 1;
        }
        size_t n = fread(cpu.mem, 1, MEM_SIZE, f);
        fclose(f);
        printf("Loaded %zu bytes from %s\n", n, argv[1]);
    } else {
        load_program(&cpu);
    }

    printf("Starting simulation...\n\n");

    int max_steps = 100;
    while (!cpu.halted && max_steps-- > 0) {
        uint32_t insn = fetch(&cpu);
        printf("pc=0x%03x  insn=0x%08x\n", cpu.pc, insn);
        execute(&cpu, insn);
    }

    dump_regs(&cpu);
    return 0;
}
