/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>
#include <current.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	/*
	 * Initialize as needed.
	 */
	as->regions = LLcreate();
	struct region* startReg = kmalloc(sizeof(struct region));
	startReg->start = -1;
	startReg->end = -1;
	as->regions->data = startReg;
	as->stackbound = 0;
	
	as->pageTable = LLcreate();
	struct pte* firstPage = kmalloc(sizeof(struct pte));
	firstPage->vpn = -1;
	firstPage->ppn = -1;
	as->pageTable->data = firstPage;

	/*
	 * Init stack here
	 */
	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	//int spl = splhigh();
	struct addrspace *newas;

	newas = as_create();
	if (newas==NULL) {
		return ENOMEM;
	}
	newas->stackbound = old->stackbound;
	LinkedList* copyout = old->regions->next;


	// hey deal with the read write execute;
	//Copy region definitions
	while(copyout){
		vaddr_t oldstart = (((struct region*)copyout->data)->start);
		vaddr_t oldend = (((struct region*)copyout->data)->end);
		//kprintf("start %p, end %p\n", (void*)oldstart, (void*)oldend);
		as_define_region(newas, oldstart, (size_t)(oldend - oldstart), 1,1,1);
		copyout = LLnext(copyout);

	}

	//Create new page table
	LinkedList* newPageTable = LLcreate();
	struct pte* firstPage = kmalloc(sizeof(struct pte));
	firstPage->vpn = -1;
	firstPage->ppn = -1;
	newPageTable->data = firstPage;

	//For entry in old page table, put entry in new table with matching vpn
	LinkedList* oldPT = old->pageTable->next;
	LinkedList* newPT = newPageTable;
	while(1){
		struct pte* oldpte = (struct pte*)oldPT->data; 
		struct pte* newpte = kmalloc(sizeof(struct pte));
		newpte->vpn = oldpte->vpn;
		//Alloc a new page for that entry, give the pte the ppn corresponding to that page
		vaddr_t allocAddr = alloc_kpages(1);
		newpte->ppn = (allocAddr - 0x80000000);
		newpte->inmem = 1;
		LLaddWithDatum((char*)"spongebobu", newpte, newPT);
		//Copy mem from old page to new page
		memcpy((void*)PADDR_TO_KVADDR(newpte->ppn), (const void*)PADDR_TO_KVADDR((oldpte->ppn)), PAGE_SIZE);
		if(LLnext(oldPT) == NULL){
			break;
		}
		newPT = LLnext(newPT);
		oldPT = LLnext(oldPT);
	}
	/* TESTING CODE */
	// oldPT = old->pageTable;
	// newPT = newPageTable;
	// 	kprintf("oldPT:\n");
	// while(oldPT){
	// 	kprintf("vpn: %p, ppn: %p page name: %s\n", (void*)((struct pte*)(oldPT->data))->vpn, (void*)((struct pte*)(oldPT->data))->ppn, oldPT->name);
	// 	oldPT = LLnext(oldPT);	
	// }
	// 	kprintf("newPT:\n");
	// while(newPT){
	// 	kprintf("vpn: %p, ppn: %p page name: %s\n", (void*)((struct pte*)(newPT->data))->vpn, (void*)((struct pte*)(newPT->data))->ppn, newPT->name);
	// 	newPT = LLnext(newPT);
	// }
	newas->pageTable = newPageTable;
	*ret = newas;
	(void) ret;
	//splx(spl);
	return 0;
}

void
as_destroy(struct addrspace *as)
{
	/*
	 * Clean up as needed.
	 * MUST KFREE THE DATA IN SIDE OF LINKED LIST
	 */

	/* Free region list */
	LinkedList* toFree = as->regions;
	LinkedList* next = NULL;
	while(toFree){
		next = toFree->next;
		kfree((struct region*)toFree->data);
		LLdestroy(toFree);
		toFree = next;
	}

	/* Free page table */

	toFree = as->pageTable;

	while(toFree){
		next = toFree->next;
		kfree((struct pte*)toFree->data);
		LLdestroy(toFree);
		toFree = next;
	}
	
	kfree(as);
}

void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		return;
	}

	 //Disable interrupts on this CPU while frobbing the TLB. 
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
		 int readable, int writeable, int executable)
{
	/* Align the region. First, the base... */
	memsize += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	memsize = (memsize + PAGE_SIZE - 1) & PAGE_FRAME;

	LinkedList* llcur = as->regions;
	struct region* reg = kmalloc(sizeof(struct region));
	reg->start = vaddr;
	reg->end = vaddr + memsize;
	while(LLnext(llcur)){
		llcur = llcur->next;
	}

	LLaddWithDatum((char*)"wumbo", reg, llcur);

	//Should we bzero?
	(void)as;
	(void)vaddr;
	(void)memsize;
	(void)readable;
	(void)writeable;
	(void)executable;
	//kprintf("Defined region: start: %p, end: %p", (void*)(((struct region*)(llcur->next->data))->start), (void*)(((struct region*)(llcur->next->data))->end));
		
	return 0;
}

int
as_prepare_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	/*
	 * Write this.
	 */

	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	/*
	 * Write this.
	 */
	
	//Should we bzero?

	/* Initial user-level stack pointer */

	*stackptr = USERSTACK;
	as->stackbound = *stackptr - 0x100000;

	as_define_region(as, as->stackbound, 0x100000, 1, 1, 0);
	(void)as;
	return 0;
}

