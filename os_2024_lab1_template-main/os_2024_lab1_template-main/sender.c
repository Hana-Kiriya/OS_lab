#include "sender.h"


#define QUEUE_NAME "/posix_queue"
#define SHM_NAME "/shm_comm"
#define MSG_STOP ""

void send(message_t message, mailbox_t* mailbox_ptr){
    /*  TODO: 
        1. Use flag to determine the communication method
        2. According to the communication method, send the message
    */
    if(mailbox_ptr -> flag == 1){
        //msgsnd： 將 message 發送到mailbox_ptr -> storage.msqid 的佇列中
        //不計mtype大小，僅從data[]開始計算，，所以減去sizeof(long)
        if(mq_send(mailbox_ptr -> storage.mqdes, message.data, message.size, 0) == -1){
            perror("mq_send failed");
            exit(1);
        }
    }
    else if(mailbox_ptr -> flag == 2){
        memcpy(mailbox_ptr -> storage.shm_addr, &message, sizeof(message_t)); //將message的內容複製到共享記憶體區
    }
    else{
        printf("Unknown communication mechanism\n");
        exit(1);
    } 
}

int main(int argc, char* argv[]){
    /*  TODO: 
        1) Call send(message, &mailbox) according to the flow in slide 4
        2) Measure the total sending time
        3) Get the mechanism and the input file from command line arguments
            • e.g. ./sender 1 input.txt
                    (1 for Message Passing, 2 for Shared Memory)
        4) Get the messages to be sent from the input file
        5) Print information on the console according to the output format
        6) If the message form the input file is EOF, send an exit message to the receiver.c
        7) Print the total sending time and terminate the sender.c
    */
    if(argc < 3){ //0: program name, 1: mechanism flag, 2: input file name
        printf("Usage: %s <mechanism> <input file>\n", argv[0]); 
        exit(1);
    }

    int mechanism = atoi(argv[1]);
    char* input_file = argv[2];
    
    mailbox_t mailbox;
    mailbox.flag = mechanism;
    

    //創資源
    
    if(mechanism == 1){
        struct mq_attr attr;
        attr.mq_flags = 0;
        attr.mq_maxmsg = 1; //佇列最多儲存數量
        attr.mq_msgsize = 1024;
        attr.mq_curmsgs = 0; //訊息佇列當前無訊息(後續由系統自動管理)
        
        mailbox.storage.mqdes = mq_open(QUEUE_NAME, O_CREAT | O_WRONLY, 0644, &attr); //WRONLY:只寫入
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

        ftruncate(shm_fd, sizeof(message_t)); //設定共享記憶體大小
        mailbox.storage.shm_addr = mmap(0, sizeof(message_t), PROT_WRITE | PROT_READ, PROT_SHARED, shm_fd, 0); //mmap: 映射共享記憶體區到程式的虛擬地址中
        if(mailbox.storage.shm_addr  == MAP_FAILED){
            perror("mmap failed");
            exit(1);
        }
        printf("\e[1;36mShared Memory\e[m\n");
    }
    
    FILE* file = fopen(input_file, "r");
    if(!file){
        perror("fopen failed");
        exit(1);
    }

    message_t message;
    char buffer[1024];
    struct timespec start, end;
    double time_taken = 0.0;
    while(fgets(buffer, 1024, file)){ //一次讀一行
        message.size = strlen(buffer);
        strncpy(message.data, buffer, 1024);
        clock_gettime(CLOCK_MONOTONIC, &start);
        send(message, &mailbox);
        clock_gettime(CLOCK_MONOTONIC, &end);
        printf("\e[1;36mSending message: \e[m%s\n", message.data);    
        
        time_taken += (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) * 1e-9;
    }
    fclose(file);

    strncpy(message.data, MSG_STOP, 1024);
    message.size = strlen(message.data);
    send(message, &mailbox);
    if(mechanism == 1){
        mq_close(mailbox.storage.mqdes);
        mq_unlink(QUEUE_NAME);
    }
    else if(mechanism == 2){
        munmap(mailbox.storage.shm_addr, sizeof(message_t));
        shm_unlink(SHM_NAME);
    }
    printf("\e[1;31mEnd of input file! exit!\e[m\n");
    printf("Total time taken in sending msg: %f s\n", time_taken);

    return 0;
}