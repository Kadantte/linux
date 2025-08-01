// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023-2024, Linaro Ltd.
 */

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/pm_runtime.h>

#include <dt-bindings/clock/qcom,sm8750-dispcc.h>

#include "common.h"
#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-regmap.h"
#include "clk-regmap-divider.h"
#include "clk-regmap-mux.h"
#include "reset.h"
#include "gdsc.h"

/* Need to match the order of clocks in DT binding */
enum {
	DT_BI_TCXO,
	DT_BI_TCXO_AO,
	DT_AHB_CLK,
	DT_SLEEP_CLK,

	DT_DSI0_PHY_PLL_OUT_BYTECLK,
	DT_DSI0_PHY_PLL_OUT_DSICLK,
	DT_DSI1_PHY_PLL_OUT_BYTECLK,
	DT_DSI1_PHY_PLL_OUT_DSICLK,

	DT_DP0_PHY_PLL_LINK_CLK,
	DT_DP0_PHY_PLL_VCO_DIV_CLK,
	DT_DP1_PHY_PLL_LINK_CLK,
	DT_DP1_PHY_PLL_VCO_DIV_CLK,
	DT_DP2_PHY_PLL_LINK_CLK,
	DT_DP2_PHY_PLL_VCO_DIV_CLK,
	DT_DP3_PHY_PLL_LINK_CLK,
	DT_DP3_PHY_PLL_VCO_DIV_CLK,
};

#define DISP_CC_MISC_CMD	0xF000

enum {
	P_BI_TCXO,
	P_DISP_CC_PLL0_OUT_MAIN,
	P_DISP_CC_PLL1_OUT_EVEN,
	P_DISP_CC_PLL1_OUT_MAIN,
	P_DISP_CC_PLL2_OUT_MAIN,
	P_DP0_PHY_PLL_LINK_CLK,
	P_DP0_PHY_PLL_VCO_DIV_CLK,
	P_DP1_PHY_PLL_LINK_CLK,
	P_DP1_PHY_PLL_VCO_DIV_CLK,
	P_DP2_PHY_PLL_LINK_CLK,
	P_DP2_PHY_PLL_VCO_DIV_CLK,
	P_DP3_PHY_PLL_LINK_CLK,
	P_DP3_PHY_PLL_VCO_DIV_CLK,
	P_DSI0_PHY_PLL_OUT_BYTECLK,
	P_DSI0_PHY_PLL_OUT_DSICLK,
	P_DSI1_PHY_PLL_OUT_BYTECLK,
	P_DSI1_PHY_PLL_OUT_DSICLK,
	P_SLEEP_CLK,
};

static const struct pll_vco pongo_elu_vco[] = {
	{ 38400000, 38400000, 0 },
};

static const struct pll_vco taycan_elu_vco[] = {
	{ 249600000, 2500000000, 0 },
};

static struct alpha_pll_config disp_cc_pll0_config = {
	.l = 0xd,
	.alpha = 0x6492,
	.config_ctl_val = 0x19660387,
	.config_ctl_hi_val = 0x098060a0,
	.config_ctl_hi1_val = 0xb416cb20,
	.user_ctl_val = 0x00000000,
	.user_ctl_hi_val = 0x00000002,
};

static struct clk_alpha_pll disp_cc_pll0 = {
	.offset = 0x0,
	.vco_table = taycan_elu_vco,
	.num_vco = ARRAY_SIZE(taycan_elu_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_TAYCAN_ELU],
	.clkr = {
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_pll0",
			.parent_data = &(const struct clk_parent_data) {
				.index = DT_BI_TCXO,
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_taycan_elu_ops,
		},
	},
};

static struct alpha_pll_config disp_cc_pll1_config = {
	.l = 0x1f,
	.alpha = 0x4000,
	.config_ctl_val = 0x19660387,
	.config_ctl_hi_val = 0x098060a0,
	.config_ctl_hi1_val = 0xb416cb20,
	.user_ctl_val = 0x00000000,
	.user_ctl_hi_val = 0x00000002,
};

static struct clk_alpha_pll disp_cc_pll1 = {
	.offset = 0x1000,
	.vco_table = taycan_elu_vco,
	.num_vco = ARRAY_SIZE(taycan_elu_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_TAYCAN_ELU],
	.clkr = {
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_pll1",
			.parent_data = &(const struct clk_parent_data) {
				.index = DT_BI_TCXO,
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_taycan_elu_ops,
		},
	},
};

static const struct alpha_pll_config disp_cc_pll2_config = {
	.l = 0x493,
	.alpha = 0x0,
	.config_ctl_val = 0x60000f68,
	.config_ctl_hi_val = 0x0001c808,
	.config_ctl_hi1_val = 0x00000000,
	.config_ctl_hi2_val = 0x040082f4,
	.test_ctl_val = 0x00008000,
	.test_ctl_hi_val = 0x0080c496,
	.test_ctl_hi1_val = 0x40100180,
	.test_ctl_hi2_val = 0x441001bc,
	.test_ctl_hi3_val = 0x002003d8,
	.user_ctl_val = 0x00000400,
	.user_ctl_hi_val = 0x00e50302,
};

static struct clk_alpha_pll disp_cc_pll2 = {
	.offset = 0x2000,
	.vco_table = pongo_elu_vco,
	.num_vco = ARRAY_SIZE(pongo_elu_vco),
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_PONGO_ELU],
	.clkr = {
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_pll2",
			.parent_data = &(const struct clk_parent_data) {
				.index = DT_SLEEP_CLK,
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_pongo_elu_ops,
		},
	},
};

static const struct parent_map disp_cc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
};

static const struct clk_parent_data disp_cc_parent_data_0[] = {
	{ .index = DT_BI_TCXO },
};

static const struct clk_parent_data disp_cc_parent_data_0_ao[] = {
	{ .index = DT_BI_TCXO_AO },
};

static const struct parent_map disp_cc_parent_map_1[] = {
	{ P_BI_TCXO, 0 },
	{ P_DSI0_PHY_PLL_OUT_DSICLK, 1 },
	{ P_DSI0_PHY_PLL_OUT_BYTECLK, 2 },
	{ P_DSI1_PHY_PLL_OUT_DSICLK, 3 },
	{ P_DSI1_PHY_PLL_OUT_BYTECLK, 4 },
};

static const struct clk_parent_data disp_cc_parent_data_1[] = {
	{ .index = DT_BI_TCXO },
	{ .index = DT_DSI0_PHY_PLL_OUT_DSICLK },
	{ .index = DT_DSI0_PHY_PLL_OUT_BYTECLK },
	{ .index = DT_DSI1_PHY_PLL_OUT_DSICLK },
	{ .index = DT_DSI1_PHY_PLL_OUT_BYTECLK },
};

static const struct parent_map disp_cc_parent_map_2[] = {
	{ P_BI_TCXO, 0 },
	{ P_DP3_PHY_PLL_VCO_DIV_CLK, 3 },
	{ P_DP1_PHY_PLL_VCO_DIV_CLK, 4 },
	{ P_DP2_PHY_PLL_VCO_DIV_CLK, 6 },
};

static const struct clk_parent_data disp_cc_parent_data_2[] = {
	{ .index = DT_BI_TCXO },
	{ .index = DT_DP3_PHY_PLL_VCO_DIV_CLK },
	{ .index = DT_DP1_PHY_PLL_VCO_DIV_CLK },
	{ .index = DT_DP2_PHY_PLL_VCO_DIV_CLK },
};

static const struct parent_map disp_cc_parent_map_3[] = {
	{ P_BI_TCXO, 0 },
	{ P_DP1_PHY_PLL_LINK_CLK, 2 },
	{ P_DP2_PHY_PLL_LINK_CLK, 3 },
	{ P_DP3_PHY_PLL_LINK_CLK, 4 },
};

static const struct clk_parent_data disp_cc_parent_data_3[] = {
	{ .index = DT_BI_TCXO },
	{ .index = DT_DP1_PHY_PLL_LINK_CLK },
	{ .index = DT_DP2_PHY_PLL_LINK_CLK },
	{ .index = DT_DP3_PHY_PLL_LINK_CLK },
};

static const struct parent_map disp_cc_parent_map_4[] = {
	{ P_BI_TCXO, 0 },
	{ P_DSI0_PHY_PLL_OUT_DSICLK, 1 },
	{ P_DISP_CC_PLL2_OUT_MAIN, 2 },
	{ P_DSI1_PHY_PLL_OUT_DSICLK, 3 },
};

static const struct clk_parent_data disp_cc_parent_data_4[] = {
	{ .index = DT_BI_TCXO },
	{ .index = DT_DSI0_PHY_PLL_OUT_DSICLK },
	{ .hw = &disp_cc_pll2.clkr.hw },
	{ .index = DT_DSI1_PHY_PLL_OUT_DSICLK },
};

static const struct parent_map disp_cc_parent_map_5[] = {
	{ P_BI_TCXO, 0 },
	{ P_DP0_PHY_PLL_LINK_CLK, 1 },
	{ P_DP0_PHY_PLL_VCO_DIV_CLK, 2 },
	{ P_DP3_PHY_PLL_VCO_DIV_CLK, 3 },
	{ P_DP1_PHY_PLL_VCO_DIV_CLK, 4 },
	{ P_DP2_PHY_PLL_VCO_DIV_CLK, 6 },
};

static const struct clk_parent_data disp_cc_parent_data_5[] = {
	{ .index = DT_BI_TCXO },
	{ .index = DT_DP0_PHY_PLL_LINK_CLK },
	{ .index = DT_DP0_PHY_PLL_VCO_DIV_CLK },
	{ .index = DT_DP3_PHY_PLL_VCO_DIV_CLK },
	{ .index = DT_DP1_PHY_PLL_VCO_DIV_CLK },
	{ .index = DT_DP2_PHY_PLL_VCO_DIV_CLK },
};

static const struct parent_map disp_cc_parent_map_6[] = {
	{ P_BI_TCXO, 0 },
	{ P_DSI0_PHY_PLL_OUT_BYTECLK, 2 },
	{ P_DSI1_PHY_PLL_OUT_BYTECLK, 4 },
};

static const struct clk_parent_data disp_cc_parent_data_6[] = {
	{ .index = DT_BI_TCXO },
	{ .index = DT_DSI0_PHY_PLL_OUT_BYTECLK },
	{ .index = DT_DSI1_PHY_PLL_OUT_BYTECLK },
};

static const struct parent_map disp_cc_parent_map_7[] = {
	{ P_BI_TCXO, 0 },
	{ P_DISP_CC_PLL1_OUT_MAIN, 4 },
	{ P_DISP_CC_PLL1_OUT_EVEN, 6 },
};

static const struct clk_parent_data disp_cc_parent_data_7[] = {
	{ .index = DT_BI_TCXO },
	{ .hw = &disp_cc_pll1.clkr.hw },
	{ .hw = &disp_cc_pll1.clkr.hw },
};

static const struct parent_map disp_cc_parent_map_8[] = {
	{ P_BI_TCXO, 0 },
	{ P_DP0_PHY_PLL_LINK_CLK, 1 },
	{ P_DP1_PHY_PLL_LINK_CLK, 2 },
	{ P_DP2_PHY_PLL_LINK_CLK, 3 },
	{ P_DP3_PHY_PLL_LINK_CLK, 4 },
};

static const struct clk_parent_data disp_cc_parent_data_8[] = {
	{ .index = DT_BI_TCXO },
	{ .index = DT_DP0_PHY_PLL_LINK_CLK },
	{ .index = DT_DP1_PHY_PLL_LINK_CLK },
	{ .index = DT_DP2_PHY_PLL_LINK_CLK },
	{ .index = DT_DP3_PHY_PLL_LINK_CLK },
};

static const struct parent_map disp_cc_parent_map_9[] = {
	{ P_BI_TCXO, 0 },
	{ P_DISP_CC_PLL0_OUT_MAIN, 1 },
	{ P_DISP_CC_PLL1_OUT_MAIN, 4 },
	{ P_DISP_CC_PLL1_OUT_EVEN, 6 },
};

static const struct clk_parent_data disp_cc_parent_data_9[] = {
	{ .index = DT_BI_TCXO },
	{ .hw = &disp_cc_pll0.clkr.hw },
	{ .hw = &disp_cc_pll1.clkr.hw },
	{ .hw = &disp_cc_pll1.clkr.hw },
};

static const struct parent_map disp_cc_parent_map_10[] = {
	{ P_BI_TCXO, 0 },
	{ P_DISP_CC_PLL2_OUT_MAIN, 2 },
};

static const struct clk_parent_data disp_cc_parent_data_10[] = {
	{ .index = DT_BI_TCXO },
	{ .hw = &disp_cc_pll2.clkr.hw },
};

static const struct parent_map disp_cc_parent_map_11[] = {
	{ P_SLEEP_CLK, 0 },
};

static const struct clk_parent_data disp_cc_parent_data_11[] = {
	{ .index = DT_SLEEP_CLK },
};

static const struct freq_tbl ftbl_disp_cc_esync0_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	{ }
};

static struct clk_rcg2 disp_cc_esync0_clk_src = {
	.cmd_rcgr = 0x80c0,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_4,
	.freq_tbl = ftbl_disp_cc_esync0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_esync0_clk_src",
		.parent_data = disp_cc_parent_data_4,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_4),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_byte2_ops,
	},
};

static struct clk_rcg2 disp_cc_esync1_clk_src = {
	.cmd_rcgr = 0x80d8,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_4,
	.freq_tbl = ftbl_disp_cc_esync0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_esync1_clk_src",
		.parent_data = disp_cc_parent_data_4,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_4),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_byte2_ops,
	},
};

static const struct freq_tbl ftbl_disp_cc_mdss_ahb_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(37500000, P_DISP_CC_PLL1_OUT_MAIN, 16, 0, 0),
	F(75000000, P_DISP_CC_PLL1_OUT_MAIN, 8, 0, 0),
	{ }
};

static struct clk_rcg2 disp_cc_mdss_ahb_clk_src = {
	.cmd_rcgr = 0x8360,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_7,
	.freq_tbl = ftbl_disp_cc_mdss_ahb_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_ahb_clk_src",
		.parent_data = disp_cc_parent_data_7,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_7),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_shared_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_byte0_clk_src = {
	.cmd_rcgr = 0x8180,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_1,
	.freq_tbl = ftbl_disp_cc_esync0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_byte0_clk_src",
		.parent_data = disp_cc_parent_data_1,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_1),
		.flags = CLK_SET_RATE_PARENT | CLK_OPS_PARENT_ENABLE,
		.ops = &clk_byte2_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_byte1_clk_src = {
	.cmd_rcgr = 0x819c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_1,
	.freq_tbl = ftbl_disp_cc_esync0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_byte1_clk_src",
		.parent_data = disp_cc_parent_data_1,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_1),
		.flags = CLK_SET_RATE_PARENT | CLK_OPS_PARENT_ENABLE,
		.ops = &clk_byte2_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_dptx0_aux_clk_src = {
	.cmd_rcgr = 0x8234,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_0,
	.freq_tbl = ftbl_disp_cc_esync0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_dptx0_aux_clk_src",
		.parent_data = disp_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_dptx0_link_clk_src = {
	.cmd_rcgr = 0x81e8,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_8,
	.freq_tbl = ftbl_disp_cc_esync0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_dptx0_link_clk_src",
		.parent_data = disp_cc_parent_data_8,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_8),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_byte2_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_dptx0_pixel0_clk_src = {
	.cmd_rcgr = 0x8204,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_5,
	.freq_tbl = ftbl_disp_cc_esync0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_dptx0_pixel0_clk_src",
		.parent_data = disp_cc_parent_data_5,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_5),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_dp_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_dptx0_pixel1_clk_src = {
	.cmd_rcgr = 0x821c,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_5,
	.freq_tbl = ftbl_disp_cc_esync0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_dptx0_pixel1_clk_src",
		.parent_data = disp_cc_parent_data_5,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_5),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_dp_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_dptx1_aux_clk_src = {
	.cmd_rcgr = 0x8298,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_0,
	.freq_tbl = ftbl_disp_cc_esync0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_dptx1_aux_clk_src",
		.parent_data = disp_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_dptx1_link_clk_src = {
	.cmd_rcgr = 0x827c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_3,
	.freq_tbl = ftbl_disp_cc_esync0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_dptx1_link_clk_src",
		.parent_data = disp_cc_parent_data_3,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_3),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_byte2_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_dptx1_pixel0_clk_src = {
	.cmd_rcgr = 0x824c,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_2,
	.freq_tbl = ftbl_disp_cc_esync0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_dptx1_pixel0_clk_src",
		.parent_data = disp_cc_parent_data_2,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_2),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_dp_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_dptx1_pixel1_clk_src = {
	.cmd_rcgr = 0x8264,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_2,
	.freq_tbl = ftbl_disp_cc_esync0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_dptx1_pixel1_clk_src",
		.parent_data = disp_cc_parent_data_2,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_2),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_dp_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_dptx2_aux_clk_src = {
	.cmd_rcgr = 0x82fc,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_0,
	.freq_tbl = ftbl_disp_cc_esync0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_dptx2_aux_clk_src",
		.parent_data = disp_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_dptx2_link_clk_src = {
	.cmd_rcgr = 0x82b0,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_3,
	.freq_tbl = ftbl_disp_cc_esync0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_dptx2_link_clk_src",
		.parent_data = disp_cc_parent_data_3,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_3),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_byte2_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_dptx2_pixel0_clk_src = {
	.cmd_rcgr = 0x82cc,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_2,
	.freq_tbl = ftbl_disp_cc_esync0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_dptx2_pixel0_clk_src",
		.parent_data = disp_cc_parent_data_2,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_2),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_dp_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_dptx2_pixel1_clk_src = {
	.cmd_rcgr = 0x82e4,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_2,
	.freq_tbl = ftbl_disp_cc_esync0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_dptx2_pixel1_clk_src",
		.parent_data = disp_cc_parent_data_2,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_2),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_dp_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_dptx3_aux_clk_src = {
	.cmd_rcgr = 0x8348,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_0,
	.freq_tbl = ftbl_disp_cc_esync0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_dptx3_aux_clk_src",
		.parent_data = disp_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_dptx3_link_clk_src = {
	.cmd_rcgr = 0x832c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_3,
	.freq_tbl = ftbl_disp_cc_esync0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_dptx3_link_clk_src",
		.parent_data = disp_cc_parent_data_3,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_3),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_byte2_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_dptx3_pixel0_clk_src = {
	.cmd_rcgr = 0x8314,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_2,
	.freq_tbl = ftbl_disp_cc_esync0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_dptx3_pixel0_clk_src",
		.parent_data = disp_cc_parent_data_2,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_2),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_dp_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_esc0_clk_src = {
	.cmd_rcgr = 0x81b8,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_6,
	.freq_tbl = ftbl_disp_cc_esync0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_esc0_clk_src",
		.parent_data = disp_cc_parent_data_6,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_6),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_shared_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_esc1_clk_src = {
	.cmd_rcgr = 0x81d0,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_6,
	.freq_tbl = ftbl_disp_cc_esync0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_esc1_clk_src",
		.parent_data = disp_cc_parent_data_6,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_6),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_shared_ops,
	},
};

static const struct freq_tbl ftbl_disp_cc_mdss_mdp_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(85714286, P_DISP_CC_PLL0_OUT_MAIN, 3, 0, 0),
	F(100000000, P_DISP_CC_PLL0_OUT_MAIN, 3, 0, 0),
	F(156000000, P_DISP_CC_PLL0_OUT_MAIN, 3, 0, 0),
	F(207000000, P_DISP_CC_PLL0_OUT_MAIN, 3, 0, 0),
	F(337000000, P_DISP_CC_PLL0_OUT_MAIN, 3, 0, 0),
	F(417000000, P_DISP_CC_PLL0_OUT_MAIN, 3, 0, 0),
	F(532000000, P_DISP_CC_PLL0_OUT_MAIN, 3, 0, 0),
	F(575000000, P_DISP_CC_PLL0_OUT_MAIN, 3, 0, 0),
	{ }
};

static struct clk_rcg2 disp_cc_mdss_mdp_clk_src = {
	.cmd_rcgr = 0x8150,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_9,
	.freq_tbl = ftbl_disp_cc_mdss_mdp_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_mdp_clk_src",
		.parent_data = disp_cc_parent_data_9,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_9),
		.flags = CLK_SET_RATE_PARENT,
		/*
		 * TODO: Downstream does not manage the clock directly, but
		 * places votes via new hardware block called "cesta".
		 * It is not clear whether such approach should be taken instead
		 * of manual control.
		 */
		.ops = &clk_rcg2_shared_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_pclk0_clk_src = {
	.cmd_rcgr = 0x8108,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_1,
	.freq_tbl = ftbl_disp_cc_esync0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_pclk0_clk_src",
		.parent_data = disp_cc_parent_data_1,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_1),
		.flags = CLK_SET_RATE_PARENT | CLK_OPS_PARENT_ENABLE,
		.ops = &clk_pixel_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_pclk1_clk_src = {
	.cmd_rcgr = 0x8120,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_1,
	.freq_tbl = ftbl_disp_cc_esync0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_pclk1_clk_src",
		.parent_data = disp_cc_parent_data_1,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_1),
		.flags = CLK_SET_RATE_PARENT | CLK_OPS_PARENT_ENABLE,
		.ops = &clk_pixel_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_pclk2_clk_src = {
	.cmd_rcgr = 0x8138,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_1,
	.freq_tbl = ftbl_disp_cc_esync0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_pclk2_clk_src",
		.parent_data = disp_cc_parent_data_1,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_1),
		.flags = CLK_SET_RATE_PARENT | CLK_OPS_PARENT_ENABLE,
		.ops = &clk_pixel_ops,
	},
};

static struct clk_rcg2 disp_cc_mdss_vsync_clk_src = {
	.cmd_rcgr = 0x8168,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_0,
	.freq_tbl = ftbl_disp_cc_esync0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_vsync_clk_src",
		.parent_data = disp_cc_parent_data_0,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
};

static const struct freq_tbl ftbl_disp_cc_osc_clk_src[] = {
	F(38400000, P_DISP_CC_PLL2_OUT_MAIN, 1, 0, 0),
	{ }
};

static struct clk_rcg2 disp_cc_osc_clk_src = {
	.cmd_rcgr = 0x80f0,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_10,
	.freq_tbl = ftbl_disp_cc_osc_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_osc_clk_src",
		.parent_data = disp_cc_parent_data_10,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_10),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
};

static const struct freq_tbl ftbl_disp_cc_sleep_clk_src[] = {
	F(32000, P_SLEEP_CLK, 1, 0, 0),
	{ }
};

static struct clk_rcg2 disp_cc_sleep_clk_src = {
	.cmd_rcgr = 0xe064,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_11,
	.freq_tbl = ftbl_disp_cc_sleep_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_sleep_clk_src",
		.parent_data = disp_cc_parent_data_11,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_11),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_rcg2 disp_cc_xo_clk_src = {
	.cmd_rcgr = 0xe044,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = disp_cc_parent_map_0,
	.freq_tbl = ftbl_disp_cc_esync0_clk_src,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_xo_clk_src",
		.parent_data = disp_cc_parent_data_0_ao,
		.num_parents = ARRAY_SIZE(disp_cc_parent_data_0_ao),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_ops,
	},
};

static struct clk_regmap_div disp_cc_mdss_byte0_div_clk_src = {
	.reg = 0x8198,
	.shift = 0,
	.width = 4,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_byte0_div_clk_src",
		.parent_hws = (const struct clk_hw*[]) {
			&disp_cc_mdss_byte0_clk_src.clkr.hw,
		},
		.num_parents = 1,
		.ops = &clk_regmap_div_ops,
	},
};

static struct clk_regmap_div disp_cc_mdss_byte1_div_clk_src = {
	.reg = 0x81b4,
	.shift = 0,
	.width = 4,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_byte1_div_clk_src",
		.parent_hws = (const struct clk_hw*[]) {
			&disp_cc_mdss_byte1_clk_src.clkr.hw,
		},
		.num_parents = 1,
		.ops = &clk_regmap_div_ops,
	},
};

static struct clk_regmap_div disp_cc_mdss_dptx0_link_div_clk_src = {
	.reg = 0x8200,
	.shift = 0,
	.width = 4,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_dptx0_link_div_clk_src",
		.parent_hws = (const struct clk_hw*[]) {
			&disp_cc_mdss_dptx0_link_clk_src.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_regmap_div disp_cc_mdss_dptx1_link_div_clk_src = {
	.reg = 0x8294,
	.shift = 0,
	.width = 4,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_dptx1_link_div_clk_src",
		.parent_hws = (const struct clk_hw*[]) {
			&disp_cc_mdss_dptx1_link_clk_src.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_regmap_div disp_cc_mdss_dptx2_link_div_clk_src = {
	.reg = 0x82c8,
	.shift = 0,
	.width = 4,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_dptx2_link_div_clk_src",
		.parent_hws = (const struct clk_hw*[]) {
			&disp_cc_mdss_dptx2_link_clk_src.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_regmap_div disp_cc_mdss_dptx3_link_div_clk_src = {
	.reg = 0x8344,
	.shift = 0,
	.width = 4,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "disp_cc_mdss_dptx3_link_div_clk_src",
		.parent_hws = (const struct clk_hw*[]) {
			&disp_cc_mdss_dptx3_link_clk_src.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_branch disp_cc_esync0_clk = {
	.halt_reg = 0x80b8,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x80b8,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_esync0_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_esync0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_esync1_clk = {
	.halt_reg = 0x80bc,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x80bc,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_esync1_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_esync1_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_accu_shift_clk = {
	.halt_reg = 0xe060,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0xe060,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_accu_shift_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_xo_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_ahb1_clk = {
	.halt_reg = 0xa028,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xa028,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_ahb1_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_ahb_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_ahb_clk = {
	.halt_reg = 0x80b0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x80b0,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_ahb_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_ahb_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_byte0_clk = {
	.halt_reg = 0x8034,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8034,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_byte0_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_byte0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_byte0_intf_clk = {
	.halt_reg = 0x8038,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8038,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_byte0_intf_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_byte0_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_byte1_clk = {
	.halt_reg = 0x803c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x803c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_byte1_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_byte1_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_byte1_intf_clk = {
	.halt_reg = 0x8040,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8040,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_byte1_intf_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_byte1_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dptx0_aux_clk = {
	.halt_reg = 0x8064,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8064,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_dptx0_aux_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_dptx0_aux_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dptx0_crypto_clk = {
	.halt_reg = 0x8058,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8058,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_dptx0_crypto_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_dptx0_link_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dptx0_link_clk = {
	.halt_reg = 0x804c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x804c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_dptx0_link_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_dptx0_link_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dptx0_link_intf_clk = {
	.halt_reg = 0x8054,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8054,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_dptx0_link_intf_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_dptx0_link_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dptx0_pixel0_clk = {
	.halt_reg = 0x805c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x805c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_dptx0_pixel0_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_dptx0_pixel0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dptx0_pixel1_clk = {
	.halt_reg = 0x8060,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8060,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_dptx0_pixel1_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_dptx0_pixel1_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dptx0_usb_router_link_intf_clk = {
	.halt_reg = 0x8050,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8050,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_dptx0_usb_router_link_intf_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_dptx0_link_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dptx1_aux_clk = {
	.halt_reg = 0x8080,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8080,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_dptx1_aux_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_dptx1_aux_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dptx1_crypto_clk = {
	.halt_reg = 0x807c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x807c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_dptx1_crypto_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_dptx1_link_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dptx1_link_clk = {
	.halt_reg = 0x8070,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8070,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_dptx1_link_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_dptx1_link_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dptx1_link_intf_clk = {
	.halt_reg = 0x8078,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8078,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_dptx1_link_intf_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_dptx1_link_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dptx1_pixel0_clk = {
	.halt_reg = 0x8068,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8068,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_dptx1_pixel0_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_dptx1_pixel0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dptx1_pixel1_clk = {
	.halt_reg = 0x806c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x806c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_dptx1_pixel1_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_dptx1_pixel1_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dptx1_usb_router_link_intf_clk = {
	.halt_reg = 0x8074,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8074,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_dptx1_usb_router_link_intf_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_dptx1_link_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dptx2_aux_clk = {
	.halt_reg = 0x8098,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8098,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_dptx2_aux_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_dptx2_aux_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dptx2_crypto_clk = {
	.halt_reg = 0x8094,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8094,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_dptx2_crypto_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_dptx2_link_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dptx2_link_clk = {
	.halt_reg = 0x808c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x808c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_dptx2_link_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_dptx2_link_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dptx2_link_intf_clk = {
	.halt_reg = 0x8090,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8090,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_dptx2_link_intf_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_dptx2_link_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dptx2_pixel0_clk = {
	.halt_reg = 0x8084,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8084,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_dptx2_pixel0_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_dptx2_pixel0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dptx2_pixel1_clk = {
	.halt_reg = 0x8088,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8088,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_dptx2_pixel1_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_dptx2_pixel1_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dptx3_aux_clk = {
	.halt_reg = 0x80a8,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x80a8,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_dptx3_aux_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_dptx3_aux_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dptx3_crypto_clk = {
	.halt_reg = 0x80ac,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x80ac,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_dptx3_crypto_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_dptx3_link_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dptx3_link_clk = {
	.halt_reg = 0x80a0,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x80a0,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_dptx3_link_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_dptx3_link_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dptx3_link_intf_clk = {
	.halt_reg = 0x80a4,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x80a4,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_dptx3_link_intf_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_dptx3_link_div_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_dptx3_pixel0_clk = {
	.halt_reg = 0x809c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x809c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_dptx3_pixel0_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_dptx3_pixel0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_esc0_clk = {
	.halt_reg = 0x8044,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8044,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_esc0_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_esc0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_esc1_clk = {
	.halt_reg = 0x8048,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8048,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_esc1_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_esc1_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_mdp1_clk = {
	.halt_reg = 0xa004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xa004,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_mdp1_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_mdp_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_mdp_clk = {
	.halt_reg = 0x8010,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8010,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_mdp_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_mdp_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_aon_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_mdp_lut1_clk = {
	.halt_reg = 0xa014,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xa014,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_mdp_lut1_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_mdp_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_mdp_lut_clk = {
	.halt_reg = 0x8020,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x8020,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_mdp_lut_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_mdp_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_non_gdsc_ahb_clk = {
	.halt_reg = 0xc004,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0xc004,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_non_gdsc_ahb_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_ahb_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_pclk0_clk = {
	.halt_reg = 0x8004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8004,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_pclk0_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_pclk0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_pclk1_clk = {
	.halt_reg = 0x8008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8008,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_pclk1_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_pclk1_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_pclk2_clk = {
	.halt_reg = 0x800c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x800c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_pclk2_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_pclk2_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_vsync1_clk = {
	.halt_reg = 0xa024,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0xa024,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_vsync1_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_vsync_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_mdss_vsync_clk = {
	.halt_reg = 0x8030,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x8030,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_mdss_vsync_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_mdss_vsync_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch disp_cc_osc_clk = {
	.halt_reg = 0x80b4,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x80b4,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "disp_cc_osc_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&disp_cc_osc_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct gdsc mdss_gdsc = {
	.gdscr = 0x9000,
	.en_rest_wait_val = 0x2,
	.en_few_wait_val = 0x2,
	.clk_dis_wait_val = 0xf,
	.pd = {
		.name = "mdss_gdsc",
	},
	.pwrsts = PWRSTS_OFF_ON,
	.flags = POLL_CFG_GDSCR | HW_CTRL | RETAIN_FF_ENABLE,
};

static struct gdsc mdss_int2_gdsc = {
	.gdscr = 0xb000,
	.en_rest_wait_val = 0x2,
	.en_few_wait_val = 0x2,
	.clk_dis_wait_val = 0xf,
	.pd = {
		.name = "mdss_int2_gdsc",
	},
	.pwrsts = PWRSTS_OFF_ON,
	.flags = POLL_CFG_GDSCR | HW_CTRL | RETAIN_FF_ENABLE,
};

static struct clk_regmap *disp_cc_sm8750_clocks[] = {
	[DISP_CC_ESYNC0_CLK] = &disp_cc_esync0_clk.clkr,
	[DISP_CC_ESYNC0_CLK_SRC] = &disp_cc_esync0_clk_src.clkr,
	[DISP_CC_ESYNC1_CLK] = &disp_cc_esync1_clk.clkr,
	[DISP_CC_ESYNC1_CLK_SRC] = &disp_cc_esync1_clk_src.clkr,
	[DISP_CC_MDSS_ACCU_SHIFT_CLK] = &disp_cc_mdss_accu_shift_clk.clkr,
	[DISP_CC_MDSS_AHB1_CLK] = &disp_cc_mdss_ahb1_clk.clkr,
	[DISP_CC_MDSS_AHB_CLK] = &disp_cc_mdss_ahb_clk.clkr,
	[DISP_CC_MDSS_AHB_CLK_SRC] = &disp_cc_mdss_ahb_clk_src.clkr,
	[DISP_CC_MDSS_BYTE0_CLK] = &disp_cc_mdss_byte0_clk.clkr,
	[DISP_CC_MDSS_BYTE0_CLK_SRC] = &disp_cc_mdss_byte0_clk_src.clkr,
	[DISP_CC_MDSS_BYTE0_DIV_CLK_SRC] = &disp_cc_mdss_byte0_div_clk_src.clkr,
	[DISP_CC_MDSS_BYTE0_INTF_CLK] = &disp_cc_mdss_byte0_intf_clk.clkr,
	[DISP_CC_MDSS_BYTE1_CLK] = &disp_cc_mdss_byte1_clk.clkr,
	[DISP_CC_MDSS_BYTE1_CLK_SRC] = &disp_cc_mdss_byte1_clk_src.clkr,
	[DISP_CC_MDSS_BYTE1_DIV_CLK_SRC] = &disp_cc_mdss_byte1_div_clk_src.clkr,
	[DISP_CC_MDSS_BYTE1_INTF_CLK] = &disp_cc_mdss_byte1_intf_clk.clkr,
	[DISP_CC_MDSS_DPTX0_AUX_CLK] = &disp_cc_mdss_dptx0_aux_clk.clkr,
	[DISP_CC_MDSS_DPTX0_AUX_CLK_SRC] = &disp_cc_mdss_dptx0_aux_clk_src.clkr,
	[DISP_CC_MDSS_DPTX0_CRYPTO_CLK] = &disp_cc_mdss_dptx0_crypto_clk.clkr,
	[DISP_CC_MDSS_DPTX0_LINK_CLK] = &disp_cc_mdss_dptx0_link_clk.clkr,
	[DISP_CC_MDSS_DPTX0_LINK_CLK_SRC] = &disp_cc_mdss_dptx0_link_clk_src.clkr,
	[DISP_CC_MDSS_DPTX0_LINK_DIV_CLK_SRC] = &disp_cc_mdss_dptx0_link_div_clk_src.clkr,
	[DISP_CC_MDSS_DPTX0_LINK_INTF_CLK] = &disp_cc_mdss_dptx0_link_intf_clk.clkr,
	[DISP_CC_MDSS_DPTX0_PIXEL0_CLK] = &disp_cc_mdss_dptx0_pixel0_clk.clkr,
	[DISP_CC_MDSS_DPTX0_PIXEL0_CLK_SRC] = &disp_cc_mdss_dptx0_pixel0_clk_src.clkr,
	[DISP_CC_MDSS_DPTX0_PIXEL1_CLK] = &disp_cc_mdss_dptx0_pixel1_clk.clkr,
	[DISP_CC_MDSS_DPTX0_PIXEL1_CLK_SRC] = &disp_cc_mdss_dptx0_pixel1_clk_src.clkr,
	[DISP_CC_MDSS_DPTX0_USB_ROUTER_LINK_INTF_CLK] =
		&disp_cc_mdss_dptx0_usb_router_link_intf_clk.clkr,
	[DISP_CC_MDSS_DPTX1_AUX_CLK] = &disp_cc_mdss_dptx1_aux_clk.clkr,
	[DISP_CC_MDSS_DPTX1_AUX_CLK_SRC] = &disp_cc_mdss_dptx1_aux_clk_src.clkr,
	[DISP_CC_MDSS_DPTX1_CRYPTO_CLK] = &disp_cc_mdss_dptx1_crypto_clk.clkr,
	[DISP_CC_MDSS_DPTX1_LINK_CLK] = &disp_cc_mdss_dptx1_link_clk.clkr,
	[DISP_CC_MDSS_DPTX1_LINK_CLK_SRC] = &disp_cc_mdss_dptx1_link_clk_src.clkr,
	[DISP_CC_MDSS_DPTX1_LINK_DIV_CLK_SRC] = &disp_cc_mdss_dptx1_link_div_clk_src.clkr,
	[DISP_CC_MDSS_DPTX1_LINK_INTF_CLK] = &disp_cc_mdss_dptx1_link_intf_clk.clkr,
	[DISP_CC_MDSS_DPTX1_PIXEL0_CLK] = &disp_cc_mdss_dptx1_pixel0_clk.clkr,
	[DISP_CC_MDSS_DPTX1_PIXEL0_CLK_SRC] = &disp_cc_mdss_dptx1_pixel0_clk_src.clkr,
	[DISP_CC_MDSS_DPTX1_PIXEL1_CLK] = &disp_cc_mdss_dptx1_pixel1_clk.clkr,
	[DISP_CC_MDSS_DPTX1_PIXEL1_CLK_SRC] = &disp_cc_mdss_dptx1_pixel1_clk_src.clkr,
	[DISP_CC_MDSS_DPTX1_USB_ROUTER_LINK_INTF_CLK] =
		&disp_cc_mdss_dptx1_usb_router_link_intf_clk.clkr,
	[DISP_CC_MDSS_DPTX2_AUX_CLK] = &disp_cc_mdss_dptx2_aux_clk.clkr,
	[DISP_CC_MDSS_DPTX2_AUX_CLK_SRC] = &disp_cc_mdss_dptx2_aux_clk_src.clkr,
	[DISP_CC_MDSS_DPTX2_CRYPTO_CLK] = &disp_cc_mdss_dptx2_crypto_clk.clkr,
	[DISP_CC_MDSS_DPTX2_LINK_CLK] = &disp_cc_mdss_dptx2_link_clk.clkr,
	[DISP_CC_MDSS_DPTX2_LINK_CLK_SRC] = &disp_cc_mdss_dptx2_link_clk_src.clkr,
	[DISP_CC_MDSS_DPTX2_LINK_DIV_CLK_SRC] = &disp_cc_mdss_dptx2_link_div_clk_src.clkr,
	[DISP_CC_MDSS_DPTX2_LINK_INTF_CLK] = &disp_cc_mdss_dptx2_link_intf_clk.clkr,
	[DISP_CC_MDSS_DPTX2_PIXEL0_CLK] = &disp_cc_mdss_dptx2_pixel0_clk.clkr,
	[DISP_CC_MDSS_DPTX2_PIXEL0_CLK_SRC] = &disp_cc_mdss_dptx2_pixel0_clk_src.clkr,
	[DISP_CC_MDSS_DPTX2_PIXEL1_CLK] = &disp_cc_mdss_dptx2_pixel1_clk.clkr,
	[DISP_CC_MDSS_DPTX2_PIXEL1_CLK_SRC] = &disp_cc_mdss_dptx2_pixel1_clk_src.clkr,
	[DISP_CC_MDSS_DPTX3_AUX_CLK] = &disp_cc_mdss_dptx3_aux_clk.clkr,
	[DISP_CC_MDSS_DPTX3_AUX_CLK_SRC] = &disp_cc_mdss_dptx3_aux_clk_src.clkr,
	[DISP_CC_MDSS_DPTX3_CRYPTO_CLK] = &disp_cc_mdss_dptx3_crypto_clk.clkr,
	[DISP_CC_MDSS_DPTX3_LINK_CLK] = &disp_cc_mdss_dptx3_link_clk.clkr,
	[DISP_CC_MDSS_DPTX3_LINK_CLK_SRC] = &disp_cc_mdss_dptx3_link_clk_src.clkr,
	[DISP_CC_MDSS_DPTX3_LINK_DIV_CLK_SRC] = &disp_cc_mdss_dptx3_link_div_clk_src.clkr,
	[DISP_CC_MDSS_DPTX3_LINK_INTF_CLK] = &disp_cc_mdss_dptx3_link_intf_clk.clkr,
	[DISP_CC_MDSS_DPTX3_PIXEL0_CLK] = &disp_cc_mdss_dptx3_pixel0_clk.clkr,
	[DISP_CC_MDSS_DPTX3_PIXEL0_CLK_SRC] = &disp_cc_mdss_dptx3_pixel0_clk_src.clkr,
	[DISP_CC_MDSS_ESC0_CLK] = &disp_cc_mdss_esc0_clk.clkr,
	[DISP_CC_MDSS_ESC0_CLK_SRC] = &disp_cc_mdss_esc0_clk_src.clkr,
	[DISP_CC_MDSS_ESC1_CLK] = &disp_cc_mdss_esc1_clk.clkr,
	[DISP_CC_MDSS_ESC1_CLK_SRC] = &disp_cc_mdss_esc1_clk_src.clkr,
	[DISP_CC_MDSS_MDP1_CLK] = &disp_cc_mdss_mdp1_clk.clkr,
	[DISP_CC_MDSS_MDP_CLK] = &disp_cc_mdss_mdp_clk.clkr,
	[DISP_CC_MDSS_MDP_CLK_SRC] = &disp_cc_mdss_mdp_clk_src.clkr,
	[DISP_CC_MDSS_MDP_LUT1_CLK] = &disp_cc_mdss_mdp_lut1_clk.clkr,
	[DISP_CC_MDSS_MDP_LUT_CLK] = &disp_cc_mdss_mdp_lut_clk.clkr,
	[DISP_CC_MDSS_NON_GDSC_AHB_CLK] = &disp_cc_mdss_non_gdsc_ahb_clk.clkr,
	[DISP_CC_MDSS_PCLK0_CLK] = &disp_cc_mdss_pclk0_clk.clkr,
	[DISP_CC_MDSS_PCLK0_CLK_SRC] = &disp_cc_mdss_pclk0_clk_src.clkr,
	[DISP_CC_MDSS_PCLK1_CLK] = &disp_cc_mdss_pclk1_clk.clkr,
	[DISP_CC_MDSS_PCLK1_CLK_SRC] = &disp_cc_mdss_pclk1_clk_src.clkr,
	[DISP_CC_MDSS_PCLK2_CLK] = &disp_cc_mdss_pclk2_clk.clkr,
	[DISP_CC_MDSS_PCLK2_CLK_SRC] = &disp_cc_mdss_pclk2_clk_src.clkr,
	[DISP_CC_MDSS_VSYNC1_CLK] = &disp_cc_mdss_vsync1_clk.clkr,
	[DISP_CC_MDSS_VSYNC_CLK] = &disp_cc_mdss_vsync_clk.clkr,
	[DISP_CC_MDSS_VSYNC_CLK_SRC] = &disp_cc_mdss_vsync_clk_src.clkr,
	[DISP_CC_OSC_CLK] = &disp_cc_osc_clk.clkr,
	[DISP_CC_OSC_CLK_SRC] = &disp_cc_osc_clk_src.clkr,
	[DISP_CC_PLL0] = &disp_cc_pll0.clkr,
	[DISP_CC_PLL1] = &disp_cc_pll1.clkr,
	[DISP_CC_PLL2] = &disp_cc_pll2.clkr,
	[DISP_CC_SLEEP_CLK_SRC] = &disp_cc_sleep_clk_src.clkr,
	[DISP_CC_XO_CLK_SRC] = &disp_cc_xo_clk_src.clkr,
};

static const struct qcom_reset_map disp_cc_sm8750_resets[] = {
	[DISP_CC_MDSS_CORE_BCR] = { 0x8000 },
	[DISP_CC_MDSS_CORE_INT2_BCR] = { 0xa000 },
	[DISP_CC_MDSS_RSCC_BCR] = { 0xc000 },
};

static struct gdsc *disp_cc_sm8750_gdscs[] = {
	[MDSS_GDSC] = &mdss_gdsc,
	[MDSS_INT2_GDSC] = &mdss_int2_gdsc,
};

static const struct regmap_config disp_cc_sm8750_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0xf004, /* 0x10000, 0x10004 and maybe others are for TZ */
	.fast_io = true,
};

static const struct qcom_cc_desc disp_cc_sm8750_desc = {
	.config = &disp_cc_sm8750_regmap_config,
	.clks = disp_cc_sm8750_clocks,
	.num_clks = ARRAY_SIZE(disp_cc_sm8750_clocks),
	.resets = disp_cc_sm8750_resets,
	.num_resets = ARRAY_SIZE(disp_cc_sm8750_resets),
	.gdscs = disp_cc_sm8750_gdscs,
	.num_gdscs = ARRAY_SIZE(disp_cc_sm8750_gdscs),
};

static const struct of_device_id disp_cc_sm8750_match_table[] = {
	{ .compatible = "qcom,sm8750-dispcc" },
	{ }
};
MODULE_DEVICE_TABLE(of, disp_cc_sm8750_match_table);

static int disp_cc_sm8750_probe(struct platform_device *pdev)
{
	struct regmap *regmap;
	int ret;

	ret = devm_pm_runtime_enable(&pdev->dev);
	if (ret)
		return ret;

	ret = pm_runtime_resume_and_get(&pdev->dev);
	if (ret)
		return ret;

	regmap = qcom_cc_map(pdev, &disp_cc_sm8750_desc);
	if (IS_ERR(regmap)) {
		ret = PTR_ERR(regmap);
		goto err_put_rpm;
	}

	clk_taycan_elu_pll_configure(&disp_cc_pll0, regmap, &disp_cc_pll0_config);
	clk_taycan_elu_pll_configure(&disp_cc_pll1, regmap, &disp_cc_pll1_config);
	clk_pongo_elu_pll_configure(&disp_cc_pll2, regmap, &disp_cc_pll2_config);

	/* Enable clock gating for MDP clocks */
	regmap_update_bits(regmap, DISP_CC_MISC_CMD, 0x10, 0x10);

	/* Keep some clocks always-on */
	qcom_branch_set_clk_en(regmap, 0xe07c); /* DISP_CC_SLEEP_CLK */
	qcom_branch_set_clk_en(regmap, 0xe05c); /* DISP_CC_XO_CLK */
	qcom_branch_set_clk_en(regmap, 0xc00c); /* DISP_CC_MDSS_RSCC_AHB_CLK */
	qcom_branch_set_clk_en(regmap, 0xc008); /* DISP_CC_MDSS_RSCC_VSYNC_CLK */

	ret = qcom_cc_really_probe(&pdev->dev, &disp_cc_sm8750_desc, regmap);
	if (ret)
		goto err_put_rpm;

	pm_runtime_put(&pdev->dev);

	return 0;

err_put_rpm:
	pm_runtime_put_sync(&pdev->dev);

	return ret;
}

static struct platform_driver disp_cc_sm8750_driver = {
	.probe = disp_cc_sm8750_probe,
	.driver = {
		.name = "disp_cc-sm8750",
		.of_match_table = disp_cc_sm8750_match_table,
	},
};

module_platform_driver(disp_cc_sm8750_driver);

MODULE_DESCRIPTION("QTI DISPCC SM8750 Driver");
MODULE_LICENSE("GPL");
