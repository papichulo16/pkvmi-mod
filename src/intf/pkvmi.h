#ifndef HELLO_H
#define HELLO_H

#include <linux/ioctl.h>

struct file;

#define HELLO_MSG_LEN 32

#define HELLO_HYPM_INIT _IOR('i', 1, long)
#define HELLO_HYPM_GREET _IOR('h', 2, char[HELLO_MSG_LEN])

long hello_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
int pkvm_driver_init(void);
void vm_table_pa_find(void);

#endif
