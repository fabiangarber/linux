// ioctl_toggle.h
#ifndef IOCTL_TOGGLE_H
#define IOCTL_TOGGLE_H

#include <linux/ioctl.h>

#define MAGIC_NUM 'k'
#define IOCTL_TOGGLE_GET _IOR(MAGIC_NUM, 0, int)
#define IOCTL_TOGGLE_SET _IOW(MAGIC_NUM, 1, int)

#endif

