#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>

#include "pkvmi.h"

/* 
 * for now this file is supposed to find the GVA for 
 * __kvm_nvhe_vm_table through kallsyms, then
 * turn it into a GPA through kimage_voffset in kallsyms
 *
 * I want to pass that PA to the EL2 module since it is 
 * correct in hypervisor space because of link time shenanigans 
 *
 * I lowkey dont understand why that is even there, but that is the goal
 *
 * well I do, its just retarded imo. but maybe I dont know much about linkers
 * */

typedef void* (*kallsyms_lookup_name_t)(const char *name);

void* pkvm_vm_table_pa;

static kallsyms_lookup_name_t get_kallsyms_lookup_name(void) {
 
    struct kprobe kp = {
        .symbol_name = "kallsyms_lookup_name",
    };

    kallsyms_lookup_name_t f;
    int ret = register_kprobe(&kp);

    if (ret < 0) {
        pr_err("===== kprobe bad\n");
        return NULL;
    }

    f = (kallsyms_lookup_name_t) kp.addr;
    unregister_kprobe(&kp);

    return f;
}

int vm_table_pa_find(void) {
 
    kallsyms_lookup_name_t fun_lookup_name = get_kallsyms_lookup_name();
    void* kimage_voffset = NULL;
    void* pkvm_vm_table_va = NULL;

    if (!fun_lookup_name) {
        pr_err("===== fun lookup bad\n");
        return -EFAULT;
    }

    kimage_voffset = fun_lookup_name("kimage_voffset");
    pkvm_vm_table_va = fun_lookup_name("__kvm_nvhe_vm_table");

    pr_info("===== kimage_voffset 0x%px\n", kimage_voffset);
    pr_info("===== __kvm_nvhe_vm_table VA 0x%px\n", pkvm_vm_table_va);

    if (!kimage_voffset || !pkvm_vm_table_va)
      return -EFAULT;
   
    kimage_voffset = *(void **) kimage_voffset;
    pkvm_vm_table_pa = (void *) ((unsigned long) pkvm_vm_table_va - (unsigned long) kimage_voffset);

    pr_info("===== __kvm_nvhe_vm_table PA 0x%px\n", pkvm_vm_table_pa);

    return 0;
}

