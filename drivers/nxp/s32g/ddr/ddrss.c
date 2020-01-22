/*
 * Copyright 2019 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdbool.h>
#include <lib/utils_def.h>
#include <common/debug.h>
#include <nxp/s32g/ddr/ddrss.h>
#include "s32g_mc_rgm.h"

static inline void write_regconf_16(struct regconf *rc, size_t length)
{
	size_t i;

	for (i = 0; i < length; i++)
		mmio_write_16(DDRSS_BASE_ADDR + rc[i].addr, rc[i].data);
}

static inline void write_regconf_32(struct regconf *rc, size_t length)
{
	size_t i;

	for (i = 0; i < length; i++)
		mmio_write_32(DDRSS_BASE_ADDR + rc[i].addr, rc[i].data);
}

static inline void deassert_ddr_reset(void)
{
	uint32_t rgm_prst_0;

	rgm_prst_0 = mmio_read_32(S32G_MC_RGM_PRST(0));
	rgm_prst_0 &= ~(BIT(3) | BIT(0));
	mmio_write_32(S32G_MC_RGM_PRST(0), rgm_prst_0);
	while (mmio_read_32(S32G_MC_RGM_PSTAT(0)) != rgm_prst_0)
		;
}

static uint16_t mailbox_read_mail(void)
{
	uint16_t mail;

	while (mmio_read_16(UCTSHADOWREGS) & UCTWRITEPROTSHADOW_MASK)
		;
	mail = mmio_read_16(UCTWRITEONLYSHADOW);
	mmio_write_16(DCTWRITEPROT, 0);
	while (!(mmio_read_16(UCTSHADOWREGS) & UCTWRITEPROTSHADOW_MASK))
		;
	mmio_write_16(DCTWRITEPROT, DCTWRITEPROT_MASK);

	return mail;
}

static bool run_firmware(void)
{
	mmio_write_8(HDTCTRL, HDTCTRL_FIRMWARE_COMPLETION);
	mmio_write_16(MICROCONTMUXSEL, MICROCONTMUXSEL_MASK);
	mmio_write_16(MICRORESET, RESETTOMICRO_MASK | STALLTOMICRO_MASK);
	mmio_write_16(MICRORESET, STALLTOMICRO_MASK);
	mmio_write_16(MICRORESET, 0);

	return (mailbox_read_mail() == MAIL_TRAINING_SUCCESS);
}

void ddrss_init(struct ddrss_conf *ddrss_conf,
		struct ddrss_firmware *ddrss_firmware)
{
	write_regconf_32(ddrss_conf->ddrc_conf, ddrss_conf->ddrc_conf_length);

	mmio_write_32(REG_GRP0, mmio_read_32(REG_GRP0) | AXI_PARITY_EN_MASK);
	mmio_write_32(REG_GRP0, mmio_read_32(REG_GRP0) | AXI_PARITY_TYPE_MASK);
	mmio_write_32(REG_GRP0, mmio_read_32(REG_GRP0) | DFI1_ENABLED_MASK);
	deassert_ddr_reset();
	mmio_write_32(DBG1, 0);
	mmio_write_32(SWCTL, 0);
	while (mmio_read_32(SWSTAT) & SW_DONE_ACK_MASK)
		;

	mmio_write_32(DFIMISC, CTL_IDLE_EN_MASK | DIS_DYN_ADR_TRI_MASK);
	while (mmio_read_32(DFIMISC) & DFI_INIT_COMPLETE_EN_MASK)
		;

	write_regconf_16(ddrss_conf->ddrphy_conf,
			 ddrss_conf->ddrphy_conf_length);
	write_regconf_16(ddrss_firmware->imem_1d,
			 ddrss_firmware->imem_1d_length);
	write_regconf_16(ddrss_firmware->dmem_1d,
			 ddrss_firmware->dmem_1d_length);

	mmio_write_32(MICROCONTMUXSEL, MICROCONTMUXSEL_MASK);
	mmio_write_32(PLLCTRL1_P0,
		      (0x1 << PLLCPINTCTRL_OFFSET) |
		      (0x1 << PLLCPPROPCTRL_OFFSET));
	mmio_write_32(PLLTESTMODE_P0, 0x24);
	mmio_write_32(PLLCTRL4_P0,
		      (0x1f << PLLCPINTGSCTRL_OFFSET) |
		      (0x5 << PLLCPPROPGSCTRL_OFFSET));

	mmio_write_16(DCTWRITEPROT, DCTWRITEPROT_MASK);
	mmio_write_16(UCTWRITEPROT, UCTWRITEPROT_MASK);

	if (!run_firmware()) {
		ERROR("ddrss: 1D firmware execution failed!\n");
		panic();
	}
	mmio_write_16(MICRORESET, STALLTOMICRO_MASK);
	write_regconf_16(ddrss_conf->pie, ddrss_conf->pie_length);
	while (mmio_read_16(CALBUSY) & CALBUSY_MASK)
		;

	mmio_write_32(SWCTL, 0);
	mmio_write_32(DFIMISC, mmio_read_32(DFIMISC) | DFI_INIT_START_MASK);
	mmio_write_32(SWCTL, SW_DONE_MASK);
	while (mmio_read_32(SWSTAT) != SW_DONE_ACK_MASK)
		;

	mmio_write_32(DFIMISC, mmio_read_32(DFIMISC) | DFI_INIT_START_MASK);
	mmio_write_32(SWCTL, 0);
	mmio_write_32(DFIMISC, mmio_read_32(DFIMISC) & (~DFI_INIT_START_MASK));
	mmio_write_32(DFIMISC,
		      mmio_read_32(DFIMISC) | DFI_INIT_COMPLETE_EN_MASK);
	mmio_write_32(PWRCTL, mmio_read_32(PWRCTL) & (~SELFREF_SW_MASK));
	mmio_write_32(SWCTL, SW_DONE_MASK);
	while (mmio_read_32(SWSTAT) != SW_DONE_ACK_MASK)
		;
	while ((mmio_read_32(STAT) & OPERATING_MODE_MASK) !=
			OPERATING_MODE_NORMAL)
		;
	while (mmio_read_32(DFISTAT) != DFI_INIT_COMPLETE_MASK)
		;

	mmio_write_32(RFSHCTL3,
		      mmio_read_32(RFSHCTL3) & (~DIS_AUTO_REFRESH_MASK));
	mmio_write_32(PWRCTL, mmio_read_32(PWRCTL) | SELFREF_EN_MASK);
	mmio_write_32(PWRCTL, mmio_read_32(PWRCTL) | POWERDOWN_EN_MASK);
	mmio_write_32(PWRCTL,
		      mmio_read_32(PWRCTL) | EN_DFI_DRAM_CLK_DISABLE_MASK);
	mmio_write_32(PCTRL_0, mmio_read_32(PCTRL_0) | PORT_EN_MASK);
	mmio_write_32(PCTRL_1, mmio_read_32(PCTRL_1) | PORT_EN_MASK);
	mmio_write_32(PCTRL_2, mmio_read_32(PCTRL_2) | PORT_EN_MASK);
}