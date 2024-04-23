
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

unsigned int getcount(unsigned long long n) {
    unsigned int count = 0;
    while (n) {
        n &= (n - 1);  // clear the least significant bit set
        count++;
    }
    return count;
}

void setBit(unsigned long long *n, int index, int value) {
    if (value == 1) {
        *n |= (1ULL << index);  // Set bit at position 'index' to 1
    } else {
        *n &= ~(1ULL << index); // Set bit at position 'index' to 0
    }
}


int isBitSet(unsigned long long n, int index) {
    return (n & (1ULL << index)) != 0;  // Check if the bit at 'index' is set
}
