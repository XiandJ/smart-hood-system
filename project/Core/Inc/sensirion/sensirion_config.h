#ifndef SENSIRION_CONFIG_H
#define SENSIRION_CONFIG_H

#include <stdint.h>

#ifndef NULL
#define NULL ((void *)0)
#endif

#ifndef __cplusplus
#if __STDC_VERSION__ >= 199901L
#include <stdbool.h>
#else
#ifndef bool
#define bool int
#define true 1
#define false 0
#endif
#endif
#endif

#endif /* SENSIRION_CONFIG_H */
