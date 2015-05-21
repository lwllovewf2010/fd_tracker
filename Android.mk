LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

define all-c++-files-under
$(patsubst ./%,%, \
$(shell cd $(LOCAL_PATH) ; \
find -L $(1) -name "*.cpp" -and -not -name ".*") \
)
endef

LOCAL_SRC_FILES = $(call all-c++-files-under, ./)

LOCAL_CFLAGS := -std=gnu++11

LOCAL_C_INCLUDES += \
	art/runtime/ \
	external/libcxx/include \
	external/openssl/include \

LOCAL_SHARED_LIBRARIES := \
	libart \
	liblog \
	libdl \
	libutils \
	libc++ \
	libcrypto \
	libcutils \

LOCAL_MODULE := libfd_tracker

include $(BUILD_SHARED_LIBRARY)
