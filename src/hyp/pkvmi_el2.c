#include <asm/kvm_pkvm_module.h>
#include <nvhe/trap_handler.h>

static struct pkvm_module_ops* ops;

int pkvm_hello_init(struct pkvm_module_ops* _ops) {

  ops = _ops;

  return 0;
}

void pkvm_hello_hvc(struct kvm_cpu_context *ctx) {

  cpu_reg(ctx, 1) = 67;
}

