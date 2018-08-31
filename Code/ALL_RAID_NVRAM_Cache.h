
#include<string.h>
#include "filetable.h"
#include <sys/time.h>
#define BLOCK_SIZE 4096
#define BLOCK_CACHE_SIZE 5000
#define No_of_disk 5
#define CACHE_SIZE 100
#define MAX_BLOCK 5000
#define MAX_CACHE_SIZE	5000
#define TRACE_FILE "trace2.csv"
#define MAX_ACCESS 70000

#define NVRAM_BLOCK_CACHE_SIZE 5000
#define NVRAM_CACHE_SIZE 1000

#define resilience 90
#define THRESHOLD 0.5
#define Del_Threshold 0.1



/**************Structure declaration************/
//typedef struct dataBuffer
//{
//	char buff[BLOCK_SIZE];
//}dataBuffer;

typedef struct block
{
	int tag;
	char buffer[BLOCK_SIZE];
}block;

typedef struct blk
{
	int modify;
	int used;
	block *bptr;
}blk;

typedef struct cacheStripe
{
	int sno;
	int dirty;
	int NVRAM_stat;
	blk block_array[No_of_disk];
	struct cacheStripe *prev;
	struct cacheStripe *next;
}cacheStripe;

typedef struct cacheQueue{
    int sno;
    struct cacheQueue *next;
    struct cacheQueue *prev;
}cacheQueue;

typedef struct cacheQueueParam{
    cacheQueue *front;
    cacheQueue *tail;
    int size; 
}cacheQueueParam;

typedef struct nvramStripe
{
	int sno;
	int dirty;
	blk block_array[No_of_disk];
	struct nvramStripe *prev;
	struct nvramStripe *next;
}nvramStripe;

typedef struct nvramQueue{
    int sno;
    struct nvramQueue *next;
    struct nvramQueue *prev;
}nvramQueue;

typedef struct nvramQueueParam{
    nvramQueue *front;
    nvramQueue *tail;
    int size; 
}nvramQueueParam;

typedef struct lNvramQueue{
    int sno;
    struct lNvramQueue *next;
    struct lNvramQueue *prev;
}lNvramQueue;

typedef struct lNvramQueueParam{
    lNvramQueue *front;
    lNvramQueue *tail;
    int size; 
}lNvramQueueParam;

typedef struct lNvramDataBlock
{
	int modify;
	int used;
	char buff[BLOCK_SIZE];
}lNvramDataBlock;

typedef struct lNvramStripe
{
	int sno;
	int dirty;
	lNvramDataBlock block_array[No_of_disk];
	struct lNvramStripe *prev;
	struct lNvramStripe *next;
}lNvramStripe;

/**************** variable declaration**********************/
//extern sqType read_io_queue;
//extern sqType write_io_queue;
block block_cache[BLOCK_CACHE_SIZE];
cacheQueueParam *qp;
cacheStripe *hashTable[CACHE_SIZE];
char writeData[BLOCK_SIZE];
/*dataBuffer db_array[No_of_disk];*/

block nvram_cache[NVRAM_BLOCK_CACHE_SIZE];
nvramStripe *nvramHashTable[NVRAM_CACHE_SIZE];
nvramQueueParam *nqp;
/*dataBuffer nvram_data_buffer[No_of_disk];*/

lNvramQueueParam *ml, *cl, *fl;
lNvramStripe *lNvramHashTable[NVRAM_CACHE_SIZE];
/*dataBuffer lNvramTransfer[No_of_disk];*/


int policy;
int cacheMiss=0;
int cacheHit=0;
int noiop=0;
int readIop=0;
int totalwriteBackCount=0;

int usedBlock = 0;
int timerC = 0;
int areadLatency = 0;
int blockCount =0;
//int modifiedStripe = 0;

int nvramblockCount=0;
int modifiedNvramStripe=0;
float diff=0.05;
float decrement=0.01;
int resThreshold;
int conThreshold;
int unnecessaryWrite=0;
int diskAccess=0;
int writeBackCount=0;
int modThreshold;


int writeMissCount=0;
int persistCount=0;
int missEvict = 0;
float preS =1;


/************** Function Declaration********************/
void initiatizeBlockCache();
void initializeHashTable();
void initializeQueueParam();
void cacheReadRequest(int stripeno,int blockno);
void databuffer_initialize();
//
void cacheQueueUpdate(int stripeno);
void hashUpdateread(int stripeno,int blockno);
int getBlockCacheIndex(int i);
int searchCache(int stripeno,int blockno);
void updateCache(int stripeno);
void denqueue();
void usedUpdateAtHash(int stripeno,int blockno);
void deleteClean();
int delligibleStripe(int stripeno);
void restoreBlocks(cacheStripe *s);
void cacheWriteRequest(int stripeno, int blockno,char writedata[]);
void hashUpdateWrite(int stripeno, int blockno);
void hashUpdateWriteHit(int stripeno, int blockno);
void deleteLRUCleanElement(int sno);
void clean_the_stripe(int stripeno);

void initiatizeNvramCache();
void initializeNvramHashTable();
void initializeNvramQueueParam();
void nvramPersist(int stripeno, int blockno);
void nvram_databuffer_initialize(int stripeno);
void nvramQueueUpdate(int stripeno);
void nvramhashUpdate(int stripeno, int blockno);
int getNvramBlockCacheIndex(int k);
int searchNvramCache(int stripeno, int blockno);
void updateNvramCache(int stripeno);
void nvram_denqueue();
void nvramUpdateAtHash(int stripeno,int blockno);
void nvram_deleteClean();
int delligibleNvramStripe(int stripeno);
void restoreNvramBlocks(nvramStripe *s);
void deleteNvramLRUCleanElement(int sno);
void write_to_disk();
void nvramQueueContent();

void initializelNvramTransfer();
void initializeLargeNvramHashTable();
void initializeNvramMLQueueParam();
void initializeNvramCLQueueParam();
//void initializeNvramFLQueueParam();
void store(int sno);
void add_cl(int sno);
void add_ml(int sno);
void addLNvramHash(int sno);
int searchInML(int sno);
int searchInCL(int sno);
void lNvramPersist(int stripeno,int blockno);
void updateLNvramHash(int stripeno,int blockno);
void addLNvramHashPersist(int sno,int blockno);
//void addLNvramHash(int sno,int blockno);
void writeback_to_disk();
void store_to_CL(lNvramQueue *t);
void putPending(int sno);
void putClean(int sno);
float estimated_S();
int searchInNVRAM(int stripeno);
void show_ml();
void show_cl();
void deletelNvramHashTable(int sno);
void transferCL_to_ML();






