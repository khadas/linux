#ifndef _AML_NOTIFY_H
#define _AML_NOTIFY_H
#include <linux/notifier.h>
int aout_notifier_call_chain(unsigned long val, void *v);
int aout_unregister_client(struct notifier_block *);
int aout_register_client(struct notifier_block *);
#endif  /* _AML_NOTIFY_H */
