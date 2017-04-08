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
getppages(unsigned long npages){
	
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
				
			}
			used += 4096 * npages;
			pagesAlloced += npages;
			paddr_t addr = get_corePage(startOfCurBlock)->block - 0x80000000;
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

	paddr_t startOfNewBlock = getppages(npages);
	if (startOfNewBlock==0) {
			return 0;
		}
	return PADDR_TO_KVADDR(startOfNewBlock);

}

void free_kpages(vaddr_t addr){
	spinlock_acquire(&stealmem_lock);
	vaddr_t base = get_corePage(0)->block;
	int page = (addr - base)/4096;
	int basePage =  get_corePage(page)->firstpage;
	int npages = get_corePage(basePage)->npages;

	for(int i = basePage; i < basePage + npages; ++i){
		get_corePage(i)->allocated = 0;
		get_corePage(i)->firstpage = -1;
		get_corePage(i)->npages = 0;
	}
	used -= (npages * 4096);
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
	(void)faulttype;
	(void)faultaddress;
	return 0;
}

