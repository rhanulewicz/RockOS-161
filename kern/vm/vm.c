#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <spl.h>
#include <cpu.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>
#include <vnode.h>
#include <vfs.h>
#include <stat.h>

static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

static unsigned long pagesAlloced = 0;
static unsigned int used =  0;

struct lock* swapLock;
struct bitmap* swapMap;

void vm_bootstrap(){
	return;
	//So far this is mostly testing code to see if we can open the swapdisk
	swapLock = lock_create("swap_lock");
	struct stat* statBox = kmalloc(sizeof(struct stat));
	struct vnode* llfile = kmalloc(sizeof(struct vnode));
	if(llfile == NULL || statBox == NULL){
		panic("fucked it");
	}
	char foo [] = "lhd0raw:";
	int res = vfs_open(foo, 1, 0, &llfile);
	(void)res;
	if(res > 0){
		panic("%d\n", res);
	}
	VOP_STAT(llfile, statBox);

	//Initialize the swapdisk bitmap
	int disksize = statBox->st_size;
	swapMap = bitmap_create((unsigned)(disksize/PAGE_SIZE));
	
	kprintf("Disk size (bytes): %d\n",  (int)statBox->st_size);
	kprintf("Bitmap size: %d\n",  disksize/PAGE_SIZE);

	(void) statBox;
	(void) llfile;

	return;
}


/*
 * Level of indirection not necessary, but allows us to swap between ASST2 and ASST3 
 * configurations without breaking
 */
void coremap_bootstrap(void){
	coremap_init();
}

static
paddr_t
getppages(unsigned long npages, bool user, struct pte* owner){
	
	spinlock_acquire(&stealmem_lock);
	paddr_t ramsize = ram_getsize();
	// PAGE_SIZE defined in arch/mips/include/vm.h
	int numberOfEntries = (int)ramsize/PAGE_SIZE;
	int startOfCurBlock = 0;
	unsigned long contiguousFound = 0;
	for(int i = 0; i < numberOfEntries; i++){
		if(contiguousFound == npages){
			for(int j = startOfCurBlock; (unsigned long)j < startOfCurBlock + npages; j++){
				get_corePage(j)->allocated = 1;
				get_corePage(j)->firstpage = startOfCurBlock;
				get_corePage(j)->npages = npages;
				get_corePage(j)->user = user;
				get_corePage(j)->owner_pte = owner;
			}
			used += PAGE_SIZE * npages;
			pagesAlloced += npages;
			paddr_t addr = index_to_pblock(startOfCurBlock);
			spinlock_release(&stealmem_lock);
			
			return addr; 
			
		}
		if(get_corePage(i)->allocated == 1){
			contiguousFound = 0;
			startOfCurBlock = i + 1;
			continue;
		}
		contiguousFound++;

	 }

	spinlock_release(&stealmem_lock);
	return (paddr_t)0;

}


/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(unsigned npages){

	paddr_t startOfNewBlock = getppages(npages, false, NULL);
	
	if (startOfNewBlock==0) {
			return 0;
		}

	return PADDR_TO_KVADDR(startOfNewBlock);

}

vaddr_t alloc_upages(unsigned npages, struct pte* owner){

	paddr_t startOfNewBlock = getppages(npages, true, owner);
	
	if (startOfNewBlock==0) {
			return 0;
		}

	bzero((void*)PADDR_TO_KVADDR(startOfNewBlock), npages * PAGE_SIZE);
	
	return PADDR_TO_KVADDR(startOfNewBlock);

}

vaddr_t alloc_kpages_nozero(unsigned npages){
	paddr_t startOfNewBlock = getppages(npages, false, NULL);
	
	if (startOfNewBlock==0) {
			return 0;
		}

	return PADDR_TO_KVADDR(startOfNewBlock);
}

void free_kpages(vaddr_t addr){
	spinlock_acquire(&stealmem_lock);
	vaddr_t base = PADDR_TO_KVADDR(index_to_pblock(0));
	int page = (addr - base) / PAGE_SIZE;
	int basePage =  get_corePage(page)->firstpage;
	int npages = get_corePage(basePage)->npages;

	for(int i = basePage; i < basePage + npages; ++i){
		get_corePage(i)->allocated = 0;
		get_corePage(i)->firstpage = -1;
		get_corePage(i)->npages = 0;
		get_corePage(i)->user = false;
		get_corePage(i)->owner_pte = NULL;
		
	}
	
	used -= (npages * PAGE_SIZE);
	spinlock_release(&stealmem_lock);
	return;
}

/*
 * Return amount of memory (in bytes) used by allocated coremap pages.  If
 * there are ongoing allocations, this value could change after it is returned
 * to the caller. But it should have been correct at some point in time.
 */
unsigned int coremap_used_bytes(void){
	return used;
}

/* TLB shootdown handling called from interprocessor_interrupt */
void vm_tlbshootdown(const struct tlbshootdown * tlbs){
	(void)tlbs;
	return;
}

/*
 * Copies a page from disk to memory. (source, destination)
 */
void blockread(int swapIndex, paddr_t paddr){
	return;
}

/*
 * Copies a page from memory to disk. (source, destination)
 */
void blockwrite(paddr_t paddr, int swapIndex){
	return;
}

/* Fault handling function called by trap code */
int vm_fault(int faulttype, vaddr_t faultaddress){

	int spl;
	faultaddress &= PAGE_FRAME;
	//Check if address is in valid region
	LinkedList* curreg = curthread->t_proc->p_addrspace->regions->next;
	
	if(curreg == NULL){
		return EFAULT;
	}
	while(1){
		vaddr_t regstart = ((struct region*)(curreg->data))->start;
		vaddr_t regend = ((struct region*)(curreg->data))->end;
		if(faultaddress >= regstart && faultaddress < regend){
			//must check perms of that region to make sure that we are jelly load mode is used becasue the code segement should not be writeable after we load the code segement in the first time and we need a way to know when we are doing that.
			// if(curthread->t_proc->p_addrspace->loadMode == 0 &&((faulttype == 0 && ((struct region*)(curreg->data))->read == 0) || (faulttype == 1 && ((struct region*)(curreg->data))->write == 0))){
			// 	kprintf("perms failed %d %d %p\n",((struct region*)(curreg->data))->write, faulttype, (void*) regstart);
			// 	return EFAULT;
			// }
			//Found valid region. Must search page table for vpn.
			LinkedList* curpte = curthread->t_proc->p_addrspace->pageTable;
			while(1){
				if((faultaddress) == ((struct pte*)curpte->data)->vpn){
					//Page is in table. Now check if it's in memory.
					if(((struct pte*)curpte->data)->inmem){
						//Verified page in memory. Now we need to load the TLB
						//I made large and irresponsible assumptions at this line. Debug here when broke.
						spl = splhigh();
						uint32_t ehi, elo;
						ehi = faultaddress;
						elo = (uint32_t)(((struct pte*)curpte->data)->ppn);
						if(tlb_probe(ehi, elo) >= 0){
							tlb_write(ehi, elo | TLBLO_DIRTY | TLBLO_VALID, tlb_probe(ehi, elo | TLBLO_DIRTY | TLBLO_VALID));
							splx(spl);
							return 0;
						}
						tlb_random(ehi, elo | TLBLO_DIRTY | TLBLO_VALID);
						splx(spl);
						return 0;
					}
					//else page fault:  need swapping but that's for 3.3
					
				}
				if(LLnext(curpte) == NULL){
					break;
				}
				curpte = LLnext(curpte);
			}
			//Failed to find page in table. Must allocate new one.

			struct pte* newPage = kmalloc(sizeof(*newPage));
			newPage->vpn = faultaddress;
			
			vaddr_t allocAddr = alloc_upages(1, newPage);
			newPage->ppn = (allocAddr - 0x80000000);
			newPage->inmem = true;

			LLaddWithDatum((char*)"weast", newPage, curpte);
			//Frob TLB
			spl = splhigh();
			uint32_t ehi, elo;
			ehi = faultaddress;
			elo = newPage->ppn;

			if(tlb_probe(ehi, elo) >= 0){
				tlb_write(ehi, elo | TLBLO_DIRTY | TLBLO_VALID, tlb_probe(ehi, elo | TLBLO_DIRTY | TLBLO_VALID));
				splx(spl);
				return 0;
			}
			
			tlb_random((uint32_t)newPage->vpn, (uint32_t)newPage->ppn | TLBLO_DIRTY | TLBLO_VALID);
			splx(spl);
			return 0;

		}

		if(LLnext(curreg) == NULL){
			break;
		}

		curreg = LLnext(curreg);
	}
	//Seg fault
	(void)faulttype;
	(void)faultaddress;
	return EFAULT;
}

