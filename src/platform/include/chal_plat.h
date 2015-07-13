#ifndef CHAL_PLAT_H
#define CHAL_PLAT_H

#ifdef COS_LINUX
#include <linux/sched.h>
#include <linux/kernel.h>
#endif

//void *chal_pa2va(void *pa);

#ifdef COS_LINUX
struct per_core_cos_thd
{
	struct task_struct *cos_thd;
} CACHE_ALIGNED;

extern struct per_core_cos_thd cos_thd_per_core[NUM_CPU];

static inline void 
__chal_pgtbl_switch(paddr_t pt)
{
	struct mm_struct *mm;
	struct task_struct *cos_thd;

	cos_thd = cos_thd_per_core[get_cpuid()].cos_thd;

	BUG_ON(!cos_thd);
	/* 
	 * We aren't doing reference counting here on the mm (via
	 * get_task_mm) because we know that this mm will survive
	 * until the module is unloaded (i.e. it is refcnted at a
	 * granularity of the creation of the composite file
	 * descriptor open/close.)
	 */
	mm = cos_thd->mm;
	mm->pgd = (pgd_t *)chal_pa2va(pt);

	return;
}
#endif

/*
 * If for some reason Linux preempts the composite thread, then when
 * it starts it back up again, it needs to know what page tables to
 * use.  Thus update the current mm_struct.
 */
static inline void 
chal_pgtbl_switch(paddr_t pt)
{
#ifdef COS_LINUX
	native_write_cr3(pt);
//#define HOST_PGTBL_UPDATE
#ifdef HOST_PGTBL_UPDATE
	__chal_pgtbl_switch(pt);
#endif
#endif
}

/* This flushes all levels of cache of the current logical CPU. */
static inline void
chal_flush_cache(void)
{
	asm volatile("wbinvd": : :"memory");
}

#ifdef COS_LINUX
static inline unsigned int 
hpage_index(unsigned long n)
{
        unsigned int idx = n >> HPAGE_SHIFT;
        return (idx << HPAGE_SHIFT) != n ? idx + 1 : idx;
}

static inline void
chal_remote_tlb_flush(int target_cpu)
{
	smp_call_function_single(target_cpu, tlb_mandatory_flush, NULL, 1);
}

static inline void
chal_flush_tlb_global(void)
{
	unsigned long orig_cr4;

	orig_cr4 = native_read_cr4();
	/* Unset PGE (Page Global Enabled) bit, then restore cr4. This
	 * will flush TLB. */
	native_write_cr4(orig_cr4 & ~X86_CR4_PGE);
	native_write_cr4(orig_cr4);
}

/* This won't flush global TLB (pinned with PGE) entries. */
static inline void
chal_flush_tlb(void)
{
	native_write_cr3(native_read_cr3());
}
#else 
static inline void
chal_flush_tlb_global(void) {}
static inline void
chal_remote_tlb_flush(int target_cpu) {}
/* This won't flush global TLB (pinned with PGE) entries. */
static inline void
chal_flush_tlb(void) {}
#endif

#endif	/* CHAL_PLAT_H */
