
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

int getSetBitIndices(bitmap n, int indices[64]) {
    int count = 0;
    for (int i = 0; i < 64; i++) {
        if (n & (1ULL << i)) {  // Check if the i-th bit is set
            indices[count++] = i;  // Store the index and increment count
        }
    }
    return count;  // Return the number of set bits
}

