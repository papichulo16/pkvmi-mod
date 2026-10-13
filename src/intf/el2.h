
extern struct pkvm_module_ops* ops;
extern void* shared_page;

void host_hyp_reg_dump(void* dst_page);
void host_read_pa(void* dst_page, void* pa);

