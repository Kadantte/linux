// SPDX-License-Identifier: GPL-2.0-only
/*
 * processor thermal device RFIM control
 * Copyright (c) 2020, Intel Corporation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include "processor_thermal_device.h"

MODULE_IMPORT_NS("INT340X_THERMAL");

struct mmio_reg {
	int read_only;
	u32 offset;
	int bits;
	u16 mask;
	u16 shift;
};

struct mapping_table {
	const char *attr_name;
	const u32 value;
	const char *mapped_str;
};

/* These will represent sysfs attribute names */
static const char * const fivr_strings[] = {
	"vco_ref_code_lo",
	"vco_ref_code_hi",
	"spread_spectrum_pct",
	"spread_spectrum_clk_enable",
	"rfi_vco_ref_code",
	"fivr_fffc_rev",
	NULL
};

static const struct mmio_reg tgl_fivr_mmio_regs[] = {
	{ 0, 0x5A18, 3, 0x7, 11}, /* vco_ref_code_lo */
	{ 0, 0x5A18, 8, 0xFF, 16}, /* vco_ref_code_hi */
	{ 0, 0x5A08, 8, 0xFF, 0}, /* spread_spectrum_pct */
	{ 0, 0x5A08, 1, 0x1, 8}, /* spread_spectrum_clk_enable */
	{ 1, 0x5A10, 12, 0xFFF, 0}, /* rfi_vco_ref_code */
	{ 1, 0x5A14, 2, 0x3, 1}, /* fivr_fffc_rev */
};

static const char * const dlvr_strings[] = {
	"dlvr_spread_spectrum_pct",
	"dlvr_control_mode",
	"dlvr_control_lock",
	"dlvr_rfim_enable",
	"dlvr_freq_select",
	"dlvr_hardware_rev",
	"dlvr_freq_mhz",
	"dlvr_pll_busy",
	NULL
};

static const struct mmio_reg dlvr_mmio_regs[] = {
	{ 0, 0x15A08, 5, 0x1F, 0}, /* dlvr_spread_spectrum_pct */
	{ 0, 0x15A08, 1, 0x1, 5}, /* dlvr_control_mode */
	{ 0, 0x15A08, 1, 0x1, 6}, /* dlvr_control_lock */
	{ 0, 0x15A08, 1, 0x1, 7}, /* dlvr_rfim_enable */
	{ 0, 0x15A08, 12, 0xFFF, 8}, /* dlvr_freq_select */
	{ 1, 0x15A10, 2, 0x3, 30}, /* dlvr_hardware_rev */
	{ 1, 0x15A10, 16, 0xFFFF, 0}, /* dlvr_freq_mhz */
	{ 1, 0x15A10, 1, 0x1, 16}, /* dlvr_pll_busy */
};

static const struct mmio_reg lnl_dlvr_mmio_regs[] = {
	{ 0, 0x5A08, 5, 0x1F, 0}, /* dlvr_spread_spectrum_pct */
	{ 0, 0x5A08, 1, 0x1, 5}, /* dlvr_control_mode */
	{ 0, 0x5A08, 1, 0x1, 6}, /* dlvr_control_lock */
	{ 0, 0x5A08, 1, 0x1, 7}, /* dlvr_rfim_enable */
	{ 0, 0x5A08, 2, 0x3, 8}, /* dlvr_freq_select */
	{ 1, 0x5A10, 2, 0x3, 30}, /* dlvr_hardware_rev */
	{ 1, 0x5A10, 2, 0x3, 0}, /* dlvr_freq_mhz */
	{ 1, 0x5A10, 1, 0x1, 23}, /* dlvr_pll_busy */
};

static const struct mapping_table lnl_dlvr_mapping[] = {
	{"dlvr_freq_select", 0, "2227.2"},
	{"dlvr_freq_select", 1, "2140"},
	{"dlvr_freq_mhz", 0, "2227.2"},
	{"dlvr_freq_mhz", 1, "2140"},
	{NULL, 0, NULL},
};

static int match_mapping_table(const struct mapping_table *table, const char *attr_name,
			       bool match_int_value, const u32 value, const char *value_str,
			       char **result_str, u32 *result_int)
{
	bool attr_matched = false;
	int i = 0;

	if (!table)
		return -EOPNOTSUPP;

	while (table[i].attr_name) {
		if (strncmp(table[i].attr_name, attr_name, strlen(attr_name)))
			goto match_next;

		attr_matched = true;

		if (match_int_value) {
			if (table[i].value != value)
				goto match_next;

			*result_str = (char *)table[i].mapped_str;
			return 0;
		}

		if (strncmp(table[i].mapped_str, value_str, strlen(table[i].mapped_str)))
			goto match_next;

		*result_int = table[i].value;

		return 0;
match_next:
		i++;
	}

	/* If attribute name is matched, then the user space value is invalid */
	if (attr_matched)
		return -EINVAL;

	return -EOPNOTSUPP;
}

static int get_mapped_string(const struct mapping_table *table, const char *attr_name,
			     u32 value, char **result)
{
	return match_mapping_table(table, attr_name, true, value, NULL, result, NULL);
}

static int get_mapped_value(const struct mapping_table *table, const char *attr_name,
			    const char *value, unsigned int *result)
{
	return match_mapping_table(table, attr_name, false, 0, value, NULL, result);
}

/* These will represent sysfs attribute names */
static const char * const dvfs_strings[] = {
	"rfi_restriction_run_busy",
	"rfi_restriction_err_code",
	"rfi_restriction_data_rate",
	"rfi_restriction_data_rate_base",
	"ddr_data_rate_point_0",
	"ddr_data_rate_point_1",
	"ddr_data_rate_point_2",
	"ddr_data_rate_point_3",
	"rfi_disable",
	NULL
};

static const struct mmio_reg adl_dvfs_mmio_regs[] = {
	{ 0, 0x5A38, 1, 0x1, 31}, /* rfi_restriction_run_busy */
	{ 0, 0x5A38, 7, 0x7F, 24}, /* rfi_restriction_err_code */
	{ 0, 0x5A38, 8, 0xFF, 16}, /* rfi_restriction_data_rate */
	{ 0, 0x5A38, 16, 0xFFFF, 0}, /* rfi_restriction_data_rate_base */
	{ 0, 0x5A30, 10, 0x3FF, 0}, /* ddr_data_rate_point_0 */
	{ 0, 0x5A30, 10, 0x3FF, 10}, /* ddr_data_rate_point_1 */
	{ 0, 0x5A30, 10, 0x3FF, 20}, /* ddr_data_rate_point_2 */
	{ 0, 0x5A30, 10, 0x3FF, 30}, /* ddr_data_rate_point_3 */
	{ 0, 0x5A40, 1, 0x1, 0}, /* rfi_disable */
};

static const struct mapping_table *dlvr_mapping;
static const struct mmio_reg *dlvr_mmio_regs_table;

#define RFIM_SHOW(suffix, table)\
static ssize_t suffix##_show(struct device *dev,\
			      struct device_attribute *attr,\
			      char *buf)\
{\
	const struct mmio_reg *mmio_regs = dlvr_mmio_regs_table;\
	const struct mapping_table *mapping = dlvr_mapping;\
	struct proc_thermal_device *proc_priv;\
	struct pci_dev *pdev = to_pci_dev(dev);\
	const char **match_strs;\
	int ret, err;\
	u32 reg_val;\
	char *str;\
\
	proc_priv = pci_get_drvdata(pdev);\
	if (table == 1) {\
		match_strs = (const char **)dvfs_strings;\
		mmio_regs = adl_dvfs_mmio_regs;\
	} else if (table == 2) { \
		match_strs = (const char **)dlvr_strings;\
	} else {\
		match_strs = (const char **)fivr_strings;\
		mmio_regs = tgl_fivr_mmio_regs;\
	} \
	ret = match_string(match_strs, -1, attr->attr.name);\
	if (ret < 0)\
		return ret;\
	reg_val = readl((void __iomem *) (proc_priv->mmio_base + mmio_regs[ret].offset));\
	ret = (reg_val >> mmio_regs[ret].shift) & mmio_regs[ret].mask;\
	err = get_mapped_string(mapping, attr->attr.name, ret, &str);\
	if (!err)\
		return sprintf(buf, "%s\n", str);\
	if (err == -EOPNOTSUPP)\
		return sprintf(buf, "%u\n", ret);\
	return err;\
}

#define RFIM_STORE(suffix, table)\
static ssize_t suffix##_store(struct device *dev,\
			       struct device_attribute *attr,\
			       const char *buf, size_t count)\
{\
	const struct mmio_reg *mmio_regs = dlvr_mmio_regs_table;\
	const struct mapping_table *mapping = dlvr_mapping;\
	struct proc_thermal_device *proc_priv;\
	struct pci_dev *pdev = to_pci_dev(dev);\
	unsigned int input;\
	const char **match_strs;\
	int ret, err;\
	u32 reg_val;\
	u32 mask;\
\
	proc_priv = pci_get_drvdata(pdev);\
	if (table == 1) {\
		match_strs = (const char **)dvfs_strings;\
		mmio_regs = adl_dvfs_mmio_regs;\
	} else if (table == 2) { \
		match_strs = (const char **)dlvr_strings;\
	} else {\
		match_strs = (const char **)fivr_strings;\
		mmio_regs = tgl_fivr_mmio_regs;\
	} \
	\
	ret = match_string(match_strs, -1, attr->attr.name);\
	if (ret < 0)\
		return ret;\
	if (mmio_regs[ret].read_only)\
		return -EPERM;\
	err = get_mapped_value(mapping, attr->attr.name, buf, &input);\
	if (err == -EINVAL)\
		return err;\
	if (err == -EOPNOTSUPP) {\
		err = kstrtouint(buf, 10, &input);\
		if (err)\
			return err;\
	} \
	mask = GENMASK(mmio_regs[ret].shift + mmio_regs[ret].bits - 1, mmio_regs[ret].shift);\
	reg_val = readl((void __iomem *) (proc_priv->mmio_base + mmio_regs[ret].offset));\
	reg_val &= ~mask;\
	reg_val |= (input << mmio_regs[ret].shift);\
	writel(reg_val, (void __iomem *) (proc_priv->mmio_base + mmio_regs[ret].offset));\
	return count;\
}

RFIM_SHOW(vco_ref_code_lo, 0)
RFIM_SHOW(vco_ref_code_hi, 0)
RFIM_SHOW(spread_spectrum_pct, 0)
RFIM_SHOW(spread_spectrum_clk_enable, 0)
RFIM_SHOW(rfi_vco_ref_code, 0)
RFIM_SHOW(fivr_fffc_rev, 0)

RFIM_STORE(vco_ref_code_lo, 0)
RFIM_STORE(vco_ref_code_hi, 0)
RFIM_STORE(spread_spectrum_pct, 0)
RFIM_STORE(spread_spectrum_clk_enable, 0)
RFIM_STORE(rfi_vco_ref_code, 0)
RFIM_STORE(fivr_fffc_rev, 0)

RFIM_SHOW(dlvr_spread_spectrum_pct, 2)
RFIM_SHOW(dlvr_control_mode, 2)
RFIM_SHOW(dlvr_control_lock, 2)
RFIM_SHOW(dlvr_hardware_rev, 2)
RFIM_SHOW(dlvr_freq_mhz, 2)
RFIM_SHOW(dlvr_pll_busy, 2)
RFIM_SHOW(dlvr_freq_select, 2)
RFIM_SHOW(dlvr_rfim_enable, 2)

RFIM_STORE(dlvr_spread_spectrum_pct, 2)
RFIM_STORE(dlvr_rfim_enable, 2)
RFIM_STORE(dlvr_freq_select, 2)
RFIM_STORE(dlvr_control_mode, 2)
RFIM_STORE(dlvr_control_lock, 2)

static DEVICE_ATTR_RW(dlvr_spread_spectrum_pct);
static DEVICE_ATTR_RW(dlvr_control_mode);
static DEVICE_ATTR_RW(dlvr_control_lock);
static DEVICE_ATTR_RW(dlvr_freq_select);
static DEVICE_ATTR_RO(dlvr_hardware_rev);
static DEVICE_ATTR_RO(dlvr_freq_mhz);
static DEVICE_ATTR_RO(dlvr_pll_busy);
static DEVICE_ATTR_RW(dlvr_rfim_enable);

static struct attribute *dlvr_attrs[] = {
	&dev_attr_dlvr_spread_spectrum_pct.attr,
	&dev_attr_dlvr_control_mode.attr,
	&dev_attr_dlvr_control_lock.attr,
	&dev_attr_dlvr_freq_select.attr,
	&dev_attr_dlvr_hardware_rev.attr,
	&dev_attr_dlvr_freq_mhz.attr,
	&dev_attr_dlvr_pll_busy.attr,
	&dev_attr_dlvr_rfim_enable.attr,
	NULL
};

static const struct attribute_group dlvr_attribute_group = {
	.attrs = dlvr_attrs,
	.name = "dlvr"
};

static DEVICE_ATTR_RW(vco_ref_code_lo);
static DEVICE_ATTR_RW(vco_ref_code_hi);
static DEVICE_ATTR_RW(spread_spectrum_pct);
static DEVICE_ATTR_RW(spread_spectrum_clk_enable);
static DEVICE_ATTR_RW(rfi_vco_ref_code);
static DEVICE_ATTR_RW(fivr_fffc_rev);

static struct attribute *fivr_attrs[] = {
	&dev_attr_vco_ref_code_lo.attr,
	&dev_attr_vco_ref_code_hi.attr,
	&dev_attr_spread_spectrum_pct.attr,
	&dev_attr_spread_spectrum_clk_enable.attr,
	&dev_attr_rfi_vco_ref_code.attr,
	&dev_attr_fivr_fffc_rev.attr,
	NULL
};

static const struct attribute_group fivr_attribute_group = {
	.attrs = fivr_attrs,
	.name = "fivr"
};

RFIM_SHOW(rfi_restriction_run_busy, 1)
RFIM_SHOW(rfi_restriction_err_code, 1)
RFIM_SHOW(rfi_restriction_data_rate, 1)
RFIM_SHOW(rfi_restriction_data_rate_base, 1)
RFIM_SHOW(ddr_data_rate_point_0, 1)
RFIM_SHOW(ddr_data_rate_point_1, 1)
RFIM_SHOW(ddr_data_rate_point_2, 1)
RFIM_SHOW(ddr_data_rate_point_3, 1)
RFIM_SHOW(rfi_disable, 1)

RFIM_STORE(rfi_restriction_run_busy, 1)
RFIM_STORE(rfi_restriction_err_code, 1)
RFIM_STORE(rfi_restriction_data_rate, 1)
RFIM_STORE(rfi_restriction_data_rate_base, 1)
RFIM_STORE(rfi_disable, 1)

static DEVICE_ATTR_RW(rfi_restriction_run_busy);
static DEVICE_ATTR_RW(rfi_restriction_err_code);
static DEVICE_ATTR_RW(rfi_restriction_data_rate);
static DEVICE_ATTR_RW(rfi_restriction_data_rate_base);
static DEVICE_ATTR_RO(ddr_data_rate_point_0);
static DEVICE_ATTR_RO(ddr_data_rate_point_1);
static DEVICE_ATTR_RO(ddr_data_rate_point_2);
static DEVICE_ATTR_RO(ddr_data_rate_point_3);
static DEVICE_ATTR_RW(rfi_disable);

static ssize_t rfi_restriction_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	u16 id = 0x0008;
	u32 input;
	int ret;

	ret = kstrtou32(buf, 10, &input);
	if (ret)
		return ret;

	ret = processor_thermal_send_mbox_write_cmd(to_pci_dev(dev), id, input);
	if (ret)
		return ret;

	return count;
}

static ssize_t rfi_restriction_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	u16 id = 0x0007;
	u64 resp;
	int ret;

	ret = processor_thermal_send_mbox_read_cmd(to_pci_dev(dev), id, &resp);
	if (ret)
		return ret;

	return sprintf(buf, "%llu\n", resp);
}

static ssize_t ddr_data_rate_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	u16 id = 0x0107;
	u64 resp;
	int ret;

	ret = processor_thermal_send_mbox_read_cmd(to_pci_dev(dev), id, &resp);
	if (ret)
		return ret;

	return sprintf(buf, "%llu\n", resp);
}

static DEVICE_ATTR_RW(rfi_restriction);
static DEVICE_ATTR_RO(ddr_data_rate);

static struct attribute *dvfs_attrs[] = {
	&dev_attr_rfi_restriction_run_busy.attr,
	&dev_attr_rfi_restriction_err_code.attr,
	&dev_attr_rfi_restriction_data_rate.attr,
	&dev_attr_rfi_restriction_data_rate_base.attr,
	&dev_attr_ddr_data_rate_point_0.attr,
	&dev_attr_ddr_data_rate_point_1.attr,
	&dev_attr_ddr_data_rate_point_2.attr,
	&dev_attr_ddr_data_rate_point_3.attr,
	&dev_attr_rfi_disable.attr,
	&dev_attr_ddr_data_rate.attr,
	&dev_attr_rfi_restriction.attr,
	NULL
};

static const struct attribute_group dvfs_attribute_group = {
	.attrs = dvfs_attrs,
	.name = "dvfs"
};

int proc_thermal_rfim_add(struct pci_dev *pdev, struct proc_thermal_device *proc_priv)
{
	int ret;

	if (proc_priv->mmio_feature_mask & PROC_THERMAL_FEATURE_FIVR) {
		ret = sysfs_create_group(&pdev->dev.kobj, &fivr_attribute_group);
		if (ret)
			return ret;
	}

	if (proc_priv->mmio_feature_mask & PROC_THERMAL_FEATURE_DLVR) {
		switch (pdev->device) {
		case PCI_DEVICE_ID_INTEL_LNLM_THERMAL:
		case PCI_DEVICE_ID_INTEL_PTL_THERMAL:
		case PCI_DEVICE_ID_INTEL_WCL_THERMAL:
			dlvr_mmio_regs_table = lnl_dlvr_mmio_regs;
			dlvr_mapping = lnl_dlvr_mapping;
			break;
		default:
			dlvr_mmio_regs_table = dlvr_mmio_regs;
			break;
		}
		ret = sysfs_create_group(&pdev->dev.kobj, &dlvr_attribute_group);
		if (ret)
			return ret;
	}

	if (proc_priv->mmio_feature_mask & PROC_THERMAL_FEATURE_DVFS) {
		ret = sysfs_create_group(&pdev->dev.kobj, &dvfs_attribute_group);
		if (ret && proc_priv->mmio_feature_mask & PROC_THERMAL_FEATURE_FIVR) {
			sysfs_remove_group(&pdev->dev.kobj, &fivr_attribute_group);
			return ret;
		}
		if (ret && proc_priv->mmio_feature_mask & PROC_THERMAL_FEATURE_DLVR) {
			sysfs_remove_group(&pdev->dev.kobj, &dlvr_attribute_group);
			return ret;
		}
	}

	return 0;
}
EXPORT_SYMBOL_GPL(proc_thermal_rfim_add);

void proc_thermal_rfim_remove(struct pci_dev *pdev)
{
	struct proc_thermal_device *proc_priv = pci_get_drvdata(pdev);

	if (proc_priv->mmio_feature_mask & PROC_THERMAL_FEATURE_FIVR)
		sysfs_remove_group(&pdev->dev.kobj, &fivr_attribute_group);

	if (proc_priv->mmio_feature_mask & PROC_THERMAL_FEATURE_DLVR)
		sysfs_remove_group(&pdev->dev.kobj, &dlvr_attribute_group);

	if (proc_priv->mmio_feature_mask & PROC_THERMAL_FEATURE_DVFS)
		sysfs_remove_group(&pdev->dev.kobj, &dvfs_attribute_group);
}
EXPORT_SYMBOL_GPL(proc_thermal_rfim_remove);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Processor Thermal RFIM Interface");
