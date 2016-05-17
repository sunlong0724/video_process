
/**
*最简单的基于FFmpeg的音频编码器
*Simplest FFmpeg Audio Encoder
*
*雷霄骅 Lei Xiaohua
*leixiaohua1020@126.com
*中国传媒大学/数字电视技术
*Communication University of China / Digital TV Technology
*http://blog.csdn.net/leixiaohua1020
*
*本程序实现了音频PCM采样数据编码为压缩码流（MP3，WMA，AAC等）。
*是最简单的FFmpeg音频编码方面的教程。
*通过学习本例子可以了解FFmpeg的编码流程。
*This software encode PCM data to AAC bitstream.
*It's the simplest audio encoding software based on FFmpeg.
*Suitable for beginner of FFmpeg
*/

#include <stdio.h>

#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
//Windows
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
};
#else
//Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#ifdef __cplusplus
};
#endif
#endif


int flush_encoder(AVFormatContext *fmt_ctx, unsigned int stream_index) {
	int ret;
	int got_frame;
	AVPacket enc_pkt;
	if (!(fmt_ctx->streams[stream_index]->codec->codec->capabilities &
		CODEC_CAP_DELAY))
		return 0;
	while (1) {
		enc_pkt.data = NULL;
		enc_pkt.size = 0;
		av_init_packet(&enc_pkt);
		ret = avcodec_encode_audio2(fmt_ctx->streams[stream_index]->codec, &enc_pkt,
			NULL, &got_frame);
		av_frame_free(NULL);
		if (ret < 0)
			break;
		if (!got_frame) {
			ret = 0;
			break;
		}
		printf("Flush Encoder: Succeed to encode 1 frame!\tsize:%5d\n", enc_pkt.size);
		/* mux encoded frame */
		ret = av_write_frame(fmt_ctx, &enc_pkt);
		if (ret < 0)
			break;
	}
	return ret;
}

int main(int argc, char* argv[])
{
	AVFormatContext* pFormatCtx;
	AVOutputFormat* fmt;
	AVStream* audio_st;
	AVCodecContext* pCodecCtx;
	AVCodec* pCodec;

	uint8_t* frame_buf;
	AVFrame* pFrame;
	AVPacket pkt;

	int got_frame = 0;
	int ret = 0;
	int size = 0;

	FILE *in_file = NULL;	                        //Raw PCM data
	int framenum = 1000;                          //Audio frame number
	const char* out_file = "trailer_1080p_encode.aac";          //Output URL
	int i;

	in_file = fopen("trailer_1080p.pcm", "rb");

	av_register_all();

	//Method 1.
	pFormatCtx = avformat_alloc_context();
	fmt = av_guess_format(NULL, out_file, NULL);
	pFormatCtx->oformat = fmt;


	//Method 2.
	//avformat_alloc_output_context2(&pFormatCtx, NULL, NULL, out_file);
	//fmt = pFormatCtx->oformat;

	//Open output URL
	if (avio_open(&pFormatCtx->pb, out_file, AVIO_FLAG_READ_WRITE) < 0) {
		printf("Failed to open output file!\n");
		return -1;
	}

	audio_st = avformat_new_stream(pFormatCtx, 0);
	if (audio_st == NULL) {
		return -1;
	}
	pCodecCtx = audio_st->codec;
	pCodecCtx->codec_id = fmt->audio_codec;
	pCodecCtx->codec_type = AVMEDIA_TYPE_AUDIO;
	pCodecCtx->sample_fmt = AV_SAMPLE_FMT_S16;
	pCodecCtx->sample_rate = 44100;
	pCodecCtx->channel_layout = AV_CH_LAYOUT_STEREO;
	pCodecCtx->channels = av_get_channel_layout_nb_channels(pCodecCtx->channel_layout);
	pCodecCtx->bit_rate = 64000;

	//Show some information
	av_dump_format(pFormatCtx, 0, out_file, 1);

	pCodec = avcodec_find_encoder(pCodecCtx->codec_id);
	if (!pCodec) {
		printf("Can not find encoder!\n");
		return -1;
	}
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
		printf("Failed to open encoder!\n");
		return -1;
	}
	pFrame = av_frame_alloc();
	pFrame->nb_samples = pCodecCtx->frame_size;
	pFrame->format = pCodecCtx->sample_fmt;

	size = av_samples_get_buffer_size(NULL, pCodecCtx->channels, pCodecCtx->frame_size, pCodecCtx->sample_fmt, 1);
	frame_buf = (uint8_t *)av_malloc(size);
	avcodec_fill_audio_frame(pFrame, pCodecCtx->channels, pCodecCtx->sample_fmt, (const uint8_t*)frame_buf, size, 1);

	//Write Header
	avformat_write_header(pFormatCtx, NULL);

	av_new_packet(&pkt, size);

	for (i = 0; i<framenum; i++) {
		//Read PCM
		if (fread(frame_buf, 1, size, in_file) <= 0) {
			printf("Failed to read raw data! \n");
			return -1;
		}
		else if (feof(in_file)) {
			break;
		}
		pFrame->data[0] = frame_buf;  //PCM Data

		pFrame->pts = i * 100;
		got_frame = 0;
		//Encode
		ret = avcodec_encode_audio2(pCodecCtx, &pkt, pFrame, &got_frame);
		if (ret < 0) {
			printf("Failed to encode!\n");
			return -1;
		}
		if (got_frame == 1) {
			printf("Succeed to encode 1 frame! \tsize:%5d\n", pkt.size);
			pkt.stream_index = audio_st->index;
			ret = av_write_frame(pFormatCtx, &pkt);
			av_free_packet(&pkt);
		}
	}

	//Flush Encoder
	ret = flush_encoder(pFormatCtx, 0);
	if (ret < 0) {
		printf("Flushing encoder failed\n");
		return -1;
	}

	//Write Trailer
	av_write_trailer(pFormatCtx);

	//Clean
	if (audio_st) {
		avcodec_close(audio_st->codec);
		av_free(pFrame);
		av_free(frame_buf);
	}
	avio_close(pFormatCtx->pb);
	avformat_free_context(pFormatCtx);

	fclose(in_file);

	return 0;
}


extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
}


class CFFmpegAudioWriter {

	int32_t encode() {
		AVStream *st;

		/* pts of the next frame that will be generated */
		int64_t next_pts;
		int samples_count;

		AVFrame *frame;
		AVFrame *tmp_frame;

		float t, tincr, tincr2;

		struct SwsContext *sws_ctx;
		struct SwrContext *swr_ctx;


		AVOutputFormat *fmt;
		AVFormatContext *oc;
		AVCodec *audio_codec;

		/* allocate the output media context */
		avformat_alloc_output_context2(&oc, NULL, NULL, "1.mp4");

		if (!oc)
			return 1;

		fmt = oc->oformat;


		if (fmt->audio_codec != AV_CODEC_ID_NONE) {
			//add_stream(&audio_st, oc, &audio_codec, fmt->audio_codec);

			AVCodecContext *c;
			int i;

			/* find the encoder */
			audio_codec = avcodec_find_encoder(fmt->audio_codec);
			if (!(audio_codec)) {
				fprintf(stderr, "Could not find encoder for '%s'\n",
					avcodec_get_name(codec_id));
				exit(1);
			}

			st = avformat_new_stream(oc, audio_codec);
			if (!st) {
				fprintf(stderr, "Could not allocate stream\n");
				exit(1);
			}
			st->id = oc->nb_streams - 1;
			c = st->codec;
		}

		c->sample_fmt = (*codec)->sample_fmts ?
			(*codec)->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
		c->bit_rate = 64000;
		c->sample_rate = 44100;
		if ((*codec)->supported_samplerates) {
			c->sample_rate = (*codec)->supported_samplerates[0];
			for (i = 0; (*codec)->supported_samplerates[i]; i++) {
				if ((*codec)->supported_samplerates[i] == 44100)
					c->sample_rate = 44100;
			}
		}
		c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
		c->channel_layout = AV_CH_LAYOUT_STEREO;
		if ((*codec)->channel_layouts) {
			c->channel_layout = (*codec)->channel_layouts[0];
			for (i = 0; (*codec)->channel_layouts[i]; i++) {
				if ((*codec)->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
					c->channel_layout = AV_CH_LAYOUT_STEREO;
			}
		}
		c->channels = av_get_channel_layout_nb_channels(c->channel_layout);
		ost->st->time_base = (AVRational) { 1, c->sample_rate };
	}
};