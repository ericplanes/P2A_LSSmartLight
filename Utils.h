#ifndef _UTILS_H_
#define _UTILS_H_

#define FALSE 0
#define TRUE 1

#define BOOL unsigned char       // returns 0 or 1
#define SYS_STATUS unsigned char // returns a define with SYS_STATUS at the beginning
#define BYTE unsigned char
#define WORD unsigned short

#define TIMESTAMP_SIZE sizeof("hhmmssDDMMYYYY")

// Different status that we have
#define SYS_STATUS_OFF 0
#define SYS_STATUS_LOW 1
#define SYS_STATUS_MOD 2
#define SYS_STATUS_HIGH 3
#define SYS_STATUS_CRIT 4

#endif 