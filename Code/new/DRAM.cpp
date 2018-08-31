
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


