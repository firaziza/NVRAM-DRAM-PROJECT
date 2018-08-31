#ifndef FILETABLE_H
#define FILETABLE_H
#pragma once
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<pthread.h>
#include<iostream>
#include<string>
#include <list>
#include <vector>
#include <sys/time.h>
#include <semaphore.h>
using namespace std;
#define IO_QUEUE_SIZE 20
//#include<assert.h>
#define DEVICE_NUMBER	5
#define BLOCK_SIZE	4096  //define in fsc
#define DISK_SIZE	20000 //change to 8GB or 2000 000 blocks in disk
#define FILE_DATA	"file_size2.csv"
#define REVERSED_WRITE_BACK_P 0.8
#define DISK_CAPICITY 0.7
#define File_number	500
#define WRITEFILE       "or_write_latency_100vs10.csv"
extern int queue_size;
extern pthread_mutex_t mx_readqueue;
extern pthread_cond_t full_readqueue;
extern pthread_cond_t empty_readqueue;
extern pthread_mutex_t mx_writequeue;
extern pthread_cond_t full_writequeue;
extern pthread_cond_t empty_writequeue;
extern pthread_cond_t checkempty_readqueue;
extern pthread_mutex_t checkempty_mutex;
//extern std:: condition_variable vc_readqueue;
//extern std:: condition_variable vc_writequeue;
extern FILE *writeLatency;
typedef struct dataBuffer
{
	char buff[BLOCK_SIZE];
}dataBuffer;


class IOrq_event{
public:

    IOrq_event(int stripeno){ 
            strpno=stripeno;
	    start = 0;
	    stop = 0;
            
    }
    IOrq_event(){
	strpno = 0;
	start = 0;
	stop = 0;
    }

    ~IOrq_event(){};
    long int start;
    long int stop; 
    int strpno;
   
    enum State{
        IDLE,
        PENDING,
        WAITING,
        FINISHED
    };
    State state;

};
//const size_t fixedsize = 10;
//typedef 
enum QueueResult{
    OK = 1,
    CLOSED = -1
};
enum ProcessStatus{
    FAIL = -1,
    SUCESS = 1
};
void *processReadQueue(void *argument);
void *processWriteQueue(void *argument);
//#include "FSC.h"
struct disk{
	int disk_num;
	int current_offset;

};

typedef struct arg_struct{
    int  sno;
    int i;

}arg_struct;

typedef struct arg_struct_ioqueue{
    std::list<IOrq_event> read_io_queue;
    std::list<IOrq_event> write_io_queue;
}arg_struct_ioqueue;
/*
typedef struct ioreq_event{
	int flags;
	int blkno;
	int fileid;

}ioreq_event;
i*/
typedef struct arg_struct_read{
	int  sno;
	int i;
	char *buffer;

}arg_struct_read;
struct file_entry{
	int fileid;
	int filesize;
	int start_disk;
	int start_offset;
};

extern dataBuffer db_array[DEVICE_NUMBER];
extern dataBuffer nvram_data_buffer[DEVICE_NUMBER];
extern dataBuffer lNvramTransfer[DEVICE_NUMBER];
extern std::list<IOrq_event> read_io_queue;
extern std::list<IOrq_event> write_io_queue;
extern char file_table_writeData[BLOCK_SIZE];
extern int total_files;
void raid_create(int device_number);
int is_parity(int disk_num,int stripe_number);
void ini_file_table();
int total_file_num(FILE *file_data);
int file_table_create();
int allocate_disk_filetable();
//file_entry * find(int fileid);
int requestStripeNm(int file_name,int block_num);
void write_back(int stripenum,char *p,int i);
file_entry * find(int fileid);
void initial_ptr();
void initial_block();
void initial_file_entry();
int getblocknum(int fileid,int block_num);
//void create_full_stripe_read_threads(cacheStripe newStripe1);
void *threadWork(void *arg);
void *readInThread(void *argument);
void writeBack(int sno);
int getParitybnum(int stripe_num);
void read_push_back(int stripno);
void write_push_back(int stripno);
#endif
