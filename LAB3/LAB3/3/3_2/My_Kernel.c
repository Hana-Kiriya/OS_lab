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
    ssize_t len = 0;
    if (*offset > 0) { // //偏移不為0，表已讀取過
        return 0;
    }

    if(buffer_len > BUFSIZE){ //檢查寫入的資料大小，若超過緩衝區大小則返回錯誤代碼
        return -ENOSPC;
    }

    //確定實際要寫入的大小
    if(buffer_len > BUFSIZE - 1) len = BUFSIZE - 1; //預留一個給 '\0'
    else len = buffer_len;

    //從使用者空間複製到緩衝區buf
    int ret = copy_from_user(buf, ubuf, buffer_len);
    if(ret != 0){
        pr_err("Failed to copy data from user space.\n");
        return -EFAULT;
    }

    //在緩衝區尾部添加執行緒資料 
    len += sprintf(buf + len, "PID: %d, TID: %d, time: %lld\n", current -> tgid, current -> pid, current -> utime/100/1000);
    buf[len] = '\0';

    //更新偏移量
    *offset += len;
    
    //返回實際寫入的字節數
    pr_info("Kernel received: %s\n", buf);

    return len;
    /****************/
}


static ssize_t Myread(struct file *fileptr, char __user *ubuf, size_t buffer_len, loff_t *offset) {
    if (*offset > 0) { //偏移不為0，表已讀取過
        return 0;
    }

    //從緩衝區複製到使用者空間
    int ret = copy_to_user(ubuf, buf, buffer_len);
    if (ret != 0) {
        pr_err("Failed to copy data to user space.\n");
        return -EFAULT;
    }

    //更新偏移量
    *offset += buffer_len;

    //返回成功傳送的字節數
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
    remove_proc_entry(procfs_name, NULL); //避免kernel_panic
    pr_info("My kernel says GOODBYE");
}

module_init(My_Kernel_Init);
module_exit(My_Kernel_Exit);

MODULE_LICENSE("GPL");
