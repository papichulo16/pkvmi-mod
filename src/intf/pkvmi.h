#ifndef PKVMI_H
#define PKVMI_H

#include <linux/ioctl.h>

#include "pkvmi_err.h"
#include "pkvmi_regs.h"

struct file;
extern void* pkvm_vm_table_pa;
extern void* kimage_voff;

#define IOCTL_MSG_LEN 0x1000

#define HYPM_INIT _IOR('h', 2, char[IOCTL_MSG_LEN])
#define HYPM_REG_DUMP _IOR('h', 3, char[IOCTL_MSG_LEN])
#define HYPM_READ_PA _IOWR('h', 4, char[IOCTL_MSG_LEN])
#define HYPM_REG_DUMP_EL1 _IOR('h', 5, char[IOCTL_MSG_LEN])
#define HYPM_READ_PA_EL1 _IOWR('h', 6, char[IOCTL_MSG_LEN])

long pkvmi_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
unsigned long pkvm_driver_init(void);
unsigned long vm_table_pa_find(void);
void pkvmi_log_err(unsigned long ret);

#endif

