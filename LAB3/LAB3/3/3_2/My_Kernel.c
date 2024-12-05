#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <asm/current.h>
#include <asm/uaccess.h>

#define procfs_name "Mythread_info"
#define BUFSIZE  1024
char buf[BUFSIZE]; //kernel buffer

static ssize_t Mywrite(struct file *fileptr, const char __user *ubuf, size_t buffer_len, loff_t *offset){
    /*Your code here*/
    size_t w_len;

    if(buffer_len > BUFSIZE - 1) w_len = BUFSIZE - 1;
    else w_len = buffer_len;

    int ret = copy_from_user(buf, ubuf, w_len);
    if(ret != 0){
        pr_err("Failed to copy data from user space.\n");
        return -EFAULT;
    }

    buf[w_len] = '\0';

    pr_info("Kernel received: %s\n", buf);

    return w_len;
    /****************/
}


static ssize_t Myread(struct file *fileptr, char __user *ubuf, size_t buffer_len, loff_t *offset) {
    size_t len = 0; // Calculate buffer length
    if (*offset > 0) { // If offset is non-zero, that means it's already read
        return 0;
    }

    // Instead of reading /proc/Mythread_info again, you can just append messages directly
    len += snprintf(buf + len, BUFSIZE - len, "Thread 1 says hello!\n");
    len += snprintf(buf + len, BUFSIZE - len, "PID: %d, TID: %d, time: %d\n", current -> tgid, current -> pid, current -> utime/100/1000);
#if defined(THREAD_NUMBER) && THREAD_NUMBER == 2
    len += snprintf(buf + len, BUFSIZE - len, "Thread 2 says hello!\n");
    len += snprintf(buf + len, BUFSIZE - len, "PID: %d, TID: %d, time: %d\n", current -> tgid, current -> pid, current -> utime/100/1000);
#endif

    // Ensure buffer length doesn't exceed the provided size
    if (len > buffer_len) {
        len = buffer_len;
    }

    int ret = copy_to_user(ubuf, buf, len);
    if (ret != 0) {
        pr_err("Failed to copy data to user space.\n");
        return -EFAULT;
    }

    *offset += len;
    return len;
}


static struct proc_ops Myops = {
    .proc_read = Myread,
    .proc_write = Mywrite,
};

static int My_Kernel_Init(void){
    proc_create(procfs_name, 0644, NULL, &Myops);   
    pr_info("My kernel says Hi");
    return 0;
}

static void My_Kernel_Exit(void){
    pr_info("My kernel says GOODBYE");
}

module_init(My_Kernel_Init);
module_exit(My_Kernel_Exit);

MODULE_LICENSE("GPL");
