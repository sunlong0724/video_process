#include "ffmpeg_h264enc.h"

#include <signal.h>
#include <fstream>
#include <iostream>

void CFFmpegH264Encoder::start() {
	m_exited = false;
	m_impl = std::thread(&CFFmpegH264Encoder::run, this);
}

void CFFmpegH264Encoder::stop() {
	m_exited = true;
	m_impl.join();
}

void CFFmpegH264Encoder::join() {
	m_impl.join();
}


int32_t CFFmpegH264Encoder::run() {
	/* find the mpeg1 video encoder */
	m_pCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
	if (!m_pCodec) {
		fprintf(stderr, "Codec not found\n");
		goto end;
	}

	m_pCodecCtx = avcodec_alloc_context3(m_pCodec);
	if (!m_pCodecCtx) {
		fprintf(stderr, "Could not allocate video codec context\n");
		goto end;
	}

	/* put sample parameters */
	m_pCodecCtx->bit_rate = 3000000;				//FIXME: want to parameterlize
												/* resolution must be a multiple of two */
	m_pCodecCtx->width = m_src_w;
	m_pCodecCtx->height = m_src_h;
	/* frames per second */
	m_pCodecCtx->time_base = { 1, ENCODER_FRAME_RATE };
	/* emit one intra frame every ten frames
	* check frame pict_type before passing frame
	* to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
	* then gop_size is ignored and the output of encoder
	* will always be I frame irrespective to gop_size
	*/
	m_pCodecCtx->gop_size = ENCODER_GOP_SIZE;
	m_pCodecCtx->max_b_frames = 1;
	m_pCodecCtx->pix_fmt = AV_PIX_FMT_YUV420P;

	av_opt_set(m_pCodecCtx->priv_data, "preset", "ultrafast", 0);
	av_opt_set(m_pCodecCtx->priv_data, "crf", "28.000", 0);
	av_opt_set(m_pCodecCtx->priv_data, "tune", "zerolatency", 0);

	//can happen an exception!!!!
	//av_opt_set(pCodecCtx->priv_data, "profile", "baseline", 0);

	/* open it */
	if (avcodec_open2(m_pCodecCtx, m_pCodec, NULL) < 0) {
		fprintf(stderr, "Could not open codec\n");
		goto end;
	}

	m_yuv_buffer_len = m_src_h*m_src_w * 3 / 2;
	m_yuv_buffer = new int8_t[m_yuv_buffer_len];

	while (!m_exited) {
		AVPacket pkt;
		AVFrame yuv_frame;
		int32_t nByteRead = 0;

		av_init_packet(&pkt);
		pkt.data = NULL;    // packet data will be allocated by the encoder
		pkt.size = 0;

		//read yuvs
		nByteRead = m_source_callback(m_yuv_buffer, m_yuv_buffer_len);
		if (0 == nByteRead) {
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
			continue;
		}
		av_image_fill_arrays(yuv_frame.data, yuv_frame.linesize, (const uint8_t*)m_yuv_buffer, AV_PIX_FMT_YUV420P, m_src_w, m_src_h, 16);
		yuv_frame.pts = av_frame_get_best_effort_timestamp(&yuv_frame);

		/* encode the image */
		int got_pkt_ptr = 0;
		int ret = avcodec_encode_video2(m_pCodecCtx, &pkt, &yuv_frame, &got_pkt_ptr);
		if (ret < 0) {
			fprintf(stderr, "Error encoding frame\n");
			goto end;
		}
		if (got_pkt_ptr) {
			m_sink_callback((int8_t*)pkt.data, pkt.size);
		}
		av_packet_unref(&pkt);
	}

end:
	delete[] m_yuv_buffer;
	avcodec_close(m_pCodecCtx);
	avcodec_free_context(&m_pCodecCtx);
	return 0;
}

bool CFFmpegH264Encoder::init(int16_t src_w, int16_t src_h, _SourceDataCallback ReadNextFrameCallback, _SinkDataCallback WriteFrameCallback) {
	m_source_callback = ReadNextFrameCallback;
	m_sink_callback = WriteFrameCallback;
	m_src_h = src_h;
	m_src_w = src_w;

	av_register_all();

	return true;
}

bool CFFmpegH264Encoder::uninit() {
	return true;
}

static int i = 0;
static int j = 0;
int32_t ReadNextFrame(int8_t* buffer, int32_t buffer_len, void* ctx) {
	size_t nBytesRead = 0;
	std::ifstream* ifsp = (std::ifstream*)ctx;

	if (!ifsp || !buffer) return 0;

	ifsp->read((char*)buffer, buffer_len);
	fprintf(stderr, "%s, %d\n",__FUNCTION__, ++i);
	if (ifsp->fail()) {
		ifsp->clear();
		ifsp->seekg(ifsp->beg);
		return 0;
	}
	return buffer_len;
}

int32_t WriteNextFrame(int8_t* buffer, int32_t buffer_len, void*  ctx) {
	std::ofstream* ofsp  = (std::ofstream*)ctx;
	ofsp->write((char*)buffer, buffer_len);
	fprintf(stderr,"%s, %d\n", __FUNCTION__, ++j);
	return buffer_len;
}


bool g_running_flag = true;
void sig_cb(int sig)
{
	if (sig == SIGINT) {
		std::cout << __FUNCTION__ << std::endl;
		g_running_flag = false;
	}
}


int main(int argc, char** argv) {

	int32_t src_w = 1920;
	int32_t src_h = 1080;

	std::ifstream ifs("trailer_1080p.yuv", std::ifstream::in | std::ifstream::binary);
	std::ofstream ofs("trailer_1080p.264", std::ofstream::out | std::ofstream::binary);

	CFFmpegH264Encoder encoder;

	encoder.init(src_w, src_h, std::bind(ReadNextFrame, std::placeholders::_1, std::placeholders::_2, &ifs), std::bind(WriteNextFrame, std::placeholders::_1, std::placeholders::_2, &ofs));
	encoder.start();

	while (g_running_flag)
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

	encoder.stop();
	ifs.close();
	ofs.close();

	return 0;
}