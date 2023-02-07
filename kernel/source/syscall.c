#include <syscall.h>
#include <isr.h>

#include <monitor.h>
#include <task.h>

static void syscall_handler(registers_t *regs);

static void *syscalls[5] = {
    &monitor_write,
    &monitor_write_hex,
    &monitor_write_dec,
    &fork,
    &stop,
};
u32int num_syscalls = 5;

void initialise_syscalls() {
    register_interrupt_handler(0x80, &syscall_handler);
}

void __attribute__((optimize("O0"))) syscall_handler(registers_t *regs) {
    if (regs->eax >= num_syscalls)
        return;
    
    int tt = regs->eax;
    void *location = syscalls[regs->eax];
    
    int ret;
    
    asm volatile ("         \
        push %1;            \
        push %2;            \
        push %3;            \
        push %4;            \
        push %5;            \
        call *%6;           \
        add $0x14, %%esp    \
    " : "=a"(ret) : "r"(regs->edi), "r"(regs->esi), "r"(regs->edx), "r"(regs->ecx), "r"(regs->ebx), "r"(location));
    // pop %%ebx is replaced by just add to esp. As I understood, looking add objdump -d , here ebx is used instead of ebp. Probably, some optimization.

    regs->eax = ret; // optimize("O0") is required, otherwise this line disappears
}

DEFN_SYSCALL1(monitor_write, 0, const char*)
DEFN_SYSCALL1(monitor_write_hex, 1, const char*)
DEFN_SYSCALL1(monitor_write_dec, 2, const char*)
DEFN_SYSCALL0(fork, 3)
DEFN_SYSCALL0(stop, 4)
