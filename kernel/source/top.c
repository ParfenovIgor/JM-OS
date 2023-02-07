#include <monitor.h>
#include <syscall.h>

void top() {
    int frk = syscall_fork();
    
    int cnt = 0;
    while (1) {
        volatile int k = 0;
        int i;
        for (i = 0; i < 10000000; i++) {
            k += i;
        }
        if (frk) {
            syscall_monitor_write("This is parent: ");
        }
        else {
            syscall_monitor_write("This is child : ");
        }
        syscall_monitor_write_hex(cnt);
        syscall_monitor_write("\n");
        cnt++;
        if (frk && cnt == 0x30) {
            break;
        }
        if (frk == 0 && cnt == 0xA0) {
            break;
        }
    }
}
