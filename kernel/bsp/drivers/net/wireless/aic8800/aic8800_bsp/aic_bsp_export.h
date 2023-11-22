#ifndef __AIC_BSP_EXPORT_H
#define __AIC_BSP_EXPORT_H

#define AICBSP_RESV_MEM_SUPPORT

enum aicbsp_subsys {
	AIC_BLUETOOTH,
	AIC_WIFI,
};

enum aicbsp_pwr_state {
	AIC_PWR_OFF,
	AIC_PWR_ON,
};

enum skb_buff_id {
	AIC_RESV_MEM_TXDATA,
};

enum chip_rev_id {
	CHIP_REV_ID_U01 = 0x1,
	CHIP_REV_ID_U02 = 0x3,
	CHIP_REV_ID_U03 = 0x7,
	CHIP_REV_ID_U04 = 0x7,
};

enum chip_id {
	AIC8800D_CHIP_SUBREV_ID_U04 = 0x20,
};

enum AIC_PRODUCT_ID {
	PRODUCT_ID_AIC8800D = 0,
	PRODUCT_ID_AIC8800DC,
	PRODUCT_ID_AIC8800DW,
	PRODUCT_ID_AIC8800D80
};

enum aicbsp_cpmode_type {
	AICBSP_CPMODE_WORK,
	AICBSP_CPMODE_TEST,
	AICBSP_CPMODE_MAX,
};

struct device_match_entry {
	u16 vid;
	u16 pid;
	u16 chipid;
	char *name;
	u16 rev;
	u16 subrev;
	u16 mcuid;
	u32 feature;
};

struct skb_buff_pool {
	uint32_t id;
	uint32_t size;
	const char *name;
	uint8_t used;
	struct sk_buff *skb;
};

struct aicbsp_feature_t {
	int      hwinfo;
	uint32_t sdio_clock;
	uint8_t  sdio_phase;
	bool     fwlog_en;
	struct device_match_entry *chipinfo;
	uint8_t  cpmode;
	uint8_t irqf;
};

int aicbsp_set_subsys(int, int);
int aicbsp_get_feature(struct aicbsp_feature_t *feature);
struct sk_buff *aicbsp_resv_mem_alloc_skb(unsigned int length, uint32_t id);
void aicbsp_resv_mem_kfree_skb(struct sk_buff *skb, uint32_t id);

#endif
