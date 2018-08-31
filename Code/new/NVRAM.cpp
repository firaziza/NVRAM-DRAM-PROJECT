 
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
