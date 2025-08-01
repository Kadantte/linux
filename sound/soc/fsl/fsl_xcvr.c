// SPDX-License-Identifier: GPL-2.0
// Copyright 2019 NXP

#include <linux/bitrev.h>
#include <linux/clk.h>
#include <linux/firmware.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <sound/dmaengine_pcm.h>
#include <sound/pcm_iec958.h>
#include <sound/pcm_params.h>

#include "fsl_xcvr.h"
#include "fsl_utils.h"
#include "imx-pcm.h"

#define FSL_XCVR_CAPDS_SIZE	256
#define SPDIF_NUM_RATES		7

enum fsl_xcvr_pll_verison {
	PLL_MX8MP,
	PLL_MX95,
};

struct fsl_xcvr_soc_data {
	const char *fw_name;
	bool spdif_only;
	bool use_edma;
	bool use_phy;
	enum fsl_xcvr_pll_verison pll_ver;
};

struct fsl_xcvr {
	const struct fsl_xcvr_soc_data *soc_data;
	struct platform_device *pdev;
	struct regmap *regmap;
	struct regmap *regmap_phy;
	struct regmap *regmap_pll;
	struct clk *ipg_clk;
	struct clk *pll_ipg_clk;
	struct clk *phy_clk;
	struct clk *spba_clk;
	struct clk *pll8k_clk;
	struct clk *pll11k_clk;
	struct reset_control *reset;
	u8 streams;
	u32 mode;
	u32 arc_mode;
	void __iomem *ram_addr;
	struct snd_dmaengine_dai_dma_data dma_prms_rx;
	struct snd_dmaengine_dai_dma_data dma_prms_tx;
	struct snd_aes_iec958 rx_iec958;
	struct snd_aes_iec958 tx_iec958;
	u8 cap_ds[FSL_XCVR_CAPDS_SIZE];
	struct work_struct work_rst;
	spinlock_t lock; /* Protect hw_reset and trigger */
	struct snd_pcm_hw_constraint_list spdif_constr_rates;
	u32 spdif_constr_rates_list[SPDIF_NUM_RATES];
};

static const struct fsl_xcvr_pll_conf {
	u8 mfi;   /* min=0x18, max=0x38 */
	u32 mfn;  /* signed int, 2's compl., min=0x3FFF0000, max=0x00010000 */
	u32 mfd;  /* unsigned int */
	u32 fout; /* Fout = Fref*(MFI + MFN/MFD), Fref is 24MHz */
} fsl_xcvr_pll_cfg[] = {
	{ .mfi = 54, .mfn = 1,  .mfd = 6,   .fout = 1300000000, }, /* 1.3 GHz */
	{ .mfi = 32, .mfn = 96, .mfd = 125, .fout = 786432000, },  /* 8000 Hz */
	{ .mfi = 30, .mfn = 66, .mfd = 625, .fout = 722534400, },  /* 11025 Hz */
	{ .mfi = 29, .mfn = 1,  .mfd = 6,   .fout = 700000000, },  /* 700 MHz */
};

/*
 * HDMI2.1 spec defines 6- and 12-channels layout for one bit audio
 * stream. Todo: to check how this case can be considered below
 */
static const u32 fsl_xcvr_earc_channels[] = { 1, 2, 8, 16, 32, };
static const struct snd_pcm_hw_constraint_list fsl_xcvr_earc_channels_constr = {
	.count = ARRAY_SIZE(fsl_xcvr_earc_channels),
	.list = fsl_xcvr_earc_channels,
};

static const u32 fsl_xcvr_earc_rates[] = {
	32000, 44100, 48000, 64000, 88200, 96000,
	128000, 176400, 192000, 256000, 352800, 384000,
	512000, 705600, 768000, 1024000, 1411200, 1536000,
};
static const struct snd_pcm_hw_constraint_list fsl_xcvr_earc_rates_constr = {
	.count = ARRAY_SIZE(fsl_xcvr_earc_rates),
	.list = fsl_xcvr_earc_rates,
};

static const u32 fsl_xcvr_spdif_channels[] = { 2, };
static const struct snd_pcm_hw_constraint_list fsl_xcvr_spdif_channels_constr = {
	.count = ARRAY_SIZE(fsl_xcvr_spdif_channels),
	.list = fsl_xcvr_spdif_channels,
};

static const u32 fsl_xcvr_spdif_rates[] = {
	32000, 44100, 48000, 88200, 96000, 176400, 192000,
};
static const struct snd_pcm_hw_constraint_list fsl_xcvr_spdif_rates_constr = {
	.count = ARRAY_SIZE(fsl_xcvr_spdif_rates),
	.list = fsl_xcvr_spdif_rates,
};

static int fsl_xcvr_arc_mode_put(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *dai = snd_kcontrol_chip(kcontrol);
	struct fsl_xcvr *xcvr = snd_soc_dai_get_drvdata(dai);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int *item = ucontrol->value.enumerated.item;

	xcvr->arc_mode = snd_soc_enum_item_to_val(e, item[0]);

	return 0;
}

static int fsl_xcvr_arc_mode_get(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *dai = snd_kcontrol_chip(kcontrol);
	struct fsl_xcvr *xcvr = snd_soc_dai_get_drvdata(dai);

	ucontrol->value.enumerated.item[0] = xcvr->arc_mode;

	return 0;
}

static const u32 fsl_xcvr_phy_arc_cfg[] = {
	FSL_XCVR_PHY_CTRL_ARC_MODE_SE_EN, FSL_XCVR_PHY_CTRL_ARC_MODE_CM_EN,
};

static const char * const fsl_xcvr_arc_mode[] = { "Single Ended", "Common", };
static const struct soc_enum fsl_xcvr_arc_mode_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(fsl_xcvr_arc_mode), fsl_xcvr_arc_mode);
static struct snd_kcontrol_new fsl_xcvr_arc_mode_kctl =
	SOC_ENUM_EXT("ARC Mode", fsl_xcvr_arc_mode_enum,
		     fsl_xcvr_arc_mode_get, fsl_xcvr_arc_mode_put);

/* Capabilities data structure, bytes */
static int fsl_xcvr_type_capds_bytes_info(struct snd_kcontrol *kcontrol,
					  struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = FSL_XCVR_CAPDS_SIZE;

	return 0;
}

static int fsl_xcvr_capds_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *dai = snd_kcontrol_chip(kcontrol);
	struct fsl_xcvr *xcvr = snd_soc_dai_get_drvdata(dai);

	memcpy(ucontrol->value.bytes.data, xcvr->cap_ds, FSL_XCVR_CAPDS_SIZE);

	return 0;
}

static int fsl_xcvr_capds_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *dai = snd_kcontrol_chip(kcontrol);
	struct fsl_xcvr *xcvr = snd_soc_dai_get_drvdata(dai);

	memcpy(xcvr->cap_ds, ucontrol->value.bytes.data, FSL_XCVR_CAPDS_SIZE);

	return 0;
}

static struct snd_kcontrol_new fsl_xcvr_earc_capds_kctl = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Capabilities Data Structure",
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.info = fsl_xcvr_type_capds_bytes_info,
	.get = fsl_xcvr_capds_get,
	.put = fsl_xcvr_capds_put,
};

static int fsl_xcvr_activate_ctl(struct snd_soc_dai *dai, const char *name,
				 bool active)
{
	struct snd_soc_card *card = dai->component->card;
	struct snd_kcontrol *kctl;
	bool enabled;

	lockdep_assert_held(&card->snd_card->controls_rwsem);

	kctl = snd_soc_card_get_kcontrol(card, name);
	if (kctl == NULL)
		return -ENOENT;

	enabled = ((kctl->vd[0].access & SNDRV_CTL_ELEM_ACCESS_WRITE) != 0);
	if (active == enabled)
		return 0; /* nothing to do */

	if (active)
		kctl->vd[0].access |=  SNDRV_CTL_ELEM_ACCESS_WRITE;
	else
		kctl->vd[0].access &= ~SNDRV_CTL_ELEM_ACCESS_WRITE;

	snd_ctl_notify(card->snd_card, SNDRV_CTL_EVENT_MASK_INFO, &kctl->id);

	return 1;
}

static int fsl_xcvr_mode_put(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *dai = snd_kcontrol_chip(kcontrol);
	struct fsl_xcvr *xcvr = snd_soc_dai_get_drvdata(dai);
	struct soc_enum *e = (struct soc_enum *)kcontrol->private_value;
	unsigned int *item = ucontrol->value.enumerated.item;
	struct snd_soc_card *card = dai->component->card;
	struct snd_soc_pcm_runtime *rtd;

	xcvr->mode = snd_soc_enum_item_to_val(e, item[0]);

	fsl_xcvr_activate_ctl(dai, fsl_xcvr_arc_mode_kctl.name,
			      (xcvr->mode == FSL_XCVR_MODE_ARC));
	fsl_xcvr_activate_ctl(dai, fsl_xcvr_earc_capds_kctl.name,
			      (xcvr->mode == FSL_XCVR_MODE_EARC));
	/* Allow playback for SPDIF only */
	rtd = snd_soc_get_pcm_runtime(card, card->dai_link);
	rtd->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream_count =
		(xcvr->mode == FSL_XCVR_MODE_SPDIF ? 1 : 0);
	return 0;
}

static int fsl_xcvr_mode_get(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *dai = snd_kcontrol_chip(kcontrol);
	struct fsl_xcvr *xcvr = snd_soc_dai_get_drvdata(dai);

	ucontrol->value.enumerated.item[0] = xcvr->mode;

	return 0;
}

static const char * const fsl_xcvr_mode[] = { "SPDIF", "ARC RX", "eARC", };
static const struct soc_enum fsl_xcvr_mode_enum =
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(fsl_xcvr_mode), fsl_xcvr_mode);
static struct snd_kcontrol_new fsl_xcvr_mode_kctl =
	SOC_ENUM_EXT("XCVR Mode", fsl_xcvr_mode_enum,
		     fsl_xcvr_mode_get, fsl_xcvr_mode_put);

/** phy: true => phy, false => pll */
static int fsl_xcvr_ai_write(struct fsl_xcvr *xcvr, u8 reg, u32 data, bool phy)
{
	struct device *dev = &xcvr->pdev->dev;
	u32 val, idx, tidx;
	int ret;

	idx  = BIT(phy ? 26 : 24);
	tidx = BIT(phy ? 27 : 25);

	regmap_write(xcvr->regmap, FSL_XCVR_PHY_AI_CTRL_CLR, 0xFF | FSL_XCVR_PHY_AI_CTRL_AI_RWB);
	regmap_write(xcvr->regmap, FSL_XCVR_PHY_AI_CTRL_SET, reg);
	regmap_write(xcvr->regmap, FSL_XCVR_PHY_AI_WDATA, data);
	regmap_write(xcvr->regmap, FSL_XCVR_PHY_AI_CTRL_TOG, idx);

	ret = regmap_read_poll_timeout(xcvr->regmap, FSL_XCVR_PHY_AI_CTRL, val,
				       (val & idx) == ((val & tidx) >> 1),
				       10, 10000);
	if (ret)
		dev_err(dev, "AI timeout: failed to set %s reg 0x%02x=0x%08x\n",
			phy ? "PHY" : "PLL", reg, data);
	return ret;
}

static int fsl_xcvr_ai_read(struct fsl_xcvr *xcvr, u8 reg, u32 *data, bool phy)
{
	struct device *dev = &xcvr->pdev->dev;
	u32 val, idx, tidx;
	int ret;

	idx  = BIT(phy ? 26 : 24);
	tidx = BIT(phy ? 27 : 25);

	regmap_write(xcvr->regmap, FSL_XCVR_PHY_AI_CTRL_CLR, 0xFF | FSL_XCVR_PHY_AI_CTRL_AI_RWB);
	regmap_write(xcvr->regmap, FSL_XCVR_PHY_AI_CTRL_SET, reg | FSL_XCVR_PHY_AI_CTRL_AI_RWB);
	regmap_write(xcvr->regmap, FSL_XCVR_PHY_AI_CTRL_TOG, idx);

	ret = regmap_read_poll_timeout(xcvr->regmap, FSL_XCVR_PHY_AI_CTRL, val,
				       (val & idx) == ((val & tidx) >> 1),
				       10, 10000);
	if (ret)
		dev_err(dev, "AI timeout: failed to read %s reg 0x%02x\n",
			phy ? "PHY" : "PLL", reg);

	regmap_read(xcvr->regmap, FSL_XCVR_PHY_AI_RDATA, data);

	return ret;
}

static int fsl_xcvr_phy_reg_read(void *context, unsigned int reg, unsigned int *val)
{
	struct fsl_xcvr *xcvr = context;

	return fsl_xcvr_ai_read(xcvr, reg, val, 1);
}

static int fsl_xcvr_phy_reg_write(void *context, unsigned int reg, unsigned int val)
{
	struct fsl_xcvr *xcvr = context;

	return fsl_xcvr_ai_write(xcvr, reg, val, 1);
}

static int fsl_xcvr_pll_reg_read(void *context, unsigned int reg, unsigned int *val)
{
	struct fsl_xcvr *xcvr = context;

	return fsl_xcvr_ai_read(xcvr, reg, val, 0);
}

static int fsl_xcvr_pll_reg_write(void *context, unsigned int reg, unsigned int val)
{
	struct fsl_xcvr *xcvr = context;

	return fsl_xcvr_ai_write(xcvr, reg, val, 0);
}

static int fsl_xcvr_en_phy_pll(struct fsl_xcvr *xcvr, u32 freq, bool tx)
{
	struct device *dev = &xcvr->pdev->dev;
	u32 i, div = 0, log2, val;
	int ret;

	if (!xcvr->soc_data->use_phy)
		return 0;

	for (i = 0; i < ARRAY_SIZE(fsl_xcvr_pll_cfg); i++) {
		if (fsl_xcvr_pll_cfg[i].fout % freq == 0) {
			div = fsl_xcvr_pll_cfg[i].fout / freq;
			break;
		}
	}

	if (!div || i >= ARRAY_SIZE(fsl_xcvr_pll_cfg))
		return -EINVAL;

	log2 = ilog2(div);

	/* Release AI interface from reset */
	ret = regmap_write(xcvr->regmap, FSL_XCVR_PHY_AI_CTRL_SET,
			   FSL_XCVR_PHY_AI_CTRL_AI_RESETN);
	if (ret < 0) {
		dev_err(dev, "Error while setting IER0: %d\n", ret);
		return ret;
	}

	switch (xcvr->soc_data->pll_ver) {
	case PLL_MX8MP:
		/* PLL: BANDGAP_SET: EN_VBG (enable bandgap) */
		regmap_set_bits(xcvr->regmap_pll, FSL_XCVR_PLL_BANDGAP,
				FSL_XCVR_PLL_BANDGAP_EN_VBG);

		/* PLL: CTRL0: DIV_INTEGER */
		regmap_write(xcvr->regmap_pll, FSL_XCVR_PLL_CTRL0, fsl_xcvr_pll_cfg[i].mfi);
		/* PLL: NUMERATOR: MFN */
		regmap_write(xcvr->regmap_pll, FSL_XCVR_PLL_NUM, fsl_xcvr_pll_cfg[i].mfn);
		/* PLL: DENOMINATOR: MFD */
		regmap_write(xcvr->regmap_pll, FSL_XCVR_PLL_DEN, fsl_xcvr_pll_cfg[i].mfd);
		/* PLL: CTRL0_SET: HOLD_RING_OFF, POWER_UP */
		regmap_set_bits(xcvr->regmap_pll, FSL_XCVR_PLL_CTRL0,
				FSL_XCVR_PLL_CTRL0_HROFF | FSL_XCVR_PLL_CTRL0_PWP);
		udelay(25);
		/* PLL: CTRL0: Clear Hold Ring Off */
		regmap_clear_bits(xcvr->regmap_pll, FSL_XCVR_PLL_CTRL0,
				  FSL_XCVR_PLL_CTRL0_HROFF);
		udelay(100);
		if (tx) { /* TX is enabled for SPDIF only */
			/* PLL: POSTDIV: PDIV0 */
			regmap_write(xcvr->regmap_pll, FSL_XCVR_PLL_PDIV,
				     FSL_XCVR_PLL_PDIVx(log2, 0));
			/* PLL: CTRL_SET: CLKMUX0_EN */
			regmap_set_bits(xcvr->regmap_pll, FSL_XCVR_PLL_CTRL0,
					FSL_XCVR_PLL_CTRL0_CM0_EN);
		} else if (xcvr->mode == FSL_XCVR_MODE_EARC) { /* eARC RX */
			/* PLL: POSTDIV: PDIV1 */
			regmap_write(xcvr->regmap_pll, FSL_XCVR_PLL_PDIV,
				     FSL_XCVR_PLL_PDIVx(log2, 1));
			/* PLL: CTRL_SET: CLKMUX1_EN */
			regmap_set_bits(xcvr->regmap_pll, FSL_XCVR_PLL_CTRL0,
					FSL_XCVR_PLL_CTRL0_CM1_EN);
		} else { /* SPDIF / ARC RX */
			/* PLL: POSTDIV: PDIV2 */
			regmap_write(xcvr->regmap_pll, FSL_XCVR_PLL_PDIV,
				     FSL_XCVR_PLL_PDIVx(log2, 2));
			/* PLL: CTRL_SET: CLKMUX2_EN */
			regmap_set_bits(xcvr->regmap_pll, FSL_XCVR_PLL_CTRL0,
					FSL_XCVR_PLL_CTRL0_CM2_EN);
		}
		break;
	case PLL_MX95:
		val = fsl_xcvr_pll_cfg[i].mfi << FSL_XCVR_GP_PLL_DIV_MFI_SHIFT | div;
		regmap_write(xcvr->regmap_pll, FSL_XCVR_GP_PLL_DIV, val);
		val = fsl_xcvr_pll_cfg[i].mfn << FSL_XCVR_GP_PLL_NUMERATOR_MFN_SHIFT;
		regmap_write(xcvr->regmap_pll, FSL_XCVR_GP_PLL_NUMERATOR, val);
		regmap_write(xcvr->regmap_pll, FSL_XCVR_GP_PLL_DENOMINATOR,
			     fsl_xcvr_pll_cfg[i].mfd);
		val = FSL_XCVR_GP_PLL_CTRL_POWERUP | FSL_XCVR_GP_PLL_CTRL_CLKMUX_EN;
		regmap_write(xcvr->regmap_pll, FSL_XCVR_GP_PLL_CTRL, val);
		break;
	default:
		dev_err(dev, "Error for PLL version %d\n", xcvr->soc_data->pll_ver);
		return -EINVAL;
	}

	if (xcvr->mode == FSL_XCVR_MODE_EARC) { /* eARC mode */
		/* PHY: CTRL_SET: TX_DIFF_OE, PHY_EN */
		regmap_set_bits(xcvr->regmap_phy, FSL_XCVR_PHY_CTRL,
				FSL_XCVR_PHY_CTRL_TSDIFF_OE |
				FSL_XCVR_PHY_CTRL_PHY_EN);
		/* PHY: CTRL2_SET: EARC_TX_MODE */
		regmap_set_bits(xcvr->regmap_phy, FSL_XCVR_PHY_CTRL2,
				FSL_XCVR_PHY_CTRL2_EARC_TXMS);
	} else if (!tx) { /* SPDIF / ARC RX mode */
		if (xcvr->mode == FSL_XCVR_MODE_SPDIF)
			/* PHY: CTRL_SET: SPDIF_EN */
			regmap_set_bits(xcvr->regmap_phy, FSL_XCVR_PHY_CTRL,
					FSL_XCVR_PHY_CTRL_SPDIF_EN);
		else	/* PHY: CTRL_SET: ARC RX setup */
			regmap_set_bits(xcvr->regmap_phy, FSL_XCVR_PHY_CTRL,
					FSL_XCVR_PHY_CTRL_PHY_EN |
					FSL_XCVR_PHY_CTRL_RX_CM_EN |
					fsl_xcvr_phy_arc_cfg[xcvr->arc_mode]);
	}

	dev_dbg(dev, "PLL Fexp: %u, Fout: %u, mfi: %u, mfn: %u, mfd: %d, div: %u, pdiv0: %u\n",
		freq, fsl_xcvr_pll_cfg[i].fout, fsl_xcvr_pll_cfg[i].mfi,
		fsl_xcvr_pll_cfg[i].mfn, fsl_xcvr_pll_cfg[i].mfd, div, log2);
	return 0;
}

static int fsl_xcvr_en_aud_pll(struct fsl_xcvr *xcvr, u32 freq)
{
	struct device *dev = &xcvr->pdev->dev;
	int ret;

	freq = xcvr->soc_data->spdif_only ? freq / 5 : freq;
	clk_disable_unprepare(xcvr->phy_clk);
	fsl_asoc_reparent_pll_clocks(dev, xcvr->phy_clk,
				     xcvr->pll8k_clk, xcvr->pll11k_clk, freq);
	ret = clk_set_rate(xcvr->phy_clk, freq);
	if (ret < 0) {
		dev_err(dev, "Error while setting AUD PLL rate: %d\n", ret);
		return ret;
	}
	ret = clk_prepare_enable(xcvr->phy_clk);
	if (ret) {
		dev_err(dev, "failed to start PHY clock: %d\n", ret);
		return ret;
	}

	if (!xcvr->soc_data->use_phy)
		return 0;
	/* Release AI interface from reset */
	ret = regmap_write(xcvr->regmap, FSL_XCVR_PHY_AI_CTRL_SET,
			   FSL_XCVR_PHY_AI_CTRL_AI_RESETN);
	if (ret < 0) {
		dev_err(dev, "Error while setting IER0: %d\n", ret);
		return ret;
	}

	if (xcvr->mode == FSL_XCVR_MODE_EARC) { /* eARC mode */
		/* PHY: CTRL_SET: TX_DIFF_OE, PHY_EN */
		regmap_set_bits(xcvr->regmap_phy, FSL_XCVR_PHY_CTRL,
				FSL_XCVR_PHY_CTRL_TSDIFF_OE |
				FSL_XCVR_PHY_CTRL_PHY_EN);
		/* PHY: CTRL2_SET: EARC_TX_MODE */
		regmap_set_bits(xcvr->regmap_phy, FSL_XCVR_PHY_CTRL2,
				FSL_XCVR_PHY_CTRL2_EARC_TXMS);
	} else { /* SPDIF mode */
		/* PHY: CTRL_SET: TX_CLK_AUD_SS | SPDIF_EN */
		regmap_set_bits(xcvr->regmap_phy, FSL_XCVR_PHY_CTRL,
				FSL_XCVR_PHY_CTRL_TX_CLK_AUD_SS |
				FSL_XCVR_PHY_CTRL_SPDIF_EN);
	}

	dev_dbg(dev, "PLL Fexp: %u\n", freq);

	return 0;
}

#define FSL_XCVR_SPDIF_RX_FREQ	175000000
static int fsl_xcvr_prepare(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *dai)
{
	struct fsl_xcvr *xcvr = snd_soc_dai_get_drvdata(dai);
	bool tx = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	u32 m_ctl = 0, v_ctl = 0;
	u32 r = substream->runtime->rate, ch = substream->runtime->channels;
	u32 fout = 32 * r * ch * 10;
	int ret = 0;

	switch (xcvr->mode) {
	case FSL_XCVR_MODE_SPDIF:
		if (xcvr->soc_data->spdif_only && tx) {
			ret = regmap_update_bits(xcvr->regmap, FSL_XCVR_TX_DPTH_CTRL,
						 FSL_XCVR_TX_DPTH_CTRL_BYPASS_FEM,
						 FSL_XCVR_TX_DPTH_CTRL_BYPASS_FEM);
			if (ret < 0) {
				dev_err(dai->dev, "Failed to set bypass fem: %d\n", ret);
				return ret;
			}
		}
		fallthrough;
	case FSL_XCVR_MODE_ARC:
		if (tx) {
			ret = fsl_xcvr_en_aud_pll(xcvr, fout);
			if (ret < 0) {
				dev_err(dai->dev, "Failed to set TX freq %u: %d\n",
					fout, ret);
				return ret;
			}

			ret = regmap_set_bits(xcvr->regmap, FSL_XCVR_TX_DPTH_CTRL,
					      FSL_XCVR_TX_DPTH_CTRL_FRM_FMT);
			if (ret < 0) {
				dev_err(dai->dev, "Failed to set TX_DPTH: %d\n", ret);
				return ret;
			}

			/**
			 * set SPDIF MODE - this flag is used to gate
			 * SPDIF output, useless for SPDIF RX
			 */
			m_ctl |= FSL_XCVR_EXT_CTRL_SPDIF_MODE;
			v_ctl |= FSL_XCVR_EXT_CTRL_SPDIF_MODE;
		} else {
			/**
			 * Clear RX FIFO, flip RX FIFO bits,
			 * disable eARC related HW mode detects
			 */
			ret = regmap_set_bits(xcvr->regmap, FSL_XCVR_RX_DPTH_CTRL,
					      FSL_XCVR_RX_DPTH_CTRL_STORE_FMT |
					      FSL_XCVR_RX_DPTH_CTRL_CLR_RX_FIFO |
					      FSL_XCVR_RX_DPTH_CTRL_COMP |
					      FSL_XCVR_RX_DPTH_CTRL_LAYB_CTRL);
			if (ret < 0) {
				dev_err(dai->dev, "Failed to set RX_DPTH: %d\n", ret);
				return ret;
			}

			ret = fsl_xcvr_en_phy_pll(xcvr, FSL_XCVR_SPDIF_RX_FREQ, tx);
			if (ret < 0) {
				dev_err(dai->dev, "Failed to set RX freq %u: %d\n",
					FSL_XCVR_SPDIF_RX_FREQ, ret);
				return ret;
			}
		}
		break;
	case FSL_XCVR_MODE_EARC:
		if (!tx) {
			/** Clear RX FIFO, flip RX FIFO bits */
			ret = regmap_set_bits(xcvr->regmap, FSL_XCVR_RX_DPTH_CTRL,
					      FSL_XCVR_RX_DPTH_CTRL_STORE_FMT |
					      FSL_XCVR_RX_DPTH_CTRL_CLR_RX_FIFO);
			if (ret < 0) {
				dev_err(dai->dev, "Failed to set RX_DPTH: %d\n", ret);
				return ret;
			}

			/** Enable eARC related HW mode detects */
			ret = regmap_clear_bits(xcvr->regmap, FSL_XCVR_RX_DPTH_CTRL,
						FSL_XCVR_RX_DPTH_CTRL_COMP |
						FSL_XCVR_RX_DPTH_CTRL_LAYB_CTRL);
			if (ret < 0) {
				dev_err(dai->dev, "Failed to clr TX_DPTH: %d\n", ret);
				return ret;
			}
		}

		/* clear CMDC RESET */
		m_ctl |= FSL_XCVR_EXT_CTRL_CMDC_RESET(tx);
		/* set TX_RX_MODE */
		m_ctl |= FSL_XCVR_EXT_CTRL_TX_RX_MODE;
		v_ctl |= (tx ? FSL_XCVR_EXT_CTRL_TX_RX_MODE : 0);
		break;
	}

	ret = regmap_update_bits(xcvr->regmap, FSL_XCVR_EXT_CTRL, m_ctl, v_ctl);
	if (ret < 0) {
		dev_err(dai->dev, "Error while setting EXT_CTRL: %d\n", ret);
		return ret;
	}

	return 0;
}

static int fsl_xcvr_constr(const struct snd_pcm_substream *substream,
			   const struct snd_pcm_hw_constraint_list *channels,
			   const struct snd_pcm_hw_constraint_list *rates)
{
	struct snd_pcm_runtime *rt = substream->runtime;
	int ret;

	ret = snd_pcm_hw_constraint_list(rt, 0, SNDRV_PCM_HW_PARAM_CHANNELS,
					 channels);
	if (ret < 0)
		return ret;

	ret = snd_pcm_hw_constraint_list(rt, 0, SNDRV_PCM_HW_PARAM_RATE,
					 rates);
	if (ret < 0)
		return ret;

	return 0;
}

static int fsl_xcvr_startup(struct snd_pcm_substream *substream,
			    struct snd_soc_dai *dai)
{
	struct fsl_xcvr *xcvr = snd_soc_dai_get_drvdata(dai);
	bool tx = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	int ret = 0;

	if (xcvr->streams & BIT(substream->stream)) {
		dev_err(dai->dev, "%sX busy\n", tx ? "T" : "R");
		return -EBUSY;
	}

	/*
	 * EDMA controller needs period size to be a multiple of
	 * tx/rx maxburst
	 */
	if (xcvr->soc_data->use_edma)
		snd_pcm_hw_constraint_step(substream->runtime, 0,
					   SNDRV_PCM_HW_PARAM_PERIOD_SIZE,
					   tx ? xcvr->dma_prms_tx.maxburst :
					   xcvr->dma_prms_rx.maxburst);

	switch (xcvr->mode) {
	case FSL_XCVR_MODE_SPDIF:
	case FSL_XCVR_MODE_ARC:
		if (xcvr->soc_data->spdif_only && tx)
			ret = fsl_xcvr_constr(substream, &fsl_xcvr_spdif_channels_constr,
					      &xcvr->spdif_constr_rates);
		else
			ret = fsl_xcvr_constr(substream, &fsl_xcvr_spdif_channels_constr,
					      &fsl_xcvr_spdif_rates_constr);
		break;
	case FSL_XCVR_MODE_EARC:
		ret = fsl_xcvr_constr(substream, &fsl_xcvr_earc_channels_constr,
				      &fsl_xcvr_earc_rates_constr);
		break;
	}
	if (ret < 0)
		return ret;

	xcvr->streams |= BIT(substream->stream);

	if (!xcvr->soc_data->spdif_only) {
		struct snd_soc_card *card = dai->component->card;

		/* Disable XCVR controls if there is stream started */
		down_read(&card->snd_card->controls_rwsem);
		fsl_xcvr_activate_ctl(dai, fsl_xcvr_mode_kctl.name, false);
		fsl_xcvr_activate_ctl(dai, fsl_xcvr_arc_mode_kctl.name, false);
		fsl_xcvr_activate_ctl(dai, fsl_xcvr_earc_capds_kctl.name, false);
		up_read(&card->snd_card->controls_rwsem);
	}

	return 0;
}

static void fsl_xcvr_shutdown(struct snd_pcm_substream *substream,
			      struct snd_soc_dai *dai)
{
	struct fsl_xcvr *xcvr = snd_soc_dai_get_drvdata(dai);
	bool tx = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	u32 mask = 0, val = 0;
	int ret;

	xcvr->streams &= ~BIT(substream->stream);

	/* Enable XCVR controls if there is no stream started */
	if (!xcvr->streams) {
		if (!xcvr->soc_data->spdif_only) {
			struct snd_soc_card *card = dai->component->card;

			down_read(&card->snd_card->controls_rwsem);
			fsl_xcvr_activate_ctl(dai, fsl_xcvr_mode_kctl.name, true);
			fsl_xcvr_activate_ctl(dai, fsl_xcvr_arc_mode_kctl.name,
						(xcvr->mode == FSL_XCVR_MODE_ARC));
			fsl_xcvr_activate_ctl(dai, fsl_xcvr_earc_capds_kctl.name,
						(xcvr->mode == FSL_XCVR_MODE_EARC));
			up_read(&card->snd_card->controls_rwsem);
		}
		ret = regmap_update_bits(xcvr->regmap, FSL_XCVR_EXT_IER0,
					 FSL_XCVR_IRQ_EARC_ALL, 0);
		if (ret < 0) {
			dev_err(dai->dev, "Failed to set IER0: %d\n", ret);
			return;
		}

		/* clear SPDIF MODE */
		if (xcvr->mode == FSL_XCVR_MODE_SPDIF)
			mask |= FSL_XCVR_EXT_CTRL_SPDIF_MODE;
	}

	if (xcvr->mode == FSL_XCVR_MODE_EARC) {
		/* set CMDC RESET */
		mask |= FSL_XCVR_EXT_CTRL_CMDC_RESET(tx);
		val  |= FSL_XCVR_EXT_CTRL_CMDC_RESET(tx);
	}

	ret = regmap_update_bits(xcvr->regmap, FSL_XCVR_EXT_CTRL, mask, val);
	if (ret < 0) {
		dev_err(dai->dev, "Err setting DPATH RESET: %d\n", ret);
		return;
	}
}

static int fsl_xcvr_trigger(struct snd_pcm_substream *substream, int cmd,
			    struct snd_soc_dai *dai)
{
	struct fsl_xcvr *xcvr = snd_soc_dai_get_drvdata(dai);
	bool tx = substream->stream == SNDRV_PCM_STREAM_PLAYBACK;
	unsigned long lock_flags;
	int ret = 0;

	spin_lock_irqsave(&xcvr->lock, lock_flags);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* set DPATH RESET */
		ret = regmap_update_bits(xcvr->regmap, FSL_XCVR_EXT_CTRL,
					 FSL_XCVR_EXT_CTRL_DPTH_RESET(tx),
					 FSL_XCVR_EXT_CTRL_DPTH_RESET(tx));
		if (ret < 0) {
			dev_err(dai->dev, "Failed to set DPATH RESET: %d\n", ret);
			goto release_lock;
		}

		if (tx) {
			switch (xcvr->mode) {
			case FSL_XCVR_MODE_EARC:
				/* set isr_cmdc_tx_en, w1c */
				ret = regmap_write(xcvr->regmap,
						   FSL_XCVR_ISR_SET,
						   FSL_XCVR_ISR_CMDC_TX_EN);
				if (ret < 0) {
					dev_err(dai->dev, "err updating isr %d\n", ret);
					goto release_lock;
				}
				fallthrough;
			case FSL_XCVR_MODE_SPDIF:
				ret = regmap_set_bits(xcvr->regmap,
						      FSL_XCVR_TX_DPTH_CTRL,
						      FSL_XCVR_TX_DPTH_CTRL_STRT_DATA_TX);
				if (ret < 0) {
					dev_err(dai->dev, "Failed to start DATA_TX: %d\n", ret);
					goto release_lock;
				}
				break;
			}
		}

		/* enable DMA RD/WR */
		ret = regmap_update_bits(xcvr->regmap, FSL_XCVR_EXT_CTRL,
					 FSL_XCVR_EXT_CTRL_DMA_DIS(tx), 0);
		if (ret < 0) {
			dev_err(dai->dev, "Failed to enable DMA: %d\n", ret);
			goto release_lock;
		}

		ret = regmap_update_bits(xcvr->regmap, FSL_XCVR_EXT_IER0,
					 FSL_XCVR_IRQ_EARC_ALL, FSL_XCVR_IRQ_EARC_ALL);
		if (ret < 0) {
			dev_err(dai->dev, "Error while setting IER0: %d\n", ret);
			goto release_lock;
		}

		/* clear DPATH RESET */
		ret = regmap_update_bits(xcvr->regmap, FSL_XCVR_EXT_CTRL,
					 FSL_XCVR_EXT_CTRL_DPTH_RESET(tx),
					 0);
		if (ret < 0) {
			dev_err(dai->dev, "Failed to clear DPATH RESET: %d\n", ret);
			goto release_lock;
		}

		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		/* disable DMA RD/WR */
		ret = regmap_update_bits(xcvr->regmap, FSL_XCVR_EXT_CTRL,
					 FSL_XCVR_EXT_CTRL_DMA_DIS(tx),
					 FSL_XCVR_EXT_CTRL_DMA_DIS(tx));
		if (ret < 0) {
			dev_err(dai->dev, "Failed to disable DMA: %d\n", ret);
			goto release_lock;
		}

		ret = regmap_update_bits(xcvr->regmap, FSL_XCVR_EXT_IER0,
					 FSL_XCVR_IRQ_EARC_ALL, 0);
		if (ret < 0) {
			dev_err(dai->dev, "Failed to clear IER0: %d\n", ret);
			goto release_lock;
		}

		if (tx) {
			switch (xcvr->mode) {
			case FSL_XCVR_MODE_SPDIF:
				ret = regmap_clear_bits(xcvr->regmap,
							FSL_XCVR_TX_DPTH_CTRL,
							FSL_XCVR_TX_DPTH_CTRL_STRT_DATA_TX);
				if (ret < 0) {
					dev_err(dai->dev, "Failed to stop DATA_TX: %d\n", ret);
					goto release_lock;
				}
				if (xcvr->soc_data->spdif_only)
					break;
				else
					fallthrough;
			case FSL_XCVR_MODE_EARC:
				/* clear ISR_CMDC_TX_EN, W1C */
				ret = regmap_write(xcvr->regmap,
						   FSL_XCVR_ISR_CLR,
						   FSL_XCVR_ISR_CMDC_TX_EN);
				if (ret < 0) {
					dev_err(dai->dev,
						"Err updating ISR %d\n", ret);
					goto release_lock;
				}
				break;
			}
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

release_lock:
	spin_unlock_irqrestore(&xcvr->lock, lock_flags);
	return ret;
}

static int fsl_xcvr_load_firmware(struct fsl_xcvr *xcvr)
{
	struct device *dev = &xcvr->pdev->dev;
	const struct firmware *fw;
	int ret = 0, rem, off, out, page = 0, size = FSL_XCVR_REG_OFFSET;
	u32 mask, val;

	ret = request_firmware(&fw, xcvr->soc_data->fw_name, dev);
	if (ret) {
		dev_err(dev, "failed to request firmware.\n");
		return ret;
	}

	rem = fw->size;

	/* RAM is 20KiB = 16KiB code + 4KiB data => max 10 pages 2KiB each */
	if (rem > 16384) {
		dev_err(dev, "FW size %d is bigger than 16KiB.\n", rem);
		release_firmware(fw);
		return -ENOMEM;
	}

	for (page = 0; page < 10; page++) {
		ret = regmap_update_bits(xcvr->regmap, FSL_XCVR_EXT_CTRL,
					 FSL_XCVR_EXT_CTRL_PAGE_MASK,
					 FSL_XCVR_EXT_CTRL_PAGE(page));
		if (ret < 0) {
			dev_err(dev, "FW: failed to set page %d, err=%d\n",
				page, ret);
			goto err_firmware;
		}

		off = page * size;
		out = min(rem, size);
		/* IPG clock is assumed to be running, otherwise it will hang */
		if (out > 0) {
			/* write firmware into code memory */
			memcpy_toio(xcvr->ram_addr, fw->data + off, out);
			rem -= out;
			if (rem == 0) {
				/* last part of firmware written */
				/* clean remaining part of code memory page */
				memset_io(xcvr->ram_addr + out, 0, size - out);
			}
		} else {
			/* clean current page, including data memory */
			memset_io(xcvr->ram_addr, 0, size);
		}
	}

err_firmware:
	release_firmware(fw);
	if (ret < 0)
		return ret;

	/* configure watermarks */
	mask = FSL_XCVR_EXT_CTRL_RX_FWM_MASK | FSL_XCVR_EXT_CTRL_TX_FWM_MASK;
	val  = FSL_XCVR_EXT_CTRL_RX_FWM(FSL_XCVR_FIFO_WMK_RX);
	val |= FSL_XCVR_EXT_CTRL_TX_FWM(FSL_XCVR_FIFO_WMK_TX);
	/* disable DMA RD/WR */
	mask |= FSL_XCVR_EXT_CTRL_DMA_RD_DIS | FSL_XCVR_EXT_CTRL_DMA_WR_DIS;
	val  |= FSL_XCVR_EXT_CTRL_DMA_RD_DIS | FSL_XCVR_EXT_CTRL_DMA_WR_DIS;
	/* Data RAM is 4KiB, last two pages: 8 and 9. Select page 8. */
	mask |= FSL_XCVR_EXT_CTRL_PAGE_MASK;
	val  |= FSL_XCVR_EXT_CTRL_PAGE(8);

	ret = regmap_update_bits(xcvr->regmap, FSL_XCVR_EXT_CTRL, mask, val);
	if (ret < 0) {
		dev_err(dev, "Failed to set watermarks: %d\n", ret);
		return ret;
	}

	/* Store Capabilities Data Structure into Data RAM */
	memcpy_toio(xcvr->ram_addr + FSL_XCVR_CAP_DATA_STR, xcvr->cap_ds,
		    FSL_XCVR_CAPDS_SIZE);
	return 0;
}

static int fsl_xcvr_type_iec958_info(struct snd_kcontrol *kcontrol,
				     struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_IEC958;
	uinfo->count = 1;

	return 0;
}

static int fsl_xcvr_type_iec958_bytes_info(struct snd_kcontrol *kcontrol,
					   struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BYTES;
	uinfo->count = sizeof_field(struct snd_aes_iec958, status);

	return 0;
}

static int fsl_xcvr_rx_cs_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *dai = snd_kcontrol_chip(kcontrol);
	struct fsl_xcvr *xcvr = snd_soc_dai_get_drvdata(dai);

	memcpy(ucontrol->value.iec958.status, xcvr->rx_iec958.status, 24);

	return 0;
}

static int fsl_xcvr_tx_cs_get(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *dai = snd_kcontrol_chip(kcontrol);
	struct fsl_xcvr *xcvr = snd_soc_dai_get_drvdata(dai);

	memcpy(ucontrol->value.iec958.status, xcvr->tx_iec958.status, 24);

	return 0;
}

static int fsl_xcvr_tx_cs_put(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dai *dai = snd_kcontrol_chip(kcontrol);
	struct fsl_xcvr *xcvr = snd_soc_dai_get_drvdata(dai);

	memcpy(xcvr->tx_iec958.status, ucontrol->value.iec958.status, 24);

	return 0;
}

static struct snd_kcontrol_new fsl_xcvr_rx_ctls[] = {
	/* Channel status controller */
	{
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("", CAPTURE, DEFAULT),
		.access = SNDRV_CTL_ELEM_ACCESS_READ,
		.info = fsl_xcvr_type_iec958_info,
		.get = fsl_xcvr_rx_cs_get,
	},
	/* Capture channel status, bytes */
	{
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = "Capture Channel Status",
		.access = SNDRV_CTL_ELEM_ACCESS_READ,
		.info = fsl_xcvr_type_iec958_bytes_info,
		.get = fsl_xcvr_rx_cs_get,
	},
};

static struct snd_kcontrol_new fsl_xcvr_tx_ctls[] = {
	/* Channel status controller */
	{
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = SNDRV_CTL_NAME_IEC958("", PLAYBACK, DEFAULT),
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = fsl_xcvr_type_iec958_info,
		.get = fsl_xcvr_tx_cs_get,
		.put = fsl_xcvr_tx_cs_put,
	},
	/* Playback channel status, bytes */
	{
		.iface = SNDRV_CTL_ELEM_IFACE_PCM,
		.name = "Playback Channel Status",
		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.info = fsl_xcvr_type_iec958_bytes_info,
		.get = fsl_xcvr_tx_cs_get,
		.put = fsl_xcvr_tx_cs_put,
	},
};

static int fsl_xcvr_dai_probe(struct snd_soc_dai *dai)
{
	struct fsl_xcvr *xcvr = snd_soc_dai_get_drvdata(dai);

	snd_soc_dai_init_dma_data(dai, &xcvr->dma_prms_tx, &xcvr->dma_prms_rx);

	if (xcvr->soc_data->spdif_only)
		xcvr->mode = FSL_XCVR_MODE_SPDIF;
	else {
		snd_soc_add_dai_controls(dai, &fsl_xcvr_mode_kctl, 1);
		snd_soc_add_dai_controls(dai, &fsl_xcvr_arc_mode_kctl, 1);
		snd_soc_add_dai_controls(dai, &fsl_xcvr_earc_capds_kctl, 1);
	}
	snd_soc_add_dai_controls(dai, fsl_xcvr_tx_ctls,
				 ARRAY_SIZE(fsl_xcvr_tx_ctls));
	snd_soc_add_dai_controls(dai, fsl_xcvr_rx_ctls,
				 ARRAY_SIZE(fsl_xcvr_rx_ctls));
	return 0;
}

static const struct snd_soc_dai_ops fsl_xcvr_dai_ops = {
	.probe		= fsl_xcvr_dai_probe,
	.prepare	= fsl_xcvr_prepare,
	.startup	= fsl_xcvr_startup,
	.shutdown	= fsl_xcvr_shutdown,
	.trigger	= fsl_xcvr_trigger,
};

static struct snd_soc_dai_driver fsl_xcvr_dai = {
	.ops = &fsl_xcvr_dai_ops,
	.playback = {
		.stream_name = "CPU-Playback",
		.channels_min = 1,
		.channels_max = 32,
		.rate_min = 32000,
		.rate_max = 1536000,
		.rates = SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_IEC958_SUBFRAME_LE,
	},
	.capture = {
		.stream_name = "CPU-Capture",
		.channels_min = 1,
		.channels_max = 32,
		.rate_min = 32000,
		.rate_max = 1536000,
		.rates = SNDRV_PCM_RATE_KNOT,
		.formats = SNDRV_PCM_FMTBIT_IEC958_SUBFRAME_LE,
	},
};

static const struct snd_soc_component_driver fsl_xcvr_comp = {
	.name			= "fsl-xcvr-dai",
	.legacy_dai_naming	= 1,
};

static const struct reg_default fsl_xcvr_reg_defaults[] = {
	{ FSL_XCVR_VERSION,	0x00000000 },
	{ FSL_XCVR_EXT_CTRL,	0xF8204040 },
	{ FSL_XCVR_EXT_STATUS,	0x00000000 },
	{ FSL_XCVR_EXT_IER0,	0x00000000 },
	{ FSL_XCVR_EXT_IER1,	0x00000000 },
	{ FSL_XCVR_EXT_ISR,	0x00000000 },
	{ FSL_XCVR_EXT_ISR_SET,	0x00000000 },
	{ FSL_XCVR_EXT_ISR_CLR,	0x00000000 },
	{ FSL_XCVR_EXT_ISR_TOG,	0x00000000 },
	{ FSL_XCVR_IER,		0x00000000 },
	{ FSL_XCVR_ISR,		0x00000000 },
	{ FSL_XCVR_ISR_SET,	0x00000000 },
	{ FSL_XCVR_ISR_CLR,	0x00000000 },
	{ FSL_XCVR_ISR_TOG,	0x00000000 },
	{ FSL_XCVR_CLK_CTRL,	0x0000018F },
	{ FSL_XCVR_RX_DPTH_CTRL,	0x00040CC1 },
	{ FSL_XCVR_RX_DPTH_CTRL_SET,	0x00040CC1 },
	{ FSL_XCVR_RX_DPTH_CTRL_CLR,	0x00040CC1 },
	{ FSL_XCVR_RX_DPTH_CTRL_TOG,	0x00040CC1 },
	{ FSL_XCVR_RX_DPTH_CNTR_CTRL,	0x00000000 },
	{ FSL_XCVR_RX_DPTH_CNTR_CTRL_SET, 0x00000000 },
	{ FSL_XCVR_RX_DPTH_CNTR_CTRL_CLR, 0x00000000 },
	{ FSL_XCVR_RX_DPTH_CNTR_CTRL_TOG, 0x00000000 },
	{ FSL_XCVR_RX_DPTH_TSCR, 0x00000000 },
	{ FSL_XCVR_RX_DPTH_BCR,  0x00000000 },
	{ FSL_XCVR_RX_DPTH_BCTR, 0x00000000 },
	{ FSL_XCVR_RX_DPTH_BCRR, 0x00000000 },
	{ FSL_XCVR_TX_DPTH_CTRL,	0x00000000 },
	{ FSL_XCVR_TX_DPTH_CTRL_SET,	0x00000000 },
	{ FSL_XCVR_TX_DPTH_CTRL_CLR,	0x00000000 },
	{ FSL_XCVR_TX_DPTH_CTRL_TOG,	0x00000000 },
	{ FSL_XCVR_TX_CS_DATA_0,	0x00000000 },
	{ FSL_XCVR_TX_CS_DATA_1,	0x00000000 },
	{ FSL_XCVR_TX_CS_DATA_2,	0x00000000 },
	{ FSL_XCVR_TX_CS_DATA_3,	0x00000000 },
	{ FSL_XCVR_TX_CS_DATA_4,	0x00000000 },
	{ FSL_XCVR_TX_CS_DATA_5,	0x00000000 },
	{ FSL_XCVR_TX_DPTH_CNTR_CTRL,	0x00000000 },
	{ FSL_XCVR_TX_DPTH_CNTR_CTRL_SET, 0x00000000 },
	{ FSL_XCVR_TX_DPTH_CNTR_CTRL_CLR, 0x00000000 },
	{ FSL_XCVR_TX_DPTH_CNTR_CTRL_TOG, 0x00000000 },
	{ FSL_XCVR_TX_DPTH_TSCR, 0x00000000 },
	{ FSL_XCVR_TX_DPTH_BCR,	 0x00000000 },
	{ FSL_XCVR_TX_DPTH_BCTR, 0x00000000 },
	{ FSL_XCVR_TX_DPTH_BCRR, 0x00000000 },
	{ FSL_XCVR_DEBUG_REG_0,		0x00000000 },
	{ FSL_XCVR_DEBUG_REG_1,		0x00000000 },
};

static bool fsl_xcvr_readable_reg(struct device *dev, unsigned int reg)
{
	struct fsl_xcvr *xcvr = dev_get_drvdata(dev);

	if (!xcvr->soc_data->use_phy)
		if ((reg >= FSL_XCVR_IER && reg <= FSL_XCVR_PHY_AI_RDATA) ||
		    reg > FSL_XCVR_TX_DPTH_BCRR)
			return false;
	switch (reg) {
	case FSL_XCVR_VERSION:
	case FSL_XCVR_EXT_CTRL:
	case FSL_XCVR_EXT_STATUS:
	case FSL_XCVR_EXT_IER0:
	case FSL_XCVR_EXT_IER1:
	case FSL_XCVR_EXT_ISR:
	case FSL_XCVR_EXT_ISR_SET:
	case FSL_XCVR_EXT_ISR_CLR:
	case FSL_XCVR_EXT_ISR_TOG:
	case FSL_XCVR_IER:
	case FSL_XCVR_ISR:
	case FSL_XCVR_ISR_SET:
	case FSL_XCVR_ISR_CLR:
	case FSL_XCVR_ISR_TOG:
	case FSL_XCVR_PHY_AI_CTRL:
	case FSL_XCVR_PHY_AI_CTRL_SET:
	case FSL_XCVR_PHY_AI_CTRL_CLR:
	case FSL_XCVR_PHY_AI_CTRL_TOG:
	case FSL_XCVR_PHY_AI_RDATA:
	case FSL_XCVR_CLK_CTRL:
	case FSL_XCVR_RX_DPTH_CTRL:
	case FSL_XCVR_RX_DPTH_CTRL_SET:
	case FSL_XCVR_RX_DPTH_CTRL_CLR:
	case FSL_XCVR_RX_DPTH_CTRL_TOG:
	case FSL_XCVR_RX_CS_DATA_0:
	case FSL_XCVR_RX_CS_DATA_1:
	case FSL_XCVR_RX_CS_DATA_2:
	case FSL_XCVR_RX_CS_DATA_3:
	case FSL_XCVR_RX_CS_DATA_4:
	case FSL_XCVR_RX_CS_DATA_5:
	case FSL_XCVR_RX_DPTH_CNTR_CTRL:
	case FSL_XCVR_RX_DPTH_CNTR_CTRL_SET:
	case FSL_XCVR_RX_DPTH_CNTR_CTRL_CLR:
	case FSL_XCVR_RX_DPTH_CNTR_CTRL_TOG:
	case FSL_XCVR_RX_DPTH_TSCR:
	case FSL_XCVR_RX_DPTH_BCR:
	case FSL_XCVR_RX_DPTH_BCTR:
	case FSL_XCVR_RX_DPTH_BCRR:
	case FSL_XCVR_TX_DPTH_CTRL:
	case FSL_XCVR_TX_DPTH_CTRL_SET:
	case FSL_XCVR_TX_DPTH_CTRL_CLR:
	case FSL_XCVR_TX_DPTH_CTRL_TOG:
	case FSL_XCVR_TX_CS_DATA_0:
	case FSL_XCVR_TX_CS_DATA_1:
	case FSL_XCVR_TX_CS_DATA_2:
	case FSL_XCVR_TX_CS_DATA_3:
	case FSL_XCVR_TX_CS_DATA_4:
	case FSL_XCVR_TX_CS_DATA_5:
	case FSL_XCVR_TX_DPTH_CNTR_CTRL:
	case FSL_XCVR_TX_DPTH_CNTR_CTRL_SET:
	case FSL_XCVR_TX_DPTH_CNTR_CTRL_CLR:
	case FSL_XCVR_TX_DPTH_CNTR_CTRL_TOG:
	case FSL_XCVR_TX_DPTH_TSCR:
	case FSL_XCVR_TX_DPTH_BCR:
	case FSL_XCVR_TX_DPTH_BCTR:
	case FSL_XCVR_TX_DPTH_BCRR:
	case FSL_XCVR_DEBUG_REG_0:
	case FSL_XCVR_DEBUG_REG_1:
		return true;
	default:
		return false;
	}
}

static bool fsl_xcvr_writeable_reg(struct device *dev, unsigned int reg)
{
	struct fsl_xcvr *xcvr = dev_get_drvdata(dev);

	if (!xcvr->soc_data->use_phy)
		if (reg >= FSL_XCVR_IER && reg <= FSL_XCVR_PHY_AI_RDATA)
			return false;
	switch (reg) {
	case FSL_XCVR_EXT_CTRL:
	case FSL_XCVR_EXT_IER0:
	case FSL_XCVR_EXT_IER1:
	case FSL_XCVR_EXT_ISR:
	case FSL_XCVR_EXT_ISR_SET:
	case FSL_XCVR_EXT_ISR_CLR:
	case FSL_XCVR_EXT_ISR_TOG:
	case FSL_XCVR_IER:
	case FSL_XCVR_ISR_SET:
	case FSL_XCVR_ISR_CLR:
	case FSL_XCVR_ISR_TOG:
	case FSL_XCVR_PHY_AI_CTRL:
	case FSL_XCVR_PHY_AI_CTRL_SET:
	case FSL_XCVR_PHY_AI_CTRL_CLR:
	case FSL_XCVR_PHY_AI_CTRL_TOG:
	case FSL_XCVR_PHY_AI_WDATA:
	case FSL_XCVR_CLK_CTRL:
	case FSL_XCVR_RX_DPTH_CTRL:
	case FSL_XCVR_RX_DPTH_CTRL_SET:
	case FSL_XCVR_RX_DPTH_CTRL_CLR:
	case FSL_XCVR_RX_DPTH_CTRL_TOG:
	case FSL_XCVR_RX_DPTH_CNTR_CTRL:
	case FSL_XCVR_RX_DPTH_CNTR_CTRL_SET:
	case FSL_XCVR_RX_DPTH_CNTR_CTRL_CLR:
	case FSL_XCVR_RX_DPTH_CNTR_CTRL_TOG:
	case FSL_XCVR_TX_DPTH_CTRL:
	case FSL_XCVR_TX_DPTH_CTRL_SET:
	case FSL_XCVR_TX_DPTH_CTRL_CLR:
	case FSL_XCVR_TX_DPTH_CTRL_TOG:
	case FSL_XCVR_TX_CS_DATA_0:
	case FSL_XCVR_TX_CS_DATA_1:
	case FSL_XCVR_TX_CS_DATA_2:
	case FSL_XCVR_TX_CS_DATA_3:
	case FSL_XCVR_TX_CS_DATA_4:
	case FSL_XCVR_TX_CS_DATA_5:
	case FSL_XCVR_TX_DPTH_CNTR_CTRL:
	case FSL_XCVR_TX_DPTH_CNTR_CTRL_SET:
	case FSL_XCVR_TX_DPTH_CNTR_CTRL_CLR:
	case FSL_XCVR_TX_DPTH_CNTR_CTRL_TOG:
		return true;
	default:
		return false;
	}
}

static bool fsl_xcvr_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case FSL_XCVR_EXT_STATUS:
	case FSL_XCVR_EXT_ISR:
	case FSL_XCVR_EXT_ISR_SET:
	case FSL_XCVR_EXT_ISR_CLR:
	case FSL_XCVR_EXT_ISR_TOG:
	case FSL_XCVR_ISR:
	case FSL_XCVR_ISR_SET:
	case FSL_XCVR_ISR_CLR:
	case FSL_XCVR_ISR_TOG:
	case FSL_XCVR_PHY_AI_CTRL:
	case FSL_XCVR_PHY_AI_CTRL_SET:
	case FSL_XCVR_PHY_AI_CTRL_CLR:
	case FSL_XCVR_PHY_AI_CTRL_TOG:
	case FSL_XCVR_PHY_AI_RDATA:
	case FSL_XCVR_RX_CS_DATA_0:
	case FSL_XCVR_RX_CS_DATA_1:
	case FSL_XCVR_RX_CS_DATA_2:
	case FSL_XCVR_RX_CS_DATA_3:
	case FSL_XCVR_RX_CS_DATA_4:
	case FSL_XCVR_RX_CS_DATA_5:
	case FSL_XCVR_RX_DPTH_CNTR_CTRL:
	case FSL_XCVR_RX_DPTH_CNTR_CTRL_SET:
	case FSL_XCVR_RX_DPTH_CNTR_CTRL_CLR:
	case FSL_XCVR_RX_DPTH_CNTR_CTRL_TOG:
	case FSL_XCVR_RX_DPTH_TSCR:
	case FSL_XCVR_RX_DPTH_BCR:
	case FSL_XCVR_RX_DPTH_BCTR:
	case FSL_XCVR_RX_DPTH_BCRR:
	case FSL_XCVR_TX_DPTH_CNTR_CTRL:
	case FSL_XCVR_TX_DPTH_CNTR_CTRL_SET:
	case FSL_XCVR_TX_DPTH_CNTR_CTRL_CLR:
	case FSL_XCVR_TX_DPTH_CNTR_CTRL_TOG:
	case FSL_XCVR_TX_DPTH_TSCR:
	case FSL_XCVR_TX_DPTH_BCR:
	case FSL_XCVR_TX_DPTH_BCTR:
	case FSL_XCVR_TX_DPTH_BCRR:
	case FSL_XCVR_DEBUG_REG_0:
	case FSL_XCVR_DEBUG_REG_1:
		return true;
	default:
		return false;
	}
}

static const struct regmap_config fsl_xcvr_regmap_cfg = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = FSL_XCVR_MAX_REG,
	.reg_defaults = fsl_xcvr_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(fsl_xcvr_reg_defaults),
	.readable_reg = fsl_xcvr_readable_reg,
	.volatile_reg = fsl_xcvr_volatile_reg,
	.writeable_reg = fsl_xcvr_writeable_reg,
	.cache_type = REGCACHE_FLAT,
};

static const struct reg_default fsl_xcvr_phy_reg_defaults[] = {
	{ FSL_XCVR_PHY_CTRL,		0x58200804 },
	{ FSL_XCVR_PHY_STATUS,		0x00000000 },
	{ FSL_XCVR_PHY_ANALOG_TRIM,	0x00260F13 },
	{ FSL_XCVR_PHY_SLEW_RATE_TRIM,	0x00000411 },
	{ FSL_XCVR_PHY_DATA_TEST_DELAY,	0x00990000 },
	{ FSL_XCVR_PHY_TEST_CTRL,	0x00000000 },
	{ FSL_XCVR_PHY_DIFF_CDR_CTRL,	0x016D0009 },
	{ FSL_XCVR_PHY_CTRL2,		0x80000000 },
};

static const struct regmap_config fsl_xcvr_regmap_phy_cfg = {
	.reg_bits = 8,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = FSL_XCVR_PHY_CTRL2_TOG,
	.reg_defaults = fsl_xcvr_phy_reg_defaults,
	.num_reg_defaults = ARRAY_SIZE(fsl_xcvr_phy_reg_defaults),
	.cache_type = REGCACHE_FLAT,
	.reg_read = fsl_xcvr_phy_reg_read,
	.reg_write = fsl_xcvr_phy_reg_write,
};

static const struct regmap_config fsl_xcvr_regmap_pllv0_cfg = {
	.reg_bits = 8,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = FSL_XCVR_PLL_STAT0_TOG,
	.cache_type = REGCACHE_FLAT,
	.reg_read = fsl_xcvr_pll_reg_read,
	.reg_write = fsl_xcvr_pll_reg_write,
};

static const struct regmap_config fsl_xcvr_regmap_pllv1_cfg = {
	.reg_bits = 8,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = FSL_XCVR_GP_PLL_STATUS_TOG,
	.cache_type = REGCACHE_FLAT,
	.reg_read = fsl_xcvr_pll_reg_read,
	.reg_write = fsl_xcvr_pll_reg_write,
};

static void reset_rx_work(struct work_struct *work)
{
	struct fsl_xcvr *xcvr = container_of(work, struct fsl_xcvr, work_rst);
	struct device *dev = &xcvr->pdev->dev;
	unsigned long lock_flags;
	u32 ext_ctrl;

	dev_dbg(dev, "reset rx path\n");
	spin_lock_irqsave(&xcvr->lock, lock_flags);
	regmap_read(xcvr->regmap, FSL_XCVR_EXT_CTRL, &ext_ctrl);

	if (!(ext_ctrl & FSL_XCVR_EXT_CTRL_DMA_RD_DIS)) {
		regmap_update_bits(xcvr->regmap, FSL_XCVR_EXT_CTRL,
				   FSL_XCVR_EXT_CTRL_DMA_RD_DIS,
				   FSL_XCVR_EXT_CTRL_DMA_RD_DIS);
		regmap_update_bits(xcvr->regmap, FSL_XCVR_EXT_CTRL,
				   FSL_XCVR_EXT_CTRL_RX_DPTH_RESET,
				   FSL_XCVR_EXT_CTRL_RX_DPTH_RESET);
		regmap_update_bits(xcvr->regmap, FSL_XCVR_EXT_CTRL,
				   FSL_XCVR_EXT_CTRL_DMA_RD_DIS,
				   0);
		regmap_update_bits(xcvr->regmap, FSL_XCVR_EXT_CTRL,
				   FSL_XCVR_EXT_CTRL_RX_DPTH_RESET,
				   0);
	}
	spin_unlock_irqrestore(&xcvr->lock, lock_flags);
}

static irqreturn_t irq0_isr(int irq, void *devid)
{
	struct fsl_xcvr *xcvr = (struct fsl_xcvr *)devid;
	struct device *dev = &xcvr->pdev->dev;
	struct regmap *regmap = xcvr->regmap;
	void __iomem *reg_ctrl, *reg_buff;
	u32 isr, isr_clr = 0, val, i;

	regmap_read(regmap, FSL_XCVR_EXT_ISR, &isr);

	if (isr & FSL_XCVR_IRQ_NEW_CS) {
		dev_dbg(dev, "Received new CS block\n");
		isr_clr |= FSL_XCVR_IRQ_NEW_CS;
		if (xcvr->soc_data->fw_name) {
			/* Data RAM is 4KiB, last two pages: 8 and 9. Select page 8. */
			regmap_update_bits(xcvr->regmap, FSL_XCVR_EXT_CTRL,
					   FSL_XCVR_EXT_CTRL_PAGE_MASK,
					   FSL_XCVR_EXT_CTRL_PAGE(8));

			/* Find updated CS buffer */
			reg_ctrl = xcvr->ram_addr + FSL_XCVR_RX_CS_CTRL_0;
			reg_buff = xcvr->ram_addr + FSL_XCVR_RX_CS_BUFF_0;
			memcpy_fromio(&val, reg_ctrl, sizeof(val));
			if (!val) {
				reg_ctrl = xcvr->ram_addr + FSL_XCVR_RX_CS_CTRL_1;
				reg_buff = xcvr->ram_addr + FSL_XCVR_RX_CS_BUFF_1;
				memcpy_fromio(&val, reg_ctrl, sizeof(val));
			}

			if (val) {
				/* copy CS buffer */
				memcpy_fromio(&xcvr->rx_iec958.status, reg_buff,
					      sizeof(xcvr->rx_iec958.status));
				for (i = 0; i < 6; i++) {
					val = *(u32 *)(xcvr->rx_iec958.status + i*4);
					*(u32 *)(xcvr->rx_iec958.status + i*4) =
						bitrev32(val);
				}
				/* clear CS control register */
				memset_io(reg_ctrl, 0, sizeof(val));
			}
		} else {
			regmap_read(xcvr->regmap, FSL_XCVR_RX_CS_DATA_0,
				    (u32 *)&xcvr->rx_iec958.status[0]);
			regmap_read(xcvr->regmap, FSL_XCVR_RX_CS_DATA_1,
				    (u32 *)&xcvr->rx_iec958.status[4]);
			regmap_read(xcvr->regmap, FSL_XCVR_RX_CS_DATA_2,
				    (u32 *)&xcvr->rx_iec958.status[8]);
			regmap_read(xcvr->regmap, FSL_XCVR_RX_CS_DATA_3,
				    (u32 *)&xcvr->rx_iec958.status[12]);
			regmap_read(xcvr->regmap, FSL_XCVR_RX_CS_DATA_4,
				    (u32 *)&xcvr->rx_iec958.status[16]);
			regmap_read(xcvr->regmap, FSL_XCVR_RX_CS_DATA_5,
				    (u32 *)&xcvr->rx_iec958.status[20]);
			for (i = 0; i < 6; i++) {
				val = *(u32 *)(xcvr->rx_iec958.status + i * 4);
				*(u32 *)(xcvr->rx_iec958.status + i * 4) =
					bitrev32(val);
			}
			regmap_set_bits(xcvr->regmap, FSL_XCVR_RX_DPTH_CTRL,
					FSL_XCVR_RX_DPTH_CTRL_CSA);
		}
	}
	if (isr & FSL_XCVR_IRQ_NEW_UD) {
		dev_dbg(dev, "Received new UD block\n");
		isr_clr |= FSL_XCVR_IRQ_NEW_UD;
	}
	if (isr & FSL_XCVR_IRQ_MUTE) {
		dev_dbg(dev, "HW mute bit detected\n");
		isr_clr |= FSL_XCVR_IRQ_MUTE;
	}
	if (isr & FSL_XCVR_IRQ_FIFO_UOFL_ERR) {
		dev_dbg(dev, "RX/TX FIFO full/empty\n");
		isr_clr |= FSL_XCVR_IRQ_FIFO_UOFL_ERR;
	}
	if (isr & FSL_XCVR_IRQ_ARC_MODE) {
		dev_dbg(dev, "CMDC SM falls out of eARC mode\n");
		isr_clr |= FSL_XCVR_IRQ_ARC_MODE;
	}
	if (isr & FSL_XCVR_IRQ_DMA_RD_REQ) {
		dev_dbg(dev, "DMA read request\n");
		isr_clr |= FSL_XCVR_IRQ_DMA_RD_REQ;
	}
	if (isr & FSL_XCVR_IRQ_DMA_WR_REQ) {
		dev_dbg(dev, "DMA write request\n");
		isr_clr |= FSL_XCVR_IRQ_DMA_WR_REQ;
	}
	if (isr & FSL_XCVR_IRQ_CMDC_STATUS_UPD) {
		dev_dbg(dev, "CMDC status update\n");
		isr_clr |= FSL_XCVR_IRQ_CMDC_STATUS_UPD;
	}
	if (isr & FSL_XCVR_IRQ_PREAMBLE_MISMATCH) {
		dev_dbg(dev, "Preamble mismatch\n");
		isr_clr |= FSL_XCVR_IRQ_PREAMBLE_MISMATCH;
	}
	if (isr & FSL_XCVR_IRQ_UNEXP_PRE_REC) {
		dev_dbg(dev, "Unexpected preamble received\n");
		isr_clr |= FSL_XCVR_IRQ_UNEXP_PRE_REC;
	}
	if (isr & FSL_XCVR_IRQ_M_W_PRE_MISMATCH) {
		dev_dbg(dev, "M/W preamble mismatch\n");
		isr_clr |= FSL_XCVR_IRQ_M_W_PRE_MISMATCH;
	}
	if (isr & FSL_XCVR_IRQ_B_PRE_MISMATCH) {
		dev_dbg(dev, "B preamble mismatch\n");
		isr_clr |= FSL_XCVR_IRQ_B_PRE_MISMATCH;
	}

	if (isr & (FSL_XCVR_IRQ_PREAMBLE_MISMATCH |
		   FSL_XCVR_IRQ_UNEXP_PRE_REC |
		   FSL_XCVR_IRQ_M_W_PRE_MISMATCH |
		   FSL_XCVR_IRQ_B_PRE_MISMATCH)) {
		schedule_work(&xcvr->work_rst);
	}

	if (isr_clr) {
		regmap_write(regmap, FSL_XCVR_EXT_ISR_CLR, isr_clr);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static const struct fsl_xcvr_soc_data fsl_xcvr_imx8mp_data = {
	.fw_name = "imx/xcvr/xcvr-imx8mp.bin",
	.use_phy = true,
	.pll_ver = PLL_MX8MP,
};

static const struct fsl_xcvr_soc_data fsl_xcvr_imx93_data = {
	.spdif_only = true,
	.use_edma = true,
};

static const struct fsl_xcvr_soc_data fsl_xcvr_imx95_data = {
	.fw_name = "imx/xcvr/xcvr-imx95.bin",
	.spdif_only = true,
	.use_phy = true,
	.use_edma = true,
	.pll_ver = PLL_MX95,
};

static const struct of_device_id fsl_xcvr_dt_ids[] = {
	{ .compatible = "fsl,imx8mp-xcvr", .data = &fsl_xcvr_imx8mp_data },
	{ .compatible = "fsl,imx93-xcvr", .data = &fsl_xcvr_imx93_data},
	{ .compatible = "fsl,imx95-xcvr", .data = &fsl_xcvr_imx95_data},
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, fsl_xcvr_dt_ids);

static int fsl_xcvr_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct fsl_xcvr *xcvr;
	struct resource *rx_res, *tx_res;
	void __iomem *regs;
	int ret, irq;

	xcvr = devm_kzalloc(dev, sizeof(*xcvr), GFP_KERNEL);
	if (!xcvr)
		return -ENOMEM;

	xcvr->pdev = pdev;
	xcvr->soc_data = of_device_get_match_data(&pdev->dev);

	xcvr->ipg_clk = devm_clk_get(dev, "ipg");
	if (IS_ERR(xcvr->ipg_clk)) {
		dev_err(dev, "failed to get ipg clock\n");
		return PTR_ERR(xcvr->ipg_clk);
	}

	xcvr->phy_clk = devm_clk_get(dev, "phy");
	if (IS_ERR(xcvr->phy_clk)) {
		dev_err(dev, "failed to get phy clock\n");
		return PTR_ERR(xcvr->phy_clk);
	}

	xcvr->spba_clk = devm_clk_get(dev, "spba");
	if (IS_ERR(xcvr->spba_clk)) {
		dev_err(dev, "failed to get spba clock\n");
		return PTR_ERR(xcvr->spba_clk);
	}

	xcvr->pll_ipg_clk = devm_clk_get(dev, "pll_ipg");
	if (IS_ERR(xcvr->pll_ipg_clk)) {
		dev_err(dev, "failed to get pll_ipg clock\n");
		return PTR_ERR(xcvr->pll_ipg_clk);
	}

	fsl_asoc_get_pll_clocks(dev, &xcvr->pll8k_clk,
				&xcvr->pll11k_clk);

	if (xcvr->soc_data->spdif_only) {
		if (!(xcvr->pll8k_clk || xcvr->pll11k_clk))
			xcvr->pll8k_clk = xcvr->phy_clk;
		fsl_asoc_constrain_rates(&xcvr->spdif_constr_rates,
					 &fsl_xcvr_spdif_rates_constr,
					 xcvr->pll8k_clk, xcvr->pll11k_clk, NULL,
					 xcvr->spdif_constr_rates_list);
	}

	xcvr->ram_addr = devm_platform_ioremap_resource_byname(pdev, "ram");
	if (IS_ERR(xcvr->ram_addr))
		return PTR_ERR(xcvr->ram_addr);

	regs = devm_platform_ioremap_resource_byname(pdev, "regs");
	if (IS_ERR(regs))
		return PTR_ERR(regs);

	xcvr->regmap = devm_regmap_init_mmio_clk(dev, NULL, regs,
						 &fsl_xcvr_regmap_cfg);
	if (IS_ERR(xcvr->regmap)) {
		dev_err(dev, "failed to init XCVR regmap: %ld\n",
			PTR_ERR(xcvr->regmap));
		return PTR_ERR(xcvr->regmap);
	}

	if (xcvr->soc_data->use_phy) {
		xcvr->regmap_phy = devm_regmap_init(dev, NULL, xcvr,
						    &fsl_xcvr_regmap_phy_cfg);
		if (IS_ERR(xcvr->regmap_phy)) {
			dev_err(dev, "failed to init XCVR PHY regmap: %ld\n",
				PTR_ERR(xcvr->regmap_phy));
			return PTR_ERR(xcvr->regmap_phy);
		}

		switch (xcvr->soc_data->pll_ver) {
		case PLL_MX8MP:
			xcvr->regmap_pll = devm_regmap_init(dev, NULL, xcvr,
							    &fsl_xcvr_regmap_pllv0_cfg);
			if (IS_ERR(xcvr->regmap_pll)) {
				dev_err(dev, "failed to init XCVR PLL regmap: %ld\n",
					PTR_ERR(xcvr->regmap_pll));
				return PTR_ERR(xcvr->regmap_pll);
			}
			break;
		case PLL_MX95:
			xcvr->regmap_pll = devm_regmap_init(dev, NULL, xcvr,
							    &fsl_xcvr_regmap_pllv1_cfg);
			if (IS_ERR(xcvr->regmap_pll)) {
				dev_err(dev, "failed to init XCVR PLL regmap: %ld\n",
					PTR_ERR(xcvr->regmap_pll));
				return PTR_ERR(xcvr->regmap_pll);
			}
			break;
		default:
			dev_err(dev, "Error for PLL version %d\n", xcvr->soc_data->pll_ver);
			return -EINVAL;
		}
	}

	xcvr->reset = devm_reset_control_get_optional_exclusive(dev, NULL);
	if (IS_ERR(xcvr->reset)) {
		dev_err(dev, "failed to get XCVR reset control\n");
		return PTR_ERR(xcvr->reset);
	}

	/* get IRQs */
	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return irq;

	ret = devm_request_irq(dev, irq, irq0_isr, 0, pdev->name, xcvr);
	if (ret) {
		dev_err(dev, "failed to claim IRQ0: %i\n", ret);
		return ret;
	}

	rx_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "rxfifo");
	tx_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "txfifo");
	if (!rx_res || !tx_res) {
		dev_err(dev, "could not find rxfifo or txfifo resource\n");
		return -EINVAL;
	}
	xcvr->dma_prms_rx.chan_name = "rx";
	xcvr->dma_prms_tx.chan_name = "tx";
	xcvr->dma_prms_rx.addr = rx_res->start;
	xcvr->dma_prms_tx.addr = tx_res->start;
	xcvr->dma_prms_rx.maxburst = FSL_XCVR_MAXBURST_RX;
	xcvr->dma_prms_tx.maxburst = FSL_XCVR_MAXBURST_TX;

	platform_set_drvdata(pdev, xcvr);
	pm_runtime_enable(dev);
	regcache_cache_only(xcvr->regmap, true);
	if (xcvr->soc_data->use_phy) {
		regcache_cache_only(xcvr->regmap_phy, true);
		regcache_cache_only(xcvr->regmap_pll, true);
	}

	/*
	 * Register platform component before registering cpu dai for there
	 * is not defer probe for platform component in snd_soc_add_pcm_runtime().
	 */
	ret = devm_snd_dmaengine_pcm_register(dev, NULL, 0);
	if (ret) {
		pm_runtime_disable(dev);
		dev_err(dev, "failed to pcm register\n");
		return ret;
	}

	ret = devm_snd_soc_register_component(dev, &fsl_xcvr_comp,
					      &fsl_xcvr_dai, 1);
	if (ret) {
		pm_runtime_disable(dev);
		dev_err(dev, "failed to register component %s\n",
			fsl_xcvr_comp.name);
	}

	INIT_WORK(&xcvr->work_rst, reset_rx_work);
	spin_lock_init(&xcvr->lock);
	return ret;
}

static void fsl_xcvr_remove(struct platform_device *pdev)
{
	struct fsl_xcvr *xcvr = dev_get_drvdata(&pdev->dev);

	cancel_work_sync(&xcvr->work_rst);
	pm_runtime_disable(&pdev->dev);
}

static int fsl_xcvr_runtime_suspend(struct device *dev)
{
	struct fsl_xcvr *xcvr = dev_get_drvdata(dev);
	int ret;

	if (!xcvr->soc_data->spdif_only &&
	    xcvr->mode == FSL_XCVR_MODE_EARC) {
		/* Assert M0+ reset */
		ret = regmap_update_bits(xcvr->regmap, FSL_XCVR_EXT_CTRL,
					FSL_XCVR_EXT_CTRL_CORE_RESET,
					FSL_XCVR_EXT_CTRL_CORE_RESET);
		if (ret < 0)
			dev_err(dev, "Failed to assert M0+ core: %d\n", ret);
	}

	regcache_cache_only(xcvr->regmap, true);
	if (xcvr->soc_data->use_phy) {
		regcache_cache_only(xcvr->regmap_phy, true);
		regcache_cache_only(xcvr->regmap_pll, true);
	}

	clk_disable_unprepare(xcvr->spba_clk);
	clk_disable_unprepare(xcvr->phy_clk);
	clk_disable_unprepare(xcvr->pll_ipg_clk);
	clk_disable_unprepare(xcvr->ipg_clk);

	return 0;
}

static int fsl_xcvr_runtime_resume(struct device *dev)
{
	struct fsl_xcvr *xcvr = dev_get_drvdata(dev);
	int ret;

	ret = reset_control_assert(xcvr->reset);
	if (ret < 0) {
		dev_err(dev, "Failed to assert M0+ reset: %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(xcvr->ipg_clk);
	if (ret) {
		dev_err(dev, "failed to start IPG clock.\n");
		return ret;
	}

	ret = clk_prepare_enable(xcvr->pll_ipg_clk);
	if (ret) {
		dev_err(dev, "failed to start PLL IPG clock.\n");
		goto stop_ipg_clk;
	}

	ret = clk_prepare_enable(xcvr->phy_clk);
	if (ret) {
		dev_err(dev, "failed to start PHY clock: %d\n", ret);
		goto stop_pll_ipg_clk;
	}

	ret = clk_prepare_enable(xcvr->spba_clk);
	if (ret) {
		dev_err(dev, "failed to start SPBA clock.\n");
		goto stop_phy_clk;
	}

	ret = reset_control_deassert(xcvr->reset);
	if (ret) {
		dev_err(dev, "failed to deassert M0+ reset.\n");
		goto stop_spba_clk;
	}

	regcache_cache_only(xcvr->regmap, false);
	regcache_mark_dirty(xcvr->regmap);
	ret = regcache_sync(xcvr->regmap);

	if (ret) {
		dev_err(dev, "failed to sync regcache.\n");
		goto stop_spba_clk;
	}

	if (xcvr->soc_data->use_phy) {
		ret = regmap_write(xcvr->regmap, FSL_XCVR_PHY_AI_CTRL_SET,
				   FSL_XCVR_PHY_AI_CTRL_AI_RESETN);
		if (ret < 0) {
			dev_err(dev, "Error while release PHY reset: %d\n", ret);
			goto stop_spba_clk;
		}

		regcache_cache_only(xcvr->regmap_phy, false);
		regcache_mark_dirty(xcvr->regmap_phy);
		ret = regcache_sync(xcvr->regmap_phy);
		if (ret) {
			dev_err(dev, "failed to sync phy regcache.\n");
			goto stop_spba_clk;
		}

		regcache_cache_only(xcvr->regmap_pll, false);
		regcache_mark_dirty(xcvr->regmap_pll);
		ret = regcache_sync(xcvr->regmap_pll);
		if (ret) {
			dev_err(dev, "failed to sync pll regcache.\n");
			goto stop_spba_clk;
		}
	}

	if (xcvr->soc_data->fw_name) {
		ret = fsl_xcvr_load_firmware(xcvr);
		if (ret) {
			dev_err(dev, "failed to load firmware.\n");
			goto stop_spba_clk;
		}

		/* Release M0+ reset */
		ret = regmap_update_bits(xcvr->regmap, FSL_XCVR_EXT_CTRL,
					 FSL_XCVR_EXT_CTRL_CORE_RESET, 0);
		if (ret < 0) {
			dev_err(dev, "M0+ core release failed: %d\n", ret);
			goto stop_spba_clk;
		}

		/* Let M0+ core complete firmware initialization */
		msleep(50);
	}

	return 0;

stop_spba_clk:
	clk_disable_unprepare(xcvr->spba_clk);
stop_phy_clk:
	clk_disable_unprepare(xcvr->phy_clk);
stop_pll_ipg_clk:
	clk_disable_unprepare(xcvr->pll_ipg_clk);
stop_ipg_clk:
	clk_disable_unprepare(xcvr->ipg_clk);

	return ret;
}

static const struct dev_pm_ops fsl_xcvr_pm_ops = {
	RUNTIME_PM_OPS(fsl_xcvr_runtime_suspend, fsl_xcvr_runtime_resume, NULL)
	SYSTEM_SLEEP_PM_OPS(pm_runtime_force_suspend, pm_runtime_force_resume)
};

static struct platform_driver fsl_xcvr_driver = {
	.probe = fsl_xcvr_probe,
	.driver = {
		.name = "fsl-xcvr",
		.pm = pm_ptr(&fsl_xcvr_pm_ops),
		.of_match_table = fsl_xcvr_dt_ids,
	},
	.remove = fsl_xcvr_remove,
};
module_platform_driver(fsl_xcvr_driver);

MODULE_AUTHOR("Viorel Suman <viorel.suman@nxp.com>");
MODULE_DESCRIPTION("NXP Audio Transceiver (XCVR) driver");
MODULE_LICENSE("GPL v2");
