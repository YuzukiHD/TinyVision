#ifndef __COMMON_H__
#define __COMMON_H__
#include <stdint.h>

/* For debug */
#define SID_DEBUG 0

#if SID_DEBUG

#define SID_DBG(fmt, arg...)	printf("%s()%d - "fmt, __func__, __LINE__, ##arg)
#define SID_INF(fmt, arg...)	printf("%s()%d - "fmt, __func__, __LINE__, ##arg)
#define SID_ERR(fmt, arg...)	printf("%s()%d - "fmt, __func__, __LINE__, ##arg)
#else
#define SID_DBG(fmt, arg...)
#define SID_INF(fmt, arg...)
#define SID_ERR(fmt, arg...)	printf("%s()%d - "fmt, __func__, __LINE__, ##arg)
#endif

int sunxi_mmap_efuse_write(char *key_name, uint32_t offset_in_key,
			   char *key_buf);
int char_to_hex(char input);
int string_to_hex(char *input, char *output);
#endif //__COMMON_H__
