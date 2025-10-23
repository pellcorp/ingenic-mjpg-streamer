mjpg-streamer input plugin: input_uvc
=====================================

This plugin provides JPEG data from V4L/V4L2 compatible webcams.

Usage
=====

    mjpg_streamer -i 'input_syncframes.so [options]'
    
```
---------------------------------------------------------------
Help for input plugin..: UVC webcam grabber
---------------------------------------------------------------
The following parameters can be passed to this plugin:

[-r | --resolution ]...: the resolution of the video device,
                         can be one of the following strings:
                         QSIF QCIF CGA QVGA CIF VGA 
                         SVGA XGA SXGA 
                         or a custom value like the following
                         example: 640x480
---------------------------------------------------------------

```
