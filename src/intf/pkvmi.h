#ifndef PKVMI_H
#define PKVMI_H

#include <linux/ioctl.h>

struct file;
extern void* pkvm_vm_table_pa;

#define IOCTL_MSG_LEN 32

#define HYPM_INIT _IOR('h', 2, char[IOCTL_MSG_LEN])

long pkvmi_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
int pkvm_driver_init(void);
int vm_table_pa_find(void);

#endif
