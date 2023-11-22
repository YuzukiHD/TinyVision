# SPDX-License-Identifier: GPL-2.0-only

obj-$(CONFIG_AW_MFD_AXP2101)		+= mfd/axp2101.o
obj-$(CONFIG_AW_MFD_AXP2101_I2C)	+= mfd/axp2101-i2c.o
obj-$(CONFIG_AW_MFD_PMU_EXT)		+= mfd/pmu-ext-core.o
obj-$(CONFIG_AW_MFD_PMU_EXT_I2C)	+= mfd/pmu-ext-i2c.o
obj-$(CONFIG_AW_MFD_BMU_EXT)		+= mfd/bmu-ext-core.o
obj-$(CONFIG_AW_MFD_BMU_EXT_I2C)	+= mfd/bmu-ext-i2c.o

obj-$(CONFIG_AW_REGULATOR_AXP2101)	+= regulator/axp2101-regulator.o
obj-$(CONFIG_AW_REGULATOR_PMU_EXT)  += regulator/pmu-ext-regulator.o
obj-$(CONFIG_AW_REGULATOR_BMU_EXT)  += regulator/bmu-ext-regulator.o
obj-$(CONFIG_AW_REGULATOR_PWM) 		+= regulator/sunxi_pwm_regulator.o

obj-$(CONFIG_AW_AXP803_POWER)		+= supply/axp803_usb_power.o
obj-$(CONFIG_AW_AXP803_POWER)		+= supply/axp803_ac_power.o
obj-$(CONFIG_AW_AXP803_POWER)		+= supply/axp803_battery.o
obj-$(CONFIG_AW_AXP2202_POWER)		+= supply/axp2202_usb_power.o
obj-$(CONFIG_AW_AXP2202_POWER)		+= supply/axp2202_battery.o
obj-$(CONFIG_AW_AXP2202_GPIO_POWER) += supply/axp2202_gpio_power.o
obj-$(CONFIG_AW_AXP22X_POWER)		+= supply/axp22x_usb_power.o
obj-$(CONFIG_AW_AXP22X_POWER)		+= supply/axp22x_ac_power.o
obj-$(CONFIG_AW_AXP22X_POWER)		+= supply/axp22x_battery.o

obj-$(CONFIG_AW_ETA6973_POWER)		+= supply/eta6973_charger_power.o
obj-$(CONFIG_AW_AXP519_POWER)		+= supply/axp519_charger_power.o
obj-$(CONFIG_AW_AXP2601_POWER)		+= supply/axp2601_battery.o

obj-$(CONFIG_AW_INPUT_AXP2101_PEK)	+= power_key/axp2101-pek.o

ccflags-y += -I $(srctree)/include/linux/power
