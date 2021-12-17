#
# Copyright 2021 NXP
#
# SPDX-License-Identifier: BSD-3-Clause
#

include drivers/arm/gic/v3/gicv3.mk
include lib/libc/libc.mk
include lib/libfdt/libfdt.mk
include lib/xlat_tables_v2/xlat_tables.mk
include make_helpers/build_macros.mk

ERRATA_A53_855873	:= 1
ERRATA_A53_836870	:= 1
ERRATA_A53_1530924	:= 1
ERRATA_SPECULATIVE_AT	:= 1

# Tools
HEXDUMP ?= xxd
SED ?= sed

BL2_AT_EL3		:= 1

PLAT_INCLUDES 	+= \
			-Idrivers \
			-Iinclude/common/tbbr \
			-Iinclude/drivers \
			-Iinclude/drivers/nxp/s32g \
			-Iinclude/lib \
			-Iinclude/lib/libc \
			-Iinclude/lib/psci \
			-Iinclude/plat/arm/common \
			-Iinclude/plat/arm/soc/common \
			-Iinclude/plat/common \
			-Iplat/nxp/s32/include \

PLAT_BL_COMMON_SOURCES += \
			${GICV3_SOURCES} \
			common/fdt_wrappers.c \
			drivers/nxp/uart/linflexuart.c \
			plat/nxp/s32/s32_bl_common.c \
			plat/nxp/s32/s32_lowlevel_common.S \

BL2_SOURCES += \
			${XLAT_TABLES_LIB_SRCS} \
			common/desc_image_load.c \
			common/fdt_fixup.c \
			drivers/io/io_fip.c \
			drivers/io/io_storage.c \
			drivers/mmc/mmc.c \
			plat/nxp/s32/s32_bl2_el3.c \
			plat/nxp/s32/s32_lowlevel_bl2.S \

BL31_SOURCES += \
			${XLAT_TABLES_LIB_SRCS} \
			plat/common/plat_gicv3.c \
			plat/common/plat_psci_common.c \
			plat/nxp/s32/include/plat_macros.S \

DTC_FLAGS		+= -Wno-unit_address_vs_reg

all: check_dtc_version
check_dtc_version:
	$(eval DTC_VERSION_RAW = $(shell $(DTC) --version | cut -f3 -d" " \
							  | cut -f1 -d"-"))
	$(eval DTC_VERSION = $(shell echo $(DTC_VERSION_RAW) | sed "s/\./0/g"))
	@if [ ${DTC_VERSION} -lt 10406 ]; then \
		echo "$(DTC) version must be 1.4.6 or above"; \
		false; \
	fi

# Disable the PSCI platform compatibility layer
ENABLE_PLAT_COMPAT	:= 0

MULTI_CONSOLE_API	:= 1
LOAD_IMAGE_V2		:= 1
USE_COHERENT_MEM	:= 0

# Set RESET_TO_BL31 to boot from BL31
PROGRAMMABLE_RESET_ADDRESS	:= 1
RESET_TO_BL31			:= 0
# We need SMP boot in order to make specific initializations such as
# secure GIC registers, which U-Boot and then Linux won't be able to.
COLD_BOOT_SINGLE_CPU		:= 0

BL2_EL3_STACK_ALIGNMENT :=	512
$(eval $(call add_define_val,BL2_EL3_STACK_ALIGNMENT,$(BL2_EL3_STACK_ALIGNMENT)))

FDT_SOURCES             = $(addprefix fdts/, $(patsubst %.dtb,%.dts,$(DTB_FILE_NAME)))

### Devel & Debug options ###
ifeq (${DEBUG},1)
	CFLAGS			+= -O0
else
	CFLAGS			+= -Os
endif
# Enable dump of processor register state upon exceptions while running BL31
CRASH_REPORTING		:= 1
# As verbose as it can be
LOG_LEVEL		?= 50

# Reserve some space at the end of SRAM for external apps and include it
# in the calculation of FIP_BASE address.
EXT_APP_SIZE		:= 0x100000
$(eval $(call add_define,EXT_APP_SIZE))
FIP_MAXIMUM_SIZE	:= 0x300000
$(eval $(call add_define,FIP_MAXIMUM_SIZE))
# FIP offset from the far end of SRAM; leave it to the C code to perform
# the arithmetic
FIP_ROFFSET		:= "(EXT_APP_SIZE + FIP_MAXIMUM_SIZE)"
$(eval $(call add_define,FIP_ROFFSET))

BL2_W_DTB		:= ${BUILD_PLAT}/bl2_w_dtb.bin
all: ${BL2_W_DTB}
${BL2_W_DTB}: bl2 dtbs
	@cp ${BUILD_PLAT}/fdts/${DTB_FILE_NAME} $@
	@dd if=${BUILD_PLAT}/bl2.bin of=$@ bs=1024 seek=8 status=none

FIP_ALIGN := 512
all: add_to_fip
add_to_fip: fip ${BL2_W_DTB}
	$(eval FIP_MAXIMUM_SIZE_10 = $(shell printf "%d\n" ${FIP_MAXIMUM_SIZE}))
	${Q}${FIPTOOL} update ${FIP_ARGS} \
		--tb-fw ${BUILD_PLAT}/bl2_w_dtb.bin \
		--soc-fw-config ${BUILD_PLAT}/fdts/${DTB_FILE_NAME} \
		${BUILD_PLAT}/${FIP_NAME}
	@echo "Added BL2 and DTB to ${BUILD_PLAT}/${FIP_NAME} successfully"
	${Q}${FIPTOOL} info ${BUILD_PLAT}/${FIP_NAME}
	$(eval ACTUAL_FIP_SIZE = $(shell \
				stat --printf="%s" ${BUILD_PLAT}/${FIP_NAME}))
	@if [ ${ACTUAL_FIP_SIZE} -gt ${FIP_MAXIMUM_SIZE_10} ]; then \
		echo "FIP image exceeds the maximum size of" \
		     "0x${FIP_MAXIMUM_SIZE}"; \
		false; \
	fi

DTB_BASE		:= 0x34300000
$(eval $(call add_define,DTB_BASE))
BL2_BASE		:= 0x34302000
$(eval $(call add_define,BL2_BASE))
MKIMAGE_CFG ?= u-boot.cfgout

all: call_mkimage
call_mkimage: add_to_fip
ifeq ($(MKIMAGE),)
	$(eval BL33DIR = $(shell dirname $(BL33)))
	$(eval MKIMAGE = $(BL33DIR)/tools/mkimage)
endif
	@cd ${BL33DIR} && \
		${MKIMAGE} -e ${BL2_BASE} -a ${DTB_BASE} -T s32gen1image \
		-n ${MKIMAGE_CFG} -d ${BUILD_PLAT}/${FIP_NAME} \
		${BUILD_PLAT}/fip.s32
	@echo "Generated ${BUILD_PLAT}/fip.s32 successfully"

# If BL32_EXTRA1 option is present, include the binary it is pointing to
# in the FIP image
ifneq ($(BL32_EXTRA1),)
$(eval $(call TOOL_ADD_IMG,bl32_extra1,--tos-fw-extra1))
endif