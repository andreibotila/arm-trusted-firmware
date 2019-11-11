/*
 * Copyright (c) 2020, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PLATFORM_DEF_H
#define PLATFORM_DEF_H

#include <arch.h>
#include "../fpga_def.h"

#define PLATFORM_LINKER_FORMAT		"elf64-littleaarch64"

#define PLATFORM_LINKER_ARCH		aarch64

#define PLATFORM_STACK_SIZE		UL(0x800)

#define CACHE_WRITEBACK_SHIFT		U(6)
#define CACHE_WRITEBACK_GRANULE		(U(1) << CACHE_WRITEBACK_SHIFT)

#define PLATFORM_CORE_COUNT \
	(FPGA_MAX_CLUSTER_COUNT * FPGA_MAX_CPUS_PER_CLUSTER * FPGA_MAX_PE_PER_CPU)

#define PLAT_NUM_PWR_DOMAINS		(FPGA_MAX_CLUSTER_COUNT + \
					PLATFORM_CORE_COUNT) + 1

#define BL31_BASE			UL(0x80000000)
#define BL31_LIMIT			UL(0x80100000)

#define PLAT_MAX_RET_STATE		1
#define PLAT_MAX_OFF_STATE		2

#define PLAT_MAX_PWR_LVL		MPIDR_AFFLVL2

#define PLAT_FPGA_CONSOLE_BAUDRATE	38400

#endif
