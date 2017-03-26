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

// static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

// static unsigned long pagesAlloced = 0;

void vm_bootstrap(){
	return;
}

static
paddr_t
getppages(unsigned long npages){
	
		//THE OLD (DUMBVM) WAY:
	// 	paddr_t addr;

	// 	spinlock_acquire(&stealmem_lock);

	// 	addr = ram_stealmem(npages);

	// 	spinlock_release(&stealmem_lock);

	// (void)npages;

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
				*(int *)(get_corePage(j)) = 1;
				*(int *)(get_corePage(j) + 4) = startOfCurBlock;
				*(int*)(get_corePage(j) + 8) = npages;
				
			}
			 paddr_t temp = (*(paddr_t *)(get_corePage(startOfCurBlock)+12));
			return temp; 
			
		}
		if((*(int*)(get_corePage(i))) == 1){
			contiguousFound = 0;
			// i += get_corePage(i)->npages;
			startOfCurBlock = i + 1;
			continue;
		}
		contiguousFound++;


	 }
	// (*(paddr_t *)(get_corePage(pagesAlloced)+12)) = addr;

	//Move firstpaddr accordingly. 

	/*If they don't have to be contiguous pages, we could just update firstpaddr to be 
	wherever our page allocation left off when it finished.*/

	/*If they do have to be contiguous, or if we design them that way for now, 
	we could interate the coremap again searching for the first free page. No biggie.*/

	return (paddr_t)0;

}


/* Allocate/free kernel heap pages (called by kmalloc/kfree) */
vaddr_t alloc_kpages(unsigned npages){
	
		//THE OLD (DUMBVM) WAY
		// paddr_t pa;

		// //dumbvm_can_sleep();
		// pa = getppages(npages);
		// if (pa==0) {
		// 	return 0;
		// }
		// return PADDR_TO_KVADDR(pa);
	//Allocates n coniguous physical pages 

	/*Should call a working getppages routine that checks your coremap 
	for the status of free pages and returns appropriately*/

	paddr_t startOfNewBlock = getppages(npages);

	return PADDR_TO_KVADDR(startOfNewBlock);

}

void free_kpages(vaddr_t addr){
	(void)addr;

	return;
}

/*
 * Return amount of memory (in bytes) used by allocated coremap pages.  If
 * there are ongoing allocations, this value could change after it is returned
 * to the caller. But it should have been correct at some point in time.
 */
unsigned int coremap_used_bytes(void){
	//Should we traverse the linked list each time we want this value? Could be too slow.
	//Maybe we should keep a running total somewhere.
	return 0;
}

/* TLB shootdown handling called from interprocessor_interrupt */
void vm_tlbshootdown(const struct tlbshootdown * tlbs){
	(void)tlbs;
	return;
}

/* Fault handling function called by trap code */
int vm_fault(int faulttype, vaddr_t faultaddress){
	(void)faulttype;
	(void)faultaddress;
	return 0;
}

