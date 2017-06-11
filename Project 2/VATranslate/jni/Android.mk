LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := VATranslate.c   # your source code
LOCAL_MODULE := VATranslate    # output file name
LOCAL_CFLAGS += -pie -fPIE   # These two line mustn’t be
LOCAL_LDFLAGS += -pie -fPIE  # change.
LOCAL_FORCE_STATIC_EXECUTABLE := true

include $(BUILD_EXECUTABLE)
