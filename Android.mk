LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

define all-c++-files-under
$(patsubst ./%,%, \
$(shell cd $(LOCAL_PATH) ; \
find -L $(1) -name "*.cpp" -and -not -name ".*") \
)
endef

LOCAL_SRC_FILES = $(call all-c++-files-under, ./)

# LOCAL_C_INCLUDES += \


LOCAL_SHARED_LIBRARIES := \
	liblog \
	libdl \
	libutils \

LOCAL_MODULE := libfd_tracker

include $(BUILD_SHARED_LIBRARY)
