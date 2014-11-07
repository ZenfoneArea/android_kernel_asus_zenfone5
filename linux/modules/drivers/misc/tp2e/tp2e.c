#include <linux/module.h>
#include "tp2e.h"

/* Create generic TP2E tracepoint event */
#define CREATE_TRACE_POINTS
#include <trace/events/tp2e.h>

/* uncomment to compile test */
/* #define TP2E_TEST */
#ifdef TP2E_TEST
# include "tp2e_test.c"
#endif

#define DECLARE_PROBE
# include "tp2e_probes.h"
#undef DECLARE_PROBE

static int __init tp2e_init(void)
{
# define REGISTER_PROBE
#  include "tp2e_probes.h"
# undef REGISTER_PROBE

	return 0;
}

static void __exit tp2e_exit(void)
{
# define UNREGISTER_PROBE
#  include "tp2e_probes.h"
# undef UNREGISTER_PROBE

	tracepoint_synchronize_unregister();
}

fs_initcall(tp2e_init);
module_exit(tp2e_exit);

MODULE_LICENSE("GPL");
