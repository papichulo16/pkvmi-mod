#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <asm/kvm_pkvm_module.h>

#include "pkvmi.h"

/* 
 * for now this file is supposed to find the GVA for 
 * __kvm_nvhe_vm_table through kallsyms, then
 * turn it into a GPA through kimage_voffset in kallsyms
 *
 * I want to pass that PA to the EL2 module since it is 
 * correct in hypervisor space because of link time shenanigans 
 *
 * I lowkey dont understand why, but that is the goal
 * */

void vm_table_pa_find(void) {}

