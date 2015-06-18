#ifndef __ARCH_HIB_H_
#define __ARCH_HIB_H_

extern void call_with_stack(void (*fn)(void *), void *arg, void *sp);

extern const void __nosave_begin, __nosave_end;

#endif

