
#include "FFmpegWriter.h"

#define FFWR_PATH_MAX_LEN 1024

#define FFWR_FOPEN(FH, FN, M)           { FH=fopen(FN,M); }
// Helper macro definitions...
#define FFWR_PRINT_RET_MSG(ERR)         {PrintErrString(ERR, __FILE__, __LINE__);}
#define FFWR_CHECK_RESULT(P, X, ERR)    {if ((X) > (P)) {FFWR_PRINT_RET_MSG(ERR); return ERR;}}
#define FFWR_CHECK_POINTER(P, ERR)      {if (!(P)) {FFWR_PRINT_RET_MSG(ERR); return ERR;}}
#define FFWR_CHECK_ERROR(P, X, ERR)     {if ((X) == (P)) {FFWR_PRINT_RET_MSG(ERR); return ERR;}}
#define FFWR_IGNORE_FFWR_STS(P, X)       {if ((X) == (P)) {P = FFWR_ERR_NONE;}}
#define FFWR_BREAK_ON_ERROR(P)          {if (MFX_ERR_NONE != (P)) break;}
#define FFWR_SAFE_DELETE_ARRAY(P)       {if (P) {delete[] P; P = NULL;}}
#define FFWR_ALIGN32(X)                 (((mfxU32)((X)+31)) & (~ (mfxU32)31))
#define FFWR_ALIGN16(value)             (((value + 15) >> 4) << 4)
#define FFWR_SAFE_RELEASE(X)            {if (X) { X->Release(); X = NULL; }}
#define FFWR_MAX(A, B)                  (((A) > (B)) ? (A) : (B))

#define __UNUSED__(A) (void*)(A);

void PrintErrString(int err, const char* filestr, int line)
{
	switch (err) {
	case   0:
		printf("\n No error.\n");
		break;
	case  -1:
		printf("\n Unknown error: %s %d\n", filestr, line);
		break;
	case  -2:
		printf("\n Null pointer.  Check filename/path + permissions? %s %d\n", filestr, line);
		break;
	case  -3:
		printf("\n Unsupported feature/library load error. %s %d\n", filestr, line);
		break;
	case  -4:
		printf("\n Could not allocate memory. %s %d\n", filestr, line);
		break;
	case  -5:
		printf("\n Insufficient IO buffers. %s %d\n", filestr, line);
		break;
	case  -6:
		printf("\n Invalid handle. %s %d\n", filestr, line);
		break;
	case  -7:
		printf("\n Memory lock failure. %s %d\n", filestr, line);
		break;
	case  -8:
		printf("\n Function called before initialization. %s %d\n", filestr, line);
		break;
	case  -9:
		printf("\n Specified object not found. %s %d\n", filestr, line);
		break;
	case -10:
		printf("\n More input data expected. %s %d\n", filestr, line);
		break;
	case -11:
		printf("\n More output surfaces expected. %s %d\n", filestr, line);
		break;
	case -12:
		printf("\n Operation aborted. %s %d\n", filestr, line);
		break;
	case -13:
		printf("\n HW device lost. %s %d\n", filestr, line);
		break;
	case -14:
		printf("\n Incompatible video parameters. %s %d\n", filestr, line);
		break;
	case -15:
		printf("\n Invalid video parameters. %s %d\n", filestr, line);
		break;
	case -16:
		printf("\n Undefined behavior. %s %d\n", filestr, line);
		break;
	case -17:
		printf("\n Device operation failure. %s %d\n", filestr, line);
		break;
	case -18:
		printf("\n More bitstream data expected. %s %d\n", filestr, line);
		break;
	case -19:
		printf("\n Incompatible audio parameters. %s %d\n", filestr, line);
		break;
	case -20:
		printf("\n Invalid audio parameters. %s %d\n", filestr, line);
		break;
	default:
		printf("\nError code %d,\t%s\t%d\n\n", err, filestr, line);
	}
}


/*********************************************************************************\
Error message
\*********************************************************************************/
typedef enum
{
	/* no error */
	FFWR_ERR_NONE = 0,    /* no error */

						 /* reserved for unexpected errors */
	FFWR_ERR_UNKNOWN = -1,   /* unknown error. */

							/* error codes <0 */
	FFWR_ERR_NULL_PTR = -2,   /* null pointer */
	FFWR_ERR_UNSUPPORTED = -3,   /* undeveloped feature */
	FFWR_ERR_MEMORY_ALLOC = -4,   /* failed to allocate memory */
	FFWR_ERR_NOT_ENOUGH_BUFFER = -5,   /* insufficient buffer at input/output */
	FFWR_ERR_INVALID_HANDLE = -6,   /* invalid handle */
	FFWR_ERR_LOCK_MEMORY = -7,   /* failed to lock the memory block */
	FFWR_ERR_NOT_INITIALIZED = -8,   /* member function called before initialization */
	FFWR_ERR_NOT_FOUND = -9,   /* the specified object is not found */
	FFWR_ERR_MORE_DATA = -10,  /* expect more data at input */
	FFWR_ERR_MORE_SURFACE = -11,  /* expect more surface at output */
	FFWR_ERR_ABORTED = -12,  /* operation aborted */
	FFWR_ERR_DEVICE_LOST = -13,  /* lose the HW acceleration device */
	FFWR_ERR_INCOMPATIBLE_VIDEO_PARAM = -14,  /* incompatible video parameters */
	FFWR_ERR_INVALID_VIDEO_PARAM = -15,  /* invalid video parameters */
	FFWR_ERR_UNDEFINED_BEHAVIOR = -16,  /* undefined behavior */
	FFWR_ERR_DEVICE_FAILED = -17,  /* device operation failure */
	FFWR_ERR_MORE_BITSTREAM = -18,  /* expect more bitstream buffers at output */
	FFWR_ERR_INCOMPATIBLE_AUDIO_PARAM = -19,  /* incompatible audio parameters */
	FFWR_ERR_INVALID_AUDIO_PARAM = -20,  /* invalid audio parameters */

	FFWR_ERR_MAKE_DIRECTORY_FAILED = -21,
										/* warnings >0 */
	FFWR_WRN_IN_EXECUTION = 1,    /* the previous asynchronous operation is in execution */
	FFWR_WRN_DEVICE_BUSY = 2,    /* the HW acceleration device is busy */
	FFWR_WRN_VIDEO_PARAM_CHANGED = 3,    /* the video parameters are changed during decoding */
	FFWR_WRN_PARTIAL_ACCELERATION = 4,    /* SW is used */
	FFWR_WRN_INCOMPATIBLE_VIDEO_PARAM = 5,    /* incompatible video parameters */
	FFWR_WRN_VALUE_NOT_CHANGED = 6,    /* the value is saturated based on its valid range */
	FFWR_WRN_OUT_OF_RANGE = 7,    /* the value is out of valid range */
	FFWR_WRN_FILTER_SKIPPED = 10,   /* one of requested filters has been skipped */
	FFWR_WRN_INCOMPATIBLE_AUDIO_PARAM = 11,   /* incompatible audio parameters */

											 /* threading statuses */
	FFWR_TASK_DONE = FFWR_ERR_NONE,               /* task has been completed */
	FFWR_TASK_WORKING = 8,    /* there is some more work to do */
	FFWR_TASK_BUSY = 9     /* task is waiting for resources */

} ffwrStatus;


struct CmdOptionsValues {
	int16_t Width; // OPTION_GEOMETRY
	int16_t Height;

	int16_t Bitrate; // OPTION_BITRATE

	int16_t FrameRateN; // OPTION_FRAMERATE
	int16_t FrameRateD;

	int16_t GopPicSize;
};

struct CmdOptions {
	//CmdOptionsCtx ctx;
	CmdOptionsValues values;
};

void ParseOptions(const std::string& parameters, CmdOptions* cmd_options)
{
	char* argv[20];
	char params_buf[1024];
	strcpy(params_buf, parameters.c_str());

	char* p = strtok(params_buf, " ");
	int argc = 0;
	while (p) {
		argv[argc] = new char[strlen(p) + 1];
		strcpy(argv[argc++], p);
		p = strtok(NULL, " ");
	}

	int i = 0;
	for (i = 0; i < argc; ++i) {
	 if ( !strcmp(argv[i], "-g")) {
			int width = 0, height = 0;
			if (++i >= argc) {
				printf("error: no argument for -g option given\n");
				exit(-1);
			}
			if ((2 != sscanf(argv[i], "%dx%d", &width, &height)) || (width <= 0) || (height <= 0)) {
				printf("error: incorrect argument for -g option given\n");
				exit(-1);
			}
			cmd_options->values.Width = (uint16_t)width;
			cmd_options->values.Height = (uint16_t)height;
		}
		else if (!strcmp(argv[i], "-b")) {
			int bitrate = 0;
			if (++i >= argc) {
				printf("error: no argument for -b option given\n");
				exit(-1);
			}
			bitrate = atoi(argv[i]);
			if (bitrate <= 0) {
				printf("error: incorrect argument for -f option given\n");
				exit(-1);
			}
			cmd_options->values.Bitrate = (uint16_t)bitrate;
		}
		else if ( !strcmp(argv[i], "-f")) {
			int frN = 0, frD = 0;
			if (++i >= argc) {
				printf("error: no argument for -f option given\n");
				exit(-1);
			}
			if ((2 != sscanf(argv[i], "%d/%d", &frN, &frD)) || (frN <= 0) || (frD <= 0)) {
				printf("error: incorrect argument for -f option given\n");
				exit(-1);
			}
			cmd_options->values.FrameRateN = (uint16_t)frN;
			cmd_options->values.FrameRateD = (uint16_t)frD;
		} else if (!strcmp(argv[i], "-gop")) {//OPTION_MEASURE_GOPPICSIZE
			int gop_size = 0;
			if (++i >= argc) {
				printf("error: no argument for -gop option given\n");
				exit(-1);
			}
			gop_size = atoi(argv[i]);
			if (gop_size <= 0) {
				printf("error: incorrect argument for -gop option given\n");
				exit(-1);
			}
			cmd_options->values.GopPicSize = (uint16_t)gop_size;
		}
		else if (argv[i][0] == '-') {
			printf("error: unsupported option '%s'\n", argv[i]);
			exit(-1);
		}
		else {
			// we met fist non-option, i.e. argument
			break;
		}
	}
	if (i < argc) {
		printf("error: too many arguments\n");
		exit(-1);
	}

	//dealloc
	while (argc > 0) {
		delete[] argv[--argc];
	}
}

int32_t CFFmpegWriter::uninitialize() {

	if (m_pAVStream && m_pAVStream->codec)
		avcodec_close(m_pAVStream->codec);
	if (m_pAVFormatContext) {
		if (m_pAVFormatContext->pb || !(m_pAVFormatContext->flags & AVFMT_NOFILE))
			avio_closep(&m_pAVFormatContext->pb);
	}
	if (m_pAVFormatContext)
		avformat_free_context(m_pAVFormatContext);

	return FFWR_ERR_NONE;
}

bool CFFmpegWriter::m_register_ffmpeg = false;
int32_t CFFmpegWriter::initialize(const std::string& parameters) {
	int result = -1;
	if (!m_register_ffmpeg) {
		av_register_all();
		m_register_ffmpeg = true;
	}

	CmdOptions	cmdOpts;
	memset(&cmdOpts, 0x00, sizeof cmdOpts);

	ParseOptions(parameters, &cmdOpts);
	m_fps = cmdOpts.values.FrameRateN / cmdOpts.values.FrameRateD;

	/* allocate the output media context */
	AVOutputFormat*		pOutputFmt;
	avformat_alloc_output_context2(&m_pAVFormatContext, NULL, NULL, "temp.mp4");
	if (!m_pAVFormatContext) {
		printf("Could not deduce output format from file extension: using MPEG.\n");
		//avformat_alloc_output_context2(&oc, NULL, "mpeg", filename);
		exit(1);
	}
	pOutputFmt = m_pAVFormatContext->oformat;

	/* find the encoder */
	AVCodec* pCodec = avcodec_find_encoder(pOutputFmt->video_codec);
	if (!(pCodec)) {
		fprintf(stderr, "Could not find encoder for '%s'\n",
			avcodec_get_name(pOutputFmt->video_codec));
		exit(1);
	}

	m_pAVStream = avformat_new_stream(m_pAVFormatContext, pCodec);
	if (!m_pAVStream) {
		fprintf(stderr, "Could not allocate stream\n");
		exit(1);
	}
	m_pAVStream->id = m_pAVFormatContext->nb_streams - 1;
	/* timebase: This is the fundamental unit of time (in seconds) in terms
	* of which frame timestamps are represented. For fixed-fps content,
	* timebase should be 1/framerate and timestamp increments should be
	* identical to 1. */
	m_pAVStream->time_base = { cmdOpts.values.FrameRateD,cmdOpts.values.FrameRateN };

	//FIXME:
	//AVCodecParameters *c = m_pAVStream->codecpar;
	AVCodecContext *c = m_pAVStream->codec;
	c->codec_id = pOutputFmt->video_codec;

	c->bit_rate = cmdOpts.values.Bitrate*1000;
	/* Resolution must be a multiple of two. */
	c->width = cmdOpts.values.Width;
	c->height = cmdOpts.values.Height;


	c->time_base = m_pAVStream->time_base;
	c->gop_size = cmdOpts.values.GopPicSize; /* emit one intra frame every twelve frames at most */
	c->pix_fmt = AV_PIX_FMT_YUV420P;
	c->max_b_frames = 1;//test

	/* Some formats want stream headers to be separate. */
	if (m_pAVFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
		c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
	/* open the codec */
	result = avcodec_open2(c, pCodec, NULL);

	return result;
}

#include "Shlwapi.h"
#pragma comment(lib,"Shlwapi.lib") 

inline int32_t create_dir_0(std::string path) {
	FFWR_CHECK_ERROR(PathFileExistsA(path.c_str()), TRUE, FFWR_ERR_NONE);
	std::string temp;
	char* pch = strtok((char*)path.c_str(), "\\");
	while (pch != NULL) {
		temp.append(pch + std::string("\\"));
		if (!PathFileExistsA(temp.c_str())) {
			FFWR_CHECK_ERROR(CreateDirectory(temp.c_str(), NULL), FALSE, FFWR_ERR_MAKE_DIRECTORY_FAILED);
		}
		pch = strtok(NULL, "\\");
	}
	return FFWR_ERR_NONE;
}


int32_t CFFmpegWriter::create_video_file(const std::string& record_file_name) {
	int result;
	av_dump_format(m_pAVFormatContext, 0, record_file_name.c_str(), 1);
	/* open the output file, if needed */
	if (!(m_pAVFormatContext->flags & AVFMT_NOFILE)) {
		std::string path(record_file_name.substr(0, record_file_name.rfind("\\")));
		FFWR_CHECK_ERROR(create_dir_0(path), FFWR_ERR_MAKE_DIRECTORY_FAILED, FFWR_ERR_MAKE_DIRECTORY_FAILED)
		result = avio_open(&m_pAVFormatContext->pb, record_file_name.c_str(), AVIO_FLAG_WRITE);
		FFWR_CHECK_RESULT(result, 0, FFWR_ERR_UNKNOWN);
	}
	/* Write the stream header, if any. */
	result = avformat_write_header(m_pAVFormatContext, NULL);
	FFWR_CHECK_RESULT(result, 0, FFWR_ERR_UNKNOWN);
	m_frame_count = 0;
	return FFWR_ERR_NONE;
}

int32_t CFFmpegWriter::close_video_file() {
	int result = av_write_trailer(m_pAVFormatContext);
	FFWR_CHECK_RESULT(result, 0, FFWR_ERR_UNKNOWN);
	/* Close the output file. */
	if (!(m_pAVFormatContext->flags & AVFMT_NOFILE)) {
		avio_closep(&m_pAVFormatContext->pb);
		m_pAVFormatContext->pb = NULL;
	}
	return FFWR_ERR_NONE;
}

/*
* encode one video frame and send it to the muxer
* return 1 when encoding is finished, 0 otherwise
*/
int32_t CFFmpegWriter::write_video_frame(unsigned char* buf, int len, bool is_key_frame) {
	AVCodecContext *c;
	AVPacket pkt = { 0 };

	c = m_pAVStream->codec;

	av_init_packet(&pkt);
	pkt.data = buf;
	pkt.size = len;

	++m_frame_count;
	pkt.stream_index = m_pAVStream->index;
	//Write PTS
	AVRational time_base = m_pAVStream->time_base;//{ 1, 1000 };
	AVRational r_framerate1 = { m_fps * 2, 2 };//{ 50, 2 };
	AVRational time_base_q = { 1, AV_TIME_BASE };
	//Duration between 2 frames (us)
	int64_t calc_duration = (double)(AV_TIME_BASE)*(1 / av_q2d(r_framerate1));	//内部时间戳
	pkt.pts = av_rescale_q(m_frame_count*calc_duration, time_base_q, time_base);
	pkt.dts = pkt.pts;
	pkt.duration = av_rescale_q(calc_duration, time_base_q, time_base);
	pkt.pos = -1;

	if (is_key_frame)
		pkt.flags = pkt.flags | AV_PKT_FLAG_KEY;

	/* Write the compressed frame to the media file. */
	return av_interleaved_write_frame(m_pAVFormatContext, &pkt);
	//return av_write_frame(pFmtCtx, &pkt);
}

int32_t ReadNextFrame(unsigned char* buffer, int32_t buffer_len, void* ctx) {
	size_t nBytesRead = 0;
	FILE* fp = (FILE*)ctx;

	FFWR_CHECK_POINTER(fp, FFWR_ERR_NULL_PTR);
	FFWR_CHECK_POINTER(buffer, FFWR_ERR_NULL_PTR);

	nBytesRead = fread(buffer, 1, buffer_len, fp);
	return (int32_t)nBytesRead;
}

int32_t WriteNextFrame(unsigned char* buffer, int32_t buffer_len, void*  ctx) {
	CFFmpegWriter* p_writer = (CFFmpegWriter*)ctx;
	int nLen = p_writer->write_video_frame(buffer, buffer_len, buffer[5] == 0x10);
	if (0 == nLen)
		return (int32_t)buffer_len;
	return nLen;
}

int32_t CFFmpegRgb2Yuv::initialize(int16_t src_w, int16_t src_h, int16_t src_fmt, int16_t dst_w, int16_t dst_h, int16_t dst_fmt) {
	m_src_w = src_w;
	m_src_h = src_h;
	m_src_fmt = src_fmt;

	m_dst_w = dst_w;
	m_dst_h = dst_h;
	m_dst_fmt = dst_fmt;

	m_pSwsCtx = sws_getContext(m_src_w, m_src_h, AVPixelFormat(m_src_fmt),
		m_dst_w, m_dst_h, AVPixelFormat(m_dst_fmt), SWS_BILINEAR, 0, 0, 0);
	return 0;
}
int32_t CFFmpegRgb2Yuv::uninitialize() {
	sws_freeContext(m_pSwsCtx);
	return 0;
}

int32_t CFFmpegRgb2Yuv::yuv2rgb(unsigned char* dst, unsigned char* src){
	if (dst == NULL || src == NULL)
		return -1;
	AVFrame yuv_frame, rgb_frame;
	av_image_fill_arrays(yuv_frame.data, yuv_frame.linesize, src, AV_PIX_FMT_YUV420P, m_src_w, m_src_h, 16);
	av_image_fill_arrays(rgb_frame.data, rgb_frame.linesize, dst, (AVPixelFormat)m_dst_fmt, m_dst_w, m_dst_h, 1);

	int ret = sws_scale(m_pSwsCtx, yuv_frame.data, yuv_frame.linesize, 0, m_src_h, rgb_frame.data, rgb_frame.linesize);
	return ret;
}

int32_t CFFmpegRgb2Yuv::rgb2yuv(unsigned char* dst, unsigned char* src) {
	if (dst == NULL || src == NULL)
		return -1;
	AVFrame yuv_frame, rgb_frame;
	av_image_fill_arrays(rgb_frame.data, rgb_frame.linesize, src, AVPixelFormat(m_src_fmt), m_src_w, m_src_h, 1);
	av_image_fill_arrays(yuv_frame.data, yuv_frame.linesize, dst, AV_PIX_FMT_YUV420P, m_dst_w, m_dst_h, 16);

	int ret = sws_scale(m_pSwsCtx, rgb_frame.data, rgb_frame.linesize, 0, m_src_h, yuv_frame.data, yuv_frame.linesize);
	return ret;
}


#if 0

#include "msdk_encode.h"
int main0(int argc, char** argv) {

	std::string paramter("-g 1920x1080 -b 3000 -f 30/1 -gop 45");

	CFFmpegWriter ffwriter;
	ffwriter.initialize(paramter);
	ffwriter.create_video_file("e:\\1.mp4");

	FILE* source_fp = NULL;
	char* source_name = "trailer_1080p.yuv";
	source_fp = fopen(source_name, "rb");

	CEncodeThread encode;
	
	encode.init(paramter.data());
	encode.start(std::bind(ReadNextFrame, std::placeholders::_1, std::placeholders::_2, source_fp), std::bind(WriteNextFrame, std::placeholders::_1, std::placeholders::_2, &ffwriter));
	encode.join();

	fseek(source_fp, 0, SEEK_SET);
	encode.init(paramter.data());
	encode.start(std::bind(ReadNextFrame, std::placeholders::_1, std::placeholders::_2, source_fp), std::bind(WriteNextFrame, std::placeholders::_1, std::placeholders::_2, &ffwriter));
	encode.join();


	ffwriter.close_video_file();
	ffwriter.uninitialize();
	fclose(source_fp);
	return 0;
}

#endif