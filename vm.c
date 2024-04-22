#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"
#include "spinlock.h"
#include "vm.h"
#include "swap.h"
// #define NULL ((void *)0)
// #define PA2IDX(pa) (((uint)(pa) / PGSIZE) % (MAX_RMAP_ENTRIES))

extern char data[];  // defined by kernel.ld
pde_t *kpgdir;  // for use in scheduler()

// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.vmh
RMap rmap; 

void rmap_init(void) {
  cprintf("rmap_init\n");
  initlock(&rmap.lock, "rmap");
  for (int i = 0; i < MAX_RMAP_ENTRIES; i++) {
    char * name = "rmap_entry_" + i;
    cprintf("name: %s\n", name);
    initlock(&rmap.entries[i].lock, name);
  }
  // acquire(&rmap.lock);
  for (int i = 0; i < MAX_RMAP_ENTRIES; i++) {
    rmap.entries[i].ref_count = 0;
    rmap.entries[i].pa = 0;
    for (int j = 0; j < NPROC; j++) {
      rmap.entries[i].procs[j] = NULL;
    }
  }
  // release(&rmap.lock);
}

int increment_rmap(pte_t * pte, struct proc *p) {

    // check if pte is swapped
    if (*pte & PTE_swapped) {
      // get swap block 
      int swap_slot_no = *pte >> 12;
      struct swap_slot swap_slot = swap_slots[swap_slot_no];

      struct rmap_entry entry = swap_slot.swap_rmap;
      int ans = -1;
  // Store index of the first free slot
          int found = 0;

      // acquire lock
      acquire(&entry.lock);
      for (int j = 0; j < NPROC; j++) {
                if (entry.procs[j] == p) {
                    entry.ref_count++;  // Increment ref count if pa found
                    ans = entry.ref_count;
                    panic("Process already exists\n");
                    found = 1;
                   break;
                }
            }

            if (!found) {
                for (int j = 0; j < NPROC; j++) {
                    if (entry.procs[j] == NULL) {
                        entry.procs[j] = p;
                        entry.ref_count++;  // Increment the ref count only if adding a new process
                        ans = entry.ref_count;
                        break;
                    }
                }
            }
            release(&entry.lock);
            return ans;
    }

    uint pa = PTE_ADDR(*pte);
    acquire(&rmap.lock);
    // int idx = PA2IDX(pa);  // Convert physical address to index
    // struct rmap_entry *entry = &rmap.entries[idx];
    int ans = -1;

    int free_index = -1;  // Store index of the first free slot
    int found = 0;
    for (int i = 0; i < MAX_RMAP_ENTRIES; i++) {
        if (rmap.entries[i].pa == pa && rmap.entries[i].ref_count > 0) {
            for (int j = 0; j < NPROC; j++) {
                if (rmap.entries[i].procs[j] == p) {
                    rmap.entries[i].ref_count++;  // Increment ref count if pa found
                    ans = rmap.entries[i].ref_count;
                    panic("Process already exists\n");
                    found = 1;
                   break;
                }
            }

            if (!found) {
                for (int j = 0; j < NPROC; j++) {
                    if (rmap.entries[i].procs[j] == NULL) {
                        rmap.entries[i].procs[j] = p;
                        rmap.entries[i].ref_count++;  // Increment the ref count only if adding a new process
                        ans = rmap.entries[i].ref_count;
                        break;
                    }
                }
            }
            release(&rmap.lock);
            return ans;
        }
        if (rmap.entries[i].ref_count == 0 && free_index == -1) {
            free_index = i;  // Remember first free slot
        }
    }
    
    if (free_index != -1) {
      rmap.entries[free_index].pa = pa;
      rmap.entries[free_index].ref_count = 1;
      rmap.entries[free_index].procs[0] = p;
      ans = rmap.entries[free_index].ref_count;
      release(&rmap.lock);
      return ans;
    }

    // Check if the process is already associated with this page
    // int found = 0;
    // for (int i = 0; i < NPROC; i++) {
        // if (entry->procs[i] == p) {
            // found = 1;  // Process is already listed
            // entry->ref_count++;  // Increment the ref count
            // ans = entry->ref_count;
            // break;
        // }
    // }

    // If the process is not found, add it to the array
    // if (!found) {
        // for (int i = 0; i < NPROC; i++) {
            // if (entry->procs[i] == NULL) {
                // entry->procs[i] = p;
                // entry->ref_count++;  // Increment the ref count only if adding a new process
                // ans = entry->ref_count;
                // break;
            // }
        // }
    // }

    release(&rmap.lock);
    return ans;
}


int decrement_rmap(pte_t * pte, struct proc *p) {
  if (*pte & PTE_swapped) {
    // Handle swap case
    int swap_slot_no = *pte >> 12;
    struct swap_slot swap_slot = swap_slots[swap_slot_no];
    struct rmap_entry entry = swap_slot.swap_rmap;
    int ans = -1;
    acquire(&entry.lock);
    for (int i = 0; i < NPROC; i++) {
      if (entry.procs[i] == p) {
        entry.procs[i] = NULL;
        entry.ref_count--;
        ans = entry.ref_count;
        if (entry.ref_count == 0) {
          entry.pa = 0;  // Mark the entry as unused
          entry.ref_count = 0;
          for (int j = 0; j < NPROC; j++) {
            entry.procs[j] = NULL;
          }
          swap_slot.is_free = FREE;
        }
        
      }
    }
      release(&entry.lock);
      return ans;

  }

  uint pa = PTE_ADDR(*pte);
  // int idx = PA2IDX(pa);
  // struct rmap_entry *entry = &rmap.entries[idx];
  int ans = -1;
  if (p == NULL) {
    return 0;
  }
  acquire(&rmap.lock);

  for (int i = 0; i < MAX_RMAP_ENTRIES; i++) {
    if (rmap.entries[i].pa == pa) {
      if (rmap.entries[i].ref_count > 0) {
        rmap.entries[i].ref_count--;  // Decrement ref count if pa found
        ans = rmap.entries[i].ref_count;

        // Remove process from the list
        int done = 0;
        for (int j = 0; j < NPROC; j++) {
          if (rmap.entries[i].procs[j] == p) {
            rmap.entries[i].procs[j] = NULL;
            done = 1;
            break;
          }
        }
        if (!done) {
          panic("decrement_rmap: process not found in rmap entry");
        }
        if (rmap.entries[i].ref_count == 0) {
          rmap.entries[i].pa = 0;  // Mark the entry as unused
          rmap.entries[i].ref_count = 0;
          for (int j = 0; j < NPROC; j++) {
            rmap.entries[i].procs[j] = NULL;
          }
        }
        release(&rmap.lock);
        return ans;
      } else {
        // Error: trying to decrement a non-referenced page
        panic("decrement_rmap: ref count is already 0");
      }
    }
  }
  // Remove process from the list
  // for (int i = 0; i < NPROC; i++) {
    // if (entry->procs[i] == p) {
      // entry->procs[i] = NULL;
      // break;
    // }
  // }
  // if (--entry->ref_count < 0) {
    // Optionally clear process references or handle zero reference cleanup
    // entry->ref_count = 0;
  // }
  // ans = entry->ref_count;
  release(&rmap.lock);
  return ans;
}


// int rmap_increment(uint pa) {
//     int free_index = -1;  // Store index of the first free slot

//     for (int i = 0; i < NPDENTRIES_R * NPTENTRIES_R; i++) {
//         if (rmaps[i].pa == pa && rmaps[i].ref_count > 0) {
//             rmaps[i].ref_count++;  // Increment ref count if pa found
//             return rmaps[i].ref_count;
//         }
//         if (rmaps[i].ref_count == 0 && free_index == -1) {
//             free_index = i;  // Remember first free slot
//         }
//     }

//     // If pa was not found and there is a free slot, use it
//     if (free_index != -1) {
//         rmaps[free_index].pa = pa;
//         rmaps[free_index].ref_count = 1;
//         return rmaps[free_index].ref_count;
//     }

//     return -1; // No space in rmap and pa not found
// }



// int rmap_decrement(uint pa) {
//     for (int i = 0; i < MAX_RMAP_ENTRIES; i++) {
//         if (rmaps[i].pa == pa) {  // Find the entry matching the physical address
//             if (rmaps[i].ref_count > 0) {
//                 rmaps[i].ref_count--;  // Decrement the reference count
//                 if (rmaps[i].ref_count == 0) {
//                     // Optional: Handle the case where no references remain
//                     // This could involve freeing the page, marking the entry as empty, etc.
//                     rmaps[i].pa = 0;  // Mark the entry as unused
//                 }
//                 return rmaps[i].ref_count;  // Success
//             } else {
//                 // Error: trying to decrement a non-referenced page
//                 return -1;  // Failure or error
//             }
//         }
//     }
//     return -1; // No entry found for given physical address
// }



void
seginit(void)
{
  struct cpu *c;

  // Map "logical" addresses to virtual addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.
  c = &cpus[cpuid()];
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);
  lgdt(c->gdt, sizeof(c->gdt));
}

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va.  If alloc!=0,
// create any required page table pages.
pte_t *
walkpgdir(pde_t *pgdir, const void *va, int alloc)
{
  pde_t *pde;
  pte_t *pgtab;

  pde = &pgdir[PDX(va)];
  if(*pde & PTE_P){
    pgtab = (pte_t*)P2V(PTE_ADDR(*pde));
  } else {
    if(!alloc || (pgtab = (pte_t*)kalloc()) == 0)
      return 0;
    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);
    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table
    // entries, if necessary.
    *pde = V2P(pgtab) | PTE_P | PTE_W | PTE_U;
  }
  return &pgtab[PTX(va)];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.
static int
mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm)
{
  char *a, *last;
  pte_t *pte;

  a = (char*)PGROUNDDOWN((uint)va);
  last = (char*)PGROUNDDOWN(((uint)va) + size - 1);
  for(;;){
    if((pte = walkpgdir(pgdir, a, 1)) == 0)
      return -1;
    if(*pte & PTE_P)
      {cprintf("mappages: pte: %p\n", pte);
      cprintf("mappages: pa: %p\n", pa);
      cprintf("mappages: perm: %p\n", perm);
      cprintf("mappages: PTE_ADDR(*pte): %p\n", PTE_ADDR(*pte));
      cprintf("mappages: PTE_FLAGS(*pte): %p\n", PTE_FLAGS(*pte));
      panic("remap");}
    *pte = pa | perm | PTE_P;
    if(a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// There is one page table per process, plus one that's used when
// a CPU is not running any process (kpgdir). The kernel uses the
// current process's page table during system calls and interrupts;
// page protection bits prevent user code from using the kernel's
// mappings.
//
// setupkvm() and exec() set up every page table like this:
//
//   0..KERNBASE: user memory (text+data+stack+heap), mapped to
//                phys memory allocated by the kernel
//   KERNBASE..KERNBASE+EXTMEM: mapped to 0..EXTMEM (for I/O space)
//   KERNBASE+EXTMEM..data: mapped to EXTMEM..V2P(data)
//                for the kernel's instructions and r/o data
//   data..KERNBASE+PHYSTOP: mapped to V2P(data)..PHYSTOP,
//                                  rw data + free physical memory
//   0xfe000000..0: mapped direct (devices such as ioapic)
//
// The kernel allocates physical memory for its heap and for user memory
// between V2P(end) and the end of physical memory (PHYSTOP)
// (directly addressable from end..P2V(PHYSTOP)).

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap {
  void *virt;
  uint phys_start;
  uint phys_end;
  int perm;
} kmap[] = {
 { (void*)KERNBASE, 0,             EXTMEM,    PTE_W}, // I/O space
 { (void*)KERNLINK, V2P(KERNLINK), V2P(data), 0},     // kern text+rodata
 { (void*)data,     V2P(data),     PHYSTOP,   PTE_W}, // kern data+memory
 { (void*)DEVSPACE, DEVSPACE,      0,         PTE_W}, // more devices
};

// Set up kernel part of a page table.
pde_t*
setupkvm(void)
{
  pde_t *pgdir;
  struct kmap *k;

  if((pgdir = (pde_t*)kalloc()) == 0)
  {cprintf("lund mera\n");
    return 0;}
  memset(pgdir, 0, PGSIZE);
  if (P2V(PHYSTOP) > (void*)DEVSPACE)
    panic("PHYSTOP too high");
  for(k = kmap; k < &kmap[NELEM(kmap)]; k++)
    if(mappages(pgdir, k->virt, k->phys_end - k->phys_start,
                (uint)k->phys_start, k->perm) < 0) {
      freevm(pgdir, NULL);
      cprintf("lund tera");
      return 0;
    }
  return pgdir;
}

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void)
{
  kpgdir = setupkvm();
  switchkvm();
}

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void)
{
  lcr3(V2P(kpgdir));   // switch to the kernel page table
}

// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p)
{
  if(p == 0)
    panic("switchuvm: no process");
  if(p->kstack == 0)
    panic("switchuvm: no kstack");
  if(p->pgdir == 0)
    panic("switchuvm: no pgdir");

  pushcli();
  mycpu()->gdt[SEG_TSS] = SEG16(STS_T32A, &mycpu()->ts,
                                sizeof(mycpu()->ts)-1, 0);
  mycpu()->gdt[SEG_TSS].s = 0;
  mycpu()->ts.ss0 = SEG_KDATA << 3;
  mycpu()->ts.esp0 = (uint)p->kstack + KSTACKSIZE;
  // setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
  // forbids I/O instructions (e.g., inb and outb) from user space
  mycpu()->ts.iomb = (ushort) 0xFFFF;
  ltr(SEG_TSS << 3);
  lcr3(V2P(p->pgdir));  // switch to process's address space
  popcli();
}

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz, struct proc *p)
{
  char *mem;

  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pgdir, 0, PGSIZE, V2P(mem), PTE_W|PTE_U);
  memmove(mem, init, sz);
  // pte from mem
  pte_t * pt = walkpgdir(pgdir, mem, 0);
  increment_rmap(pt, p);

}

// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz)
{
  uint i, pa, n;
  pte_t *pte;

  if((uint) addr % PGSIZE != 0)
    panic("loaduvm: addr must be page aligned");
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, addr+i, 0)) == 0)
      panic("loaduvm: address should exist");
    pa = PTE_ADDR(*pte);
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, P2V(pa), offset+i, n) != n)
      return -1;
  }
  return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm( pte_t * pgdir, uint oldsz, uint newsz,struct proc *p)
{
  char *mem;
  uint a;

  if(newsz >= KERNBASE)
    return 0;
  if(newsz < oldsz)
    return oldsz;

  a = PGROUNDUP(oldsz);
  for(; a < newsz; a += PGSIZE){
    mem = kalloc();
    if(mem == 0){
      cprintf("allocuvm out of memory\n");
      deallocuvm(pgdir, newsz, oldsz,p );
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if(mappages(pgdir, (char*)a, PGSIZE, V2P(mem), PTE_W|PTE_U) < 0){
      cprintf("allocuvm out of memory (2)\n");
      deallocuvm(pgdir, newsz, oldsz, p);
      kfree(mem);
      return 0;
    }
    pte_t * pt = walkpgdir(pgdir, (char*)mem, 0);
    // Increment the reference count for the page
    if (increment_rmap(pt, p) == -1) {
      cprintf("allocuvm: increment_rmap failed\n");
      deallocuvm(pgdir, newsz, oldsz, p);
      kfree(mem);
      panic("allocuvm: increment_rmap failed");
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
int
deallocuvm(pte_t * pgdir, uint oldsz, uint newsz,struct proc *p )
{
  pte_t *pte;
  uint a, pa;

  if(newsz >= oldsz)
    return oldsz;

  a = PGROUNDUP(newsz);
  for(; a  < oldsz; a += PGSIZE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(!pte)
      a = PGADDR(PDX(a) + 1, 0, 0) - PGSIZE;
    else if((*pte & PTE_P) != 0){
      pa = PTE_ADDR(*pte);
      if(pa == 0)
        panic("kfree");
      char *v = P2V(pa);
      // decrement the reference count for the page
      int ref_count = decrement_rmap(pte, p);
      if ( ref_count == -1) {
        cprintf("deallocuvm: decrement_rmap failed\n");
        panic("deallocuvm: decrement_rmap failed");
        return 0;
      }
      if (ref_count == 0) {
        kfree(v);
      }
      *pte = 0;
    }
  }
  return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.
void
freevm(pde_t *pgdir, struct proc *p)
{
  uint i;

  if(pgdir == 0)
    panic("freevm: no pgdir");
  deallocuvm(pgdir, KERNBASE, 0, p);
  for(i = 0; i < NPDENTRIES; i++){
    if(pgdir[i] & PTE_P){
      char * v = P2V(PTE_ADDR(pgdir[i]));
      // apply logic for freeing
      kfree(v);
    }
  }
  kfree((char*)pgdir);
}

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void
clearpteu(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if(pte == 0)
    panic("clearpteu");
  *pte &= ~PTE_U;
}

// // Given a parent process's page table, create a copy
// // of it for a child.
// pde_t*
// copyuvm(pde_t *pgdir, uint sz)
// {
//   pde_t *d;
//   pte_t *pte;
//   uint pa, i, flags;
//   char *mem;

//   if((d = setupkvm()) == 0)
//     return 0;
//   for(i = 0; i < sz; i += PGSIZE){
//     if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
//       panic("copyuvm: pte should exist");
//     if(!(*pte & PTE_P))
//       panic("copyuvm: page not present");
//     pa = PTE_ADDR(*pte);
//     flags = PTE_FLAGS(*pte);
//     if((mem = kalloc()) == 0)
//       goto bad;
//     memmove(mem, (char*)P2V(pa), PGSIZE);
//     if(mappages(d, (void*)i, PGSIZE, V2P(mem), flags) < 0) {
//       kfree(mem);
//       goto bad;
//     }
//   }
//   return d;

// bad:
//   freevm(d);
//   return 0;
// }


// Given a parent process's page table, create a copy
// of it for a child using the Copy-On-Write (COW) approach.
pde_t*
copyuvm(pde_t *pgdir, uint sz, struct proc *p)
{
  pde_t *d;
  pte_t *pte , *new_pte;
  // uint pa, i, flags;
  uint i ;

  if((d = setupkvm()) == 0){
    cprintf("0 is returned\n");
    return 0;
  }
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, (void *) i, 0)) == 0)
      panic("copyuvm: pte should exist");
    if(!(*pte & PTE_P))
      panic("copyuvm: page not present");

    // pa = PTE_ADDR(*pte);
    // flags = PTE_FLAGS(*pte);

    // Make the page in the parent read-only
    *pte = (*pte & ~PTE_W) | PTE_U;

    new_pte = walkpgdir(d, (void *)i , 1);
    if (*new_pte & PTE_P) panic("firse wahi dikkat");

    * new_pte = * pte;

    
    // increment rmap
    if (increment_rmap(pte, p) == -1) {
      cprintf("copyuvm: increment_rmap failed\n");
      panic("copyuvm: increment_rmap failed");
      return 0;
    }


    // Duplicate the PTE for the child, also as read-only
    // if(mappages(d, (void*)i, PGSIZE, pa, flags & ~PTE_W) < 0) {
    //   goto bad;
    // }
  }

    // Invalidate the TLB to ensure the CPU uses updated page table entries
    struct proc *curproc = myproc();

    switchuvm(curproc);
    // lcr3(V2P(pgdir));

  return d;

// bad:
//   freevm(d);
//   return 0;
}



// int copy_on_write(void) {
//     struct proc *curproc = myproc();
//     uint faulting_addr = rcr2();  // Read CR2 register to get the faulting virtual address
//     pte_t *pte;
//     char *mem;

//     // Get the page table entry for the faulting address
//     if ((pte = walkpgdir(curproc->pgdir, (void *)faulting_addr, 0)) == 0)
//         panic("copy_on_write: pte should exist");

//     // Check if the fault was due to a write attempt on a read-only page
//     if (!(*pte & PTE_W)) {
//         // Allocate a new page
//         if ((mem = kalloc()) == 0)
//             panic("copy_on_write: unable to allocate memory");

//         // Get the physical address from the page table entry
//         uint pa = PTE_ADDR(*pte);
//         // Copy the contents of the old page to the new page
//         memmove(mem, (char*)P2V(pa), PGSIZE);

//         // Map the new page with write permissions
//         if (mappages(curproc->pgdir, (void*)(faulting_addr & ~0xFFF), PGSIZE, V2P(mem), PTE_FLAGS(*pte) | PTE_W) < 0) {
//             kfree(mem);
//             panic("copy_on_write: mappages failed");
//         }

//         // Unmap the old page
//         // kfree(P2V(pa));
//         *pte = 0;  // Clear the old page table entry to avoid stale mappings
//         return 1;
//     }

//     return 0;
// }

int copy_on_write(void) {
    struct proc *curproc = myproc();
    uint faulting_addr = rcr2();  // Read CR2 register to get the faulting virtual address
    pte_t *pte;
    cprintf("copy on write used\n");

    // Get the page table entry for the faulting address
    if ((pte = walkpgdir(curproc->pgdir, (void *)faulting_addr, 0)) == 0) {
        panic("copy_on_write: pte should exist\n");
    }

    // Check if the fault was due to a write attempt on a read-only page
    if (!(*pte & PTE_W) && (*pte & PTE_P)) {
        // It's a read-only page but present, handle COW
        char *mem;

        // Allocate a new page
        if ((mem = kalloc()) == 0) {
            panic("copy_on_write: unable to allocate memory");
        }

        // Copy the contents of the old page to the new page
        uint pa = PTE_ADDR(*pte);
        memmove(mem, (char*)P2V(pa), PGSIZE);

        // Map the new page with write permissions
        // if (mappages(curproc->pgdir, (void*)(faulting_addr & ~0xFFF), PGSIZE, V2P(mem), PTE_FLAGS(*pte) | PTE_W) < 0) {
        //     kfree(mem);
        //     panic("copy_on_write: mappages failed");
        // }
        int ref_count = decrement_rmap(pte, curproc);

        *pte = V2P(mem) | PTE_FLAGS(*pte) | PTE_W | PTE_P;  

        // Update the rmap if using a reference count system
        // 
        // Increment the reference count for the page
        if (increment_rmap(pte, curproc) == -1) {
            panic("copy_on_write: increment_rmap failed");
        }

        // decrement the reference count for the page
        
        if (ref_count == -1) {
            panic("copy_on_write: decrement_rmap failed");
        }

        if (ref_count == 0) {
            kfree(P2V(pa));
        }

        lcr3(V2P(curproc->pgdir));  // Refresh the TLB to reflect the PTE change

        return 1;  // Handled a COW page fault
    }
    // else panic("page writeable or not present");

    // The page fault was not due to a COW scenario
    return 0;
}


//PAGEBREAK!
// Map user virtual address to kernel address.
char*
uva2ka(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if((*pte & PTE_P) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  return (char*)P2V(PTE_ADDR(*pte));
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
  char *buf, *pa0;
  uint n, va0;

  buf = (char*)p;
  while(len > 0){
    va0 = (uint)PGROUNDDOWN(va);
    pa0 = uva2ka(pgdir, (char*)va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (va - va0);
    if(n > len)
      n = len;
    memmove(pa0 + (va - va0), buf, n);
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.
//PAGEBREAK!
// Blank page.

