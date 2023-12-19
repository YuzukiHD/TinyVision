/*
 * Based on the tcs4838 driver and the previous tcs4838 driver
 */

#ifndef __LINUX_MFD_PMU_EXT_H
#define __LINUX_MFD_PMU_EXT_H

#include <linux/regmap.h>

enum {
	TCS4838_ID = 0,
	SY8827G_ID,
	NR_PMU_EXT_VARIANTS,
};

/* List of registers for tcs4838 */
#define TCS4838_VSEL0		0x00
#define TCS4838_VSEL1		0x01
#define TCS4838_CTRL		0x02
#define TCS4838_ID1		    0x03
#define TCS4838_ID2		    0x04
#define TCS4838_PGOOD		0x05

/* List of registers for sy8827g */
#define SY8827G_VSEL0		0x00
#define SY8827G_VSEL1		0x01
#define SY8827G_CTRL		0x02
#define SY8827G_ID1		    0x03
#define SY8827G_ID2		    0x04
#define SY8827G_PGOOD		0x05

/*
 * struct tcs4838 - state holder for the tcs4838 driver
 *
 * Device data may be used to access the tcs4838 chip
 */
struct pmu_ext_dev {
	struct device 				*dev;
	struct regmap				*regmap;
	long		 				variant;
	int 						nr_cells;
	struct mfd_cell				*cells;
	const struct regmap_config	*regmap_cfg;
	void (*dts_parse)(struct pmu_ext_dev *);
};

enum {
	TCS4838_DCDC0 = 0,
	TCS4838_DCDC1,
	TCS4838_REG_ID_MAX,
};

enum {
	SY8827G_DCDC0 = 0,
	SY8827G_DCDC1,
	SY8827G_REG_ID_MAX,
};

int pmu_ext_match_device(struct pmu_ext_dev *ext);
int pmu_ext_device_init(struct pmu_ext_dev *ext);
int pmu_ext_device_exit(struct pmu_ext_dev *ext);

#endif /*  __LINUX_MFD_TCS4838_H */
