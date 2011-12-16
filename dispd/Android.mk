LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=                \
                  dispd.c \
                  disp.c \
                  dispmgr.c \
                  switch.c \
		  dvi_detection.c \
		  hdmi_detection.c \
                  uevent.c \
                  cmd_dispatch.c

LOCAL_MODULE:= dispd

LOCAL_C_INCLUDES := $(KERNEL_HEADERS)

LOCAL_CFLAGS := 

LOCAL_SHARED_LIBRARIES := libcutils
LOCAL_MODULE_TAGS := eng
include $(BUILD_EXECUTABLE)
