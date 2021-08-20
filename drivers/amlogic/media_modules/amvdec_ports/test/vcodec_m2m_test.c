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
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
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
#include <pthread.h>

#define INBUF_SIZE	(4096)
#define DUMP_DIR	"/data/video_frames"
static int dump;

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

static void dump_yuv(AVFrame *frame, char *filename)
{
	FILE *f;

	printf("name: %s, resolution: %dx%d, size: %d\n",
		filename, frame->width, frame->height,
		frame->buf[0]->size + frame->buf[1]->size);

	f = fopen(filename,"w+");

	fwrite(frame->buf[0]->data, 1, frame->buf[0]->size, f);
	fwrite(frame->buf[1]->data, 1, frame->buf[1]->size, f);

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

		if (dump)
			dump_yuv(frame, buf);
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
	const char *forced_codec_name = NULL;

	printf("entry read thread, tid: %ld.\n", vctx->tid);

	ic = avformat_alloc_context();
	if (!ic) {
		fprintf(stderr, "Could not allocate avformat context.\n");
		goto out;
	}

	err = avformat_open_input(&ic, vctx->filename, NULL, NULL);
	if (err < 0) {
		fprintf(stderr, "Could not open avformat input.\n");
		goto out;
	}

	err = avformat_find_stream_info(ic, NULL);
	if (err < 0) {
		fprintf(stderr, "find stream info err.\n");
		goto out;
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
		goto out;
	}

	stream = ic->streams[vctx->vst_idx];

	//codec = avcodec_find_decoder(stream->codecpar->codec_id);
	switch (stream->codecpar->codec_id) {
		case AV_CODEC_ID_H264:
			forced_codec_name = "h264_v4l2m2m";
			break;
		case AV_CODEC_ID_HEVC:
			forced_codec_name = "hevc_v4l2m2m";
			break;
		case AV_CODEC_ID_VP9:
			forced_codec_name = "vp9_v4l2m2m";
			break;
		case AV_CODEC_ID_MPEG1VIDEO:
			forced_codec_name = "mpeg1_v4l2m2m";
			break;
		case AV_CODEC_ID_MPEG2VIDEO:
			forced_codec_name = "mpeg2_v4l2m2m";
			break;
		case AV_CODEC_ID_VC1:
			forced_codec_name = "vc1_v4l2m2m";
			break;
		case AV_CODEC_ID_H263:
			forced_codec_name = "h263_v4l2m2m";
			break;
		case AV_CODEC_ID_MPEG4:
			forced_codec_name = "mpeg4_v4l2m2m";
			break;
		case AV_CODEC_ID_MJPEG:
			forced_codec_name = "mjpeg_v4l2m2m";
			break;
	}

	codec = avcodec_find_decoder_by_name(forced_codec_name);
	if (!codec) {
		fprintf(stderr, "Unsupported codec with id %d for input stream %d\n",
			stream->codecpar->codec_id, stream->index);
		goto out;
	}

	dec_ctx = avcodec_alloc_context3(codec);
	if (!dec_ctx) {
		fprintf(stderr, "Could not allocate video codec context\n");
		goto out;
	}

	err = avcodec_parameters_to_context(dec_ctx, stream->codecpar);
	if (err < 0) {
		fprintf(stderr, "Could not set paras to context\n");
		goto out;
	}

	av_codec_set_pkt_timebase(dec_ctx, stream->time_base);
	dec_ctx->framerate = stream->avg_frame_rate;

	if (avcodec_open2(dec_ctx, codec, NULL) < 0) {
		fprintf(stderr, "Could not open codec for input stream %d\n",
			stream->index);
		goto out;
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
		goto out;
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
			//printf("read video data size: %d.\n", pkt->size);
			if (pkt->size)
				decode(dec_ctx, frame, pkt, DUMP_DIR);
		}

		av_packet_unref(pkt);

		usleep(8 * 1000);
	}

	/* flush the decoder */
	decode(dec_ctx, frame, NULL, DUMP_DIR);
out:
	if (dec_ctx)
		avcodec_free_context(&dec_ctx);

	if (frame)
		av_frame_free(&frame);

	if (ic) {
	    avformat_close_input(&ic);
	    avformat_free_context(ic);
	}

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
	if (!vctx->filename) {
	    av_free(vctx);
	    return -1;
	}

	pthread_mutex_init(&vctx->pthread_mutex, NULL);
	pthread_cond_init(&vctx->pthread_cond, NULL);

	ret = pthread_create(&vctx->tid, NULL, read_thread, (void *)vctx);
	if (ret == 0) {
		pthread_setname_np(pid, "read_thread");

		pthread_mutex_lock(&vctx->pthread_mutex);
		pthread_cond_wait(&vctx->pthread_cond, &vctx->pthread_mutex);
		pthread_join(vctx->tid, NULL);
		pthread_mutex_unlock(&vctx->pthread_mutex);
	}

	av_free(vctx->filename);
	av_free(vctx);

	printf("creat the read thread, ret: %d.\n", ret);

	return 0;
}

int main(int argc, char **argv)
{
	int ret;
	const char *filename;
	int log_level = 0;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s <input file>\n ==> %s/frame-123\n", argv[0], DUMP_DIR);
		exit(0);
	}

	filename = argv[1];
	if (argv[2]) {
		if (!strcmp(argv[2], "dump"))
			dump = 1;
		else
			log_level = atoi(argv[2]);
	}

	mkdir(DUMP_DIR, 0664);

	/*set debug level*/
	av_log_set_level(log_level); //AV_LOG_DEBUG

	/* register all the codecs */
	avcodec_register_all();

	ret = open_input_file(filename);
	if (ret < 0)
		fprintf(stderr, "open input file fail.\n");

	return 0;
}
