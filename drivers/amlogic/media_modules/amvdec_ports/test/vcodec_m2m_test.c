/*
* Copyright (C) 2017 Amlogic, Inc. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License along
* with this program; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*
* Description:
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>

#define INBUF_SIZE	(4096)
#define DUMP_DIR	"/mnt/video_frames"

typedef struct VcodecCtx {
	AVFormatContext *fmt_ctx;
	AVStream *stream;
	int nb_streams;
	int vst_idx;
	char *filename;
	pthread_t tid;
	pthread_mutex_t pthread_mutex;
	pthread_cond_t pthread_cond;
} VcodecCtx;

static void pgm_save(unsigned char *buf, int wrap, int xsize, int ysize,
                     char *filename)
{
	FILE *f;
	int i;

	printf("wrap: %d, xsize: %d, ysize: %d, filename: %s\n", wrap, xsize, ysize, filename);

	f = fopen(filename,"w+");
	fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);

	for (i = 0; i < ysize; i++)
		fwrite(buf + i * wrap, 1, xsize, f);
	fclose(f);
}

static void decode(AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt,
                   const char *filename)
{
	char buf[1024];
	int ret;

	ret = avcodec_send_packet(dec_ctx, pkt);
	if (ret < 0) {
		fprintf(stderr, "Error sending a packet for decoding\n");
		return;
	}

	while (ret >= 0) {
		ret = avcodec_receive_frame(dec_ctx, frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			return;
		else if (ret < 0) {
			fprintf(stderr, "Error during decoding, ret: %s\n", av_err2str(ret));
			break;
		}

		//fprintf(stderr, "saving frame %3d\n", dec_ctx->frame_number);
		fflush(stdout);

		/* the picture is allocated by the decoder. no need to free it */
		snprintf(buf, sizeof(buf), "%s/frame-%d", filename, dec_ctx->frame_number);
		pgm_save(frame->data[0], frame->linesize[0],
			frame->width, frame->height, buf);
	}
}

static void* read_thread(void *arg)
{
	int ret, err;
	AVFormatContext *ic = NULL;
	AVCodecContext *dec_ctx = NULL;
	AVStream *stream = NULL;
	AVCodec *codec = NULL;
	AVPacket pkt1, *pkt = &pkt1;
	AVFrame *frame = NULL;
	int vst_idx = 0;
	int has_video = 0;
	unsigned int st_idx = 0;
	VcodecCtx *vctx = arg;

	printf("entry read thread, tid: %ld.\n", vctx->tid);

	ic = avformat_alloc_context();
	if (!ic) {
		fprintf(stderr, "Could not allocate avformat context.\n");
		return NULL;
	}

	err = avformat_open_input(&ic, vctx->filename, NULL, NULL);
	if (err < 0) {
		fprintf(stderr, "Could not open avformat input.\n");
		return NULL;
	}

	err = avformat_find_stream_info(ic, NULL);
	if (err < 0) {
		fprintf(stderr, "find stream info err.\n");
		return NULL;
	}

	for (st_idx = 0; st_idx < ic->nb_streams; st_idx++) {
		AVStream *st = ic->streams[st_idx];

		enum AVMediaType type = st->codecpar->codec_type;
		st->discard = AVDISCARD_ALL;

		if (type == AVMEDIA_TYPE_VIDEO) {
			st->discard = AVDISCARD_NONE;
			vctx->vst_idx = st_idx;
			has_video = 1;
			break;
		}
	}

	if (!has_video) {
		fprintf(stderr, "no video stream.\n");
		return NULL;
	}

        stream = ic->streams[vctx->vst_idx];

        //codec = avcodec_find_decoder(stream->codecpar->codec_id);
	codec = avcodec_find_decoder_by_name("h264_v4l2m2m");
        if (!codec) {
		fprintf(stderr, "Unsupported codec with id %d for input stream %d\n",
			stream->codecpar->codec_id, stream->index);
		return NULL;
        }

	dec_ctx = avcodec_alloc_context3(codec);
	if (!dec_ctx) {
		fprintf(stderr, "Could not allocate video codec context\n");
		return NULL;
	}

	err = avcodec_parameters_to_context(dec_ctx, stream->codecpar);
	if (err < 0) {
		fprintf(stderr, "Could not set paras to context\n");
		return NULL;
	}

	av_codec_set_pkt_timebase(dec_ctx, stream->time_base);
	dec_ctx->framerate = stream->avg_frame_rate;

	if (avcodec_open2(dec_ctx, codec, NULL) < 0) {
		fprintf(stderr, "Could not open codec for input stream %d\n",
			stream->index);
		return NULL;
	}

	printf("fmt ctx: %p, stream: %p, video st idx: %d, num: %d\n",
		ic, stream, vst_idx, ic->nb_streams);
        printf("format: %s\n",ic->iformat->name);

	ic->flags |= AVFMT_FLAG_GENPTS;
	ic->debug = 0xff;

	//if (ic->pb)
	//    ic->pb->eof_reached = 0;

	frame = av_frame_alloc();
	if (!frame) {
		fprintf(stderr, "Could not allocate video frame\n");
		return NULL;
	}

	for (;;) {
		ret = av_read_frame(ic, pkt);
		if (ret < 0) {
			if (ret == AVERROR_EOF || avio_feof(ic->pb)) {
				printf("read data end, ret: %d.\n", ret);
				break;
			}

			if (ic->pb && ic->pb->error)
			    break;

			printf("read data fail, ret: %d.\n", ret);
			continue;
		}

		if (pkt->stream_index == vctx->vst_idx) {
			//packet_queue_put(&is->audioq, pkt);
			printf("read video data size: %d.\n", pkt->size);
			if (pkt->size)
				decode(dec_ctx, frame, pkt, DUMP_DIR);
		} else
			av_packet_unref(pkt);
	}

	/* flush the decoder */
	decode(dec_ctx, frame, NULL, DUMP_DIR);

	avcodec_free_context(&dec_ctx);
	av_frame_free(&frame);

	if (ic)
	    avformat_close_input(&ic);

	printf("read thread exit.\n");

	pthread_mutex_lock(&vctx->pthread_mutex);
	pthread_cond_signal(&vctx->pthread_cond);
	pthread_mutex_unlock(&vctx->pthread_mutex);

	return NULL;
}


static int open_input_file(const char *filename)
{
	int ret;
	VcodecCtx *vctx;
	pthread_t pid = pthread_self();

	vctx = av_mallocz(sizeof(VcodecCtx));
	if (!vctx)
	    return -1;

	vctx->filename = av_strdup(filename);
	if (!vctx->filename)
	    return -1;

	pthread_mutex_init(&vctx->pthread_mutex, NULL);
	pthread_cond_init(&vctx->pthread_cond, NULL);

	ret = pthread_create(&vctx->tid, NULL, read_thread, (void *)vctx);
	if (ret == 0) {
		pthread_setname_np(pid, "read_thread");

		pthread_mutex_lock(&vctx->pthread_mutex);
		pthread_cond_wait(&vctx->pthread_cond, &vctx->pthread_mutex);
		pthread_mutex_unlock(&vctx->pthread_mutex);
	}

	printf("creat the read thread, ret: %d.\n", ret);

	return 0;
}

int main(int argc, char **argv)
{
	int ret;
	const char *filename, *outfilename;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <input file>\n ==> %s/frame-123\n", argv[0], DUMP_DIR);
		exit(0);
	}

	filename    = argv[1];
	outfilename = argv[2];

	mkdir(DUMP_DIR, 0664);

	/*set debug level*/
	//av_log_set_level(AV_LOG_DEBUG);

	av_register_all();

	ret = open_input_file(filename);
	if (ret < 0) {
		fprintf(stderr, "open input file fail.\n");
		goto out;
	}

out:
	return ret < 0;
}
