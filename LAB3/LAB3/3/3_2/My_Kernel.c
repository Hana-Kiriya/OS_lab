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
    ssize_t len;
    if (*offset > 0) { // If offset is non-zero, that means it's already read
        return 0;
    }

    if(buffer_len > BUFSIZE){
        return -ENOSPC;
    }
    if(buffer_len > BUFSIZE - 1) len = BUFSIZE - 1;
    else len = buffer_len;

    int ret = copy_from_user(buf, ubuf, buffer_len);
    if(ret != 0){
        pr_err("Failed to copy data from user space.\n");
        return -EFAULT;
    }

    len = sprintf(buf + buffer_len, "PID: %d, TID: %d, time: %lld\n", current -> tgid, current -> pid, current -> utime/100/1000);
    buf[len] = '\0';

    *offset += len;
    *offset += buffer_len;
    buffer_len += len;
    pr_info("Kernel received: %s\n", buf);

    return len;
    /****************/
}


static ssize_t Myread(struct file *fileptr, char __user *ubuf, size_t buffer_len, loff_t *offset) {
    if (*offset > 0) { // If offset is non-zero, that means it's already read
        return 0;
    }

    // Ensure buffer length doesn't exceed the provided size
    if (len > buffer_len) {
        len = buffer_len;
    }

    int ret = copy_to_user(ubuf, buf, buffer_len);
    if (ret != 0) {
        pr_err("Failed to copy data to user space.\n");
        return -EFAULT;
    }

    *offset += buffer_len;
    return buffer_len;
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
