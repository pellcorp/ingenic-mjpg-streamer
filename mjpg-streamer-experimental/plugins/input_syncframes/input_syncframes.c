/*******************************************************************************
# Linux-UVC streaming input-plugin for MJPG-streamer                           #
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <pthread.h>
#include <syslog.h>

#include <linux/types.h>          /* for videodev2.h */
#include <linux/videodev2.h>

#include <syncframe.h>
#include <v4l2-jpegenc.h>
#include "v4l2uvc.h" // this header will includes the ../../mjpg_streamer.h

#include "../../utils.h"

//#define SAVEIMGE
#ifdef SAVEIMGE
#define save_image(filename, width, height, size, buf)			\
    do{									\
        FILE *fd = fopen(filename, "w+");					\
        if (fd == 0)							\
        fprintf(stderr, "error open file %s %s\n", filename, strerror(errno)); \
        else {								\
            fwrite(buf, size, 1, fd);						\
            fclose(fd);							\
        }									\
    }while(0)
#else
#define save_image(filename, width, height, size, buf)
#endif

/* private functions and variables to this plugin */
static globals *pglobal;
static __u32 minimum_size = 0;
static int dynctrls = 1;
static __u32 every = 1;
static int wantTimestamp = 0;
static struct timeval timestamp;
static int softfps = -1;
static __u32 timeout = 5;
static __u32 dv_timings = 0;
static int mono_frame = 0;
static char camera0_name[32];
static char camera1_name[32];
static char jpeg_name[32];
static int y_only = 0;

void *sync_frame_thread(void *);
void sync_freame_cleanup(void *);
void help(void);
int input_cmd(int plugin, __u32 control, __u32 group, int value, char *value_string);

static void sync_frame_combination(void *output, void *inputl, void *inputr, __u32 width, __u32 height)
{
    int i;
    __u8 *out = (__u8 *)output;
    __u8 *inl = (__u8 *)inputl;
    __u8 *inr = (__u8 *)inputr;

    save_image("framel.yuv", 0, 0, width * height * 3 / 2, inl);
    save_image("framer.yuv", 0, 0, width * height * 3 / 2, inr);

    /*Y*/
    for(i = 0; i < height; i++) {
        memcpy(out, inl, width);
        memcpy(out + width, inr, width);
        inl += width;
        inr += width;
        out += width << 1;
    }

    /*UV*/
    if(y_only) {
	    memset(out, 0x80, width * height);
    } else {
	    for(i = 0; i < height / 2; i++) {
		    memcpy(out, inl, width);
		    memcpy(out + width, inr, width);
		    inl += width;
		    inr += width;
		    out += width << 1;
	    }
    }
}

static void sync_frame_mono(void *output, void *input, __u32 width, __u32 height)
{
    int i;
    __u8 *out = (__u8 *)output;
    __u8 *in = (__u8 *)input;

    save_image("frame.yuv", 0, 0, width * height * 3 / 2, in);
    height = height;

    /*Y*/
    for(i = 0; i < height; i++) {
        memcpy(out, in, width);
        in += width;
        out += width;
    }

    /*UV*/
    if(y_only) {
	memset(out, 0x80, width * height / 2);
    } else {
	    for(i = 0; i < height / 2; i++) {
		    memcpy(out, in, width);
		    in += width;
		    out += width;
	    }
    }

}


/*** plugin interface functions ***/
/******************************************************************************
  Description.: This function initializes the plugin. It parses the commandline-
  parameter and stores the default and parsed values in the
  struct camera_frame camera0_info;
  struct camera_frame camera1_info;
  struct frame_info frame_isp0;
  struct frame_info frame_isp1;
  appropriate variables.
  Input Value.: param contains among others the command-line string
  Return Value: 0 if everything is fine
  1 if "--help" was triggered, in this case the calling programm
  should stop running and leave.
 ******************************************************************************/

int input_init(input_parameter *param, int id)
{
    context *pctx = NULL;
    struct config *config = NULL;
    struct encode_config  *encode_config = NULL;
    int width, height, format, i, length;

    width = 640;
    height = 480;

    pctx = calloc(1, sizeof(context));
    if (pctx == NULL) {
        IPRINT("error allocating context");
        exit(EXIT_FAILURE);
    }
    config = (struct config *)malloc(sizeof(struct config));
    encode_config = (struct encode_config *)malloc(sizeof(struct encode_config));

    /* setup defaults. */
    strncpy(camera0_name, "/dev/video3", sizeof(camera0_name));
    strncpy(camera1_name, "/dev/video4", sizeof(camera1_name));
    strncpy(jpeg_name, "/dev/video1", sizeof(jpeg_name));

    pglobal = param->global;
    pglobal->in[id].context = pctx;

    /* initialize the mutes variable */
    if(pthread_mutex_init(&pctx->controls_mutex, NULL) != 0) {
        IPRINT("could not initialize mutex variable\n");
        exit(EXIT_FAILURE);
    }

    /* show all parameters for DBG purposes */
    for(i = 0; i < param->argc; i++) {
        DBG("argv[%d]=%s\n", i, param->argv[i]);
    }

    reset_getopt();
    while(1) {
        int option_index = 0, c = 0;
        static struct option long_options[] = {
            {"h", no_argument, 0, 0},
            {"help", no_argument, 0, 0},
            {"r", required_argument, 0, 0},
            {"resolution", required_argument, 0, 0},
            {"u", no_argument, 0, 0},
            {"nv12", no_argument, 0, 0},
            {"v", no_argument, 0, 0},
            {"nv21", no_argument, 0, 0},
            {"m", no_argument, 0, 0},
            {"mono", no_argument, 0, 0},
            {"camera_device0", required_argument, 0, 0},
            {"camera_device1", required_argument, 0, 0},
            {"jpeg_device", required_argument, 0, 0},
            {"y_only", no_argument, 0, 0},
            {0, 0, 0, 0}
        };

        /* parsing all parameters according to the list above is sufficent */
        c = getopt_long_only(param->argc, param->argv, "", long_options, &option_index);

        /* no more options to parse */
        if(c == -1) break;

        /* unrecognized option */
        if(c == '?') {
            help();
            return 1;
        }

        /* dispatch the given options */
        switch(option_index) {
            /* h, help */
            case 0:
            case 1:
                DBG("case 0,1\n");
                help();
                return 1;
                break;

                /* r, resolution */
            case 2:
            case 3:
                DBG("case 4,5\n");
                parse_resolution_opt(optarg, &width, &height);
                break;

                /* u, nv12 */
            case 4:
            case 5:
                DBG("case 8,9\n");
                format = V4L2_PIX_FMT_NV12;
                break;

                /* v, nv21 */
            case 6:
            case 7:
                DBG("case 10,11\n");
                format = V4L2_PIX_FMT_NV21;
                break;
	    case 8:
	    case 9:
		mono_frame = 1;
		break;
		/* camera device0*/
	    case 10:
		strncpy(camera0_name, optarg, sizeof(camera0_name));
		break;
		/* camera device1*/
	    case 11:
		strncpy(camera1_name, optarg, sizeof(camera1_name));
		break;

		/* jpeg device*/
	    case 12:
		strncpy(jpeg_name, optarg, sizeof(jpeg_name));
		break;
	    case 13:
		y_only = 1;
		break;

            default:
                DBG("default case\n");
                help();
                return 1;
        }
    }

    printf("camera0_name: %s\n", camera0_name);
    printf("camera1_name: %s\n", camera1_name);
    printf("jpeg_name: %s\n", jpeg_name);
    printf("mono_frame: %d\n", mono_frame);
    format = V4L2_PIX_FMT_NV12;
    DBG("input id: %d\n", id);
    pctx->id = id;
    pctx->pglobal = param->global;

    /* allocate webcam datastructure */
    pctx->videoIn = calloc(1, sizeof(struct vdIn));
    if(pctx->videoIn == NULL) {
        IPRINT("not enough memory for videoIn\n");
        exit(EXIT_FAILURE);
    }

    pctx->videoIn->config = config;
    pctx->videoIn->encode_config = encode_config;

    pctx->videoIn->config->width = width;
    pctx->videoIn->config->height = height;
    pctx->videoIn->config->pixfmt = format;

    if(mono_frame) {
	    pctx->videoIn->config->camera_name = camera0_name;
    } else {
	    pctx->videoIn->config->camera_r_name = camera0_name;
	    pctx->videoIn->config->camera_l_name = camera1_name;
    }
    pctx->videoIn->config->bcount = 3;

    if(mono_frame) {
	    pctx->videoIn->encode_config->width = width;
    } else {
	    /* dual camera. */
	    pctx->videoIn->encode_config->width = width * 2;
    }
    pctx->videoIn->encode_config->height = height;
    pctx->videoIn->encode_config->pixfmt = format;
    pctx->videoIn->encode_config->name = "/dev/video1";
    /* display the parsed values */
    char *fmtString = NULL;
    switch (format) {
#ifndef NO_LIBJPG
        case V4L2_PIX_FMT_NV12:
            fmtString = "NV12";
            break;
        case V4L2_PIX_FMT_NV21:
            fmtString = "NV21";
            break;
#endif
        default:
            fmtString = "Unknown format";
    }

    IPRINT("Format............: %s\n", fmtString);
#ifndef NO_LIBJPEG
#endif

    DBG("vdIn pn: %d\n", id);
    /* open video device and prepare data structure */

    if(pctx->videoIn->config == NULL){
        fprintf(stderr, "could not allocate memory\n");
        return -1;
    }

    IPRINT("cleaning up resources allocated by input thread\n");
    pctx->videoIn->width = width;
    pctx->videoIn->height = height;
    pctx->videoIn->formatIn = format;
    pctx->videoIn->framesizeIn = width * height * 3 / 2;

    if(mono_frame) {
	    pglobal->in[id]._videow = pctx->videoIn->width;
    } else {
	    pglobal->in[id]._videow = pctx->videoIn->width * 2;
    }
    pglobal->in[id]._videoh = pctx->videoIn->height;
    pglobal->in[id]._videofmt = pctx->videoIn->formatIn;

    if (cameras_init(mono_frame == 1 ? MONOFRAME : SYNCFRAME, pctx->videoIn->config)) {
        fprintf(stderr, "cameras_init failed\n");
        return 0;
    }

    if(jpeg_enc_init(pctx->videoIn->encode_config) == -1) {
        free(pctx->videoIn->config);
        free(pctx->videoIn->encode_config);
        fprintf(stderr, "jpeg_enc_init\n");
        return -1;
    }

    return 0;
}

/******************************************************************************
  Description.: Stops the execution of worker thread
  Input Value.: -
  Return Value: always 0
 ******************************************************************************/
int input_stop(int id)
{
    input * in = &pglobal->in[id];
    context *pctx = (context*)in->context;

    DBG("will cancel camera thread #%02d\n", id);
    pthread_cancel(pctx->threadID);
    return 0;
}

/******************************************************************************
  Description.: spins of a worker thread
  Input Value.: -
  Return Value: always 0
 ******************************************************************************/
int input_run(int id)
{
    input * in = &pglobal->in[id];
    context *pctx = (context*)in->context;

    in->buf = malloc(pctx->videoIn->framesizeIn << 1);
    if(in->buf == NULL) {
        fprintf(stderr, "could not allocate memory\n");
        exit(EXIT_FAILURE);
    }
    in->_videosize = pctx->videoIn->framesizeIn << 1;
    in->_userdata = malloc(in->_videosize);
    if(in->_userdata == NULL) {
        free(in->buf);
        fprintf(stderr, "could not allocate memory\n");
        exit(EXIT_FAILURE);
    }

    DBG("launching camera thread #%02d\n", id);
    /* create thread and pass context to thread function */
    pthread_create(&(pctx->threadID), NULL, sync_frame_thread, in);
    pthread_detach(pctx->threadID);
    return 0;
}

/*** private functions for this plugin below ***/
/******************************************************************************
  Description.: print a help message to stderr
  Input Value.: -
  Return Value: -
 ******************************************************************************/
void help(void)
{
    int i;

    resolutions_help("                          ");

    fprintf(stderr,
            " [-f | --fps ]..........: frames per second\n" \
            "                          (camera may coerce to different value)\n" \
            " [-q | --quality ] .....: set quality of JPEG encoding\n" \
            " [-m | --minimum_size ].: drop frames smaller then this limit, useful\n" \
            "                          if the webcam produces small-sized garbage frames\n" \
            "                          may happen under low light conditions\n" \
            " [-e | --every_frame ]..: drop all frames except numbered\n" \
            " [-n | --no_dynctrl ]...: do not initalize dynctrls of Linux-UVC driver\n" \

            " [-l | --led ]..........: switch the LED \"on\", \"off\", let it \"blink\" or leave\n" \
            "                          it up to the driver using the value \"auto\"\n" \
            " [-t | --tvnorm ] ......: set TV-Norm pal, ntsc or secam\n" \
            " [-u | --uyvy ] ........: Use UYVY format, default: MJPEG (uses more cpu power)\n" \
            " [-y | --yuv  ] ........: Use YUV format, default: MJPEG (uses more cpu power)\n" \
            " ---------------------------------------------------------------\n");


}

/******************************************************************************
  Description.: this thread worker grabs a frame and copies it to the global buffer
  Input Value.: unused
  Return Value: unused, always NULL
 ******************************************************************************/
void *sync_frame_thread(void *arg)
{
    int i = 0;
    char name[12] = {0};
    input * in = (input*)arg;
    context *pcontext = (context*)in->context;
    __u8 *inputl = NULL;
    __u8 *inputr = NULL;
    __u8 *input = NULL;
    __u32 every_count = 0;

    /* set cleanup handler to cleanup allocated resources */
    pthread_cleanup_push(sync_freame_cleanup, in);

    while(!pglobal->stop) {

	    if(mono_frame) {
		    if((pcontext->videoIn->monoframe = dqbuf_monoframe(pcontext->videoIn->config)) == NULL)
			    goto endloop;

		    input = pcontext->videoIn->monoframe->camera_buf;
	    } else {
		    if((pcontext->videoIn->syncframe = dqbuf_syncframe(pcontext->videoIn->config)) == NULL)
			    goto endloop;

		    inputl = pcontext->videoIn->syncframe->Lcamera.camera_buf;
		    inputr = pcontext->videoIn->syncframe->Rcamera.camera_buf;
	    }

        /* copy JPG picture to global buffer */
        pthread_mutex_lock(&pglobal->in[pcontext->id].db);

        /*
         * If capturing in YUV mode convert to JPEG now.
         * This compression requires many CPU cycles, so try to avoid YUV format.
         * Getting JPEGs straight from the webcam, is one of the major advantages of
         * Linux-UVC compatible devices.
         */
#ifndef NO_LIBJPEG
        if ((pcontext->videoIn->formatIn == V4L2_PIX_FMT_NV12) ||
                (pcontext->videoIn->formatIn == V4L2_PIX_FMT_NV21)){
            DBG("compressing frame from input: %d size %d\n", (int)pcontext->id, pcontext->videoIn->framesizeIn);

	    if(mono_frame) {
		    sync_frame_mono(pglobal->in[pcontext->id]._userdata,
				    (void *)input, pcontext->videoIn->width, pcontext->videoIn->height);
		    qbuf_monoframe(pcontext->videoIn->config, pcontext->videoIn->monoframe);

	    } else {
		    sync_frame_combination(pglobal->in[pcontext->id]._userdata,
				    (void *)inputl, (void *)inputr, pcontext->videoIn->width, pcontext->videoIn->height);
		    qbuf_syncframe(pcontext->videoIn->config, pcontext->videoIn->syncframe);

	    }

            pcontext->videoIn->encode_config->output_buf = pglobal->in[pcontext->id].buf;
            pcontext->videoIn->encode_config->input_buf = pglobal->in[pcontext->id]._userdata;

            pglobal->in[pcontext->id].size = jpeg_enc_start(pcontext->videoIn->encode_config);

            /* copy this frame's timestamp to user space */
            pglobal->in[pcontext->id].timestamp = pcontext->videoIn->tmptimestamp;
        } else {
            IPRINT("Frame format error\n");
        }
#endif

        /* signal fresh_frame */
        pthread_cond_broadcast(&pglobal->in[pcontext->id].db_update);
        pthread_mutex_unlock(&pglobal->in[pcontext->id].db);
    }

endloop:

    DBG("leaving input thread, calling cleanup function now\n");
    pthread_cleanup_pop(1);

    return NULL;
}

/******************************************************************************
  Description.:
  Input Value.:
  Return Value:
 ******************************************************************************/
void sync_freame_cleanup(void *arg)
{
    input * in = (input*)arg;
    context *pctx = (context*)in->context;

    IPRINT("cleaning up resources allocated by input thread\n");

    if (pctx->videoIn != NULL) {
        uninit_video(pctx->videoIn->config);
        jpeg_enc_stop(pctx->videoIn->encode_config);
        free(pctx->videoIn);
        pctx->videoIn = NULL;
    }

    free(in->buf);
    in->buf = NULL;
    in->size = 0;
    free(in->_userdata);
    in->_userdata = NULL;
    in->_videow = 0;
    in->_videoh = 0;
    in->_videosize = 0;
}
