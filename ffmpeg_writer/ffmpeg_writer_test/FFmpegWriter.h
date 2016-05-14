#pragma once
#define DLL_API __declspec(dllexport)  

#include <string>
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
}

class DLL_API CFFmpegRgb2Yuv {
public:
	int32_t initialize(int16_t src_w, int16_t src_h, int16_t src_fmt, int16_t dst_w, int16_t dst_h, int16_t dst_fmt);
	int32_t uninitialize();
	int32_t yuv2rgb(unsigned char* dst, unsigned char* src);
	int32_t rgb2yuv(unsigned char* dst, unsigned char* src);

private:
	int16_t m_src_w;
	int16_t m_src_h;
	int16_t m_src_fmt;
	int16_t m_dst_w;
	int16_t m_dst_h;
	int16_t m_dst_fmt;

	struct SwsContext*  m_pSwsCtx;
};

class DLL_API CFFmpegWriter {
public:
	int32_t initialize(const std::string& parameters);
	int32_t uninitialize();
	int32_t create_video_file(const std::string& record_file_name);
	int32_t close_video_file();
	int32_t write_video_frame(unsigned char* buf, int len, bool is_key_frame);

private:
	AVFormatContext*	m_pAVFormatContext;
	AVStream*			m_pAVStream;
	int32_t				m_frame_count;
	int16_t				m_fps;
	static  bool		m_register_ffmpeg;

};


