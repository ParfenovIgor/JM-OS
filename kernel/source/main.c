#include <monitor.h>
#include <descriptor_tables.h>
#include <timer.h>
#include <paging.h>
#include <kheap.h>
#include <multiboot.h>
#include <fs.h>
#include <initrd.h>
#include <task.h>
#include <syscall.h>

u32int initial_esp;

int main(struct multiboot* mboot_ptr, u32int initial_stack) {
    initial_esp = initial_stack;
    
    init_descriptor_tables();

    monitor_clear();
    
    asm volatile ("sti");
    init_timer(50);
    
    ASSERT(mboot_ptr->mods_count > 0);
    u32int initrd_location = *((u32int*)mboot_ptr->mods_addr);
    u32int initrd_end = *(u32int*)(mboot_ptr->mods_addr + 4);
    extern u32int placement_address;
    placement_address = initrd_end;
    
    initialise_paging();
    
    fs_root = initialise_initrd(initrd_location);
    int i = 0;
    struct dirent *node = 0;
    while ((node = readdir_fs(fs_root, i)) != 0) {
        monitor_write("Found file ");
        monitor_write(node->name);
        fs_node_t *fsnode = finddir_fs(fs_root, node->name);
        
        if ((fsnode->flags & 0x7) == FS_DIRECTORY) {
            monitor_write("\n\t(directory)\n");
        }
        else {
            monitor_write("\n\t contents: \"");
            char buf[256];
            u32int sz = read_fs(fsnode, 0, 256, buf);
            int j;
            for (j = 0; j < sz; j++)
                monitor_put(buf[j]);
            
            monitor_write("\"\n");
        }
        i++;
    }
    
    initialise_tasking();
    
    initialise_syscalls();
    switch_to_user_mode();
    
    extern top();
    top();
    
    syscall_stop();
    
    return 0xDEADBABA;
}
