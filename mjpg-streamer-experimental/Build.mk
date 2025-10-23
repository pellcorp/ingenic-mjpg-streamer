LOCAL_PATH := $(my-dir)

include $(CLEAR_VARS)
CMAKE_PATH=$(LOCAL_PATH)
LOCAL_MODULE:= mjpg-streamer
LOCAL_MODULE_TAGS :=optional
#LOCAL_DEPANNER_MODULES := stereo-demo
LOCAL_DEPANNER_MODULES += sync-frames
LOCAL_DEPANNER_MODULES += mjpg-streamer-start
include $(BUILD_CMAKE_DEVICE)


#===mjpg-streamer start.sh============
include $(CLEAR_VARS)
LOCAL_MODULE := mjpg-streamer-start
LOCAL_MODULE_TAGS :=optional
LOCAL_MODULE_PATH := $(TARGET_FS_BUILD)/$(TARGET_TESTSUIT_DIR)
LOCAL_COPY_FILES := mjpg-streamer-start.sh:mjpg-streamer-start.sh\
		dual-mjpg-streamer-start.sh:dual-mjpg-streamer-start.sh
include $(BUILD_MULTI_COPY)
