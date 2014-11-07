#ifndef _TP2E_H_
#define _TP2E_H_

#include <linux/kct.h>

#undef kct_log
#define kct_log(ev_type, submitter_name, ev_name, data0, data1, data2) \
	do {								\
		if (kct_alloc_event) {					\
			struct ct_event *__ev =			\
				kct_alloc_event(submitter_name,	\
						ev_name,		\
						ev_type,		\
						GFP_ATOMIC,		\
						0);			\
			if (__ev) {					\
				if (data0)				\
					kct_add_attchmt(&__ev,		\
							CT_ATTCHMT_DATA0, \
							strlen(data0) + 1, \
							data0, GFP_ATOMIC); \
				if (data1)				\
					kct_add_attchmt(&__ev,		\
							CT_ATTCHMT_DATA1, \
							strlen(data1) + 1, \
							data1, GFP_ATOMIC); \
				if (data2)				\
					kct_add_attchmt(&__ev,		\
							CT_ATTCHMT_DATA2, \
							strlen(data2) + 1, \
							data2, GFP_ATOMIC); \
				kct_log_event(__ev, GFP_ATOMIC);	\
			}						\
		}							\
	} while (0)


struct tp_probe_ops {
	char *name;
	void *probe;
};

#define DEFINE_PROBE(event_name, probe_fn)

#endif /* _TP2E_H_ */

#ifdef DECLARE_PROBE
#undef DEFINE_PROBE
#define DEFINE_PROBE(event_name, probe_fn)			   \
	static struct tp_probe_ops tp_probe_ops_##event_name = {   \
		.name =	event_name,				   \
		.probe = (void*)probe_fn,			   \
	}
#endif /* DECLARE_PROBE */

#ifdef REGISTER_PROBE
#undef DEFINE_PROBE
#define DEFINE_PROBE(event_name, probe_fn)				\
	do {								\
		pr_debug("Registering probe for %s\n",			\
			 tp_probe_ops_##event_name.name);		\
		tracepoint_probe_register(tp_probe_ops_##event_name.name, \
					  tp_probe_ops_##event_name.probe, \
					  NULL);			\
	} while (0)
#endif /* REGISTER_PROBE */

#ifdef UNREGISTER_PROBE
#undef DEFINE_PROBE
#define DEFINE_PROBE(event_name, probe_fn)				\
	do {								\
		pr_debug("Unregistering probe for %s\n",		\
			 tp_probe_ops_##event_name.name);		\
		tracepoint_probe_unregister(tp_probe_ops_##event_name.name, \
					    tp_probe_ops_##event_name.probe, \
					    NULL);			\
	} while (0)
#endif /* UNREGISTER_PROBE */
