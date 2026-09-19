#ifndef HELLO_H
#define HELLO_H

#include <linux/ioctl.h>

#define HELLO_MSG_LEN 32

#define HELLO_IOC_GREET _IOR('h', 1, char[HELLO_MSG_LEN])

#endif
