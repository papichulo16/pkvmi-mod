#include <asm/kvm_pkvm_module.h>
#include <nvhe/trap_handler.h>

int pkvm_hello_init(const struct pkvm_module_ops *ops) {

  /* Init the EL2 code */

  return 0;
}

void pkvm_hello_hvc(struct kvm_cpu_context *ctx) {

  cpu_reg(ctx, 1) = 67;
}

