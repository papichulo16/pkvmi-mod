#ifndef PKVMI_ERR_H
#define PKVMI_ERR_H

enum pkvmi_err {
  PKVMI_OK,

  PKVMI_ERR_EL2_LOAD,
  PKVMI_ERR_HVC_REGISTER,
  PKVMI_ERR_KPROBE,
  PKVMI_ERR_KIMAGE_VOFF,
  PKVMI_ERR_VM_TABLE_SYM,
  PKVMI_ERR_MISC_REGISTER,
  PKVMI_ERR_SHARED_ALLOC,

  PKVMI_ERR_VM_TABLE_PA,
  PKVMI_ERR_VM_TABLE_HVA,
  PKVMI_ERR_VM_TABLE_PTR,
  PKVMI_ERR_SHARE_BUSY,
  PKVMI_ERR_HOST_SHARE,
  PKVMI_ERR_PIN_SHARED,

  PKVMI_ERR_NR,
};

#define PKVMI_ERR(code, err) ((err) ? ((unsigned long) (code) << 32) | (unsigned int) (err) : 0UL)
#define PKVMI_ERR_CODE(ret)  ((unsigned int) ((unsigned long) (ret) >> 32))
#define PKVMI_ERR_ERRNO(ret) ((int) (unsigned int) (ret))

#endif
