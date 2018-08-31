
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include<string.h>
//#include "global.h"
#include "ALL_RAID_NVRAM_Cache.h"
#include "filetable.h"
#include <mutex>
#include <thread>
#include <queue>
#include <unordered_map>
#include <forward_list>


int queue_size = REVERSED_WRITE_BACK_P*DISK_CAPICITY*100;
std::list<IOrq_event> read_io_queue;
std::list<IOrq_event> write_io_queue;
pthread_mutex_t mx_readqueue = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t empty_readqueue = PTHREAD_COND_INITIALIZER;
pthread_cond_t full_readqueue = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mx_writequeue = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t empty_writequeue = PTHREAD_COND_INITIALIZER;
pthread_cond_t full_writequeue = PTHREAD_COND_INITIALIZER;

int main()
{
    int blockno, stripeno, operation;
    initiatizeBlockCache();
    initializeHashTable();
    initializeQueueParam();

    initiatizeNvramCache();
    initializeNvramHashTable();
    initializeNvramQueueParam();
    modThreshold=NVRAM_CACHE_SIZE*THRESHOLD;

    initializelNvramTransfer();
    initializeLargeNvramHashTable();
    initializeNvramMLQueueParam();
    initializeNvramCLQueueParam();

    /********** For I/O ***************/
    char line[200];
    raid_create(No_of_disk);
    ini_file_table();
    file_table_create();
    allocate_disk_filetable();
    int fileid,offset;
    FILE *fp=fopen(TRACE_FILE,"r");
    unsigned int i=0;

    int maxres=(int)((resilience+diff)*NVRAM_CACHE_SIZE);
    //utilityThreshold= (No_of_disk - 2.0)/3.0;

//	writeLatency = fopen("write_latency_3000.csv","w");
    struct timeval stop, start;

    /***** Accessing trace ************/
    printf("\n Select The Policy:\n Select 1 for only DRAM\n Select 2 for DRAM and NVRAM to persist\n Select 3 for DRAM and NVRAM persis and store evicted element from DRAM\n");
    scanf("%d",&policy);
    //create a io queue thread
    pthread_t thread1;
    pthread_t thread2;
    arg_struct_ioqueue args1;
    args1.read_io_queue = read_io_queue;
    args1.write_io_queue = write_io_queue;
    pthread_create(&thread1,NULL,processReadQueue,(void *)&args1);
    pthread_create(&thread2,NULL,processWriteQueue,(void *)&args1);
    printf("created\n");
    for(i=0;i<MAX_ACCESS;i++)
    {

        printf("\ni the access : %d *********************************************************************** ", i);
        fscanf(fp,"%d,%d,%d\n",&operation,&fileid,&offset);
        stripeno=requestStripeNm(fileid,offset);
        blockno=getblocknum(fileid,offset);
        //	printf("\n requested Stripe no: %d", stripeno);

        if(policy==1)
        {
            if(operation==0)
            {
                //	printf("\n Read operation");
                cacheReadRequest(stripeno,blockno);
            }
            else
            {
                //	printf("\n Write Operation");
                memset(writeData,'1',BLOCK_SIZE);
                cacheWriteRequest(stripeno,blockno,writeData);
                //		printf("\n Cache write is done");
                //	gettimeofday(&start,NULL);
                writeBack(stripeno);
                //	gettimeofday(&stop,NULL);
                //	fprintf(writeLatency,"%lu\n",stop.tv_usec-start.tv_usec);
                clean_the_stripe(stripeno);
            }
        }

        else if(policy==2)
        {

            if(operation==0)
            {
                //	printf("\n Read operation");
                cacheReadRequest(stripeno,blockno);
            }
            else
            {

                //	printf("\n Write Operation");
                memset(writeData,'1',BLOCK_SIZE);
                cacheWriteRequest(stripeno,blockno,writeData);
                //	printf("\n Cache write is done");
                nvramPersist(stripeno,blockno);
                if(modifiedNvramStripe>modThreshold)
                {
                    //printf("write to dislk\n");
                    write_to_disk();
                }
                clean_the_stripe(stripeno);
            }
        }

        else if(policy==3)
        {
            printf ("\n Policy 3");

            //void initializeNvramFLQueueParam();


            if(operation==0)
            {
                cacheReadRequest(stripeno,blockno);
            }
            else
            {
                memset(writeData,'1',BLOCK_SIZE);
                cacheWriteRequest(stripeno,blockno,writeData);
                //lNvramPersist(stripeno,blockno);
                clean_the_stripe(stripeno);
                if(persistCount>=10)
                {
                    resThreshold=resilience-diff*NVRAM_CACHE_SIZE;
                    //conThreshold=0.5*NVRAM_CACHE_SIZE;
                    printf("\n Estimated S=%f",estimated_S());
                    if((estimated_S()-diff)>diff)
                    {
                        conThreshold=(int)((estimated_S()-diff)*NVRAM_CACHE_SIZE);
                    }

                    else
                    {
                        conThreshold=(int)(diff*NVRAM_CACHE_SIZE);
                    }

                    if(diff==0.0)
                    {
                        diff=0.05;
                    }
                    printf("\n conThreshold: %d",conThreshold);
                    printf("\n resThreshold: %d",resThreshold);
                    if(ml->size>resThreshold || ml->size>conThreshold)
                    {
                        //printf("write to dislk\n");
                        writeBackCount++;
                        writeback_to_disk();
                        if(writeBackCount>0.1*ml->size)
                            writeBackCount=0;
                        /*if(diff==0.05)
                        {
                            writeBackCount=1;
                        }
                        else
                        {
                            writeBackCount++;
                        }
                        diff=diff-decrement;*/

                    }
                    persistCount=0;
                    writeMissCount=0;
                    missEvict=0;
                }
            }
        }
    }
    printf("\n Cache hit:%d",cacheHit);
    printf("\n totalwriteBackCount:%d",totalwriteBackCount);
    printf("\n diskAccess:%d",diskAccess);
    printf("\n%d %d",read_io_queue.size(),write_io_queue.size());

    printf("\n");
    printf("\n%d %d",read_io_queue.size(),write_io_queue.size());
    exit(0);
    pthread_join(thread1,NULL);
    pthread_join(thread2,NULL);
//	fclose(writeLatency);
}

void initiatizeBlockCache()
{
    int i;
    for(i=0;i<BLOCK_CACHE_SIZE;i++)
    {
        block_cache[i].tag=0;
        memset(block_cache[i].buffer,'0',BLOCK_SIZE);
    }
}

void initializeHashTable()
{
    int i;
    for(i=0;i<CACHE_SIZE;i++)
    {
        hashTable[i]=NULL;
    }
}

void initializeQueueParam()
{
    qp=(cacheQueueParam *)malloc(sizeof(cacheQueueParam));
    qp->front=NULL;
    qp->tail=NULL;
    qp->size=0;
}


/************ Functions for cache Read Request *******************/
void cacheReadRequest(int stripeno, int blockno)
{
    printf("\n In cacheReadRequest");
    if(qp->front==NULL && qp->tail==NULL)
    {
        printf("\nCompulsory Miss");
        databuffer_initialize();
        //readIn(stripeno);
        //	IOrq_event ioevent(stripeno);
        //	read_io_queue.push_back(ioevent);
        read_push_back(stripeno);
        //add_cl(stripeno);
        //addLNvramHash(stripeno);
        noiop++;
        readIop++;
        cacheQueueUpdate(stripeno);
        hashUpdateread(stripeno,blockno);
        cacheMiss++;
    }
    else
    {
        //hit
        if(searchCache(stripeno,blockno)==1)
        {
            printf("\nHit in DRAM");
            updateCache(stripeno);
            usedUpdateAtHash(stripeno,blockno);
            //cacheHit++;
        }
        else
            //miss
        {
            if(qp->size<CACHE_SIZE)
            {
                printf("\nMiss in DRAM. Dram has available space");
                databuffer_initialize();
                //readIn(stripeno);
                // IOrq_event ioevent(stripeno);
                //	read_io_queue.push_back(ioevent);
                read_push_back(stripeno);
                noiop++;
                readIop++;
                if(policy==3)
                {
                    printf("\n NVRAM check");
                    if(searchInNVRAM(stripeno)!=1)
                    {
                        printf("\n NVRAM check failed");
                        read_push_back(stripeno);
                        //add_cl(stripeno);
                        //addLNvramHash(stripeno);
                    }
                    else
                    {
                        if(searchInML(stripeno)==1 ||searchInCL(stripeno)==1)
                        {
                            printf("\n Updated list");
                        }
                    }

                }
                else
                {
                    read_push_back(stripeno);
                }
                cacheQueueUpdate(stripeno);
                hashUpdateread(stripeno,blockno);
                cacheMiss++;
            }
                /*Capacity Miss*/
            else
            {
                printf("\nMiss in DRAM. Dram has no available space");
                //generatePolicy(stripeno);
                deleteClean();
                databuffer_initialize();
                //  IOrq_event ioevent1(stripeno);
                //  read_io_queue.push_back(ioevent1);
                //  readIn(stripeno);
                if(policy==3)
                {
                    if(searchInNVRAM(stripeno)!=1)
                    {
                        printf("\n NVRAM check failed");
                        read_push_back(stripeno);
                        //add_cl(stripeno);
                        //addLNvramHash(stripeno);
                    }
                    else
                    {
                        if(searchInML(stripeno)==1 ||searchInCL(stripeno)==1)
                        {
                            printf("\n Updated list");
                        }
                    }
                }
                else
                {
                    read_push_back(stripeno);
                }
                //printf("\n Choice of parity\n 1. Maintaining parity inside cache \n 2. Not maintaining parity inside cache ");
                //scanf("%d",&paritySelection);
                cacheQueueUpdate(stripeno);
                hashUpdateread(stripeno,blockno);
                cacheMiss++;
            }

        }

    }
}

void hashUpdateread(int stripeno,int blockno)
{
    int hashIndex=stripeno%CACHE_SIZE;
    cacheStripe *newStripe=(cacheStripe *)malloc(sizeof(cacheStripe));
    if(newStripe!=NULL)
    {
        newStripe->sno=stripeno;//Set stripeno
        newStripe->dirty=0;//Set dirtybit
        newStripe->NVRAM_stat=0;
        int i=0;
        for(i=0;i<No_of_disk;i++)//Set Stripe block
        {
            //printf("\n\t\t i=%d",i);
            int block_cache_index=getBlockCacheIndex(i);
            //printf("\n\t\t %d th block_cache_index=%d",i,block_cache_index);
            newStripe->block_array[i].modify=0;
            if(i==blockno)
            {
                newStripe->block_array[i].used=1;
                usedBlock++;
            }
            else
                newStripe->block_array[i].used=0;
            newStripe->block_array[i].bptr=&block_cache[block_cache_index];
        }
        newStripe->prev=NULL;
        newStripe->next=NULL;
    }
    cacheStripe *t=hashTable[hashIndex];
    if(t==NULL)
    {

        hashTable[hashIndex]=newStripe;
    }
    else
    {
        newStripe->next=t;
        newStripe->prev=NULL;
        t->prev=newStripe;
        t=newStripe;
        hashTable[hashIndex]=t;
    }
}


/*********** Functions for common Read and write request ********/
void databuffer_initialize()
{
    int i;
    for(i=0;i<No_of_disk;i++)
    {
        memset(db_array[i].buff,'0',BLOCK_SIZE);
    }
}
/*
void readIn(int sno)
{

	struct timeval stop, start;
	int ret;
	int i;
	pthread_mutex_t mutex1;
	//cacheStripe *newStripe=(cacheStripe *)malloc(sizeof(cacheStripe));
	pthread_t thread1,thread2,thread3,thread4,thread0;
	arg_struct_read args1,args2,args3,args4,args0;
//	memset(file_table_writeData,'1',BLOCK_SIZE);
	gettimeofday(&start, NULL);
	pthread_mutex_init(&mutex1,NULL);
	pthread_mutex_lock(&mutex1);
	//printf("\nread in \n");
	//args1=malloc(sizeof(arg_struct_read));
	args0.sno=sno;
	args0.i=0;
	args0.buffer=db_array[0].buff;
	args2.sno=sno;
	args2.i=2;
	args0.buffer=db_array[2].buff;
	args3.sno=sno;
	args3.i=3;
	args3.buffer=db_array[3].buff;
	args4.sno=sno;
	args4.i=4;
	args4.buffer=db_array[4].buff;
	args1.sno=sno;
	args1.i=1;
	args1.buffer=db_array[1].buff;
	timerC++;
	pthread_create(&thread0,NULL,threadWork,(void *)&args0);
	pthread_create(&thread1,NULL,threadWork,(void *)&args1);
	pthread_create(&thread2,NULL,threadWork,(void *)&args2);
	pthread_create(&thread3,NULL,threadWork,(void *)&args3);
	pthread_create(&thread4,NULL,threadWork,(void *)&args4);
	pthread_join(thread1,NULL);
	pthread_join(thread2,NULL);
	pthread_join(thread3,NULL);
	pthread_join(thread4,NULL);
	pthread_join(thread0,NULL);
//	printf("before mutex\n");
	pthread_mutex_unlock(&mutex);
	//printf("after mutex\n");
	//printf("\nread in \n");
	gettimeofday(&stop, NULL);
	areadLatency = areadLatency + (stop.tv_usec-start.tv_usec);
	fprintf(printout,"%lu\n", stop.tv_usec-start.tv_usec);
	fflush(printout);
	//printf("read %lu\n",stop.tv_usec-start.tv_usec);
}*/

void cacheQueueUpdate(int stripeno)
{
//	printf("\n In cacheQueueUpdate");
    cacheQueue *newNode= (cacheQueue *)malloc(sizeof(cacheQueue));
    newNode->sno=stripeno;
    newNode->next=NULL;
    newNode->prev=NULL;

    /************ Queue front and tail adjustment************/
    if(qp->front==qp->tail && qp->front==NULL)
    {
        qp->front=newNode;
        qp->tail = newNode;
    }
    else if(qp->front==qp->tail)
    {
        qp->front->next=newNode;
        qp->tail=newNode;
        newNode->prev=qp->front;
    }
    else
    {
        qp->tail->next=newNode;
        newNode->prev=qp->tail;
        qp->tail=newNode;
    }
    qp->size++;
}
//question??
int getBlockCacheIndex(int k)
{
    int i;
    int x;
    for(i=0;i<BLOCK_CACHE_SIZE;i++)
    {
        if(block_cache[i].tag==0)
        {
            strcpy(block_cache[i].buffer,db_array[k].buff);
            block_cache[i].tag=1;
            blockCount++;
            x=i;
            break;
        }
    }
    return x;
}

int searchCache(int stripeno,int blockno)
{
    int found=0;
    int count=0;
    //int hashIndex=stripeno%CACHE_SIZE;
    //cacheQueue *temp=qp->front;
    //cacheStripe *t=hashTable[hashIndex];
    cacheQueue *q=qp->front;
    while(q!=NULL)
    {
        if(q->sno==stripeno)
        {
            int hashIndex=stripeno%CACHE_SIZE;
            cacheStripe *t=hashTable[hashIndex];
            while(t!=NULL)
            {
                if(t->sno==stripeno && t->block_array[blockno].bptr!=NULL)
                {
                    found=1;
                    cacheHit++;
                    break;
                }
                else
                {
                    t=t->next;
                }
            }
            break;
        }
        else
        {
            q=q->next;
        }
    }
    return found;
}

void updateCache(int stripeno)
{
    if(qp->front==qp->tail)
    {
        //printf("\n Updating Null list");
        return;
    }
    else if(qp->front->sno == stripeno)
    {
        //printf("\n The front element");
        denqueue();
    }
    else if(qp->tail->sno == stripeno)
    {
        //printf("\n The tail element");
        return;
    }
    else
    {
        cacheQueue *q = qp->front->next;
        while(q!=NULL)
        {
            if(q->sno==stripeno)
            {
                q->prev->next=q->next;
                q->next->prev=q->prev;
                q->prev=qp->tail;
                qp->tail->next=q;
                qp->tail=q;
                q->next=NULL;
                break;
            }
            else
            {
                q=q->next;
            }
        }
    }
}

void denqueue()
{
    //printf("\n denqueue");
    cacheQueue *r  = qp->front;
    qp->front = qp->front->next;//Update front to next element
    qp->tail->next=r;//Append the requested node to the tail of the queue
    r->prev=qp->tail;
    r->next=NULL;
    qp->front->prev=NULL;
    qp->tail=r;
    //printCacheQueue();
}

void usedUpdateAtHash(int stripeno,int blockno)
{
    //printf("\n usedUpdateAtHash");
    int hashIndex=stripeno%CACHE_SIZE;
    cacheStripe *s=hashTable[hashIndex];
    while(s!=NULL)
    {
        if(s->sno==stripeno)
        {
            if(s->block_array[blockno].used!=1)
            {
                s->block_array[blockno].used=1;
                usedBlock++;
            }
            break;
        }
        else
        {
            s=s->next;
        }
    }
}

void deleteClean()
{
    printf("\n deleteClean");
    int sno;
    cacheQueue *td=qp->front;
    while(td!=NULL)
    {
        sno=td->sno;
        if(delligibleStripe(sno)==1)
        {
            deleteLRUCleanElement(sno);
            if(policy==3)
            {
                store(sno);

            }
            else
            {

            }
            break;
        }
        else
        {
            td=td->next;
        }
    }
    //printCacheQueue();
//	printf("\n\tThe number of modified Stripes: %d",modifiedStripe);
//	printf("\n\tThe number of used blocks: %d",usedBlock);
//	printf("\n The number of unnecessary writes: %d",unnecessaryWrite);
    qp->size--;
}

int delligibleStripe(int stripeno)
{
    printf("\n In delligibleStripe	");
    int elligible=0;
    int hashIndex=stripeno%CACHE_SIZE;
    cacheStripe *t=hashTable[hashIndex];
    //cacheStripe *s;
    while(t!=NULL)
    {
        //printf("\n In delligibleStripe	In while current stripe: %d",t->sno);
        if(t->sno==stripeno && t->dirty==0)
        {
            elligible=1;
            if(t->prev!=NULL && t->next==NULL)
            {
                t->prev->next=NULL;
            }
            else if(t->prev==NULL && t->next!=NULL)
            {
                hashTable[hashIndex]=t->next;
            }
            else if(t->prev!=NULL && t->next!=NULL)
            {
                t->prev->next=t->next;
                t->next->prev=t->prev;
            }
            else
            {
                hashTable[hashIndex]=NULL;
            }
            restoreBlocks(t);
            break;
        }
        else
        {
            //printf("\n In delligibleStripe	In while In else");
            t=t->next;
        }
        printf("\n Elligible = %d",elligible);
    }
    //free(s);
    return elligible;
}

void restoreBlocks(cacheStripe *s)
{
    printf("\n Restore Block");
    int i;
    for(i=0;i<No_of_disk;i++)
    {
        if(s->block_array[i].bptr!=NULL)
        {
            if(policy==3)
                memcpy(lNvramTransfer[i].buff,s->block_array[i].bptr->buffer,BLOCK_SIZE);
            s->block_array[i].bptr->tag=0;
            //blockCount--;
            //s->block_array[i].bptr=NULL;
            // printf("\n StripeNo:%d\tBlockno:%d\tAddress of Block:%p",s->sno,i,s->block_array[i].bptr);
        }
        //printf("\n after loop");
    }
}

void deleteLRUCleanElement(int sno)
{
    printf("\n deleteLRUCleanElement");
    cacheQueue *t=qp->front;
    while(t!=NULL)
    {
        //printf("\n The current stripe number is %d",t->sno);
        if(t->sno==sno)
        {
            if(t->prev==NULL && t->next!=NULL)
            {
                t->next->prev=NULL;
                qp->front=t->next;
            }
            else if(t->prev!=NULL && t->next!=NULL)
            {
                t->prev->next=t->next;
                t->next->prev=t->prev;
            }
            else if(t->prev!=NULL && t->next==NULL)
            {
                t->prev->next=NULL;
                qp->tail=t->prev;
            }
            else
            {
                qp->front=NULL;
                qp->tail=NULL;
            }
            free(t);
            break;
        }
        else
        {
            t=t->next;
        }
    }
}


/*********** Functions for Cache Write Requests *************/
void cacheWriteRequest(int stripeno, int blockno,char writedata[])
{
    printf("\n In CacheWriteRequest");
    if(qp->front==NULL && qp->tail==NULL)
    {
        printf("\n In Compulsory Miss");//Compulsory Miss
        databuffer_initialize();
        //	readIn(stripeno);
        //IOrq_event ioevent(stripeno);
        //read_io_queue.push_back(ioevent);
        read_push_back(stripeno);
        add_ml(stripeno);
        addLNvramHashPersist(stripeno,blockno);
        persistCount+=1;
        noiop++;
        readIop++;
        cacheQueueUpdate(stripeno);
        hashUpdateWrite(stripeno,blockno);
        //	printf("\n Check");
    }
    else
    {
        //hit
        printf("\n hit");
        if(searchCache(stripeno,blockno)==1)
        {
            updateCache(stripeno);
            hashUpdateWriteHit(stripeno,blockno);
            //cacheHit++;
        }
        else
            //miss
        {
            printf("\n In Miss but DRAM has space");
            if(qp->size<CACHE_SIZE)
            {
                databuffer_initialize();
                //readIn(stripeno);
                //IOrq_event ioevent2(stripeno);
                //	read_io_queue.push_back(ioevent2);
                read_push_back(stripeno);
                noiop++;
                readIop++;
                if(policy==3)
                {
                    printf("\n NVRAM check");
                    if(searchInNVRAM(stripeno)!=1)
                    {
                        printf("\n NVRAM check failed");
                        read_push_back(stripeno);
                        add_ml(stripeno);
                        addLNvramHashPersist(stripeno,blockno);
                        persistCount+=1;
                    }
                    else
                    {
                        if(searchInML(stripeno)==1)
                        {
                            printf("\nML list is updated");
                        }
                        if(searchInCL(stripeno)==1)
                        {
                            //when searchInCL() is called the target node is placed at front of CL list
                            transferCL_to_ML();
                        }
                        updateLNvramHash(stripeno,blockno);
                        persistCount+=1;
                    }

                }
                else
                {
                    read_push_back(stripeno);
                }
                cacheQueueUpdate(stripeno);
                hashUpdateWrite(stripeno,blockno);
            }
                /*Capacity Miss*/
            else
            {
                printf("\n In Miss but DRAM has no space");
                //generatePolicy(stripeno);
                deleteClean();
                databuffer_initialize();
                //	readIn(stripeno);
                //IOrq_event ioevents(stripeno);
                //read_io_queue.push_back(ioevents);
                if(policy==3)
                {
                    printf("\n NVRAM check");
                    if(searchInNVRAM(stripeno)!=1)
                    {
                        printf("\n NVRAM check failed");
                        read_push_back(stripeno);
                        add_ml(stripeno);
                        addLNvramHashPersist(stripeno,blockno);
                        persistCount+=1;
                    }
                    else
                    {
                        if(searchInML(stripeno)==1)
                        {
                            printf("\nML list is updated");
                        }
                        if(searchInCL(stripeno)==1)
                        {
                            //when searchInCL() is called the target node is placed at front of CL list
                            transferCL_to_ML();
                            updateLNvramHash(stripeno,blockno);
                            persistCount+=1;
                        }
                    }
                }
                else
                {
                    read_push_back(stripeno);
                }

                noiop++;
                readIop++;
                //printf("\n Choice of parity\n 1. Maintaining parity inside cache \n 2. Not maintaining parity inside cache ");
                //scanf("%d",&paritySelection);
                cacheQueueUpdate(stripeno);
                hashUpdateWrite(stripeno,blockno);
            }

        }

    }
}

void hashUpdateWrite(int stripeno, int blockno)
{
//	printf("\n hashUpdateWrite");
    int hashIndex=stripeno%CACHE_SIZE;
    cacheStripe *newStripe=(cacheStripe *)malloc(sizeof(cacheStripe));
    if(newStripe!=NULL)
    {
        newStripe->sno=stripeno;//Set stripeno
        newStripe->dirty=1;//Set dirtybit
        newStripe->NVRAM_stat = 0;
        //memset(newStripe->partialparity,'0',BLOCK_SIZE);//Set partial Parity
        int i=0;
        for(i=0;i<No_of_disk;i++)//Set Stripe block
        {
            int block_cache_index=getBlockCacheIndex(i);

            newStripe->block_array[i].bptr=&block_cache[block_cache_index];

            //block_cache[block_cache_index].tag=1;
            if(i==blockno)
            {
                memcpy(block_cache[block_cache_index].buffer,writeData,BLOCK_SIZE);
                newStripe->block_array[i].modify=1;
                newStripe->block_array[i].used=1;
                //if(paritySelection==1)
                //updatePartialParity(newStripe->partialparity,newStripe->block_array[i].bptr->buffer,BLOCK_SIZE);
                usedBlock++;
            }
            else
            {
                newStripe->block_array[i].modify=0;
                newStripe->block_array[i].used=0;
            }
            //	printf("\n blockno: i=%d",i);
            //			printf("\n %dth block address: %p",i,newStripe->block_array[i].bptr);
        }

        newStripe->prev=NULL;
        newStripe->next=NULL;
    }
    cacheStripe *t=hashTable[hashIndex];
//	printf("\nhashIndex is %d\n",hashIndex);
//	printf("\n t=%p",t);
//	printf("\nnew stripe address is %p",newStripe);
    if(t==NULL)
    {
        //	printf("\n Assigning to hashtable for first time");
        hashTable[hashIndex]=newStripe;
//		printf("\nnewStipe is %d",newStripe->sno);
        //
    }
    else
    {
        //	printf("t is not null, t stipno is %d\n",t->sno);
        newStripe->next=t;
        newStripe->prev=NULL;
        t->prev=newStripe;
        //t=newStripe;
        hashTable[hashIndex]=newStripe;
    }
    //modifiedStripe++;
    //free(newStripe);
}

void hashUpdateWriteHit(int stripeno, int blockno)
{
    //printf("\n hashUpdateWriteHit");
    int hashIndex=stripeno%CACHE_SIZE;
    cacheStripe *s=hashTable[hashIndex];
    while(s!=NULL)
    {
        if(s->sno==stripeno)
        {
            if(s->dirty!=1)
            {

                s->dirty=1;
                //modifiedStripe++;
            }
            //updatePartialParity(s->partialparity,s->block_array[blockno].bptr->buffer,BLOCK_SIZE);
            memcpy(s->block_array[blockno].bptr,writeData,BLOCK_SIZE);
            if(s->block_array[blockno].modify!=1)
                s->block_array[blockno].modify=1;
            if(s->block_array[blockno].used!=1)
            {
                s->block_array[blockno].used=1;
                usedBlock++;
            }
            break;
        }
        else
        {
            s=s->next;
        }
    }
}

void clean_the_stripe(int stripeno)
{
//	printf("\n In clean the stripe");
    int hashIndex=stripeno%CACHE_SIZE;
    cacheStripe *s=hashTable[hashIndex];
    while(s!=NULL)
    {
        if(s->sno==stripeno)
        {
            s->dirty=0;
            break;
        }
        else
            s=s->next;
    }

}

/********************************* NVRAM Functions*****************/

void initiatizeNvramCache()
{
    int i;
    for(i=0;i<NVRAM_BLOCK_CACHE_SIZE;i++)
    {
        nvram_cache[i].tag=0;
        memset(nvram_cache[i].buffer,'0',BLOCK_SIZE);
    }
}

void initializeNvramHashTable()
{
    int i;
    for(i=0;i<NVRAM_CACHE_SIZE;i++)
    {
        nvramHashTable[i]=NULL;
    }
}

void initializeNvramQueueParam()
{
    nqp=(nvramQueueParam*)malloc(sizeof(nvramQueueParam));
    nqp->front=NULL;
    nqp->tail=NULL;
    nqp->size=0;
}

void nvramPersist(int stripeno, int blockno)
{
//	printf("\n In nvramPersist");
    if(nqp->front==NULL && nqp->tail==NULL)
    {
        //Compulsory Miss
        nvram_databuffer_initialize(stripeno);
        nvramQueueUpdate(stripeno);
        nvramQueueContent();
        nvramhashUpdate(stripeno, blockno);
    }
    else
    {
        //hit
        if(searchNvramCache(stripeno,blockno)==1)
        {
            updateNvramCache(stripeno);
            nvramQueueContent();
            nvramUpdateAtHash(stripeno,blockno);
            //cacheHit++;
        }
        else
            //miss
        {
            if(qp->size<CACHE_SIZE)
            {
                databuffer_initialize();
                //	readIn(stripeno);
                //	IOrq_event ioevent(stripeno);
                //	read_io_queue.push_back(ioevent);
                read_push_back(stripeno);
                nvramQueueUpdate(stripeno);
                nvramQueueContent();
                nvramhashUpdate(stripeno, blockno);
                //cacheMiss++;
            }
                /*Capacity Miss*/
            else
            {
                //generatePolicy(stripeno);
                nvram_deleteClean();
                databuffer_initialize();
                //readIn(stripeno);
                //IOrq_event ioevent1(stripeno);
                //read_io_queue.push_back(ioevent1);
                read_push_back(stripeno);
                //printf("\n Choice of parity\n 1. Maintaining parity inside cache \n 2. Not maintaining parity inside cache ");
                //scanf("%d",&paritySelection);
                nvramQueueUpdate(stripeno);
                nvramhashUpdate(stripeno, blockno);
                //cacheMiss++;
            }

        }

    }
}

void nvram_databuffer_initialize(int stripeno)
{
    int hashIndex=stripeno%CACHE_SIZE;
    cacheStripe *s=hashTable[hashIndex];
    while(s!=NULL)
    {
        if(s->sno==stripeno)
        {
            int i;
            for(i=0;i<No_of_disk;i++)
            {
                memcpy(nvram_data_buffer[i].buff,s->block_array[i].bptr->buffer,BLOCK_SIZE);
            }
            break;
        }
        else
            s=s->next;
    }
}

void nvramQueueUpdate(int stripeno)
{
//	printf("\n In nvramQueueUpdate");
    nvramQueue *newNode= (nvramQueue *)malloc(sizeof(nvramQueue));
    newNode->sno=stripeno;
    newNode->next=NULL;
    newNode->prev=NULL;

    /************ Queue front and tail adjustment************/
    if(nqp->front==nqp->tail && nqp->front==NULL)
    {
        nqp->front=newNode;
        nqp->tail = newNode;
    }
    else if(nqp->front==nqp->tail)
    {
        nqp->front->next=newNode;
        nqp->tail=newNode;
        newNode->prev=nqp->front;
    }
    else
    {
        nqp->tail->next=newNode;
        newNode->prev=nqp->tail;
        nqp->tail=newNode;
    }
    nqp->size++;
}

void nvramhashUpdate(int stripeno, int blockno)
{
    int nvramhashIndex=stripeno%NVRAM_CACHE_SIZE;
    nvramStripe *newStripe=(nvramStripe *)malloc(sizeof(nvramStripe));
    if(newStripe!=NULL)
    {
        newStripe->sno=stripeno;//Set stripeno
        newStripe->dirty=1;//Set dirtybit
        int i=0;
        for(i=0;i<No_of_disk;i++)//Set Stripe block
        {
            //printf("\n\t\t i=%d",i);
            int nvram_block_cache_index=getNvramBlockCacheIndex(i);
            memcpy(nvram_cache[nvram_block_cache_index].buffer,nvram_data_buffer[i].buff,BLOCK_SIZE);
            //printf("\n\t\t %d th block_cache_index=%d",i,block_cache_index);

            if(i==blockno)
            {
                newStripe->block_array[i].modify=1;
                modifiedNvramStripe++;
                newStripe->block_array[i].used=1;
                usedBlock++;
            }
            else
                newStripe->block_array[i].used=0;
            newStripe->block_array[i].bptr=&nvram_cache[nvram_block_cache_index];
        }
        newStripe->prev=NULL;
        newStripe->next=NULL;
    }
    nvramStripe *t=nvramHashTable[nvramhashIndex];
    if(t==NULL)
    {

        nvramHashTable[nvramhashIndex]=newStripe;
    }
    else
    {
        newStripe->next=t;
        newStripe->prev=NULL;
        t->prev=newStripe;
        t=newStripe;
        nvramHashTable[nvramhashIndex]=t;
    }
//	printf("\nmodifiedNvramStripe: %d",modifiedNvramStripe);
}

int getNvramBlockCacheIndex(int k)
{
    int i;
    int x;
    for(i=0;i<NVRAM_BLOCK_CACHE_SIZE;i++)
    {
        if(nvram_cache[i].tag==0)
        {
            strcpy(nvram_cache[i].buffer,nvram_data_buffer[k].buff);
            nvram_cache[i].tag=1;
            nvramblockCount++;
            x=i;
            break;
        }
    }
    return x;
}

int searchNvramCache(int stripeno, int blockno)
{
    int found=0;
    int count=0;
    //int hashIndex=stripeno%CACHE_SIZE;
    //cacheQueue *temp=qp->front;
    //cacheStripe *t=hashTable[hashIndex];
    nvramQueue *nq=nqp->front;
    while(nq!=NULL)
    {
        if(nq->sno==stripeno)
        {
            int nvramhashIndex=stripeno%NVRAM_CACHE_SIZE;
            nvramStripe *t=nvramHashTable[nvramhashIndex];
            while(t!=NULL)
            {
                if(t->sno==stripeno && t->block_array[blockno].bptr!=NULL)
                {
                    found=1;
                    //cacheHit++;
                    break;
                }
                else
                {
                    t=t->next;
                }
            }
            break;
        }
        else
        {
            nq=nq->next;
        }
    }
    return found;
}

void updateNvramCache(int stripeno)
{
    if(nqp->front==nqp->tail)
    {
        //printf("\n Updating Null list");
        return;
    }
    else if(nqp->front->sno == stripeno)
    {
        //printf("\n The front element");
        nvram_denqueue();
    }
    else if(nqp->tail->sno == stripeno)
    {
        //printf("\n The tail element");
        return;
    }
    else
    {
        nvramQueue *nq = nqp->front->next;
        while(nq!=NULL)
        {
            if(nq->sno==stripeno)
            {
                nq->prev->next=nq->next;
                nq->next->prev=nq->prev;
                nq->prev=nqp->tail;
                nqp->tail->next=nq;
                nqp->tail=nq;
                nq->next=NULL;
                break;
            }
            else
            {
                nq=nq->next;
            }
        }
    }
}

void nvram_denqueue()
{
    //printf("\n denqueue");
    nvramQueue *r  = nqp->front;
    nqp->front = nqp->front->next;//Update front to next element
    nqp->tail->next=r;//Append the requested node to the tail of the queue
    r->prev=nqp->tail;
    r->next=NULL;
    nqp->front->prev=NULL;
    nqp->tail=r;
    //printCacheQueue();
}

void nvramUpdateAtHash(int stripeno,int blockno)
{
    //printf("\n usedUpdateAtHash");
    int nvramHashIndex=stripeno%NVRAM_CACHE_SIZE;
    nvramStripe *s=nvramHashTable[nvramHashIndex];
    while(s!=NULL)
    {
        if(s->sno==stripeno)
        {

            memcpy(s->block_array[blockno].bptr->buffer,nvram_data_buffer[blockno].buff,BLOCK_SIZE);
            if(s->block_array[blockno].used!=1)
            {
                s->block_array[blockno].used=1;
                usedBlock++;
            }
            if(s->block_array[blockno].modify!=1)
            {
                s->block_array[blockno].modify=1;
                modifiedNvramStripe++;
            }
            break;
        }
        else
        {
            s=s->next;
        }
    }
    printf("\nmodifiedNvramStripe: %d",modifiedNvramStripe);
}

void nvram_deleteClean()
{
    printf("\n deleteClean");
    int sno;
    nvramQueue *td=nqp->front;
    while(td!=NULL)
    {
        sno=td->sno;
        if(delligibleNvramStripe(sno)==1)
        {
            deleteNvramLRUCleanElement(sno);
            break;
        }
        else
        {
            td=td->next;
        }
    }
}

int delligibleNvramStripe(int stripeno)
{
    printf("\n In delligibleNvramStripe");
    int elligible=0;
    int nvramHashIndex=stripeno%NVRAM_CACHE_SIZE;
    nvramStripe *t=nvramHashTable[nvramHashIndex];
    //cacheStripe *s;
    while(t!=NULL)
    {
        //printf("\n In delligibleStripe	In while current stripe: %d",t->sno);
        if(t->sno==stripeno && t->dirty==0)
        {
            elligible=1;
            if(t->prev!=NULL && t->next==NULL)
            {
                t->prev->next=NULL;
            }
            else if(t->prev==NULL && t->next!=NULL)
            {
                nvramHashTable[nvramHashIndex]=t->next;
            }
            else if(t->prev!=NULL && t->next!=NULL)
            {
                t->prev->next=t->next;
                t->next->prev=t->prev;
            }
            else
            {
                nvramHashTable[nvramHashIndex]=NULL;
            }
            restoreNvramBlocks(t);
            break;
        }
        else
        {
            //printf("\n In delligibleStripe	In while In else");
            t=t->next;
        }
        //printf("\n Elligible = %d",elligible);
    }
    //free(s);
    return elligible;
}

void restoreNvramBlocks(nvramStripe *s)
{
    int i;
    printf("\nrestoreNvramBlocks");
    for(i=0;i<No_of_disk;i++)
    {
        if(s->block_array[i].bptr!=NULL)
        {
            s->block_array[i].bptr->tag=0;
            //blockCount--;
            //s->block_array[i].bptr=NULL;
            // printf("\n StripeNo:%d\tBlockno:%d\tAddress of Block:%p",s->sno,i,s->block_array[i].bptr);
        }
        //printf("\n after loop");
    }
}

void deleteNvramLRUCleanElement(int sno)
{
    printf("\ndeleteNvramLRUCleanElement");
    nvramQueue *t=nqp->front;
    while(t!=NULL)
    {
        //printf("\n The current stripe number is %d",t->sno);
        if(t->sno==sno)
        {
            if(t->prev==NULL && t->next!=NULL)
            {
                t->next->prev=NULL;
                nqp->front=t->next;
            }
            else if(t->prev!=NULL && t->next!=NULL)
            {
                t->prev->next=t->next;
                t->next->prev=t->prev;
            }
            else if(t->prev!=NULL && t->next==NULL)
            {
                t->prev->next=NULL;
                nqp->tail=t->prev;
            }
            else
            {
                nqp->front=NULL;
                nqp->tail=NULL;
            }
            //free(t);
            break;
        }
        else
        {
            t=t->next;
        }
    }
}

/*void write_to_disk()
{
	writeBackCount++;
	printf("\n write_to_disk");
	printf("\n%d %lf",modifiedNvramStripe,Del_Threshold);
	int nofwriteback=(int)modifiedNvramStripe*Del_Threshold;
	int count=0;
	int i;
	int modifiedBlockCount=0;
	int mcount=No_of_disk;
	int out1=0;
	int out2=0;
	struct timeval stop,start;
	printf("\n The total no_of_writeback should be: %d",nofwriteback);
	printf("\n mcount=%d, count=%d",mcount,count);
	//printf("condition: %d",cond);
	while((mcount>0) && (count<nofwriteback))
	{
		if(out2==1)
		{
			break;
		}
		else
		{
			printf("\n while");
			printf("\n writeback for modified stripe with: %d blocks",mcount);
			nvramQueue *td=nqp->front;
			printf("\n before cachequeue while");
			while(td!=NULL)
			{
				int sno=td->sno;
				printf("\n Cache queue Stripeno: %d",td->sno);
				int nvramHashIndex=sno%NVRAM_CACHE_SIZE;
				nvramStripe *t=nvramHashTable[nvramHashIndex];
				if(out1==1)
				{
					out2==1;
					break;
				}
				else
				{
				while(t!=NULL)
				{
					if(out1==1)
					{
						out2==1;
						break;
					}
					else
					{
						printf("\n stripenumber in hashtable: %d dirty is %d",t->sno,t->dirty);
						if(t->sno==sno && t->dirty==1)
						{
							modifiedBlockCount=0;
							printf("\nThe stripe no %d got writeback",sno);
							for(i=0;i<No_of_disk;i++)
							{
								if(t->block_array[i].bptr!=NULL && t->block_array[i].modify==1)
								{
									modifiedBlockCount++;
								}
								printf("\nmodifiedblockCount is %d, mcount %d\n",modifiedBlockCount,mcount);
							}
							if(modifiedBlockCount==mcount)
							{
							//compute Parity
						//	readIn(sno);
								printf("\nbefore write back");
								noiop++;
								gettimeofday(&start,NULL);
								writeBack(sno);
								t->dirty=0;
								diskAccess++;
								count++;
								gettimeofday(&stop,NULL);
								fprintf(writeLatency,"%lu, %lu,%lu\n",1000000*(stop.tv_sec-start.tv_sec)+stop.tv_usec - start.tv_usec);
					        	printf("write %lu\n",stop.tv_usec-start.tv_usec);
								printf("\n---------------------------------------------\n");
								fflush(writeLatency);
								modifiedNvramStripe--;
								for(i=0;i<No_of_disk;i++)
								{
									if(t->block_array[i].bptr!=NULL)
									{
										if(t->block_array[i].used==1)
										{
											t->block_array[i].used==0;
								//if(count=0)
							//		usedBlock--;
										}
										if(t->block_array[i].modify==1)
										{
											t->block_array[i].modify==0;
										}
									}
									else
									{
										unnecessaryWrite++;
									}
								}
							}
							if(count==nofwriteback)
							{
								out1=1;
								break;
							}
						}
						t=t->next;
					}
					}
				}
				td=td->next;
			}
			mcount--;
		}
	}
	printf("\nmodifiedNvramStripe: %d",modifiedNvramStripe);
//	printf("\n Actual no of write back: %d",count);
}*/

void write_to_disk()
{
    totalwriteBackCount++;
    printf("\n write_to_disk");
    printf("\n%d %lf",modifiedNvramStripe,Del_Threshold);
    int nofwriteback=(int)modifiedNvramStripe*Del_Threshold;
    printf("\n The total no_of_writeback should be: %d",nofwriteback);
    int count=0;
    int i;
    int modifiedBlockCount=0;
    int mcount;
    struct timeval stop,start;
    for(mcount=No_of_disk-1;mcount>0;mcount--)
    {
        //printf("\n Looking for mcount:%d",mcount);
        //printf("\n Last count:%d",count);
        if(count<nofwriteback)
        {
            nvramQueue *td=nqp->front;
            int sno=td->sno;
            while(td!=NULL)
            {
                int nvramHashIndex=sno%NVRAM_CACHE_SIZE;
                //printf("\n Looking for stripeno in the cache list: %d",sno);
                nvramStripe *t=nvramHashTable[nvramHashIndex];
                while(t!=NULL)
                {
                    //printf("\n the current stipe on the hash index: %d",t->sno);
                    if(t->sno==sno && t->dirty==1)
                    {
                        modifiedBlockCount=0;
                        //printf("\nThe stripe no %d got writeback",sno);
                        for(i=0;i<No_of_disk;i++)
                        {
                            if(t->block_array[i].bptr!=NULL && t->block_array[i].modify==1)
                            {
                                modifiedBlockCount++;
                            }
                            //	printf("\nmodifiedblockCount is %d, mcount %d\n",modifiedBlockCount,mcount);
                        }
                        if(modifiedBlockCount==mcount)
                        {
                            printf("\n writeback will be processed soon");
                            //compute Parity
                            //	readIn(sno);
                            //printf("\nbefore write back");
                            noiop++;
                            // 	IOrq_event ioevent(sno);
                            //	write_io_queue.push_back(ioevent);
                            write_push_back(sno);
                            //writeBack(sno);
                            t->dirty=0;
                            diskAccess++;
                            printf("diskAccess is %d\n",diskAccess);
                            count++;
                            printf("\n count updated:%d",count);
                            //	fprintf(writeLatency,"%lu, %lu,%lu\n",1000000*(stop.tv_sec-start.tv_sec)+stop.tv_usec - start.tv_usec);
                            //	printf("write %lu\n",stop.tv_usec-start.tv_usec);
                            //	printf("\n---------------------------------------------\n");
                            modifiedNvramStripe--;
                            for(i=0;i<No_of_disk;i++)
                            {
                                if(t->block_array[i].bptr!=NULL)
                                {
                                    if(t->block_array[i].used==1)
                                    {
                                        t->block_array[i].used=0;
                                        //if(count=0)
                                        //usedBlock--;
                                    }
                                    if(t->block_array[i].modify==1)
                                    {
                                        t->block_array[i].modify=0;
                                    }
                                }
                                else
                                {
                                    unnecessaryWrite++;
                                }
                            }
                        }
                        //printf("\n Current count:%d",count);
                        break;
                    }
                    else
                    {
                        t=t->next;
                    }

                }
                if(count==nofwriteback)
                {
                    break;
                }
                else
                {
                    td=td->next;
                    printf("\n ");
                }

            }
        }
        else
        {
            printf("\Done");
        }
    }
    printf("done in write back to disk\n ");

}

void nvramQueueContent()
{
//	printf("\n Nvram content **************");
    nvramQueue *td=nqp->front;
    while(td!=NULL)
    {
        //	printf("\t %d", td->sno);
        td=td->next;
    }
}

/*********************************Large NVRAM Functions********************************/

void initializelNvramTransfer()
{
    printf("\n initializelNvramTransfer");
    int i;
    for(i=0;i<No_of_disk;i++)
    {
        memset(lNvramTransfer[i].buff,'0',BLOCK_SIZE);
    }
}

void initializeLargeNvramHashTable()
{
    printf("\n initializeLargeNvramHashTable");
    int i;
    for(i=0;i<NVRAM_CACHE_SIZE;i++)
    {
        nvramHashTable[i]=NULL;
    }
}

void initializeNvramMLQueueParam()
{
    printf("\n initializeNvramMLQueueParam");
    ml=(lNvramQueueParam*)malloc(sizeof(lNvramQueueParam));
    ml->front=NULL;
    ml->tail=NULL;
    ml->size=0;
}

void initializeNvramCLQueueParam()
{
    printf("\n initializeNvramCLQueueParam");
    cl=(lNvramQueueParam*)malloc(sizeof(lNvramQueueParam));
    cl->front=NULL;
    cl->tail=NULL;
    cl->size=0;
}

/*void initializeNvramFLQueueParam()
{
	fl=(lNvramQueueParam*)malloc(sizeof(lNvramQueueParam));
	fl->front=NULL;
	fl->tail=NULL;
	fl->size=NVRAM_CACHE_SIZE;
	initializeFreeList();
}
void initializeFreeList()
{
	int count=0;
	while(count<fl->size)
	{
		lNvramQueue *newNode= (lNvramQueue *)malloc(sizeof(lNvramQueue));
		newNode->sno=0;
		newNode->next=NULL;
		newNode->prev=NULL;
		if(fl->front==NULL && fl->tail==NULL)
		{
			fl->front=newNode;
			fl->tail=newNode;
		}
		else
		{
			fl->tail->next=newNode;
			newNode->prev=fl->tail;
			fl->tail=newNode;
		}
		count++;
	}
}*/

void store(int sno)
{
    if(searchInNVRAM(sno)==0)
    {
        missEvict+=1;
        add_cl(sno);
        addLNvramHash(sno);
    }
}

void add_cl(int sno)
{
    if((ml->size+cl->size)<NVRAM_CACHE_SIZE)
    {
        lNvramQueue *newNode=(lNvramQueue *)malloc(sizeof(lNvramQueue));
        if(newNode!=NULL)
        {
            newNode->sno=sno;
            newNode->next=NULL;
            newNode->prev=NULL;

            if(cl->front==NULL && cl->tail==NULL)
            {
                cl->front=newNode;
                cl->tail=newNode;
            }
            else
            {
                cl->tail->next=newNode;
                newNode->prev=cl->tail;
                cl->tail=newNode;
            }
        }

        cl->size++;
    }
    else
    {

        //cl->tail=cl->tail->prev;
        //cl->tail->next=NULL;
        //deletelNvramHashTable(t->sno);
        //t->sno=sno;
        if(cl->front==NULL && cl->tail==NULL)
        {
            printf("\n Error in Cl size management: Ml took all NVRAM and new data wants to be stored in CL. Need to be handled by writeback");
            //cl->front=t;
            //cl->tail=t;
        }
        else if(cl->front==cl->tail && cl->front!=NULL)
        {
            lNvramQueue *t=cl->tail;
            deletelNvramHashTable(t->sno);
            t->sno=sno;
        }
        else
        {
            lNvramQueue *t=cl->tail;
            deletelNvramHashTable(t->sno);
            t->sno=sno;
            cl->tail=cl->tail->prev;
            cl->tail->next=NULL;
            cl->front->prev=t;
            t->next=cl->front;
            cl->front=t;
        }
    }
    show_cl();
}

void addLNvramHash(int sno)
{
    int hashIndex=sno%NVRAM_CACHE_SIZE;
    int i;
    lNvramStripe *newStripe=(lNvramStripe *)malloc(sizeof(lNvramStripe));
    if(newStripe!=NULL)
    {
        newStripe->sno=sno;
        newStripe->dirty=0;
        for(i=0;i<No_of_disk;i++)
        {
            memcpy(newStripe->block_array[i].buff,lNvramTransfer[i].buff,BLOCK_SIZE);
            newStripe->block_array[i].modify=0;
            newStripe->block_array[i].used=0;
        }
        newStripe->next=NULL;
        newStripe->prev=NULL;
    }

    lNvramStripe *t= lNvramHashTable[hashIndex];
    if(t==NULL)
    {
        t=newStripe;
    }
    else
    {
        //newStripe->next=t;
        //newStripe->prev=NULL;
        t->prev=newStripe;
        newStripe->next=t;
        t=newStripe;
        lNvramHashTable[hashIndex]=t;
    }
}

int searchInML(int sno)
{
    lNvramQueue *temp;
    int found=0;
    if(ml->front!=NULL)
    {
        lNvramQueue *t=ml->front;
        while(t!=NULL)
        {
            if(t->sno==sno)
            {
                temp=t;
                found=1;
                break;
            }
            else
                t=t->next;
        }
    }
    if(found==1)
    {
        temp->prev->next=temp->next;
        temp->next->prev=temp->prev;
        temp->prev=NULL;
        temp->next=NULL;
        ml->front->prev=temp;
        temp->next=ml->front;
        ml->front=temp;
        show_ml();
    }
    return found;
}

int searchInCL(int sno)
{
    int found=0;
    lNvramQueue *temp;
    if(cl->front!=NULL)
    {
        lNvramQueue *t=cl->front;
        while(t!=NULL)
        {
            if(t->sno==sno)
            {
                temp=t;
                found=1;
                break;
            }
            else
                t=t->next;
        }
    }
    if(found==1)
    {
        temp->prev->next=temp->next;
        temp->next->prev=temp->prev;
        temp->prev=NULL;
        temp->next=NULL;
        cl->front->prev=temp;
        temp->next=cl->front;
        cl->front=temp;
        show_cl();
    }
    return found;
}

void lNvramPersist(int stripeno,int blockno)
{
    persistCount+=1;
    if(searchInNVRAM(stripeno)==1 )
    {
        updateLNvramHash(stripeno,blockno);
        add_ml(stripeno);
    }
    else
    {
        add_ml(stripeno);
        if(policy==3)
        {
            writeMissCount+=1;
        }

        addLNvramHashPersist(stripeno,blockno);
    }


}

void updateLNvramHash(int stripeno,int blockno)
{
    int hashIndex=stripeno%NVRAM_CACHE_SIZE;
    lNvramStripe *t= lNvramHashTable[hashIndex];
    while(t!=NULL)
    {
        if(t->sno==stripeno)
        {
            memcpy(t->block_array[blockno].buff,writeData,BLOCK_SIZE);
            t->block_array[blockno].modify=1;
            t->block_array[blockno].used=1;
            break;
        }
        else
        {
            t=t->next;
        }
    }
}

void add_ml(int sno)
{
    if((ml->size+cl->size)<NVRAM_CACHE_SIZE )
    {
        lNvramQueue *newNode=(lNvramQueue *)malloc(sizeof(lNvramQueue));
        if(newNode!=NULL)
        {
            newNode->sno=sno;
            newNode->next=NULL;
            newNode->prev=NULL;
            if(ml->front==NULL && ml->tail==NULL)
            {
                ml->front=newNode;
                ml->tail=newNode;
                ml->size++;
            }
            else
            {
                ml->tail->next=newNode;
                newNode->prev=ml->tail;
                ml->tail=newNode;
                ml->size++;
            }
        }

    }
    else
    {

        lNvramQueue *t;
        if(cl->size==0)
        {
            t=NULL;

        }
        else if(cl->size==1)
        {
            t=cl->tail;
            cl->tail=NULL;
            cl->front=NULL;
            cl->size--;
        }
        else
        {
            t=cl->tail;
            cl->tail=cl->tail->prev;
            cl->tail->next=NULL;
            t->prev=NULL;
            cl->size--;
            show_cl();
        }
        if(t!=NULL)
        {
            deletelNvramHashTable(t->sno);
            t->sno=sno;
            if(ml->front==NULL && ml->tail==NULL)
            {
                ml->front=t;
                ml->tail=t;
                ml->size++;
                show_ml();
            }
            else
            {
                ml->front->prev=t;

                t->next=ml->front;
                ml->front=t;
                if(ml->size<resThreshold)
                    ml->size++;
                else
                {
                    printf("\n ML cant grow more. Need to be handled by writeback");
                }
                show_ml();
            }
        }
        else
            printf("\n error in ML size management: ML took total NVRAM but still wants to add more. Need to be handled by writeback");


    }
    show_ml();
}

void addLNvramHashPersist(int sno,int blockno)
{
    int hashIndex=sno%NVRAM_CACHE_SIZE;
    int i;
    lNvramStripe *newStripe=(lNvramStripe *)malloc(sizeof(lNvramStripe));
    if(newStripe!=NULL)
    {
        newStripe->sno=sno;
        newStripe->dirty=1;
        for(i=0;i<No_of_disk;i++)
        {
            if(i==blockno)
                memcpy(newStripe->block_array[i].buff,writeData,BLOCK_SIZE);
            else
                memcpy(newStripe->block_array[i].buff,lNvramTransfer[i].buff,BLOCK_SIZE);
            newStripe->block_array[i].modify=0;
            newStripe->block_array[i].used=0;
        }
        newStripe->next=NULL;
        newStripe->prev=NULL;
    }

    lNvramStripe *t= lNvramHashTable[hashIndex];
    if(t==NULL)
    {
        t=newStripe;
    }
    else
    {
        newStripe->next=t;
        newStripe->prev=NULL;
        t->prev=newStripe;
        t=newStripe;
        lNvramHashTable[hashIndex]=t;
    }
}

void writeback_to_disk()
{
    printf("\n Writeback issued");
    int count=writeBackCount;
    printf("\n writeBackCount: %d",writeBackCount);
    show_ml();
    //int writeBackCount=(int)writebackrate*NVRAM_CACHE_SIZE;
    while(count>0)
    {
        //lru selection, need to fix later with most modified
        printf("\n Count: %d",count);
        lNvramQueue *t=ml->tail;
        int sno=t->sno;
        ml->tail=ml->tail->prev;
        ml->tail->next=NULL;
        t->next=NULL;
        t->prev=NULL;
        putPending(sno);
        write_push_back(sno);
        //ml->size--;
        //need signal
        //if(ml->size>0)
        ml->size--;
        store_to_CL(t);
        //addLNvramHash(sno);

        putClean(sno);
        count--;

    }
}

void store_to_CL(lNvramQueue *p)
{

    int pos=(int)0.25*cl->size;
    lNvramQueue *t;
    if(pos==0)
    {
        t=cl->front;
    }

    else
    {
        t=cl->tail;
        while(pos>0)
        {
            if(t->prev!=NULL)
            {
                t=t->prev;
                pos--;
            }

        }
    }
    if(pos==0)
    {
        if(cl->tail==cl->front && cl->tail==NULL)
        {
            cl->front=p;
            cl->tail=p;
            cl->size++;
        }
        else
        {
            t->prev=p;
            p->next=t;
            cl->front=p;
            cl->size++;
        }
    }
    else
    {
        p->next=t;
        p->prev=t->prev;
        t->prev->next=p;
        t->prev=p;
        cl->size++;
    }

}

void putPending(int sno)
{
    int hashIndex=sno%NVRAM_CACHE_SIZE;
    lNvramStripe *t= lNvramHashTable[hashIndex];
    while(t!=NULL)
    {
        if(t->sno==sno)
        {
            t->dirty=2;
            break;
        }
        else
        {
            t=t->next;
        }
    }
}

void putClean(int sno)
{
    printf("\n putClean");
    int hashIndex=sno%NVRAM_CACHE_SIZE;
    lNvramStripe *t= lNvramHashTable[hashIndex];
    while(t!=NULL)
    {
        if(t->sno==sno)
        {
            t->dirty=0;
            break;
        }
        else
        {
            t=t->next;
        }
    }
}

float estimated_S()
{
    float set=0;
    if(writeMissCount!=0||missEvict!=0) {
        set = float(writeMissCount / (writeMissCount + missEvict));
    }
    set = 0.5*set+0.5*preS;
    preS = set;
    return set;
}

int searchInNVRAM(int stripeno)
{
    int found=0;
    int hashIndex=stripeno%NVRAM_CACHE_SIZE;
    lNvramStripe *t= lNvramHashTable[hashIndex];
    printf("\n NVRAM Searching starts ");
    while(t!=NULL)
    {
        if(t->sno==stripeno)
        {
            found=1;
            int i;
            printf("\n Found in NVRAM");
            for(i=0;i<No_of_disk;i++)
            {
                memcpy(db_array[i].buff,t->block_array[i].buff,BLOCK_SIZE);
                printf("\t Preparing dbarray[%d]",i);
            }
            printf("\n");
            break;
        }
        else
        {
            t=t->next;
        }
    }
    return found;
}

void show_ml()
{
    lNvramQueue *t=ml->front;
    printf("\n ML content:");
    while(t!=NULL)
    {
        printf("\t %d",t->sno);
        t=t->next;
    }
    printf("\nEnd of the list:%d ",ml->tail->sno);
    printf("\n Ml size: %d",ml->size);
}

void deletelNvramHashTable(int sno)
{
    int hashIndex=sno%NVRAM_CACHE_SIZE;
    lNvramStripe *t= lNvramHashTable[hashIndex];
    printf("\n NVRAM Searching starts ");
    while(t!=NULL)
    {
        if(t->sno==sno)
        {
            t->next->prev=t->prev;
            t->prev->next=t->next;
            t->prev=NULL;
            t->next=NULL;
        }
        else
        {
            t=t->next;
        }
    }
}

void show_cl()
{
    lNvramQueue *t=cl->front;
    printf("\n CL content:");
    while(t!=NULL)
    {
        printf("\t %d",t->sno);
        t=t->next;
    }
    //printf("\nEnd of the list:%d ",cl->tail->sno);
}

void transferCL_to_ML()
{
    if(ml->size<resThreshold)
    {
        lNvramQueue *t=cl->front;
        cl->front=cl->front->next;
        cl->front->prev=NULL;
        cl->size--;
        t->next=NULL;
        t->prev=NULL;
        ml->front->prev=t;
        t->next=ml->front;
        ml->front=t;
        ml->size++;
    }
}
