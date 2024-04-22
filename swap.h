#define SWAP_SLOT_SIZE 8 // Each swap slot takes up 8 blocks
// #define NSLOTS 286 //total number of slots 32
// #define PTE_swapped     0x008   // Swapped  
#define FREE 1
#define OCCUPIED 0
// #include "types.h"
// #include "spinlock.h"
// #include "vm.h"


struct rmap_entry;

struct swap_slot {
  uint page_perm; // Permission of the swapped memory page
  uint is_free;   // Indicates if the swap slot is free (0) or in use (1)
  struct rmap_entry swap_rmap;
};


extern struct swap_slot swap_slots[NSLOTS]; // NSLOTS is calculated based on NSWAPBLOCKS and the size of a swap slot
// todo read and write functions

// extern struct swap_slot swap_slots[NSLOTS]; // NSLOTS is calculated based on NSWAPBLOCKS and the size of a swap slot

void init_swap(void);
int swapalloc(void);
void write_block(int block_addr, char *data);

void write_swap_slot(char *page, int slot_index);

void read_page_from_swap(int slot_id, void* page_data);

void clear_swap_slot(void);

char* swap_out();
void swap_in();

// static uint
// balloc(uint dev)
// {
//   int b, bi, m;
//   struct buf *bp;

//   bp = 0;
//   for(b = 0; b < sb.size; b += BPB){
//     bp = bread(dev, BBLOCK(b, sb));
//     for(bi = 0; bi < BPB && b + bi < sb.size; bi++){
//       m = 1 << (bi % 8);
//       if((bp->data[bi/8] & m) == 0){  // Is block free?
//         bp->data[bi/8] |= m;  // Mark block in use.
//         log_write(bp);
//         brelse(bp);
//         bzero(dev, b + bi);
//         return b + bi;
//       }
//     }
//     brelse(bp);
//   }
//   panic("balloc: out of blocks");
// }