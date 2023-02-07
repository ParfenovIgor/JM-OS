#include <task.h>
#include <paging.h>
#include <monitor.h>

volatile task_t *current_task;
volatile task_t *ready_queue;

extern page_directory_t *kernel_directory;
extern page_directory_t *current_directory;
extern void alloc_frame(page_t*, int, int);
extern u32int initial_esp;
extern u32int read_eip();

u32int next_pid = 1;

void move_stack(void *new_stack_start, u32int size) {
    u32int i;
    for (i = (u32int)new_stack_start; i >= ((u32int)new_stack_start - size); i -= 0x1000) {
        alloc_frame(get_page(i, 1, current_directory), 0, 1);
    }
    
    u32int pd_addr;
    asm volatile ("mov %%cr3, %0" : "=r" (pd_addr));
    asm volatile ("mov %0, %%cr3" : : "r" (pd_addr));
    
    u32int old_stack_pointer;
    asm volatile ("mov %%esp, %0" : "=r" (old_stack_pointer));
    u32int old_base_pointer;
    asm volatile ("mov %%ebp, %0" : "=r" (old_base_pointer));
    
    u32int offset = (u32int)new_stack_start - initial_esp;
    u32int new_stack_pointer = old_stack_pointer + offset;
    u32int new_base_pointer = old_base_pointer + offset;
    
    memcpy((void*)new_stack_pointer, (void*)old_stack_pointer, initial_esp - old_stack_pointer);

    for (i = (u32int)new_stack_start; i > (u32int)new_stack_start - size; i -= 4) {
        u32int tmp = *(u32int*)i;
        if ((old_stack_pointer < tmp) && (tmp < initial_esp)) {
            tmp = tmp + offset;
            u32int *tmp2 = (u32int*)i;
            *tmp2 = tmp;
        }
    }
    
    asm volatile ("mov %0, %%esp" : : "r" (new_stack_pointer));
    asm volatile ("mov %0, %%ebp" : : "r" (new_base_pointer));
}

void set_stack(void *new_stack_start, u32int size) {
    u32int i;
    for (i = (u32int)new_stack_start; i >= ((u32int)new_stack_start - size); i -= 0x1000) {
        alloc_frame(get_page(i, 1, current_directory), 0, 1);
    }
}

void free_stack(void *stack_start, u32int size) {
    u32int i;
    for (i = (u32int)stack_start; i >= ((u32int)stack_start - size); i -= 0x1000) {
        free_frame(get_page(i, 1, current_directory));
    }
}

void initialise_tasking() {
    asm volatile ("cli");
    
    move_stack((void*)0xE0000000, 0x2000);
    set_stack(0xF0000000, 0x2000); // I couldn't understand, what author meant by kernel stack allocation. As heap is appeared before page identity mapping, it is in "kernel" space of memory and is "copied by reference". This means, there is only one kernel stack, which causes failure on fork as the top of stack at child phase is overwritten, and it can't even return. Here, the kernel stack is copied for every process.
    
    current_task = ready_queue = (task_t*)kmalloc(sizeof(task_t));
    current_task->id = next_pid++;
    current_task->esp = current_task->ebp = 0;
    current_task->eip = 0;
    current_task->page_directory = current_directory;
    current_task->next = 0;
    current_task->kernel_stack = kmalloc_a(KERNEL_STACK_SIZE); // current_task->kernel_stack is not used at all. Howerever, if remove kmalloc_a, a crash on mov cr3 will appear. Seems, like bug in heap management.
    
    asm volatile ("sti");
}

int fork() {
    asm volatile ("cli");
    
    task_t *parent_task = (task_t*)current_task;
    
    page_directory_t *directory = clone_directory(current_directory);
    
    task_t *new_task = (task_t*)kmalloc(sizeof(task_t));
    new_task->id = next_pid++;
    new_task->esp = new_task->ebp = 0;
    new_task->eip = 0;
    new_task->page_directory = directory;
    current_task->kernel_stack = kmalloc_a(KERNEL_STACK_SIZE);
    new_task->next = 0;
    
    task_t *tmp_task = (task_t*)ready_queue;
    while (tmp_task->next)
        tmp_task = tmp_task->next;
    tmp_task->next = new_task;

    u32int eip = read_eip();
    
    if (current_task == parent_task) {
        u32int esp;
        asm volatile ("mov %%esp, %0" : "=r"(esp));
        u32int ebp;
        asm volatile ("mov %%ebp, %0" : "=r"(ebp));
        new_task->esp = esp;
        new_task->ebp = ebp;
        new_task->eip = eip;
        asm volatile ("sti");
        
        return new_task->id;
    }
    else {
        return 0;
    }
}

extern void perform_task_switch(u32int, u32int, u32int, u32int);

void stop() {
    task_t *prev_task = 0;
    task_t *ptr = current_task;
    while (1) {
        if (!ptr->next)
            ptr = ready_queue;
        else
            ptr = ptr->next;
        if (ptr->next == current_task) {
            prev_task = ptr;
            break;
        }
        if (ptr == current_task) {
            break;
        }
    }
    task_t *next_task = 0;
    if (!prev_task) {
        if (current_task->next) {
            next_task = current_task->next;
            ready_queue = next_task;
        }
        else {
        }
    }
    else {
        if (current_task->next) {
            next_task = current_task->next;
            prev_task->next = next_task;
        }
        else {
            next_task = ready_queue;
            prev_task->next = 0;
        }
    }
    
    monitor_write("Process finished: ");
    monitor_write_hex(current_task->id);
    monitor_write("\n");
    
    if (!next_task) {
        PANIC("All processes stopped");
    }
    
    free_stack(0xE0000000, 0x2000); // Only main stack is freed. If try to free kernel stack, while in used, it can be overwritten by allocations below. I don't know, how to do it correctly.
    free_directory(current_task->page_directory);
    kfree(current_task);
    
    current_task = next_task;
    
    u32int esp, ebp, eip;
    eip = current_task->eip;
    esp = current_task->esp;
    ebp = current_task->ebp;

    current_directory = current_task->page_directory;

    perform_task_switch(eip, current_directory->physicalAddr, ebp, esp);
}

void switch_task() {
    if (!current_task)
        return;
    u32int esp, ebp, eip;
    asm volatile ("mov %%esp, %0" : "=r"(esp));
    asm volatile ("mov %%ebp, %0" : "=r"(ebp));
    eip = read_eip();
    
    if (eip == 0x12345)
        return;
    
    current_task->eip = eip;
    current_task->esp = esp;
    current_task->ebp = ebp;
    
    current_task = current_task->next;
    if (!current_task)
        current_task = ready_queue;
    
    eip = current_task->eip;
    esp = current_task->esp;
    ebp = current_task->ebp;
    
    current_directory = current_task->page_directory;

    perform_task_switch(eip, current_directory->physicalAddr, ebp, esp);
}

int getpid() {
    return current_task->id;
}

void switch_to_user_mode() {
    set_kernel_stack(0xF0000000);
    
    asm volatile ("     \
        cli;            \
        mov $0x23, %ax; \
        mov %ax, %ds;   \
        mov %ax, %es;   \
        mov %ax, %fs;   \
        mov %ax, %gs;   \
        mov %esp, %eax; \
        pushl $0x23;    \
        pushl %eax;     \
        pushf;          \
        pop %eax;       \
        or $0x200, %eax;\
        push %eax;      \
        pushl $0x1B;    \
        push $1f;       \
        iret;           \
    1:                  \
    ");
}
