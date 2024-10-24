#include "receiver.h"

#define QUEUE_NAME "/posix_queue"
#define SHM_NAME "/shm_comm"
#define MSG_STOP "End"

void receive(message_t* message_ptr, mailbox_t* mailbox_ptr){
    /*  TODO: 
        1. Use flag to determine the communication method
        2. According to the communication method, receive the message
    */
    if(mailbox_ptr -> flag == 1){
        ssize_t received_bytes = mq_receive(mailbox_ptr -> storage.mqdes, message_ptr -> data, 1024, NULL);
        if(received_bytes == -1){ //1024: attr.mq_msgsize = 1024, NULL: 忽略優先級
            perror("mq_send failed");
            exit(1);
        }
        message_ptr -> size = received_bytes;
        message_ptr -> data[message_ptr -> size] = '\0';
    }
    else if(mailbox_ptr -> flag == 2){
        memcpy(message_ptr, mailbox_ptr -> storage.shm_addr, sizeof(message_t)); //將共享記憶體區的內容複製到message
    }
    else{
        printf("Unknown communication mechanism\n");
        exit(1);
    } 
}

int main(int argc, char* argv[]){
    /*  TODO: 
        1) Call receive(&message, &mailbox) according to the flow in slide 4
        2) Measure the total receiving time
        3) Get the mechanism from command line arguments
            • e.g. ./receiver 1
        4) Print information on the console according to the output format
        5) If the exit message is received, print the total receiving time and terminate the receiver.c
    */
    if(argc < 2){
        printf("Usage: %s <mechanism>\n", argv[0]); 
        exit(1);
    }

    int mechanism = atoi(argv[1]);
    mailbox_t mailbox;
    mailbox.flag = mechanism;

    sem_t *send_sem = sem_open("/send_sem", O_CREAT, 0644, 0); //initial = 0 //transfer this
    sem_t *rec_sem = sem_open("/rec_sem", O_CREAT, 0644, 0); //initial = 0 //wait this
    
    if(send_sem == SEM_FAILED || rec_sem == SEM_FAILED){
        perror("sem_open failed");
        exit(1);
    }
    
    if(mechanism == 1){
        struct mq_attr attr;
        attr.mq_flags = 0;
        attr.mq_maxmsg = 1; //佇列最多儲存數量
        attr.mq_msgsize = 1024;
        attr.mq_curmsgs = 0; //訊息佇列當前無訊息(後續由系統自動管理)

        mailbox.storage.mqdes = mq_open(QUEUE_NAME, O_CREAT | O_RDONLY, 0644, &attr); //WRONLY:只寫入
        if(mailbox.storage.mqdes == (mqd_t)-1){
            perror("mq_open failed");
            exit(1);
        }
        
        printf("\e[1;36mMessage Passing\e[m\n");
    }
    else if(mechanism == 2){
        int shm_fd;
        shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666); //RDWR:共享記憶體需讀取與寫入
        if(shm_fd == -1){
            perror("shm_open failed");
            exit(1);
        }
        if(ftruncate(shm_fd, sizeof(message_t)) == -1){//設定共享記憶體大小
            perror("ftruncate failed");
            exit(1);
        } 
        
        mailbox.storage.shm_addr = mmap(0, sizeof(message_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0); //mmap: 映射共享記憶體區到程式的虛擬地址中
        if(mailbox.storage.shm_addr == MAP_FAILED){
            perror("mmap failed");
            exit(1);
        }
        printf("\e[1;36mShared Memory\e[m\n");
    }

    message_t message;
    struct timespec start, end;
    double time_taken = 0.0;

    while(1){
        sem_wait(rec_sem);
        clock_gettime(CLOCK_MONOTONIC, &start);
        receive(&message, &mailbox);
        clock_gettime(CLOCK_MONOTONIC, &end);
        
        if(strcmp(message.data, MSG_STOP) == 0){
            break;
        }

        printf("\e[1;36mReceiving message: \e[m %s", message.data); 
        time_taken += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
        sem_post(send_sem);
    }

    if(mechanism == 1){
        mq_close(mailbox.storage.mqdes);
        mq_unlink(QUEUE_NAME);
    }
    else if(mechanism == 2){
        munmap(mailbox.storage.shm_addr, sizeof(message_t));
        shm_unlink(SHM_NAME);
    }
    printf("\e[1;31\nmSender exit!\e[m\n");
    printf("Total time taken in receiving msg: %f s\n", time_taken);
    memset(&message, 0, sizeof(message_t));
    sem_close(send_sem);
    sem_close(rec_sem);
    sem_unlink("/send_sem");
    sem_unlink("/rec_sem");
    return 0;
}
