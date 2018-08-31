#include "filetable.h"
//#include "global.h"
/*
d_create(DEVICE_NUMBER);
	ini_file_table();
	file_table_create();
	allocate_disk_filetable();
	find(2);
	find(1);
	find(2);
	printf("request %d\n",requestStripeNm(2,5));
	printf("re %d\n",requestStripeNm(9,3));
	write_back(1,ini_block,1);
	return 0;

}
*/
FILE *ptrs[DEVICE_NUMBER];
int  ini_block[BLOCK_SIZE];
struct file_entry *file_table[File_number];
char file_table_writeData[BLOCK_SIZE];
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t checkempty_readqueue = PTHREAD_COND_INITIALIZER;
pthread_mutex_t checkempty_mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t mutex_read_finish;
dataBuffer db_array[DEVICE_NUMBER];
dataBuffer nvram_data_buffer[DEVICE_NUMBER];
dataBuffer lNvramTransfer[DEVICE_NUMBER];
FILE *printout;
FILE *writeout;
FILE *rwqueue;
void initial_ptr(){

	int i=0;
	for(i=0;i<DEVICE_NUMBER;i++){
		ptrs[i]=NULL;


	}


}

void initial_block(){

	int i=0;
	for(i=0;i<BLOCK_SIZE;i++)
	{
		ini_block[i]=0;
	}
}

void initial_file_entry()
{
	int i=0;
	for(i=0;i<File_number;i++)
	{

		file_table[i]=NULL;	
	}
}

int total_files=0;
void ini_file_table(){
	char line[201];
	file_entry *fe;

	FILE *file_data=fopen(FILE_DATA,"r");
	if(file_data==NULL){
		printf("couldnot open fileSizeDA file\n");
	}

	total_files=total_file_num(file_data);
//	*file_table[total_files];
//	printf("total file num is %d\n",total_files);

	fclose(file_data);


}
int total_file_num(FILE *file_data){
	int lines=0;
	int ch=0;
	while(!feof(file_data)){
		ch=fgetc(file_data);
		if(ch=='\n')
			lines++;
	}
	return lines+1;
}

int allocate_disk_filetable(){
	int j=0;
	int i;
	disk *disk0=(disk *)malloc(sizeof(disk));
	file_entry *fe=(file_entry *)malloc(sizeof(file_entry));
	disk0->disk_num=0;
	disk0->current_offset=0;
	disk *disk1=(disk *)malloc(sizeof(disk));
	disk *disk2=(disk *)malloc(sizeof(disk));
	disk *disk3=(disk *)malloc(sizeof(disk));
	disk *disk4=(disk *)malloc(sizeof(disk));
	disk1->disk_num=1;
	disk1->current_offset=0;
	disk2->disk_num=2;
	disk2->current_offset=0;
	disk3->disk_num=3;
	disk3->current_offset=0;

	int current_offset=0;
	for(i=0;i<total_files;i++){
        fe=file_table[i];
        int filesize=file_table[i]->filesize;
        if (i == 0) {
            disk0->disk_num = 0;
            fe->start_disk=disk0->disk_num;
            fe->start_offset=disk0->current_offset;
            disk0->current_offset=disk0->current_offset+filesize - 1;
        }
		if (i == 1) {
			disk1->disk_num = 1;
			fe->start_disk=disk1->disk_num;
			fe->start_offset=disk1->current_offset;
			disk1->current_offset=disk1->current_offset+filesize - 1;
		}
		if (i == 2) {
			disk2->disk_num = 2;
			fe->start_disk=disk2->disk_num;
			fe->start_offset=disk2->current_offset;
			disk2->current_offset=disk2->current_offset+filesize - 1;
		}
		if (i == 3) {
			disk3->disk_num = 3;
			fe->start_disk=disk3->disk_num;
			fe->start_offset=disk3->current_offset;
			disk3->current_offset=disk3->current_offset+filesize - 1;
		}
        if ( i!= 0 && i % (DEVICE_NUMBER-1) == 0) {

			disk0->disk_num = 0;
			fe->start_disk=disk0->disk_num;
			fe->start_offset=disk0->current_offset;
			disk0->current_offset=disk0->current_offset+filesize;

        }
        if (i!=1 && i %(DEVICE_NUMBER-1) == 1) {
            disk1->disk_num = 1;
            fe->start_disk=disk1->disk_num;
            fe->start_offset=disk1->current_offset;
			disk1->current_offset=disk1->current_offset+filesize;

        }
        if (i!= 2 && i %(DEVICE_NUMBER -1) == 2){
            disk2->disk_num = 2;
            fe->start_disk=disk2->disk_num;
            fe->start_offset=disk2->current_offset;
			disk2->current_offset=disk2->current_offset+filesize;

        }
        if (i!=3 && i%(DEVICE_NUMBER-1) ==3 ) {
            disk3->disk_num = 3;
            fe->start_disk=disk3->disk_num;
            fe->start_offset=disk3->current_offset;
			disk3->current_offset=disk3->current_offset+filesize;

        }

	}
	for(i=0;i<total_files;i++){
		//printf("fe %d,%d disk %d,offset %d\n",file_table[i]->fileid,file_table[i]->filesize,file_table[i]->start_disk,file_table[i]->start_offset);
		
	}
	free(disk1);
	free(fe);
	free(disk0);
	free(disk2);
	free(disk3);
	return 0;
}
int file_table_create(){
	int fileid;
	int filesize;
	FILE *file_data=fopen(FILE_DATA,"r");
	disk *disk1=(disk *)malloc(sizeof(disk));
	disk1->disk_num=0;
	disk1->current_offset=0;
	char line[20];
	if(file_data==NULL){
		printf("couldnot open fileSizeDA file\n");
	}
	int i=total_files;
	int j=0;

	for(i=0;i<total_files;i++){
		file_entry *fe=(file_entry *)malloc(sizeof(file_entry));	
		fscanf(file_data,"%d,%d",&fileid,&filesize);
	//	printf("%d,%d\n",fileid,filesize);	
		fe->start_disk=disk1->disk_num;
		fe->start_offset=disk1->current_offset;
		fe->fileid=fileid;
		fe->filesize=filesize;
		//printf("id %d size %d start_disknum %d offset %d\n ",fe->fileid,fe->filesize,fe->start_disk,fe->start_offset);
		//allocate_disk_filetable(fe,disk1);
		file_table[i]=fe;
	//	printf("file_table %d,%d\n",file_table[i]->fileid,file_table[i]->filesize);
		//free(fe);

	}
		
	free(disk1);
		fclose(file_data);
	return 0;
}
void raid_create(int device_number){
	char diskname[DEVICE_NUMBER];
	int i;
	int j;
    	printout = fopen("read_latency_1000.csv","w");
	writeout = fopen(WRITEFILE,"a+");
	rwqueue = fopen("read_write_ioqueue.csv","w");
	memset(ini_block,'0',BLOCK_SIZE);
	for(i=0;i<DEVICE_NUMBER;i++){
		//error occur stack smashing detected;
	//	sprintf(diskname,"/mnt/raid5/disk%d/%d.disk",i+1,i);
		sprintf(diskname,"%d.disk",i);
		//printf("diskname is %s\n",diskname);
		ptrs[i]=fopen(diskname,"w+");
		if(ptrs[i]==NULL){
			printf("could not create a disk\n");
			exit(1);

		}
			if(ini_block==NULL){
			printf("initial block is null in disk\n");
			exit(1);
		}
		/*
		for(j=0;j<DISK_SIZE;j++){

			fwrite(ini_block,1,BLOCK_SIZE,ptrs[i]);

		}
		*/
	//	printf("finish created disk\n");
		rewind(ptrs[i]);
	//	fclose(ptrs[i]);
	}
	

}

int is_parity(int disk_num,int stripe_number){
	int is_parity=0;
//	printf("disk number is %d, stripe_number is %d\n",disk_num,stripe_number);
	if(((stripe_number%DEVICE_NUMBER)+disk_num)==DEVICE_NUMBER-1){
			is_parity=1;
		//	printf("parity\n");
	}else{

		is_parity=0;

	}

	return is_parity;



}
int getParitybnum(int stripe_num) {
	for(int i = 0; i < DEVICE_NUMBER; i++) {
		if (((stripe_num % DEVICE_NUMBER) + i )== DEVICE_NUMBER -1 ){
			return i;
		}
	}
}

int requestStripeNm(int file_name,int block_num){
	
	int stripe_num;
//	int current_disk_num;
	int offset=block_num;
	file_entry *fe;
	fe=find(file_name);
	stripe_num=fe->start_offset+offset;


	return stripe_num;
}
file_entry *find(int fileid){
	//file_current *new;
	int i=0;
//	int j=0;
//	for(j=0;i<total_files+1;i++){

//		printf("fe %d,%d\n",file_table[i]->fileid,file_table[i]->filesize);
		

//	}

	while(file_table[i]->fileid!=fileid&&i<501){
		
		if(file_table[i]==NULL){

			printf("file_table[i] is NULLi,can't find file\n");
				exit(0);
		}else{
			i++;

		}
		
	//	printf("i: %d %d %d\n",i,file_table[i]->fileid,fileid);
		

	}
//	printf("find %d %d %d %d\n",file_table[i]->fileid,file_table[i]->filesize,file_table[i]->start_disk,file_table[i]->start_offset);
	return file_table[i];


}

//ioreq_event *iotrace_validate_get_ioreq_event(FILE *origfile,ioreq_event *new){

//	char line[201];
int getblocknum(int fileid,int block_num){
	file_entry *fe;
	fe=find(fileid);
	int current_disk_num=fe->start_disk;
	int stripeno=fe->start_offset;

	return current_disk_num;

}
/*
void write_back(int stripenum,char *p,int i){

		fseek(ptrs[i],stripenum*BLOCK_SIZE,SEEK_SET);		
		fwrite(p,1,BLOCK_SIZE,ptrs[i]);



	
}
 */
void *threadWork(void *argument)
{

    //memset(file_table_writeData,'1',BLOCK_SIZE);
    arg_struct *args = (arg_struct *) argument;
    printf("write back--------------- \n");
    fseek(ptrs[args->i],args->sno*BLOCK_SIZE,SEEK_SET);
    //fread(file_table_writeData,1,BLOCK_SIZE,ptrs[args->i]);
    fwrite(file_table_writeData,1,BLOCK_SIZE,ptrs[args->i]);
   // printf("done\n");
    pthread_exit(0);
}
void *readInThread(void *argument){
  //  pthread_mutex_init(&mutex1,NULL);
  //  pthread_mutex_lock(&mutex1);
    arg_struct_read *args= (arg_struct_read*)argument;
	fseek(ptrs[args->i],args->sno*BLOCK_SIZE,SEEK_SET);
//	fwrite(args->buffer,BLOCK_SIZE,1,ptrs[args->i]);
    fread(db_array[args->i].buff,1,BLOCK_SIZE,ptrs[args->i]);
  //  pthread_mutex_unlock(&mutex1);
   // free(args);
	pthread_exit(0);
}



void writeBack(int sno)
{
	printf("writeback\n");
	int ret;
	int i;
	struct timeval stop, start;
	pthread_mutex_t mutex4;
//	writeout = fopen("writelatency.csv", "a+");
	pthread_t thread1,thread2,thread3,thread4,thread0;
	arg_struct args1,args2,args3,args4,args0;
	memset(file_table_writeData,'1',BLOCK_SIZE);
	gettimeofday(&start,NULL);
	pthread_mutex_init(&mutex4,NULL);
	pthread_mutex_lock(&mutex4);
	args0.sno=sno;
	args0.i=0;
	args2.sno=sno;
	args2.i=2;
	args3.sno=sno;
	args3.i=3;
	args4.sno=sno;
	args4.i=4;
	args1.sno=sno;
	args1.i=1;




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
	fprintf(rwqueue,"after writeback %d\n",write_io_queue.size());
	//  printf("before mutex\n");
	pthread_mutex_unlock(&mutex4);
//	gettimeofday(&stop,NULL);
	//fprintf(writeout,"%ld\n", 1000000*(stop.tv_sec-start.tv_sec)+stop.tv_usec-start.tv_usec);
        //fprintf(printout,"%ld\n", 1000000*(stop.tv_sec-start.tv_sec)+stop.tv_usec-start.tv_usec);

  //  	fflush(writeout);
   // printf("read %lu\n",stop.tv_usec-start.tv_usec);
   //	fclose(writeout);

	//   printf("after mutex\n");
//	pthread_exit(NULL);
}
void readIn(int sno)
{
    printf("read in\n");
    struct timeval stop, start;
    int ret;
    int i;

    pthread_mutex_t mutex3;
    FILE *printout_read= fopen("print_read_latency.csv","a+");

    //cacheStripe *newStripe=(cacheStripe *)malloc(sizeof(cacheStripe));
    pthread_t thread1,thread2,thread3,thread4,thread0;
    arg_struct_read args_1,args_2,args_3,args_4,args_0;
//    typedef arg_struct_read *agrs0;
//    agrs0 args_0;
//    //arg_struct_read args1,args2,args3,args4;//args0;
//    args_0 =(agrs0)malloc(sizeof(arg_struct_read));
//    typedef arg_struct_read *agrs1;
//    agrs1 args_1;
//    //arg_struct_read args1,args2,args3,args4;//args0;
//    args_1 =(agrs1)malloc(sizeof(arg_struct_read));
//    typedef arg_struct_read *agrs2;
//    agrs2 args_2;
//    //arg_struct_read args1,args2,args3,args4;//args0;
//    args_2 =(agrs2)malloc(sizeof(arg_struct_read));
//    typedef arg_struct_read *agrs3;
//    agrs3 args_3;
//    //arg_struct_read args1,args2,args3,args4;//args0;
//    args_3 =(agrs3)malloc(sizeof(arg_struct_read));
//    typedef arg_struct_read *agrs4;
//    agrs4 args_4;
//    //arg_struct_read args1,args2,args3,args4;//args0;
//    args_4 =(agrs4)malloc(sizeof(arg_struct_read));
    memset(file_table_writeData,'1',BLOCK_SIZE);
    gettimeofday(&start, NULL);
    pthread_mutex_init(&mutex3,NULL);
    pthread_mutex_lock(&mutex3);
    printf("\nread in \n");
    //args1=malloc(sizeof(arg_struct_read));
    args_0.sno=sno;
    args_0.i=0;
   // args0->buffer=file_table_writeData;
    strcpy(args_0.buffer,file_table_writeData);
    args_2.sno=sno;
    args_2.i=2;
    //args0.buffer=file_table_writeData;
    strcpy(args_2.buffer,file_table_writeData);
    args_3.sno=sno;
    args_3.i=3;
    //args3.buffer=file_table_writeData;
    strcpy(args_3.buffer,file_table_writeData);
    args_4.sno=sno;
    args_4.i=4;
    strcpy(args_4.buffer,file_table_writeData);
   // args4.buffer=file_table_writeData;
    args_1.sno=sno;
    args_1.i=1;
    strcpy(args_1.buffer,file_table_writeData);
    //args1.buffer=file_table_writeData;

    pthread_create(&thread0,NULL,readInThread,(void *)&args_0);
    pthread_create(&thread1,NULL,readInThread,(void *)&args_1);
    pthread_create(&thread2,NULL,readInThread,(void *)&args_2);
    pthread_create(&thread3,NULL,readInThread,(void *)&args_3);
    pthread_create(&thread4,NULL,readInThread,(void *)&args_4);
    pthread_join(thread1,NULL);
    pthread_join(thread2,NULL);
    pthread_join(thread3,NULL);
    pthread_join(thread4,NULL);
    pthread_join(thread0,NULL);
//	printf("before mutex\n");
    pthread_mutex_unlock(&mutex3);
    //printf("after mutex\n");
    //printf("\nread in \n");
    gettimeofday(&stop, NULL);

    fprintf(printout_read,"%ld\n", 1000000*(stop.tv_sec-start.tv_sec)+stop.tv_usec-start.tv_usec);
	fprintf(rwqueue,"after readin %d\n",read_io_queue.size());
    fflush(printout_read);
   // printf("read %lu\n",stop.tv_usec-start.tv_usec);
    fclose(printout_read);
}

void read_push_back(int stripno){
    struct timeval now;
    sem_init(&mutex_read_finish,0,0);
    IOrq_event event(stripno);
    pthread_mutex_lock(&mx_readqueue);
    printf("read_io_queue %d\n",read_io_queue.size());
    while(read_io_queue.size() == queue_size){
	pthread_cond_wait(&empty_readqueue,&mx_readqueue);
	printf("waiting\n");
    }
    gettimeofday(&now,NULL); 
    //event.start = 1000000*now.tv_sec+now.tv_usec;
    printf("%lu",1000000*now.tv_sec+now.tv_usec);
    read_io_queue.push_back(event);
	fprintf(rwqueue,"push read %d\n",read_io_queue.size());

	pthread_mutex_unlock(&mx_readqueue);
    pthread_cond_signal(&full_readqueue);
    sem_wait(&mutex_read_finish);
    printf("size of read queue %d\n",read_io_queue.size());
	//fprintf(rwqueue,"push read %d\n",read_io_queue.size());
    printf("hellob\n");
}


void write_push_back(int stripno) {
    struct timeval now;
    IOrq_event event(stripno);
    pthread_mutex_lock(&mx_writequeue);
    printf("write io queue %d\n",write_io_queue.size());
    while(write_io_queue.size()==queue_size){
	printf("waiting\n");
	pthread_cond_wait(&empty_writequeue,&mx_writequeue);
    }
   // gettimeofday(&now,NULL);
    //event.start = 1000000*now.tv_sec+now.tv_usec;
    printf("%lu",1000000*now.tv_sec+now.tv_usec);
	
    write_io_queue.push_back(event);
    pthread_cond_wait(&empty_writequeue,&mx_writequeue);
	fprintf(rwqueue,"push write %d\n",write_io_queue.size());
    pthread_mutex_unlock(&mx_writequeue);
    pthread_cond_signal(&full_writequeue);
    printf("size of write queue%d\n",write_io_queue.size());
}

void *processReadQueue(void *argument){

    while(true) {
        //printf("\n%d %d",read_io_queue.size(),write_io_queue.size());
        pthread_mutex_lock(&mx_readqueue);

        printf("\n%d %d", read_io_queue.size(), write_io_queue.size());
            // pthread_mutex_lock(&checkempty_mutex);
            while (read_io_queue.size() == 0) {
                printf("wait for full\n");
                pthread_cond_signal(&checkempty_readqueue);
                pthread_cond_wait(&full_readqueue, &mx_readqueue);
            }
            //   pthread_mutex_unlock(&checkempty_mutex);
            //	printf("\n%d %d",read_io_queue.size(),write_io_queue.size());
            printf("read queue is not empty\n");
            IOrq_event readQueueCurrent = read_io_queue.front();
            int rdStrpnum = readQueueCurrent.strpno;
            pthread_mutex_lock(&mx_writequeue);
            if (!write_io_queue.empty()) {
                std::list<IOrq_event>::iterator it;
                std::list<IOrq_event>::iterator wrp;
                //scan wright queue
             //  pthread_mutex_lock(&mx_writequeue);
                printf("x1\n");
                wrp = write_io_queue.end();
                for (it = write_io_queue.begin(); it != write_io_queue.end();) {
                    printf("xxxx1\n");
                    int witStrpnum = it->strpno;
                    if (witStrpnum == rdStrpnum) {
                        printf("2\n");
                        if (wrp != write_io_queue.end()) {
                            printf("3\n");
                            if (wrp == it) {
                                it++;
                                write_io_queue.erase(wrp);
                                wrp = write_io_queue.begin();
                                printf("4\n");
                            } else {
                                write_io_queue.erase(wrp);
                                wrp = it;
                                it++;
                                printf("5\n");
                            }
                        } else {
                            it++;
                        }
                    } else {
                        it++;
                    }
                }
                printf("9\n");
                printf("10\n");
                if (wrp != write_io_queue.end() && wrp->strpno == rdStrpnum) {
                   // write_io_queue.emplace_front(*wrp);
                    write_io_queue.erase(wrp);
                    /*
                    IOrq_event writeQueueConsist = write_io_queue.front();
                    int wrStrpCon = writeQueueConsist.strpno;
                    fprintf(rwqueue, "find a same stripe number writeback %d, %d\n", wrStrpCon, rdStrpnum);
                    writeBack(wrStrpCon);
                    write_io_queue.pop_front();
                     */
                    wrp = write_io_queue.begin();
                   // readIn(rdStrpnum);

                    // pass data to read queue
                    struct timeval now;
                    gettimeofday(&now, NULL);
                    wrp->stop = 1000000 * now.tv_sec + now.tv_usec;
                    fprintf(printout, "%ld\n", wrp->stop - wrp->start);
                 //   pthread_mutex_unlock(&mx_writequeue);
                    read_io_queue.pop_front();
                    sem_post(&mutex_read_finish);
                    printf("xxxx6\n");
                } else {
                 //   pthread_mutex_unlock(&mx_writequeue);
		    struct timeval now_1,start_1;
		    gettimeofday(&start_1,NULL);
		    readQueueCurrent.start = 1000000 * start_1.tv_sec + start_1.tv_usec;
                    readIn(rdStrpnum);
                    gettimeofday(&now_1, NULL);
                    readQueueCurrent.stop = 1000000 * now_1.tv_sec + now_1.tv_usec;
                    fprintf(printout, "%ld\n", readQueueCurrent.stop - readQueueCurrent.start);
                    // gettimeofday(&(it->stop),NULL);
                    // fprintf(printout,"%ld\n", 1000000*(it->stop.tv_sec-it->start.tv_sec)+it->stop.tv_usec-it->start.tv_usec);
                    read_io_queue.pop_front();
                    sem_post(&mutex_read_finish);
                    printf("xxxx7\n");
                }
                printf("hello unlock write\n");
                 // pthread_mutex_unlock(&mx_writequeue);

            } else {
               // pthread_mutex_unlock(&mx_writequeue);
		struct timeval now_2,start_2;
		readQueueCurrent.start = 1000000 * start_2.tv_sec + start_2.tv_usec;
                readIn(rdStrpnum);
                
                gettimeofday(&now_2, NULL);
                readQueueCurrent.stop = 1000000 * now_2.tv_sec + now_2.tv_usec;
                fprintf(printout, "%ld\n", readQueueCurrent.stop - readQueueCurrent.start);
                //	gettimeofday(&(it->stop),NULL);
                //  fprintf(printout,"%ld\n", 1000000*(it->stop.tv_sec-it->start.tv_sec)+it->stop.tv_usec-it->start.tv_usec);
                read_io_queue.pop_front();
                sem_post(&mutex_read_finish);
                printf("xxxxx8\n");
            }
            pthread_mutex_unlock(&mx_writequeue);

            pthread_mutex_unlock(&mx_readqueue);
            printf("unlock read queue \n");
            // printf("size of ")
            pthread_cond_signal(&empty_readqueue);
            //	pthread_mutex_lock(&checkempty_mutex);

            //	while(read_io_queue.size()==0){
            //		pthread_cond_signal(&checkempty_readqueue);
            //		printf("signal read queue is empty\n");
            //	}
            //	pthread_mutex_unlock(&checkempty_mutex);

            //printf("ioqueue\n");
            //  pthread_exit(0);I
            //  fflush(printout);
            //   fflush(writeout);

    }
	fclose(printout);
	fclose(writeout);
}

void *processWriteQueue(void *argument){

	while(true) {
        //  printf("\n write %d %d",read_io_queue.size(),write_io_queue.size());
         // printf("hello  ======\n");
          pthread_mutex_lock(&mx_writequeue);
          if (write_io_queue.size() != 0) {
            printf("inside of if statement\n");
        //    pthread_mutex_lock(&mx_writequeue);
            //  pthread_mutex_lock(&mx_readqueue);
            //	pthread_mutex_lock(&checkempty_mutex);
            printf("hello i am in while loop \n");
            //read_io_queue.size() != 0
            //  || read_io_queue.size() != 0
            while (write_io_queue.size() == 0) {
                fprintf(rwqueue, "waiting for read is empty!\n");
                fprintf(rwqueue, "size of read_io_queue is %d\n", read_io_queue.size());
                printf("write waiting\n");
                pthread_cond_wait(&full_writequeue, &mx_writequeue);


            }
            while (read_io_queue.size() != 0) {
               // pthread_cond_wait
                pthread_cond_wait(&checkempty_readqueue,&mx_writequeue);
            }
            //  pthread_mutex_unlock(&mx_readqueue);
            //	pthread_mutex_unlock(&mx_readqueue);
            //    pthread_mutex_unlock(&checkempty_mutex);
            printf("--------------------------------------------------\n");

                fprintf(rwqueue, "size of read queue %d\n", read_io_queue.size());
                fprintf(rwqueue, "find a same stripe number and size is 0 for readqueue\n");
                IOrq_event writeQueueInit = write_io_queue.front();
                int winStrpnum = writeQueueInit.strpno;
                std::list<IOrq_event>::iterator itw;
                std::list<IOrq_event>::iterator wend;
                itw = write_io_queue.begin();
                wend = write_io_queue.end();
                IOrq_event *wp = NULL;
                for (++itw; itw != wend;) {
                    int wconStrpnum = itw->strpno;
                    if (wconStrpnum == winStrpnum) {
                        winStrpnum = wconStrpnum;
                        write_io_queue.pop_front();
                        write_io_queue.emplace_front(*itw);
                        write_io_queue.erase(itw++);
                    } else {
                        itw++;
                    }
                }
		struct timeval now_3;
                struct timeval now_2;
                int wrStrpnum = write_io_queue.front().strpno;
		gettimeofday(&now_3,NULL);
		write_io_queue.front().start= 1000000*now_3.tv_sec+now_3.tv_usec;
                writeBack(wrStrpnum);
                //  gettimeofday(&(write_io_queue.stop),NULL);
                //  fprintf(writeout,"%ld\n", 1000000*(write_io_queue.stop.tv_sec-write_io_queue.start.tv_sec)+write_io_queue.stop.tv_usec-write_io_queue.start.tv_usec);
                gettimeofday(&now_2, NULL);
                write_io_queue.front().stop = 1000000 * now_2.tv_sec + now_2.tv_usec;
                fprintf(writeout, "%ld\n", write_io_queue.front().stop - write_io_queue.front().start);
                write_io_queue.pop_front();
                //   pthread_mutex_unlock(&mx_readqueue);
               // pthread_mutex_unlock(&mx_writequeue);
                pthread_cond_signal(&empty_writequeue);



            //printf("ioqueue\n");
            //  pthread_exit(0);I
            //  fflush(printout);
            //   fflush(writeout);
        }
        pthread_mutex_unlock(&mx_writequeue);
    }
	fclose(printout);
	fclose(writeout);
}
