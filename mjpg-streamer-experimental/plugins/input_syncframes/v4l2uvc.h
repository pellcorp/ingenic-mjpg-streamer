/*******************************************************************************
# Linuc-UVC streaming input-plugin for MJPG-streamer                           #
#                                                                              #
# This package work with the Logitech UVC based webcams with the mjpeg feature #
#                                                                              #
# Copyright (C) 2005 2006 Laurent Pinchart &&  Michel Xhaard                   #
#                    2007 Lucas van Staden                                     #
#                    2007 Tom Stöveken                                         #
#                                                                              #
# This program is free software; you can redistribute it and/or modify         #
# it under the terms of the GNU General Public License as published by         #
# the Free Software Foundation; either version 2 of the License, or            #
# (at your option) any later version.                                          #
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
 *******************************************************************************/

#ifndef V4L2_UVC_H
#define V4L2_UVC_H


#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>

#include "../../mjpg_streamer.h"

#include <syncframe.h>
#include <v4l2-jpegenc.h>


struct vdIn {
    int width;
    int height;
    int formatIn;
    int framesizeIn;
    struct timeval tmptimestamp;
    struct config *config;
    struct encode_config *encode_config;
    struct cameras_buf *syncframe;
    struct buf_info *monoframe;
};

/* optional initial settings */
typedef struct {
    int quality_set, quality,
        sh_set, sh,
        co_set, co,
        br_set, br_auto, br,
        sa_set, sa,
        wb_set, wb_auto, wb,
        ex_set, ex_auto, ex,
        bk_set, bk,
        rot_set, rot,
        hf_set, hf,
        vf_set, vf,
        pl_set, pl,
        gain_set, gain_auto, gain,
        cagc_set, cagc_auto, cagc,
        cb_set, cb_auto, cb;
} context_settings;

/* context of each camera thread */
typedef struct {
    int id;
    globals *pglobal;
    pthread_t threadID;
    pthread_mutex_t controls_mutex;
    struct vdIn *videoIn;
    context_settings *init_settings;
} context;

#endif
