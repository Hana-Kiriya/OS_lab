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
    size_t len = 0; // Calculate buffer length
    if (*offset > 0) { // If offset is non-zero, that means it's already read
        return 0;
    }

    // Read the content of the /proc/Mythread_info file to check how many lines there are
    struct file *proc_file = filp_open("/proc/Mythread_info", O_RDONLY, 0);
    if (IS_ERR(proc_file)) {
        pr_err("Failed to open /proc/Mythread_info\n");
        return -EFAULT;
    }

    char file_contents[BUFSIZE];
    loff_t pos = 0;
    ssize_t bytes_read = kernel_read(proc_file, file_contents, BUFSIZE, &pos);

    if (bytes_read < 0) {
        pr_err("Failed to read /proc/Mythread_info\n");
        filp_close(proc_file, NULL);
        return -EFAULT;
    }

    file_contents[bytes_read] = '\0'; // Null-terminate the string

    // Count the number of lines in the file content
    int line_count = 0;
    char *line = file_contents;
    while (line && *line) {
        if (*line == '\n') {
            line_count++;
        }
        line++;
    }

    // If there's only one line in the file
    if (line_count == 1) {
        len += snprintf(buf + len, BUFSIZE - len, "%s\n", file_contents);
        len += snprintf(buf + len, BUFSIZE - len, "Thread 1 says hello!\n");
        if(len >= BUFSIZE){ //緩衝區滿了，截斷資料並停止寫入
            pr_warn("Buffer overflow in Myread.\n");
            len = BUFSIZE;
            break;
        }
    } 
    // If there are two lines in the file
    else if (line_count == 2) {
        char *first_line = file_contents;
        char *second_line = strchr(file_contents, '\n') + 1;

        // First line + "Thread 1 says hello"
        len += snprintf(buf + len, BUFSIZE - len, "%s\n", first_line);
        len += snprintf(buf + len, BUFSIZE - len, "Thread 1 says hello!\n");

        // Second line + "Thread 2 says hello"
        len += snprintf(buf + len, BUFSIZE - len, "%s\n", second_line);
        len += snprintf(buf + len, BUFSIZE - len, "Thread 2 says hello!\n");
        if(len >= BUFSIZE){ //緩衝區滿了，截斷資料並停止寫入
            pr_warn("Buffer overflow in Myread.\n");
            len = BUFSIZE;
            break;
        }
    }

    // Ensure buffer length doesn't exceed the provided size
    if (len > buffer_len) {
        len = buffer_len;
    }

    int ret = copy_to_user(ubuf, buf, len);
    if (ret != 0) {
        pr_err("Failed to copy data to user space.\n");
        filp_close(proc_file, NULL);
        return -EFAULT;
    }

    *offset += len;
    filp_close(proc_file, NULL);
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
