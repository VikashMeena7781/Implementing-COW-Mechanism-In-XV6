#include "swap.h"
#include "defs.h"
#include "types.h"
#include "spinlock.h"  // Defines struct spinlock
#include "sleeplock.h" // Uses struct spinlock
#include "fs.h"
#include "mmu.h"
#include "param.h"
#include "buf.h"
#include "proc.h"
#include "memlayout.h"
#include "x86.h"

struct swap_slot swap_slots[NSLOTS]; // NSLOTS is calculated based on NSWAPBLOCKS and the size of a swap slot

void init_swap(void) {
    for (int i = 0; i < NSLOTS; i++) {
        swap_slots[i].is_free = FREE; // Initially, all slots are free
        swap_slots[i].page_perm = 0; // Default permissions
    }
}


int swapalloc(void) {
    for (int i = 0; i < NSLOTS; i++) {
        if (swap_slots[i].is_free == FREE) {
            swap_slots[i].is_free = OCCUPIED; // Mark the slot as occupied
            return i; // Return the index of the allocated slot
        }
    }
    return -1; // No free slot found
}


void write_swap_block(int block_addr, char *data) {
    struct buf *b = bread(0, block_addr); // Load the block from disk into buffer cache
    memmove(b->data, data, BSIZE);   // Copy data to the buffer block
    bwrite(b);    // Write the buffer block back to disk
    brelse(b);    // Release the buffer block
}

extern struct superblock sb;

void write_swap_slot(char *page, int slot_index) {
    if (slot_index < 0 || slot_index >= NSLOTS) {
        panic("Invalid swap slot index");
    }
    int block_addr = sb.swapstart + slot_index * 8; // Calculate starting block for this slot
    for (int i = 0; i < 8; i++) {
        cprintf("Writing block %d\n", block_addr + i);
        write_swap_block(block_addr + i, page + (i * BSIZE));
    }
}
// When you're dealing with functions like write_swap_slot(char *page, int slot_index), the parameter page is expected to be the address of the start of a memory page. Inside such a function, you might perform operations that involve adding an offset to page, like page + (i * BSIZE), to access different parts of the memory page for reading or writing.

//     This addition calculates the address of the block i blocks away from the start of page. It doesn't change page itself but computes a new address.
//     This is how you iterate through the memory page in block-sized chunks, suitable for block-wise disk I/O operations.

void read_swap_slot(char *page, int slot_index) {
    if (slot_index < 0 || slot_index >= NSLOTS) {
        panic("Invalid swap slot index");
    }
    int block_addr = sb.swapstart + slot_index * 8; // Calculate starting block for this slot
    for (int i = 0; i < 8; i++) {
        cprintf("Reading swap slot block %d\n", block_addr + i);
        struct buf *b = bread(0, block_addr + i); // Read the block from disk into buffer cache
        memmove(page + (i * BSIZE), b->data, BSIZE); // Copy data from the buffer block to page
        brelse(b); // Release the buffer block
    }
}

char* swap_out(){
    cprintf("swap out\n");
    struct proc* current_proc = myproc();
    int swap_slot = swapalloc();
    if (swap_slot==-1){
        panic("No free swap slot found");
        cprintf("No free swap slot found\n");
        return 0;
    }else{
        cprintf("Free swap slot found\n");
        struct proc* victim_proc = get_victim_proc();
        cprintf("Victim process found\n");
        victim_proc->rss-=PGSIZE;

        pte_t* victim_page = get_victim_page(victim_proc);
        cprintf("Victim page found\n");
        char* page = (char*)P2V(PTE_ADDR(*victim_page));

        swap_slots[swap_slot].page_perm = PTE_FLAGS(*victim_page);

        write_swap_slot(page,swap_slot);
        cprintf("Swap slot written\n");
        // page table entry  of victim process's page..
        *victim_page = (swap_slot << 12) | PTE_swapped;
        lcr3(V2P(current_proc->pgdir));
        cprintf("Page table entry updated\n");
        return page;
    }
}


void swap_in(){
    cprintf("swap in\n");
    struct proc *curproc = myproc();
    uint addr = rcr2();
    pde_t *pgdir = curproc->pgdir;
    pte_t *pte = walkpgdir(pgdir, (char*)addr, 0); 
    int swap_slot_no = *pte >> 12;
    struct swap_slot swap_slot = swap_slots[swap_slot_no];
    uint perms = swap_slot.page_perm;
    char *mem=kalloc() ;    
    read_swap_slot(mem, swap_slot_no); 
    *pte=V2P(mem) | perms | PTE_P ; 
    pte = walkpgdir(pgdir, (char*)addr, 0); 
    swap_slots[swap_slot_no].is_free = FREE;
    curproc->rss+= PGSIZE;
}


void clear_swap_slot(void){
    struct proc* process = myproc();
    // uint addr = rcr2();
    pde_t *pgdir = process->pgdir;
    for(uint va = 0;va < process->sz;va++){
        pte_t *pte = walkpgdir(pgdir, (void*)va, 0); 
        if(pte && (*pte & PTE_swapped)) {
            int swap_slot_no=*pte >> 12;
            swap_slots[swap_slot_no].is_free=1;
        }
        
    }

}
