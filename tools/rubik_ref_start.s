# Entry point for tools/rubik_ref.c: set gp for linker relaxation, run main,
# then exit through ecall 10 with main's failure count left in a0.
    .text
    .globl _start
_start:
    .option push
    .option norelax
    la   gp, __global_pointer$
    .option pop
    call main
    li   a7, 10
    ecall
