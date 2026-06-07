
.global _start
_start:
    ldr x8, =STACK_TOP
    mov sp, x8

    bl enable_floating_points

    bl start

    sleep_loop:
        wfi
        b sleep_loop

enable_floating_points:
    mrs    x1, cpacr_el1
    mov    x0, #(3 << 20)
    orr    x0, x1, x0
    msr    cpacr_el1, x0
    ret
