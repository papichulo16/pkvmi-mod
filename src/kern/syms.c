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

typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);

void* kimage_voff = NULL;
void* pkvm_vm_table_pa;

static int get_kallsyms_lookup_name(kallsyms_lookup_name_t* out) {

    struct kprobe kp = {
        .symbol_name = "kallsyms_lookup_name",
    };

    int ret = register_kprobe(&kp);

    if (ret < 0)
        goto end;

    *out = (kallsyms_lookup_name_t) kp.addr;
    unregister_kprobe(&kp);

end:
    return ret;
}

unsigned long vm_table_pa_find(void) {

    kallsyms_lookup_name_t fun_lookup_name = NULL;
    void* pkvm_vm_table_va = NULL;
    int err = get_kallsyms_lookup_name(&fun_lookup_name);
    unsigned long ret = PKVMI_ERR(PKVMI_ERR_KPROBE, err);

    if (err < 0)
        goto end;

    kimage_voff = (void *) fun_lookup_name("kimage_voffset");
    pkvm_vm_table_va = (void *) fun_lookup_name("__kvm_nvhe_vm_table");

    pr_info("===== kimage_voffset 0x%px\n", kimage_voff);
    pr_info("===== __kvm_nvhe_vm_table VA 0x%px\n", pkvm_vm_table_va);

    ret = PKVMI_ERR(PKVMI_ERR_KIMAGE_VOFF, -ENOENT);

    if (!kimage_voff)
        goto end;

    ret = PKVMI_ERR(PKVMI_ERR_VM_TABLE_SYM, -ENOENT);

    if (!pkvm_vm_table_va)
        goto end;

    kimage_voff = *(void **) kimage_voff;
    pkvm_vm_table_pa = (void *) ((unsigned long) pkvm_vm_table_va - (unsigned long) kimage_voff);

    pr_info("===== __kvm_nvhe_vm_table PA 0x%px\n", pkvm_vm_table_pa);

    ret = PKVMI_OK;

end:
    return ret;
}
