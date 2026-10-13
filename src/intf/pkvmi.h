#ifndef PKVMI_H
#define PKVMI_H

#include <linux/ioctl.h>

#include "pkvmi_regs.h"

struct file;
extern void* pkvm_vm_table_pa;

#define IOCTL_MSG_LEN 0x1000

#define HYPM_INIT _IOR('h', 2, char[IOCTL_MSG_LEN])
#define HYPM_REG_DUMP _IOR('h', 3, char[IOCTL_MSG_LEN])
#define HYPM_READ_PA _IOWR('h', 4, char[IOCTL_MSG_LEN])

long pkvmi_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
int pkvm_driver_init(void);
int vm_table_pa_find(void);

#endif

