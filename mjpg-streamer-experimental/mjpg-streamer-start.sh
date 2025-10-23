#!/bin/sh

#/******************************************************************************
#                                                                              #
#      MJPG-streamer allows to stream JPG frames from an input-plugin          #
#      to several output plugins                                               #
#                                                                              #
#      Copyright (C) 2007 Tom Stöveken                                         #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; version 2 of the License.                      #
#                                                                              #
# This program is distributed in the hope that it will be useful,              #
# but WITHOUT ANY WARRANTY; without even the implied warranty of               #
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                #
# GNU General Public License for more details.                                 #
#                                                                              #
# You should have received a copy of the GNU General Public License            #
# along with this program; if not, write to the Free Software                  #
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA    #
#                                                                              #
#******************************************************************************/

## This example shows how to invoke mjpg-streamer from the command line

## using example
## 1. ifconfig eth0 192.168.4.85
## 2. ./mjpg-streamer-start.sh
## 3. Open the web page, then input 192.168.4.85:8080/stream.html

export LD_LIBRARY_PATH="$(pwd)"

# eg: args for input:
# -camera_device0 /dev/video3
# -camera_device1 /dev/video4
# -jpeg_device /dev/video1
# -m use mono frame, when for mono_frame, only camera_device0 is active.

#mjpg_streamer -i "/usr/lib/mjpg-streamer/input_syncframes.so -r 1280x720" -o "/usr/lib/mjpg-streamer/output_http.so -w /usr/share/mjpg-streamer/www"

mjpg_streamer -i "/usr/lib/mjpg-streamer/input_syncframes.so -r 1280x720 -m -camera_device0 /dev/video6" -o "/usr/lib/mjpg-streamer/output_http.so -w /usr/share/mjpg-streamer/www"

# using soft jpeg encoder.
#mjpg_streamer -i "/usr/lib/mjpg-streamer/input_softjpeg -r 1280x720 -m -camera_device0 /dev/video6 --quality 80" -o "/usr/lib/mjpg-streamer/output_http.so -w /usr/share/mjpg-streamer/www"



