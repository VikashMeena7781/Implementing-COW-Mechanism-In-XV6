// // #include "swap.h"
// #include "defs.h"
// #include "types.h"
// #include "spinlock.h"  // Defines struct spinlock
// #include "vm.h"   // Def
// #include "swap.h"
// #include "sleeplock.h" // Uses struct spinlock
// #include "fs.h"
// #include "mmu.h"
// #include "param.h"
// #include "buf.h"
// #include "proc.h"
// #include "memlayout.h"
// #include "x86.h"
// // #include "vm.h"


#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"
#include "spinlock.h"
#include "fs.h"
#include "sleeplock.h"
#include "buf.h"
#include "vm.h"
#include "swap.h"


void deep_copy_rmap_entry(struct rmap_entry *dest, struct rmap_entry *src) {
    // Copying simple fields directly
    dest->lock = src->lock;  // Assuming spinlock can be copied this way, might need special handling
    dest->ref_count = src->ref_count;
    dest->pa = src->pa;

    // Assuming procs needs to be deeply copied to avoid pointer sharing
    for (int i = 0; i < NPROC_R; i++) {
        if (src->procs[i] != NULL) {
            // Allocate new memory for each proc struct if needed or just copy the pointer value
            // depending on whether you need completely independent proc structures
            // or just to prevent the original proc pointer array manipulation from affecting this one.
            // Here we are just copying pointers assuming proc structures themselves are not modified, only replaced or reassigned
            dest->procs[i] = src->procs[i];
        } else {
            dest->procs[i] = NULL;
        }
    }
}

// RMap rmap;

struct swap_slot swap_slots[SWAPBLOCKS]; // NSLOTS is calculated based on NSWAPBLOCKS and the size of a swap slot

void init_swap(void) {
    // readsb(dev, &sb);
    for (int i = 0; i < SWAPBLOCKS; i++) {
        swap_slots[i].is_free = FREE; // Initially, all slots are free
        swap_slots[i].page_perm = 0; // Default permissions
    }
}
void update(pte_t *pte, int swap_slot);
void update_swap_in(int swap_slot, pde_t *pgdir, uint addr);

int swapalloc(void) {
    // cprintf("allocating free slot\n");
    for (int i = 0; i < SWAPBLOCKS; i++) {
        if (swap_slots[i].is_free == FREE) {
            swap_slots[i].is_free = OCCUPIED; // Mark the slot as occupied
            return i; // Return the index of the allocated slot
        }
    }
    // cprintf("%d\n",SWAPBLOCKS);
    return -1; // No free slot found
}


void write_swap_block(int block_addr, char *data) {
    // cprintf("Swap block writing\n");
    struct buf *b = bread(ROOTDEV, block_addr); // Load the block from disk into buffer cache
    // acquire(&rmap.lock); // Acquire the lock for the buffer block
    // cprintf("Swap block read\n");
    if (holding(&rmap.lock) == 0) {
        // cprintf("rmap lock is not held\n");
    }
    else {
        // cprintf("rmap lock is held\n");
    }
    // // cprintf("data: %s\n", data);
    memmove(b->data, data,   BSIZE);   // Copy data to the buffer block
    // // cprintf("Data copied to buffer block\n");
    // release(&rmap.lock); // Release the lock for the buffer block
    bwrite(b);    // Write the buffer block back to disk
    brelse(b);    // Release the buffer block
    // cprintf("Swap block released\n");
}

extern struct superblock sb;

void write_swap_slot(char *page, int slot_index) {
    if (slot_index < 0 || slot_index >= SWAPBLOCKS) {
        panic("Invalid swap slot index");
    }
    // cprintf("sb start %d\n", sb.swapstart);
    int block_addr = sb.swapstart + slot_index * 8; // Calculate starting block for this slot
    for (int i = 0; i < 8; i++) {
        // cprintf("Writing block %d\n", block_addr + i);
        write_swap_block(block_addr + i, page + (i * BSIZE));
    }
}

void read_swap_slot(char *page, int slot_index) {
    if (slot_index < 0 || slot_index >= SWAPBLOCKS) {
        panic("Invalid swap slot index");
    }
    int block_addr = sb.swapstart + slot_index * 8; // Calculate starting block for this slot
    for (int i = 0; i < 8; i++) {
        // cprintf("Reading swap slot block %d\n", block_addr + i);
        struct buf *b = bread(ROOTDEV, block_addr + i); // Read the block from disk into buffer cache

        memmove(page + (i * BSIZE), b->data, BSIZE); // Copy data from the buffer block to page
        brelse(b); // Release the buffer block
    }
}

char* swap_out(){
    // cprintf("swap out\n");
    struct proc* current_proc = myproc();
    int swap_slot = swapalloc();
    if (swap_slot==-1){
        panic("No free swap slot found");
        // cprintf("No free swap slot found\n");
        return 0;
    }else{
        // cprintf("Free swap slot found\n");
        struct proc* victim_proc = get_victim_proc();
        // cprintf("victim proc id: %d\n", victim_proc->pid);
        // cprintf("Victim process found\n");
        // victim_proc->rss-=PGSIZE;

        pte_t* victim_page = get_victim_page(victim_proc);
        // cprintf("Victim page found\n");
        char* page = (char*)P2V(PTE_ADDR(*victim_page));

        swap_slots[swap_slot].page_perm = PTE_FLAGS(*victim_page);

        write_swap_slot(page,swap_slot);



        update(victim_page, swap_slot);
    



        swap_slots[swap_slot].is_free = OCCUPIED;
        // cprintf("Page updated\n");
        // acquire(&rmap.lock);
        // release(&rmap.lock);
        // cprintf("Swap slot written\n");
        // page table entry  of victim process's page..
        // cprintf("pa of victim: %d\n", PTE_ADDR(*victim_page));
        // cprintf("victim page: %d\n", *victim_page);
        *victim_page = (swap_slot << 12) | PTE_swapped ;
        *victim_page &= ~PTE_P;
        // cprintf("victim page after update: %d\n", *victim_page);
        kfree(page);
        lcr3(V2P(current_proc->pgdir));
        // cprintf("Page table entry updated\n");
        return page;
    }
}


void swap_in(){
    // cprintf("swap in\n");
    struct proc *curproc = myproc();
    // CR2 holds the linear address that caused a page fault. 
    uint addr = rcr2();
    pde_t *pgdir = curproc->pgdir;
    // cprintf("swap in addr: %d\n", addr);
    // cprintf("swap in pid: %d\n", curproc->pid);
    pte_t *pte = walkpgdir(pgdir, (char*)addr, 0); 
    if (!pte || !(*pte & PTE_swapped)) {
        panic("swap_in: page not swapped out");
        // // cprintf("pte: %p\n", *pte);
        // // cprintf("no swap");
        // return;
    }
    int swap_slot_no = *pte >> 12;
    update_swap_in(swap_slot_no, pgdir, addr);
    // struct swap_slot swap_slot = swap_slots[swap_slot_no];
    // uint perms = swap_slot.page_perm;
    // char *mem=kalloc() ;    
    // read_swap_slot(mem, swap_slot_no); 
    // *pte=V2P(mem) | perms | PTE_P ; 
    // // just to ensure that updates reflects
    // pte = walkpgdir(pgdir, (char*)addr, 0); 

    // swap_slots[swap_slot_no].is_free = FREE;
    // curproc->rss+= PGSIZE;
}


void clear_swap_slot(void){
    struct proc* process = myproc();
    // uint addr = rcr2();
    pde_t *pgdir = process->pgdir;
    for(uint va = 0;va < process->sz;va++){
        pte_t *pte = walkpgdir(pgdir, (void*)va, 0); 
        if(pte && (*pte & PTE_swapped)) {
            int swap_slot_no=*pte >> 12;
            swap_slots[swap_slot_no].is_free=FREE;
        }
        
    }

}


void update(pte_t *pte, int swap_slot) {
    if (*pte & PTE_swapped) {
        panic("update: page is swapped out");
    }
    uint pa = PTE_ADDR(*pte); // Extract the physical address from the page table entry
    struct rmap_entry* entry;
    // cprintf("pa: %d\n", pa);
    // cprintf("pte: %d\n", *pte);
    // cprintf("update tried");
    // acquire(&rmap.lock); 
    for (int i = 0; i < MAX_RMAP_ENTRIES; i++) {
        entry = &rmap.entries[i];
        if (entry->pa == pa) { 
            // cprintf("update in if\n");
            // swap_slots[swap_slot].swap_rmap = *entry; // Deep copy the rmap entry
            swap_slots[swap_slot].is_free=OCCUPIED;
            // swap_slots[swap_slot].swap_rmap.pa = entry.pa; 
            // swap_slots[swap_slot].swap_rmap.ref_count=entry.ref_count;

            // swap_slots[swap_slot].swap_rmap.lock=entry.lock;

            deep_copy_rmap_entry(&swap_slots[swap_slot].swap_rmap, entry);

            // for(int k=0;k<NPROC;k++){
            //     swap_slots[swap_slot].swap_rmap.procs[k]=entry.procs[k];
            // }
            for (int j = 0; j < NPROC; j++) {
                if (entry->procs[j] != NULL) {
                    // cprintf("update in loop\n");
                    pte_t *proc_pte;
                    // Not sure about this line 
                    proc_pte = walkpgdir(entry->procs[j]->pgdir, (void*)P2V(pa), 0);
                    if ((proc_pte != 0) && (*proc_pte & PTE_P)) {
                        // cprintf("update in andar\n");
                        // Set the PTE to indicate that the page is now swapped out
                        *proc_pte = (*proc_pte & 0xfff) | (swap_slot << 12) | PTE_swapped;

                        entry->procs[j]->rss-=PGSIZE;
                        *proc_pte &= ~PTE_P;
                        // cprintf("proc pte: %d\n", *proc_pte);
                        // panic("update: page is swapped out");
                    }
                        lcr3(V2P(entry->procs[j]->pgdir));
                }
            }
            // Clear the original rmap entry manually using a loop
            entry->pa = 0;
            entry->ref_count = 0;
            for (int j = 0; j < NPROC; j++) {
                entry->procs[j] = NULL;
            }

            // break; // Exit the loop once the entry is processed
        }
    }
    // release(&rmap.lock); // Release the lock
    // cprintf("update done");
    lcr3(V2P(myproc()->pgdir));  // Flush the TLB to ensure the changes take effect
}



// TO DO: change increment, decrement for swapped entries, swap in tell all processes 

void update_swap_in(int swap_slot, pde_t *pgdir, uint addr) {
    // cprintf("update in swap in\n");
    struct swap_slot* slot = &swap_slots[swap_slot];
    // cprintf("slot: %d\n", swap_slot);
    if (slot->is_free == FREE) {
        panic("update_swap_in: slot is already free");
    }

    char *mem = kalloc(); 
    // cprintf("memory allocated after kalloc\n"); 
    if (!mem) {
        panic("update_swap_in: unable to allocate memory");
    }
    uint perms = slot->page_perm;
    // struct rmap_entry *entry = &slot->swap_rmap;
    read_swap_slot(mem, swap_slot); 

    // acquire(&rmap.lock);  
    // cprintf("swap in slot read\n");

    // Iterate over all processes that might be using this page
    // for (int j = 0; j < NPROC; j++) {
    //     struct proc *p = entry->procs[j];
    //     if (p) {
    //         // not sure about this line
    //         pte_t *pte = walkpgdir(p->pgdir, (void*)P2V(entry->pa), 0);
    //         if (pte && (*pte & PTE_swapped)) {
    //             // Clear the swap indication and update the page table
    //             *pte = V2P(mem) | perms | PTE_P;
    //               // Set the physical address and permissions
    //             * pte &= ~PTE_swapped;
    //             lcr3(V2P(p->pgdir));  // Flush the TLB to ensure the changes take effect
    //             p->rss+=PGSIZE;
    //         }else{
    //             panic("Page should be marked swapped_out\n");
    //         }
    //     }
    // }

    slot->is_free = FREE;
    slot->page_perm = 0;

    // add in rmap
    // int found_index = -1;
    // for (int i = 0; i < MAX_RMAP_ENTRIES; i++) {
    //     if (rmap.entries[i].ref_count == 0) {  
    //         found_index = i;
    //         break;
    //     }
    // }

    // if (found_index == -1) {
    //     panic("update_swap_in: No free rmap entry found");
    // }

    // Copy the swap slot's rmap_entry to the found index in the original rmap
    // Not sure about this line
    // entry->pa=(uint)V2P(mem);
    // rmap.entries[found_index] = *entry;


    // // clear from swap_slot
    // entry->ref_count = 0;    
    // entry->pa = 0;          
    // for (int i = 0; i < NPROC; i++) {
    //     entry->procs[i] = NULL;
    // }
    pte_t *pte = walkpgdir(pgdir, (char*)addr, 0); 
    *pte=V2P(mem) | perms | PTE_P ; 
    // cprintf("pte: %p\n", *pte);
    // just to ensure that updates reflects
    // pte = walkpgdir(pgdir, (char*)addr, 0);

    lcr3(V2P(myproc()->pgdir));  // Flush the TLB to ensure the changes take effect
    // release(&rmap.lock);  // Release the lock
    // cprintf("update swap in done\n");

}
