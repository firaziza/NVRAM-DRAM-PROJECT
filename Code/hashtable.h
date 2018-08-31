#include <string>
#include <vector>
using namespace std;

#define CACHE_SIZE 100
#define NVRAM_BLOCK_CACHE_SIZE 5000
#define NVRAM_CACHE_SIZE 1000

class HashTable {

private:
	block block_cache[BLOCK_CACHE_SIZE];
	cacheQueueParam *qp;
	cacheStripe *hashTable[CACHE_SIZE];
	char writeData[BLOCK_SIZE];


	block nvram_cache[NVRAM_BLOCK_CACHE_SIZE];
	nvramStripe *nvramHashTable[NVRAM_CACHE_SIZE];
	nvramQueueParam *nqp;

	lNvramQueueParam *ml, *cl, *fl;
	lNvramStripe *lNvramHashTable[NVRAM_CACHE_SIZE];

public:
   	void initializeHashTable();
	
	void hashUpdateread(int stripeno,int blockno);

  	void usedUpdateAtHash(int stripeno,int blockno);
	void initializeNvramHashTable();	
	void nvramhashUpdate(int stripeno, int blockno);
	void nvramUpdateAtHash(int stripeno,int blockno);
	void initializeLargeNvramHashTable();
	void deletelNvramHashTable(int sno);
	void addLNvramHash(int sno);
	void updateLNvramHash(int stripeno,int blockno);
	void addLNvramHashPersist(int sno,int blockno);

};

