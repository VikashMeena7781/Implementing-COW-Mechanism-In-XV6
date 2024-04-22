
#define MAX_RMAP_ENTRIES 1024
// extern struct rmap_entry rmaps[MAX_RMAP_ENTRIES];



#define NPROC_R 64
#define NPDENTRIES_R 1024
#define NPTENTRIES_R 1024

struct rmap_entry {
  int ref_count;  // Count of processes referencing this page
  uint pa;        // Physical address
  struct proc *procs[NPROC_R];  // Array of pointers to processes
};

typedef struct {
    struct spinlock lock;
    struct rmap_entry entries[MAX_RMAP_ENTRIES];
} RMap;

extern RMap rmap; 


