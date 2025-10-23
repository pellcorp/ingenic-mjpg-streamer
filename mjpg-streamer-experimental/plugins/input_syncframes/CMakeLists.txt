check_include_files(syncframe.h HAVE_SYNCFRAME_H)

MJPG_STREAMER_PLUGIN_OPTION(input_syncframes "syncframes input plugin"
    ONLYIF HAVE_SYNCFRAME_H)

add_definitions(-DLINUX -D_GNU_SOURCE)
link_libraries(syncframe v4l2enc)

MJPG_STREAMER_PLUGIN_COMPILE(input_syncframes input_syncframes.c)
