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

static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

static unsigned long pagesAlloced = 0;
static unsigned int used =  0;

void vm_bootstrap(){
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
getppages(unsigned long npages, bool user){
	
	spinlock_acquire(&stealmem_lock);
	/*My guess as to what this should do: search through the array (coremap) until we
	find n contiguous free pages. Then return the starting paddr of that chunk.*/
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
			}
			used += PAGE_SIZE * npages;
			pagesAlloced += npages;
			paddr_t addr = ppn_to_pblock(startOfCurBlock);
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

	/*Should call a working getppages routine that checks your coremap 
	for the status of free pages and returns appropriately*/

	paddr_t startOfNewBlock = getppages(npages,false);
	
	if (startOfNewBlock==0) {
			return 0;
		}

	// bzero((void*)PADDR_TO_KVADDR(startOfNewBlock), npages * PAGE_SIZE);
	
	return PADDR_TO_KVADDR(startOfNewBlock);

}

vaddr_t alloc_upages(unsigned npages){

	/*Should call a working getppages routine that checks your coremap 
	for the status of free pages and returns appropriately*/

	paddr_t startOfNewBlock = getppages(npages, true);
	
	if (startOfNewBlock==0) {
			return 0;
		}

	bzero((void*)PADDR_TO_KVADDR(startOfNewBlock), npages * PAGE_SIZE);
	
	return PADDR_TO_KVADDR(startOfNewBlock);

}

vaddr_t alloc_kpages_nozero(unsigned npages){
	paddr_t startOfNewBlock = getppages(npages,false);
	
	if (startOfNewBlock==0) {
			return 0;
		}

	return PADDR_TO_KVADDR(startOfNewBlock);
}

void free_kpages(vaddr_t addr){
	spinlock_acquire(&stealmem_lock);
	vaddr_t base = PADDR_TO_KVADDR(ppn_to_pblock(0));
	int page = (addr - base) / PAGE_SIZE;
	int basePage =  get_corePage(page)->firstpage;
	int npages = get_corePage(basePage)->npages;

	for(int i = basePage; i < basePage + npages; ++i){
		get_corePage(i)->allocated = 0;
		get_corePage(i)->firstpage = -1;
		get_corePage(i)->npages = 0;
		//bzero((void*)PADDR_TO_KVADDR(ppn_to_pblock(i)), PAGE_SIZE);
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
			//Change to upages
			vaddr_t allocAddr = alloc_upages(1);
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

