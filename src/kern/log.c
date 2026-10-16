#include <linux/kernel.h>
#include <linux/printk.h>

#include "pkvmi.h"

static const char* const g_err_msgs[PKVMI_ERR_NR] = {
  [PKVMI_ERR_EL2_LOAD] = "el2 load",
  [PKVMI_ERR_HVC_REGISTER] = "hvc register",
  [PKVMI_ERR_KPROBE] = "kprobe",
  [PKVMI_ERR_KIMAGE_VOFF] = "kimage_voffset lookup",
  [PKVMI_ERR_VM_TABLE_SYM] = "vm_table lookup",
  [PKVMI_ERR_MISC_REGISTER] = "misc register",
  [PKVMI_ERR_SHARED_ALLOC] = "shared page alloc",
  [PKVMI_ERR_VM_TABLE_PA] = "el2 vm_table pa",
  [PKVMI_ERR_VM_TABLE_HVA] = "el2 vm_table hva",
  [PKVMI_ERR_VM_TABLE_PTR] = "el2 vm_table ptr",
  [PKVMI_ERR_SHARE_BUSY] = "el2 share busy",
  [PKVMI_ERR_HOST_SHARE] = "el2 host share",
  [PKVMI_ERR_PIN_SHARED] = "el2 pin shared",
};

void pkvmi_log_err(unsigned long ret) {

  unsigned int code = PKVMI_ERR_CODE(ret);
  const char* msg = "unknown";

  if (!ret)
    goto end;

  if (code < PKVMI_ERR_NR && g_err_msgs[code])
    msg = g_err_msgs[code];

  pr_err("===== %s %d\n", msg, PKVMI_ERR_ERRNO(ret));

end:
  return;
}
