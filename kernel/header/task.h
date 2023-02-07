#ifndef TASK_H
#define TASK_H

#include <common.h>
#include <descriptor_tables.h>
#include <paging.h>
#include <kheap.h>

#define KERNEL_STACK_SIZE 2048

typedef struct task {
    int id;
    u32int esp, ebp;
    u32int eip;
    page_directory_t *page_directory;
    u32int kernel_stack;
    struct task *next;
} task_t;

void initialise_tasking();

void switch_task();

int fork();

void stop();

void move_stack(void *new_stack_start, u32int size);

int getpid();

void switch_to_user_mode();

#endif // TASK_H
