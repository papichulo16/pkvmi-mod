#include <asm/kvm_pkvm_module.h>
#include <nvhe/trap_handler.h>

#include "pkvmi_args.h"

static struct pkvm_module_ops* ops;

static int find_vm_table_hva(void* vm_table_pa) {

  if (!vm_table_pa)
    return 0;

  return 67;
} 

void pkvmi_hvc(struct kvm_cpu_context *ctx) {

  switch (cpu_reg(ctx, 1)) {

    case PKVMI_INIT:
      cpu_reg(ctx, 1) = find_vm_table_hva((void *) cpu_reg(ctx, 2));
      break;

    default:
      cpu_reg(ctx, 1) = 0;
      break;
  }
}

int pkvmi_init(struct pkvm_module_ops* _ops) {

  ops = _ops;

  return 0;
}

