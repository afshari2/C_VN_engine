LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := main
LOCAL_SRC_FILES := src/main.c src/model.c src/view.c src/controller.c
LOCAL_SHARED_LIBRARIES := SDL2 SDL2_image SDL2_ttf
LOCAL_CFLAGS := -Wall
LOCAL_LDLIBS := -llog -landroid

include $(BUILD_SHARED_LIBRARY)
