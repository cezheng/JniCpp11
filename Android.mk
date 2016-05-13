LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := jnicpp11_static
LOCAL_MODULE_FILENAME := libjnicpp11

LOCAL_SRC_FILES := JniCpp11.cpp
LOCAL_CPP_FEATURES += exceptions
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)
LOCAL_C_INCLUDES := $(LOCAL_PATH) \
  $(LOCAL_PATH)/../../cocos \
  $(LOCAL_PATH)/../../cocos/platform/android \

include $(BUILD_STATIC_LIBRARY)

