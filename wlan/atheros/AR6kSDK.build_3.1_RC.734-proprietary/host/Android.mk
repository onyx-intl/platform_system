#------------------------------------------------------------------------------
# <copyright file="makefile" company="Atheros">
#    Copyright (c) 2005-2010 Atheros Corporation.  All rights reserved.
# $ATH_LICENSE_HOSTSDK0_C$
#------------------------------------------------------------------------------
#==============================================================================
# Author(s): ="Atheros"
#==============================================================================

ifneq ($(TARGET_SIMULATOR),true)

LOCAL_PATH:= $(call my-dir)

define include-ar6k-prebuilt
    include $$(CLEAR_VARS)
    LOCAL_MODULE := $(4)
    LOCAL_MODULE_STEM := $(3)
    LOCAL_MODULE_TAGS := debug eng optional
    LOCAL_MODULE_CLASS := ETC
    LOCAL_MODULE_PATH := $(2)
    LOCAL_SRC_FILES := $(1)
    include $$(BUILD_PREBUILT)
endef

define add-ar6k-prebuilt-file
    $(eval $(include-ar6k-prebuilt))
endef

# HW2.1.1 firmware

ar6k_hw21_src_dir := ../target/AR6003/hw2.1.1/bin/
ar6k_hw21_dst_dir := $(TARGET_OUT)/wifi/ath6k/AR6003/hw2.1.1

$(call add-ar6k-prebuilt-file,$(ar6k_hw21_src_dir)/athwlan.bin,$(ar6k_hw21_dst_dir),athwlan.bin,athwlan221)
$(call add-ar6k-prebuilt-file,$(ar6k_hw21_src_dir)/athwlan_router.bin,$(ar6k_hw21_dst_dir),athwlan_router.bin,athwlan_router221)
$(call add-ar6k-prebuilt-file,$(ar6k_hw21_src_dir)/athwlan_mobile.bin,$(ar6k_hw21_dst_dir),athwlan_mobile.bin,athwlan_mobile221)
$(call add-ar6k-prebuilt-file,$(ar6k_hw21_src_dir)/athwlan_tablet.bin,$(ar6k_hw21_dst_dir),athwlan_tablet.bin,athwlan_tablet221)
$(call add-ar6k-prebuilt-file,$(ar6k_hw21_src_dir)/data.patch.hw3_0.bin,$(ar6k_hw21_dst_dir),data.patch.hw3_0.bin,athpatch221)
$(call add-ar6k-prebuilt-file,$(ar6k_hw21_src_dir)/otp.bin,$(ar6k_hw21_dst_dir),otp.bin,athotp221)
$(call add-ar6k-prebuilt-file,$(ar6k_hw21_src_dir)/athtcmd_ram.bin,$(ar6k_hw21_dst_dir),athtcmd_ram.bin,athtcmd221)
$(call add-ar6k-prebuilt-file,$(ar6k_hw21_src_dir)/device.bin,$(ar6k_hw21_dst_dir),device.bin,athdevice221)

$(call add-ar6k-prebuilt-file,support/fakeBoardData_AR6003_v2_0.bin,$(ar6k_hw21_dst_dir),bdata.SD31.bin,athdata221)

ar6k_hw21_src_dir :=
ar6k_hw21_dst_dir :=

-include $(LOCAL_PATH)/tools/Android.mk

endif
