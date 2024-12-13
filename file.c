#include <linux/fs.h>
#include <linux/uaccess.h>
#include "osfs.h"

/**
 * Function: osfs_read
 * Description: Reads data from a file.
 * Inputs:
 *   - filp: The file pointer representing the file to read from.
 *   - buf: The user-space buffer to copy the data into.
 *   - len: The number of bytes to read.
 *   - ppos: The file position pointer.
 * Returns:
 *   - The number of bytes read on success.
 *   - 0 if the end of the file is reached.
 *   - -EFAULT if copying data to user space fails.
 */
static ssize_t osfs_read(struct file *filp, char __user *buf, size_t len, loff_t *ppos)
{
    struct inode *inode = file_inode(filp);
    struct osfs_inode *osfs_inode = inode->i_private;
    struct osfs_sb_info *sb_info = inode->i_sb->s_fs_info;
    void *data_block;
    ssize_t bytes_read;

    size_t remaining = len;
    size_t offset = *ppos;
    size_t total_read = 0;

    // If the file has not been allocated a data block, it indicates the file is empty
    if (osfs_inode->i_blocks == 0)
        return 0;

    if (*ppos >= osfs_inode->i_size)
        return 0;

    if (*ppos + len > osfs_inode->i_size)
        len = osfs_inode->i_size - *ppos;

    for(int i = 0; i < osfs_inode -> num_extents && remaining > 0; i++){
        struct osfs_extent *extent = &osfs_inode -> extents[i];
        size_t extent_start = extent -> start_block * BLOCK_SIZE;
        size_t extent_end = extent_start + extent -> length * BLOCK_SIZE;

        if(offset >= extent_end){
            offset -= extent -> length * BLOCK_SIZE;
            continue;
        }

        size_t extent_offset;
        if(offset > extent_start) extent_offset = offset - extent_start;
        else extent_offset = 0;

        size_t to_read = min(remaining, extent -> length* BLOCK_SIZE - extent_offset);

        data_block = sb_info->data_blocks + extent -> start_block * BLOCK_SIZE + extent_offset;
        if (copy_to_user(buf + bytes_read, data_block, to_read))
            return -EFAULT; 
        
        bytes_read += to_read;
        remaining -= to_read;
        offset = 0;
    }
    
    *ppos += bytes_read;

    return bytes_read;
}


/**
 * Function: osfs_write
 * Description: Writes data to a file.
 * Inputs:
 *   - filp: The file pointer representing the file to write to.
 *   - buf: The user-space buffer containing the data to write.
 *   - len: The number of bytes to write.
 *   - ppos: The file position pointer.
 * Returns:
 *   - The number of bytes written on success.
 *   - -EFAULT if copying data from user space fails.
 *   - Adjusted length if the write exceeds the block size.
 */
static ssize_t osfs_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{   
    //Step1: Retrieve the inode and filesystem information
    struct inode *inode = file_inode(filp);
    struct osfs_inode *osfs_inode = inode->i_private;
    struct osfs_sb_info *sb_info = inode->i_sb->s_fs_info;
    void *data_block;
    ssize_t bytes_written;
    int ret;

    // Step2: Check if a data block has been allocated; if not, allocate one //數據塊分配檢查
    uint32_t start_block, length;
    if(osfs_inode -> num_extents == 0 || !can_extend_last_extent(osfs_inode, *ppos, len)){
        length = (len + BLOCK_SIZE - 1) / BLOCK_SIZE;
        ret = osfs_alloc_data_block(sb_info, &start_block, length);
        
        if(ret){
            pr_err("osfs_write: Failed to allocate data block range\n");
            return ret;
        }

        if(osfs_inode -> num_extents < MAX_EXTENTS){
            struct osfs_extent *new_extent = &osfs_inode -> extents[osfs_inode -> num_extents++];
            new_extent -> start_block = start_block;
            new_extent -> length = length;
        }    
        else{
            pr_err("osfs_write: Maximum number of extents reached\n");
            return -ENOSPC;
        }
    }
    
    
 


    // Step3: Limit the write length to fit within one data block //寫入長度限制
    if(*ppos + len > BLOCK_SIZE){
        len = BLOCK_SIZE - *ppos; 
    }

    // Step4: Write data from user space to the data block //寫入數據
    data_block = sb_info -> data_blocks + osfs_inode -> i_block * BLOCK_SIZE + *ppos;
    if(copy_from_user(data_block, buf, len)){
        pr_err("osfs_write: Failed to copy data from user space\n");
        return -EFAULT;
    }

    // Step5: Update inode & osfs_inode attribute
    *ppos += len;
    bytes_written = len;

    //更新索引節點
    if(*ppos > osfs_inode -> i_size){
        osfs_inode -> i_size = *ppos;
        inode -> i_size = osfs_inode -> i_size;
    }

    //更新檔案大小及時間
    osfs_inode -> __i_mtime = osfs_inode -> __i_ctime = current_time(inode);

    mark_inode_dirty(inode); //標記為修改

    // Step6: Return the number of bytes written
    
    return bytes_written;
}

/**
 * Struct: osfs_file_operations
 * Description: Defines the file operations for regular files in osfs.
 */
const struct file_operations osfs_file_operations = {
    .open = generic_file_open, // Use generic open or implement osfs_open if needed
    .read = osfs_read,
    .write = osfs_write,
    .llseek = default_llseek,
    // Add other operations as needed
};

/**
 * Struct: osfs_file_inode_operations
 * Description: Defines the inode operations for regular files in osfs.
 * Note: Add additional operations such as getattr as needed.
 */
const struct inode_operations osfs_file_inode_operations = {
    // Add inode operations here, e.g., .getattr = osfs_getattr,
};
