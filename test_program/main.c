/* test_program/main.c
   -------------------------------------------------------------
   Edit this file, then run:  make run-c

   IMPORTANT, because this is bare-metal (no operating system,
   no C library):
     - The entry point MUST be named "_start", not "main". There
       is no startup code to call main() for you.
     - There is no printf. To see your results, store values into
       memory (like "result" below) and read the simulator's
       trace/register dump after it runs.
     - Only instructions the simulator actually implements will
       work: add, sub, addi, lw, sw, beq, jal, jalr, lui. Keep the
       code simple (no arrays, no function calls, no library
       calls) so the compiler doesn't emit anything else.
     - The stack pointer (sp / x2) is set up manually below, since
       there is no startup code to do it for you. Don't remove
       that line.
   ------------------------------------------------------------- */

void _start(void) {
    /* Bare-metal stack setup: normally a startup file does this
       before main() runs. We don't have one, so we do it by hand,
       as the very first thing. */
    asm volatile ("addi sp, x0, 2000" ::: "memory");

    int a = 10;
    int b = 20;
    int c = a + b;              /* try changing this line */

    volatile int *result = (int *)0x100;   /* address 256, away from our code */
    *result = c;

    asm volatile (".word 0");    /* tells the simulator to halt */
}
