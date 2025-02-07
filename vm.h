
#define MAX_RMAP_ENTRIES 1024
// extern struct rmap_entry rmaps[MAX_RMAP_ENTRIES];



#define NPROC_R 64
#define NPDENTRIES_R 1024
#define NPTENTRIES_R 1024

struct rmap_entry {
  uint pa;        // Physical address
  bitmap procbitmap;  // Array of pointers to processes
};

typedef struct {
    struct spinlock lock;
    struct rmap_entry entries[MAX_RMAP_ENTRIES];
} RMap;

extern RMap rmap; 

unsigned int getcount(unsigned long long n) ;

int getSetBitIndices(bitmap n, int indices[64]) ;

void setBit(unsigned long long *n, int index, int value) ;
int isBitSet(unsigned long long n, int index);