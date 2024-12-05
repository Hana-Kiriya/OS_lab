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


static ssize_t Myread(struct file *fileptr, char __user *ubuf, size_t buffer_len, loff_t *offset){
    //offset:偏移量，ubuf:用戶空間的緩衝區，buffer_len:緩衝區大小
    /*Your code here*/


    size_t len = 0; //計算緩衝區的偏移量

    if(*offset > 0){ //偏移不為0，表已讀取過
        return 0;
    }

    for_each_thread(current, thread){
        len += snprintf(buf + len, BUFSIZE - len, "%s\n", data);
        len += snprintf(buf + len, BUFSIZE - len, "PID: %d, TID: %d, time: %d, State: %d\n", current -> tgid, current -> pid, current -> utime/100/1000);

        if(len >= BUFSIZE){ //緩衝區滿了，截斷資料並停止寫入
            pr_warn("Buffer overflow in Myread.\n");
            len = BUFSIZE;
            break;
        }
    }
    
    if(len > buffer_len){ //確保不超過用戶空間大小
        len = buffer_len;
    }

    int ret = copy_to_user(ubuf, buf, len);
    if(ret != 0){ //若複製成功則 = 0，if不成立，反之失敗則回傳未被成功複製的字節數量，ex:欲複製6個字節，僅4個字節成功複製，則copy_to_user返還2，因2 != 0，故if成立，執行if內指令
        pr_err("Failed to copy data to user space.\n");
        return -EFAULT;
    }

    *offset += len;

    return len;
    /****************/
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
