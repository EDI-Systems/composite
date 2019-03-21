#include <cos_component.h>
#include <cos_kernel_api.h>
#include <llprint.h>
#include <pong.h>
#include <cos_debug.h>
#include <cos_types.h>
#include <hypercall.h>

void
call(void)
{
	return;
}

int
call_ret(void)
{
	return 42;
}

int
call_arg(int p1)
{
	return p1;
}

int
call_args(int p1, int p2, int p3, int p4)
{
	return p1 + p2 + p3 + p4;
}

int
call_argsrets(int p0, int p1, int p2, int p3, int *r0, int *r1)
{
	*r0 = p0;
	*r1 = p1;

	return p2;
}

int
call_subset(unsigned long p0, unsigned long p1, unsigned long *r0)
{
	*r0 = p0 + p1;
	return -p0 - p1;
}

thdid_t
call_ids(compid_t *client, compid_t *serv)
{
	*client = (compid_t)cos_inv_token();
	*serv   = cos_compid();

	return cos_thdid();
}

/* test a cos_init that doesn't exist */
