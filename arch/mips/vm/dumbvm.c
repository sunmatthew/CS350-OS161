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
#include <spl.h>
#include <spinlock.h>
#include <proc.h>
#include <current.h>
#include <mips/tlb.h>
#include <addrspace.h>
#include <vm.h>

#include "opt-A3.h"

/*
 * Dumb MIPS-only "VM system" that is intended to only be just barely
 * enough to struggle off the ground.
 */

/* under dumbvm, always have 48k of user stack */
#define DUMBVM_STACKPAGES    12

/*
 * Wrap rma_stealmem in a spinlock.
 */
static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

#if OPT_A3
paddr_t coreMapLo = 0;
paddr_t coreMapHi = 0;
bool vmBootStrapCalled = false;
unsigned int coreMapSize = 0;

static struct spinlock coremap_lock = SPINLOCK_INITIALIZER;
#endif
// Call ram_getsize to get the remaining physical memory in the system (THIS SHOULD NOT BE CALLED AGAIN)
// Logically partition the memory into fixed sized frames -- each frame is PAGE_SIZE bytes
// Keep track of the status of each frame (core-map data structure)
		// This data structure should be stored in the start of the memory returned by ram_getsize
		// The frames should start after the core-map data structure
		// Should track which frames are used and available and keep track of contiguous memory allocations
		// Consider adding info in the core-map to help determine number of pages that need to be freed
				// If 4 contiguous frames were allocated using alloc_kpages then store 4 in the core-map entry for the start of the 4 frames
void
vm_bootstrap(void)
{
	#if OPT_A3

	// Tells your how much free physical memory there is -- needed for initializing structure for tracking which parts of physical memory are free
	ram_getsize(&coreMapLo, &coreMapHi);
	coreMapSize = (coreMapHi - coreMapLo) / PAGE_SIZE;

	// Keep track of the status of each frame (core-map data structure)
			// This data structure should be stored in the start of the memory returned by ram_getsize
			// The frames should start after the core-map data structure
			// Should track which frames are used and available and keep track of contiguous memory allocations
			// Consider adding info in the core-map to help determine number of pages that need to be freed
					// If 4 contiguous frames were allocated using alloc_kpages then store 4 in the core-map entry for the start of the 4 frames
	for (int n = 0 ; n < coreMapSize ; n ++) {
		// 
	}

	vmBootStrapCalled = true;
	#else
	/* Do nothing. */
	#endif
}


// Currently gets more page frames than needed
// Two main limitations:
		// 1) assumes each segment will be allocated contiguously in physical memory
		// 2) never re-uses physical memory -- when a process exits the physical memory used
				// to hold that process's address space doesn't become available for use by other processes
				// as a result, the kernel will quickly run out of physical memory
// Remove the above limitations, in particular:
		// should be possible for pages in process'address spaces to be placed into any free frame of
				// physical memory. the kernel should no longer require that address space segments be stored
				// contiguously in physical memory
		// when a process terminates, the physical frames that were used to hold its pages should be freed and
				// become available to use by other processes
// To implement the changes, the kernel will need some way of tracking where each page in each process' virtual
		// address space is located and some way of keeping track of which frames of physical memory are free and which are in use
// Kernel uses physical memory to hold its own data structures and to hold the address spaces of user processes
		// kmalloc and kfree is used to dynamically allocate space for new kernel data structures
		// They call alloc_kpages and free_kpages to allocate or free physical memory
		// When you change the way kernel's physical memory management works, must also make that kmalloc and kfree also work
				// No need to modify implementations of kmalloc or kfree
		// Make sure that any physical pages freed by kfree becomes free and available for re-use by the kernel
				// TODO: Requires the implementation of free_kpages
// TODO: implement alloc_kpages
// When kernel is bottings (before VM system is initialized) kernel will allocate memory for data structures
		// When VM system is initialized make sure that it's aware that some parts of physical memory are already being used
		// by the kernel, so it doesn't allocate that memory for other purposes

static
paddr_t
getppages(unsigned long npages)
{
	#if OPT_A3
	if (!vmBootStrapCalled) {
		paddr_t addr;

		spinlock_acquire(&stealmem_lock);

		addr = ram_stealmem(npages);

		spinlock_release(&stealmem_lock);
		return addr;
	} else {
		// If vm_bootstrap has been called, use the core-map instead of ram_stealmem
		// This implementation needs to use the core map structure to find free pages
		paddr_t addr;
		for (int n = 0 ; n < coreMapSize ; n ++) {

		}

		// ------------ ram_stealmem code: ------------
		// size_t size;
		// paddr_t paddr;
		//
		// size = npages * PAGE_SIZE;
		//
		// if (firstpaddr + size > lastpaddr) {
		// 	return 0;
		// }
		//
		// paddr = firstpaddr;
		// firstpaddr += size;
		//
		// return paddr;

		return addr;
	}



	#else
	paddr_t addr;

	spinlock_acquire(&stealmem_lock);

	addr = ram_stealmem(npages);

	spinlock_release(&stealmem_lock);
	return addr;
	#endif
}


// Called by kmalloc to allocate physical memory
// Allocate frames for both kmalloc and address spaces
// Frames need to be contiguous
/* Allocate/free some kernel-space virtual pages */
vaddr_t
alloc_kpages(int npages)
{
	paddr_t pa;
	pa = getppages(npages);
	if (pa==0) {
		return 0;
	}
	return PADDR_TO_KVADDR(pa);
}


// Free pages allocated with alloc_kpages
// We don't specify how many pages to free, should free the same number of pages that was allocated
// Update core-map to make frames available after free_kpages
void
free_kpages(vaddr_t addr)
{
	#if OPT_A3

	#else
	/* nothing - leak the memory. */

	(void)addr;
	#endif
}

void
vm_tlbshootdown_all(void)
{
	panic("dumbvm tried to do tlb shootdown?!\n");
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("dumbvm tried to do tlb shootdown?!\n");
}


// VM releated exceptions are handled by vm_fault
// Performs address translation and loads the virtual address to physical address into the TLB
		// Iterates through the TLB to find an unused/invalid entry
		// Overwrites the unused entry with the virtual to physical address mapping required
				// by the instruction that generated the TLB exception
// If the TLB is full, call tlb_random to write the entry into a random TLB slot
// Make sure that virtual page fields in the TLB are unique


// --------- READ-ONLY TEXT SEGMENT-------------
// Currently TLB entries are loaded with TLBLO_DIRTY on (pages are read and writeable)
// Text segment should be read-only
		// Load TLB entries for the text segment with TLBLO_DIRTY off
		// elo &= ~TLBLO_DIRTY
// Determine the segment of the fault address by looking at the vbase and vtop addresses
// Unfortunately, this will cause load_elf to throw a VM_FAULT_READONLY exception (trying to write to a readonly memory location)
		// Writing to a read-only memory address causes VM_FAULT_READONLY exception (currently just causes kernel panic)
		// FIX: VM system should kill the process: modify kill_curthread to kill the current process
				// Very similar to sys__exit, but the exit code/status will be different
				// Consider which signal number this will trigger (look at the beginning of kill_curthread)
// Must load TLB entries with TLBLO_DIRTY on until load_elf has completed
		// Consider adding a flag to struct addrspace to indicate whether or not load_elf has completed
		// When load_elf completes, flush the TLB and ensure that all future TLB entries for the text segment has TLBLO_DIRTY off


// ------------- MEMORY MANAGEMENT ---------------
// The kernel uses ram_stealmem when it needs to allocate more physical memory
// The function ram_getsize is intended to be called when initialized new and improved virtual memory system (tells you how much free memory is available)
		// Should only be called once when initializing the virtual memory system
		// Once called, kernel should never call ram_getsize again to allocate physical memory -- should be calling the implemented function to allocate


// -------------- HANDLING TLB FAULTS ---------------
// Make sure the TLB handler (vm_fault) doesn't cause another TLB fault -- should avoid anything that involves touching virtual addresses
		// in the application's part of the virtual address space (avoid copyin and copyout)
int
vm_fault(int faulttype, vaddr_t faultaddress)
{
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	paddr_t paddr;
	int i;
	uint32_t ehi, elo;
	struct addrspace *as;
	int spl;

	faultaddress &= PAGE_FRAME;

	DEBUG(DB_VM, "dumbvm: fault: 0x%x\n", faultaddress);

	switch (faulttype) {
	    case VM_FAULT_READONLY:
				/* We always create pages read-write, so we can't get this */
				return EFAULT;
	    case VM_FAULT_READ:
	    case VM_FAULT_WRITE:
		break;
	    default:
		return EINVAL;
	}

	if (curproc == NULL) {
		/*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
		return EFAULT;
	}

	as = curproc_getas();
	if (as == NULL) {
		/*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
		return EFAULT;
	}

	/* Assert that the address space has been set up properly. */
	KASSERT(as->as_vbase1 != 0);
	KASSERT(as->as_pbase1 != 0);
	KASSERT(as->as_npages1 != 0);
	KASSERT(as->as_vbase2 != 0);
	KASSERT(as->as_pbase2 != 0);
	KASSERT(as->as_npages2 != 0);
	KASSERT(as->as_stackpbase != 0);
	KASSERT((as->as_vbase1 & PAGE_FRAME) == as->as_vbase1);
	KASSERT((as->as_pbase1 & PAGE_FRAME) == as->as_pbase1);
	KASSERT((as->as_vbase2 & PAGE_FRAME) == as->as_vbase2);
	KASSERT((as->as_pbase2 & PAGE_FRAME) == as->as_pbase2);
	KASSERT((as->as_stackpbase & PAGE_FRAME) == as->as_stackpbase);

	vbase1 = as->as_vbase1;
	vtop1 = vbase1 + as->as_npages1 * PAGE_SIZE;
	vbase2 = as->as_vbase2;
	vtop2 = vbase2 + as->as_npages2 * PAGE_SIZE;
	stackbase = USERSTACK - DUMBVM_STACKPAGES * PAGE_SIZE;
	stacktop = USERSTACK;


	#if OPT_A3
	bool loadElfStatus = as->load_elf_done;
	bool isTextSegment = false;
	if (faultaddress >= vbase1 && faultaddress < vtop1) {
		paddr = (faultaddress - vbase1) + as->as_pbase1;
		isTextSegment = true;
	}
	else if (faultaddress >= vbase2 && faultaddress < vtop2) {
		paddr = (faultaddress - vbase2) + as->as_pbase2;
	}
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
		paddr = (faultaddress - stackbase) + as->as_stackpbase;
	}
	else {
		return EFAULT;
	}

	#else
	if (faultaddress >= vbase1 && faultaddress < vtop1) {
		paddr = (faultaddress - vbase1) + as->as_pbase1;
	}
	else if (faultaddress >= vbase2 && faultaddress < vtop2) {
		paddr = (faultaddress - vbase2) + as->as_pbase2;
	}
	else if (faultaddress >= stackbase && faultaddress < stacktop) {
		paddr = (faultaddress - stackbase) + as->as_stackpbase;
	}
	else {
		return EFAULT;
	}
	#endif

	/* make sure it's page-aligned */
	KASSERT((paddr & PAGE_FRAME) == paddr);

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_read(&ehi, &elo, i);
		if (elo & TLBLO_VALID) {
			continue;
		}
		ehi = faultaddress;
		elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
		if (isTextSegment && loadElfStatus) {
			elo &= ~TLBLO_DIRTY;
		}
		DEBUG(DB_VM, "dumbvm: 0x%x -> 0x%x\n", faultaddress, paddr);
		tlb_write(ehi, elo, i);
		splx(spl);
		return 0;
	}


	// If the TLB is full, call tlb_random to write the entry into a random TLB slot
			// Make sure that virtual page fields in the TLB are unique
	ehi = faultaddress;
	elo = paddr | TLBLO_DIRTY | TLBLO_VALID;
	if (isTextSegment && loadElfStatus) {
		elo &= ~TLBLO_DIRTY;
	}
	tlb_random(ehi, elo);
	splx(spl);
	return 0;
}

struct addrspace *
as_create(void)
{
	struct addrspace *as = kmalloc(sizeof(struct addrspace));
	if (as==NULL) {
		return NULL;
	}


	#if OPT_A3
	as->as_vbase1 = 0;
	// as->as_pbase1 = 0;
	as->as_npages1 = 0;
	as->as_vbase2 = 0;
	// as->as_pbase2 = 0;
	as->as_npages2 = 0;
	// as->as_stackpbase = 0;
	as->load_elf_done = false;
	#else
	as->as_vbase1 = 0;
	as->as_pbase1 = 0;
	as->as_npages1 = 0;
	as->as_vbase2 = 0;
	as->as_pbase2 = 0;
	as->as_npages2 = 0;
	as->as_stackpbase = 0;
	as->load_elf_done = false;
	#endif

	return as;
}

void
as_destroy(struct addrspace *as)
{
	#if OPT_A3


	kfree(as);
	#else
	kfree(as);
	#endif
}

void
as_activate(void)
{
	int i, spl;
	struct addrspace *as;

	as = curproc_getas();
#ifdef UW
        /* Kernel threads don't have an address spaces to activate */
#endif
	if (as == NULL) {
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i=0; i<NUM_TLB; i++) {
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void
as_deactivate(void)
{
	/* nothing */
}

int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t sz,
		 int readable, int writeable, int executable)
{
	size_t npages;

	/* Align the region. First, the base... */
	sz += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	sz = (sz + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = sz / PAGE_SIZE;

	/* We don't use these - all pages are read-write */
	(void)readable;
	(void)writeable;
	(void)executable;

	if (as->as_vbase1 == 0) {
		as->as_vbase1 = vaddr;
		as->as_npages1 = npages;
		return 0;
	}

	if (as->as_vbase2 == 0) {
		as->as_vbase2 = vaddr;
		as->as_npages2 = npages;
		return 0;
	}

	/*
	 * Support for more than two regions is not available.
	 */
	kprintf("dumbvm: Warning: too many regions\n");
	return EUNIMP;
}

static
void
as_zero_region(paddr_t paddr, unsigned npages)
{
	bzero((void *)PADDR_TO_KVADDR(paddr), npages * PAGE_SIZE);
}

int
as_prepare_load(struct addrspace *as)
{
	KASSERT(as->as_pbase1 == 0);
	KASSERT(as->as_pbase2 == 0);
	KASSERT(as->as_stackpbase == 0);

	as->as_pbase1 = getppages(as->as_npages1);
	if (as->as_pbase1 == 0) {
		return ENOMEM;
	}

	as->as_pbase2 = getppages(as->as_npages2);
	if (as->as_pbase2 == 0) {
		return ENOMEM;
	}

	as->as_stackpbase = getppages(DUMBVM_STACKPAGES);
	if (as->as_stackpbase == 0) {
		return ENOMEM;
	}

	as_zero_region(as->as_pbase1, as->as_npages1);
	as_zero_region(as->as_pbase2, as->as_npages2);
	as_zero_region(as->as_stackpbase, DUMBVM_STACKPAGES);

	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	(void)as;
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	KASSERT(as->as_stackpbase != 0);

	*stackptr = USERSTACK;
	return 0;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *new;

	new = as_create();
	if (new==NULL) {
		return ENOMEM;
	}

	new->as_vbase1 = old->as_vbase1;
	new->as_npages1 = old->as_npages1;
	new->as_vbase2 = old->as_vbase2;
	new->as_npages2 = old->as_npages2;

	/* (Mis)use as_prepare_load to allocate some physical memory. */
	if (as_prepare_load(new)) {
		as_destroy(new);
		return ENOMEM;
	}

	KASSERT(new->as_pbase1 != 0);
	KASSERT(new->as_pbase2 != 0);
	KASSERT(new->as_stackpbase != 0);

	memmove((void *)PADDR_TO_KVADDR(new->as_pbase1),
		(const void *)PADDR_TO_KVADDR(old->as_pbase1),
		old->as_npages1*PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_pbase2),
		(const void *)PADDR_TO_KVADDR(old->as_pbase2),
		old->as_npages2*PAGE_SIZE);

	memmove((void *)PADDR_TO_KVADDR(new->as_stackpbase),
		(const void *)PADDR_TO_KVADDR(old->as_stackpbase),
		DUMBVM_STACKPAGES*PAGE_SIZE);

	*ret = new;
	return 0;
}
