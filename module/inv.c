/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include "include/ipc.h"
#include "include/spd.h"
#include "include/debug.h"
#include "include/measurement.h"

#include <linux/kernel.h>

#define COS_SYSCALL __attribute__((regparm(0)))

void ipc_init(void)
{
	return;
}

static inline void open_spd(struct spd_poly *spd)
{
	printk("cos: open_spd (asymmetric trust) not supported on x86.\n");
	
	return;
}

static inline void open_close_spd(struct spd_poly *o_spd,
				  struct spd_poly *c_spd)
{
	if (o_spd->pg_tbl != c_spd->pg_tbl) {
		native_write_cr3(o_spd->pg_tbl);
	}

	return;
}

static inline void open_close_spd_ret(struct spd_poly *c_spd) /*, struct spd_poly *s_spd)*/
{
	native_write_cr3(c_spd->pg_tbl);
//	printk("cos: return - opening pgtbl %x for spd %p.\n", (unsigned int)o_spd->pg_tbl, o_spd);
	
	return;
}

static inline int stale_il_or_error(struct thd_invocation_frame *frame,
				    struct invocation_cap *cap)
{
	return 1;
}

static void print_stack(struct thread *thd, struct spd *srcspd, struct spd *destspd)
{
	int i;

	printk("cos: In thd %x, src spd %x, dest spd %x, stack:\n", (unsigned int)thd, 
	       (unsigned int)srcspd, (unsigned int)destspd);
	for (i = 0 ; i <= thd->stack_ptr ; i++) {
		struct thd_invocation_frame *frame = &thd->stack_base[i];
		printk("cos: \t[cspd %x]\n", (unsigned int)frame->current_composite_spd);
	}
}

void print_regs(struct pt_regs *regs)
{
	printk("cos: EAX:%x\tEBX:%x\tECX:%x\n"
	       "cos: EDX:%x\tESI:%x\tEDI:%x\n"
	       "cos: EIP:%x\tESP:%x\tEBP:%x\n",
	       (unsigned int)regs->eax, (unsigned int)regs->ebx, (unsigned int)regs->ecx,
	       (unsigned int)regs->edx, (unsigned int)regs->esi, (unsigned int)regs->edi,
	       (unsigned int)regs->eip, (unsigned int)regs->esp, (unsigned int)regs->ebp);

	return;
}

struct inv_ret_struct {
	int thd_id;
	void *data_region;
};

extern struct invocation_cap invocation_capabilities[MAX_STATIC_CAP];
/* 
 * FIXME: 1) should probably return the static capability to allow
 * isolation level isolation access from caller, 2) all return 0
 * should kill thread.
 */
COS_SYSCALL vaddr_t ipc_walk_static_cap(struct thread *thd, unsigned int capability, 
					vaddr_t sp, vaddr_t ip, /*vaddr_t usr_def, */
					struct inv_ret_struct *ret)
{
	struct thd_invocation_frame *curr_frame;
	struct spd *curr_spd, *dest_spd;
	struct invocation_cap *cap_entry;
//	int save_regs = 0;

/* FIXME: unify with asm_ipc_defs.h */
/*
#define SAVE_REGS_CAP_OFFSET 0x80000000
	if (SAVE_REGS_CAP_OFFSET & capability) {
		save_regs = 1;
		capability &= ~SAVE_REGS_CAP_OFFSET;
	}
*/
	capability >>= 20;

	//printd("cos: ipc_walk_static_cap - thd %p, cap %x(%u), sp %x, ip %x, usrdef %x.\n",
	//       thd, (unsigned int)capability, (unsigned int)capability, 
	//       (unsigned int)sp, (unsigned int)ip, (unsigned int)usr_def);

	if (capability >= MAX_STATIC_CAP) {
		printk("cos: capability %d greater than max.\n", capability);
		return 0;
	}

	cap_entry = &invocation_capabilities[capability];

	if (!cap_entry->owner) {
		printk("cos: No owner for cap %d.\n", capability);
		return 0;
	}

	//printk("cos: \tinvoking fn %x.\n", (unsigned int)cap_entry->dest_entry_instruction);

	/* what spd are we in (what stack frame)? */
	curr_frame = &thd->stack_base[thd->stack_ptr];

	dest_spd = cap_entry->destination;
	curr_spd = cap_entry->owner;

	if (curr_spd == CAP_FREE || curr_spd == CAP_ALLOCATED_UNUSED) {
		printk("cos: Attempted use of unallocated capability.\n");
		return 0;
	}

	/*printk("cos: Invocation on cap %d from %x.\n", capability, 
	       (unsigned int)curr_frame->current_composite_spd);*/
	/*
	 * If the spd that owns this capability is part of a composite
	 * spd that is the same as the composite spd that was the
	 * entry point for this composite spd.
	 *
	 * i.e. is the capability owner in the same protection domain
	 * (via ST) as the spd entry point to the protection domain.
	 */
	if (curr_spd->composite_spd != curr_frame->current_composite_spd && /* should be || */
	    stale_il_or_error(curr_frame, cap_entry)) {
		printk("cos: Error, incorrect capability (Cap %d has cspd %x, stk has %x).\n",
		       capability, (unsigned int)cap_entry->owner->composite_spd,
		       (unsigned int)curr_frame->current_composite_spd);
		print_stack(thd, curr_spd, dest_spd);
		return 0;
		/* FIXME: do something here like kill thread/spd */
	}

	/* save the proper spd that we saved registers for */
/*	if (save_regs) {
		thd->sched_saved_regs = curr_spd;
	}
*/

	cap_entry->invocation_cnt++;

//	if (cap_entry->il & IL_INV_UNMAP) {
	open_close_spd(/*&*/dest_spd->composite_spd/*->spd_info*/, 
		       /*&*/curr_spd->composite_spd/*->spd_info*/);
//	} else {
//		open_spd(&curr_spd->spd_info);
//	}

	ret->thd_id = thd->thread_id;
	ret->data_region = thd->data_region;

	/* 
	 * ref count the composite spds:
	 * 
	 * FIXME, TODO: move composite pgd into each spd and ref count
	 * in spds.  Sum of all ref cnts is the composite ref count.
	 * This will eliminate the composite cache miss.
	 */
	
	/* add a new stack frame for the spd we are invoking (we're committed) */
	thd_invocation_push(thd, cap_entry->destination/* ->composite_spd */, sp, ip/*, usr_def*/);

	cos_meas_event(COS_MEAS_INVOCATIONS);

	return cap_entry->dest_entry_instruction;
}

/*
 * Return from an invocation by popping off of the invocation stack an
 * entry, and returning its contents (including return ip and sp).
 * This is complicated by the fact that we may return when no
 * invocation is made because a thread is terminating.
 */
COS_SYSCALL struct thd_invocation_frame *pop(struct thread *curr_thd, struct pt_regs **regs_restore)
{
	struct thd_invocation_frame *inv_frame = thd_invocation_pop(curr_thd);
	struct thd_invocation_frame *curr_frame;

	if (inv_frame == MNULL) {
		struct thread *curr = thd_get_current();

		if (curr->flags & THD_STATE_ACTIVE_UPCALL) {
			struct thread *prev = curr->interrupted_thread;
			/* FIXME: stupid cast, again */
			struct spd *dest_spd = (struct spd *)thd_get_thd_spd(prev);
			struct spd *orig_spd = (struct spd *)thd_get_thd_spd(curr);

			assert(curr->thread_brand && curr->flags & THD_STATE_UPCALL);

			if (curr->thread_brand->pending_upcall_requests > 0) {
				
			}

			curr->flags &= ~THD_STATE_ACTIVE_UPCALL;
			curr->flags |= THD_STATE_READY_UPCALL;

			/* TODO: check if there are pending upcalls
			 * and service them. */

			/* 
			 * FIXME: this should be more complicated.  If
			 * a scheduling decision has been made between
			 * when the upcall thread was scheduled, and
			 * now.  In such a case, the "previous
			 * preempted thread" could have already
			 * executed to completion, or some such.  In
			 * such a case (scheduling decision has been
			 * made to put the upcall thread to sleep),
			 * then the correct thing to do is to act like
			 * this thread has been killed (or yields, or
			 * something in between) for scheduling
			 * purposes (assuming that we don't have
			 * pending upcalls, which changes all of this.
			 */
			open_close_spd(&dest_spd->spd_info, &orig_spd->spd_info);

			assert(prev->flags & THD_STATE_PREEMPTED);
			prev->flags &= ~THD_STATE_PREEMPTED;

			thd_set_current(prev);
			*regs_restore = &prev->regs;
		} else {
			printk("Attempting to return from a component when there's no component to return to.\n");
			regs_restore = 0;
		}

		return MNULL;
	}
	
	//printk("cos: Popping spd %p off of thread %p.\n", 
	//       &inv_frame->current_composite_spd->spd_info, curr_thd);
	
	curr_frame = thd_invstk_top(curr_thd);
	/* for now just assume we always close the server spd */
	open_close_spd_ret(/*&*/curr_frame->current_composite_spd/*->spd_info*/);

	return inv_frame;	
}

/********** Composite system calls **********/

extern int switch_thread_data_page(int old_thd, int new_thd);
void switch_thread_context(struct thread *curr, struct thread *next)
{
	thd_set_current(next);

	switch_thread_data_page(curr->thread_id, next->thread_id);

	/* 
	 * TODO: with preemption, check if curr_spd for next is
	 * curr_spc for curr...if not, change page tables. 
	 */

	return;
}

COS_SYSCALL int cos_syscall_void(void)
{
	printd("cos: error - made void system call\n");

	return 0;
}

extern void cos_syscall_resume_return(void);
/*
 * Called from assembly so error conventions are ugly:
 * 0: error
 * 1: return from invocation
 * 2: return from preemption
 *
 * Does this have to be so expensive?  400 cycles above normal
 * invocation return.
 */
COS_SYSCALL int cos_syscall_resume_return_cont(void)
{
	struct thread *thd, *curr = thd_get_current();
	struct spd *curr_spd = (struct spd*)thd_get_current_spd();
	struct cos_sched_next_thd *next_thd = curr_spd->sched_shared_page;

	if (next_thd == NULL) {
		printk("cos: non-scheduler attempting to switch thread.\n");
		return 0;
	}

	thd = thd_get_by_id(next_thd->next_thd_id);
	if (thd == NULL || curr == NULL) {
		printk("cos: no thread associated with id %d.\n", next_thd->next_thd_id);
		return 0;
	}

	if (curr_spd == NULL || curr_spd->sched_depth < 0) {
		printk("cos: spd invoking resume thread not a scheduler.\n");
		return 0;
	}

	/* 
	 * FIXME: this is more complicated because of the following situation
	 *
	 * - T_1 is scheduled to run by S_1
	 * - T_1 is preempted while holding a lock in C_1 and S_2 chooses to 
	 *   run T_2 where S_2 is a scheduler of S_1
	 * - T_2 contends the same lock, which calls S_2
	 * - S_2 resumes T_1 (who counts as yielding T_1 here?), which releases 
	 *   the lock, calls S_2 which blocks T_1 setting sched_suspended to S_2
	 * - S_1 attempts to resume T_1 but cannot as it is not the scheduler that
	 *   suspended it
	 *
	 * *CRAP*
	 */
	if (thd->sched_suspended && curr_spd != thd->sched_suspended) {
		printk("cos: scheduler %d resuming thread %d, not one that suspended it %d.\n",
		       spd_get_index(curr_spd), next_thd->next_thd_id, spd_get_index(thd->sched_suspended));
		return 0;
	}


	/* 
	 * With MPD, we could have the situation where two spds
	 * constitute a composite spd.  spd B is invoked by an
	 * external spd, A, and it, in turn, invokes spd C, which is a
	 * scheduler.  If this thread is suspended and resume_returned
	 * by C, it should NOT return from the component invocation to
	 * A as this would completely bypass B.  Thus check in
	 * resume_return that the current scheduler spd is really the
	 * invoked spd.
	 *
	 * FIXME: this should not error out, but should return to the
	 * current spd with the register contents of the target thread
	 * restored.
	 */
	if (thd_invstk_top(curr)->spd != curr_spd) {
		printk("cos: attempting to resume_return when return would not be to invoking spd (due to MPD).\n");
		return 0;
	}

	curr->sched_suspended = curr_spd;
	thd->sched_suspended = NULL;
	
	switch_thread_context(curr, thd);

	if (thd->flags & THD_STATE_PREEMPTED) {
		printk("cos: resume preempted -- disabled path...why are we here?\n");
		thd->flags &= ~THD_STATE_PREEMPTED;
		return 2;
	} 
	
	return 1;
}

COS_SYSCALL int cos_syscall_get_thd_id(void)
{
	//printk("cos: request for thread is %d.\n", thd_get_current()->thread_id);
	return thd_get_current()->thread_id;
}

/* 
 * Hope the current thread saved its context...should be able to
 * resume_return to it.
 *
 * TODO: should NOT pass in fn and stack here.  Should simply upcall
 * into cos_upcall_entry, and pass in the data.  The data should point
 * to the fn and stack to be used anyway, which can be assigned at
 * user-level.
 */
COS_SYSCALL int cos_syscall_create_thread(vaddr_t fn, vaddr_t stack, 
					  void *data, int urgency)
{
	struct thread *thd, *curr;
	/* FIXME: as above, cast to spd stupid */
	struct spd *curr_spd = (struct spd*)thd_get_current_spd(), *sched;
	int i;

	/*
	 * Lets make sure that the current spd is a scheduler and has
	 * scheduler control of the current thread before we let it
	 * create any threads.
	 *
	 * FIXME: in the future, I should really just allow the base
	 * scheduler to create threads, i.e. when 0 == sched_depth.
	 */
	curr = thd_get_current();
	if (curr_spd->sched_depth < 0 || !thd_scheduled_by(curr, curr_spd)) {
		printk("cos: non-scheduler attempted to create thread.\n");
		return -1;
	}

	thd = thd_alloc(curr_spd);
	if (thd == NULL) {
		return -1;
	}

//	printk("cos: stack %x, fn %x, and data %p\n", stack, fn, data);
	thd->regs.ecx = stack;
	thd->regs.edx = fn;
	thd->regs.eax = (int)data;

	/* 
	 * Initialize the thread's path through its hierarchy of
	 * schedulers.  They will have to explicitly set the
	 * thread_notification location at a later time.
	 *
	 * OPTION: Another option here would be to simply copy the
	 * scheduler hierarchy of the current thread.  A good way to
	 * initialize the urgency values for all the schedulers.
	 */
	sched = curr_spd;
	for (i = curr_spd->sched_depth ; i >= 0 ; i--) {
		struct thd_sched_info *tsi = thd_get_sched_info(thd, i);

		tsi->scheduler = sched;
		tsi->urgency = urgency;
		tsi->thread_notifications = NULL;

		sched = sched->parent_sched;
	}

//	printk("cos: allocated thread %d.\n", thd->thread_id);
	/* TODO: set information in the thd such as schedulers[], etc... from the parent */

	//curr = thd_get_current();

	/* Suspend the current thread */
	//curr->sched_suspended = curr_spd;
	/* and activate the new one */
	//switch_thread_context(curr, thd);

	//printk("cos: switching from thread %d to %d.\n", curr->thread_id, thd->thread_id);

	//print_regs(&curr->regs);

	return thd->thread_id;
}

unsigned long long switch_coop, switch_preempt;
extern int cos_syscall_switch_thread(void);
COS_SYSCALL struct pt_regs *cos_syscall_switch_thread_cont(int *preempt)
{
	struct thread *thd, *curr = thd_get_current();
	/* 
	 * FIXME: cast disregards spd_poly, which will break mpd.
	 * Here we need to promote switch_thread syscalls to
	 * capability invocations so that we can identify the spd via
	 * capability.
	 */
	struct spd *curr_spd = (struct spd*)thd_get_thd_spd(curr);
	struct cos_sched_next_thd *next_thd = curr_spd->sched_shared_page;
	unsigned short int flags = next_thd->next_thd_flags;

	*preempt = 0;
	
	if (NULL == next_thd) {
		printk("cos: non-scheduler attempting to switch thread.\n");
		return &curr->regs;
	}

	thd = thd_get_by_id(next_thd->next_thd_id);
	if (NULL == thd) {
		printk("cos: no thread associated with id %d, cannot switch to it.\n", 
		       next_thd->next_thd_id);
		return &curr->regs;
	}

	if (!thd_scheduled_by(curr, curr_spd) ||
	    !thd_scheduled_by(thd, curr_spd)) {
		printk("cos: scheduler %d does not have scheduling control over %d or %d, cannot switch.\n",
		       spd_get_index(curr_spd), thd_get_id(curr), thd_get_id(thd));
		return &curr->regs;
	}

	/*
	 * If the thread was suspended by another scheduler, we really
	 * have no business resuming it.
	 *
	 * See corresponding comment in resume_return for the reason
	 * why this is not always true.
	 */
	if (thd->flags & THD_STATE_SCHED_EXCL) {
		if (thd->sched_suspended && curr_spd != thd->sched_suspended) {
			printk("cos: scheduler %d resuming thread %d, but spd %d suspended it.\n",
			       spd_get_index(curr_spd), thd_get_id(thd), spd_get_index(thd->sched_suspended));
			
			return &curr->regs;
		}
		thd->flags &= ~THD_STATE_SCHED_EXCL;
	}

	if (flags & COS_SCHED_EXCL_YIELD) {
		curr->flags |= THD_STATE_SCHED_EXCL;
	}

	curr->sched_suspended = curr_spd;
	thd->sched_suspended = NULL;

	switch_thread_context(curr, thd);

	/*printk("cos: switching from %d to %d in sched %d.\n",
	  curr->thread_id, thd->thread_id, spd_get_index(curr_spd));*/

	if (thd->flags & THD_STATE_PREEMPTED) {
		/* FIXME: again... */
		struct spd *dest_spd = (struct spd*)thd_get_thd_spd(thd);

		open_close_spd(&dest_spd->spd_info, &curr_spd->spd_info);
		cos_meas_event(COS_MEAS_SWITCH_PREEMPT);

		thd->flags &= ~THD_STATE_PREEMPTED;
		*preempt = 1;
	} else {
		cos_meas_event(COS_MEAS_SWITCH_COOP);
	}
	
	return &thd->regs;
}

extern void cos_syscall_kill_thd(int thd_id);
COS_SYSCALL void cos_syscall_kill_thd_cont(int thd_id)
{
	printk("cos: killing threads not yet supported.\n");

	return;

}


/**
 * Upcall interfaces:
 *
 * The current interfaces for upcalls rely on maintaining the state of
 * an execution path through components which is traversed in reverse
 * order by each upcall.  This mechanism will not work when components
 * are collapsed into larger protection domains as 1) the direct
 * function calls will not know where to execute (as the invocation
 * stack is kept in kernel) and 2) the kernel will not know in which
 * component an invocation is made from.
 *
 * Go back to the design proposed in the design document earlier.
 */

//extern void cos_syscall_brand_upcall(int thread_id);
COS_SYSCALL int cos_syscall_brand_upcall(int thread_id)
{
	return 0;
}

/*
 * TODO: Creating the thread in this function is a little
 * brain-damaged because now we are allocating threads without the
 * scheduler knowing it (shouldn't be allowed), and because there is a
 * limited number of threads, we could denial of service and no policy
 * could be installed in the system to stop us.  Solution: allow the
 * scheduler to create threads that aren't executed, but attached to
 * other threads (that make them), and these threads can later be used
 * to create brands and upcalls.  This allows the scheduler to control
 * the distrubution of threads, and essentially is a resource credit
 * to a principal where the resource here is a thread.
 *
 * FIXME: the way we record and do brand paths is incorrect currently.
 * It will work now, but not when we activate MPDs.  We need to make
 * sure that 1) all spd invocations are recorded when we are creating
 * a brand path, and 2) pointers are added only to the spds
 * themselves, not necessarily the spd's current protection domains as
 * we wish, when making upcalls to upcall into the most recent version
 * of the spd's protection domains.  This begs the question, when we
 * upcall_brand from a composite spd, how does the system know which
 * spd we are in, thus which to upcall into.  Solution: we must make
 * the upcall call be another capability which we can define a
 * user-level-cap for.  This is required anyway, as we need to have a
 * system-provided lookup for direct invocation.  When an upcall is
 * made, we walk the invocation stack till we find the current spd,
 * and upcall its return spd.  To improve usability, we should check
 * explicitely that when a brand is made, the chain of invocations
 * follows capabilities and doesn't skip spds due to mpds.
 */
struct thread *cos_brand_thread;
COS_SYSCALL int cos_syscall_brand(int thd_id, int flags)
{
	struct thread *new_thd, *brand_thd = NULL;
	/* FIXME: as above, cast to spd stupid */
	struct spd *curr_spd = (struct spd*)thd_get_current_spd();

	if (COS_BRAND_ADD_THD == flags) {
		brand_thd = thd_get_by_id(thd_id);

		if (brand_thd == NULL) {
			printk("cos: cos_syscall_brand could not find thd_id %d to add thd to.\n", 
			       (unsigned int)thd_id);
			return -1;
		}
	}

	new_thd = thd_alloc(curr_spd);
	if (NULL == new_thd) {
		return -1;
	}

	/* might be useful later for the flags to not be mutually
	 * exclusive */
	if (COS_BRAND_CREATE == flags) {
		struct thread *curr_thd = thd_get_current();

		/* the brand thread holds the invocation stack record: */
		memcpy(&new_thd->stack_base, &curr_thd->stack_base, sizeof(curr_thd->stack_base));
		new_thd->stack_ptr = curr_thd->stack_ptr;
		new_thd->cpu_id = curr_thd->cpu_id;
		new_thd->flags |= THD_STATE_BRAND;

		cos_brand_thread = new_thd;
	} else if (COS_BRAND_ADD_THD == flags) {
		new_thd->flags |= (THD_STATE_UPCALL | THD_STATE_READY_UPCALL);
		new_thd->thread_brand = brand_thd;
		new_thd->brand_inv_stack_ptr = brand_thd->stack_ptr;

		cos_brand_thread->upcall_threads = new_thd;
	}

	return new_thd->thread_id;
}

/*
 * verify that truster does in fact trust trustee
 */
static int verify_trust(struct spd *truster, struct spd *trustee)
{
	unsigned short int cap_no, max_cap, i;

	cap_no = truster->cap_base;
	max_cap = truster->cap_range + cap_no;

	for (i = cap_no ; i < max_cap ; i++) {
		if (invocation_capabilities[i].destination == trustee) {
			return 0;
		}
	}

	return -1;
}

/* 
 * I HATE this call...do away with it if possible.  But we need some
 * way to jump-start processes AND let schedulers keep track of their
 * threads.
 *
 * NOT performance sensitive: used to kick start spds and give them
 * active entities (threads).
 */
extern void cos_syscall_upcall(int thd_id);
COS_SYSCALL int cos_syscall_upcall_cont(int spd_id, vaddr_t *inv_addr)
{
	struct spd *dest = spd_get_by_index(spd_id);
	/* FIXME: cast disregards spd_poly, which will break mpd */
	struct spd *curr_spd = (struct spd*)thd_get_current_spd();
	struct thread *thd = thd_get_current();

	if (NULL == dest || NULL == curr_spd) {
		printk("cos: upcall attempt failed - dest_spd = %d, curr_spd = %d.\n",
		       spd_get_index(dest), spd_get_index(curr_spd));
		return -1;
	}

	/*
	 * Check that we are upcalling into a serice that explicitely
	 * trusts us (i.e. that the current spd is allowed to upcall
	 * into the destination.)
	 */
	if (verify_trust(dest, curr_spd)) {
		printk("cos: upcall attempted from %d to %d without trust relation.\n",
		       spd_get_index(curr_spd), spd_get_index(dest));
		return -1;
	}

	/* 
	 * Is a parent scheduler granting a thread to a child
	 * scheduler?  If so, we need to add the child scheduler to
	 * the thread's scheduler info list, UNLESS this thread is
	 * already owned by another child scheduler.
	 *
	 * FIXME: remove this later as it adds redundance with the
	 * sched_cntl call, reducing orthogonality of the syscall
	 * layer.
	 */
	if (dest->parent_sched == curr_spd) {
		struct thd_sched_info *tsi;

		/*printk("cos: upcall from %d -- make %d a scheduler.\n",
		  spd_get_index(curr_spd), spd_get_index(dest));*/

		tsi = thd_get_sched_info(thd, curr_spd->sched_depth+1);
		if (NULL == tsi->scheduler) {
			tsi->scheduler = dest;
		}
	}

	open_close_spd(/*&*/dest->composite_spd/*->spd_info*/,
		       /*&*/curr_spd->composite_spd/*->spd_info*/);
	
	/* set the thread to have dest as a new base owner */
	thd->stack_ptr = 0;
	thd->stack_base[0].current_composite_spd = /*(struct composite_spd*)*/&dest->spd_info;

	*inv_addr = dest->upcall_entry;

	cos_meas_event(COS_MEAS_UPCALLS);

	return thd->thread_id;
}

COS_SYSCALL int cos_syscall_sched_cntl(int operation, int thd_id, long option)
{
	struct thread *thd = thd_get_current();
	/* FIXME: again */
	struct spd *spd = (struct spd *)thd_get_current_spd();
	struct thd_sched_info *tsi;

	tsi = thd_get_sched_info(thd, spd->sched_depth);
	if (tsi->scheduler != spd) {
		printk("cos: spd %d attempting sched_cntl not a scheduler.\n",
		       spd_get_index(spd));
		return -1;
	}

	if (COS_SCHED_EVT_REGION == operation) {
		/* 
		 * Set the event regions for this thread in
		 * user-space.  Make sure that the current scheduler
		 * has scheduling capabilities for this thread, and
		 * that the optional argument falls within the
		 * scheduler notification page.
		 */
		
	} else if (COS_SCHED_GRANT_SCHED == operation ||
		   COS_SCHED_REVOKE_SCHED == operation) {
		/*
		 * Permit a child scheduler the capability to schedule
		 * the thread, or remove that capability.  Assumes
		 * that 1) this spd is a scheduler that has the
		 * capability to schedule the thread, 2) the target
		 * spd is a scheduler that is a child of this
		 * scheduler.
		 */ 
		struct thd_sched_info *child_tsi;
		struct thread *target_thd = thd_get_by_id(thd_id);
		struct spd *child = spd_get_by_index((int)option);
		int i;
		
		if (NULL == target_thd         || 
		    NULL == child              || 
		    spd != child->parent_sched ||
		    !thd_scheduled_by(target_thd, spd)) {
			return -1;
		}
		
		child_tsi = thd_get_sched_info(target_thd, child->sched_depth);

		if (COS_SCHED_GRANT_SCHED == operation) {
			child_tsi->scheduler = child;
		} else if (COS_SCHED_REVOKE_SCHED == operation) {
			if (child_tsi->scheduler != child) {
				return -1;
			}

			child_tsi->scheduler = NULL;
		}
		/*
		 * Blank out all schedulers that are decendents of the
		 * child.
		 */
		for (i = child->sched_depth+1 ; i < MAX_SCHED_HIER_DEPTH ; i++) {
			child_tsi = thd_get_sched_info(target_thd, i);
			child_tsi->scheduler = NULL;
		}
	}

	return 0;
}


COS_SYSCALL int cos_syscall_mpd_cntl(int operation, short int composite_spd, short int spd, void *option)
{
	int ret = 0; 

	switch(operation) {
	case COS_MPD_CREATE:
	{
		ret = spd_alloc_mpd_desc();

		break;
	}
	case COS_MPD_ADD_SPD:
	{
		break;
	}
	case COS_MPD_DEACTIVATE:
	{
		spd_mpd_release_desc(composite_spd);

		break;
	}
	case COS_MPD_ACTIVATE:
	{
		break;
	}
	case COS_MPD_INV_CNT:
	{
		//short int ospd = (int)option;
 
		break;
	}
	/*
	 * reset the spd's protection domain to include only itself.
	 */
	case COS_MPD_ISOLATE:
	{
		break;
	}
	default:
		ret = -1;
	}
	
	return ret;
}

void *cos_syscall_tbl[16] = {
	(void*)cos_syscall_void,
	(void*)cos_syscall_resume_return,
	(void*)cos_syscall_get_thd_id,
	(void*)cos_syscall_create_thread,
	(void*)cos_syscall_switch_thread,
	(void*)cos_syscall_kill_thd,
	(void*)cos_syscall_brand_upcall,
	(void*)cos_syscall_brand,
	(void*)cos_syscall_upcall,
	(void*)cos_syscall_sched_cntl,
	(void*)cos_syscall_mpd_cntl,
	(void*)cos_syscall_void,
	(void*)cos_syscall_void,
	(void*)cos_syscall_void,
	(void*)cos_syscall_void,
	(void*)cos_syscall_void,
};
