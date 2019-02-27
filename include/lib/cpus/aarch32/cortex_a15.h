/*
 * Copyright (c) 2017-2019, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CORTEX_A15_H
#define CORTEX_A15_H

#include <lib/utils_def.h>

/*******************************************************************************
 * Cortex-A15 midr with version/revision set to 0
 ******************************************************************************/
#define CORTEX_A15_MIDR			U(0x410FC0F0)

/*******************************************************************************
 * CPU Auxiliary Control register specific definitions.
 ******************************************************************************/
#define CORTEX_A15_ACTLR_INV_BTB_BIT	(U(1) << 0)
#define CORTEX_A15_ACTLR_SMP_BIT	(U(1) << 6)

#endif /* CORTEX_A15_H */
