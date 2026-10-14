
extern struct pkvm_module_ops* ops;
extern void* shared_page;
extern void* kimage_voff;

void host_el2_reg_dump(void* dst_page);
void host_el2_read_pa(void* dst_page, void* pa);

void host_el1_reg_dump(void* dst_page);
int host_el1_read_pa(void* dst_page, void* pa);
void* host_el1_map_va(unsigned long va);
void host_el1_unmap(void);

