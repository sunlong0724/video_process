#include "FFmpegWriter.h"
#include "msdk_encode.h"

#include <stdint.h>
#include <string>
#include <vector>
#include <fstream>
#include <iterator>
int32_t ReadNextFrame(unsigned char* buffer, int32_t buffer_len, void* ctx) {
	size_t nBytesRead = 0;
	FILE* fp = (FILE*)ctx;

	if (!fp || !buffer) return 0;

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

int main(int argc, char** argv) {

#if 0
	//color space convert
	int32_t yuv_len = 1920 * 1080 * 3 / 2;
	unsigned char* yuv_buf = new unsigned char[yuv_len];
	unsigned char* rgb_buf = new unsigned char[yuv_len * 2];

	std::ifstream ifs("trailer_1080p.yuv", std::ifstream::in | std::ifstream::binary);
	std::ofstream ofs("trailer_1080p.rgb", std::ofstream::out | std::ofstream::binary);

	CFFmpegRgb2Yuv rgb2yuv;
	int w = 1920;
	int h = 1080;
	rgb2yuv.initialize(w, h, 0, w, h, 0);

	int i = 0;
	while (ifs.read((char*)yuv_buf, yuv_len))	{
		rgb2yuv.yuv2rgb(rgb_buf,yuv_buf);

		ofs.write((const char*)rgb_buf, yuv_len * 2);

		printf("frame:%d\n", ++i);
	}
	ifs.close();
	ofs.close();
//	return 0;
#endif

	//encode record
	std::string paramter("-g 1920x1080 -b 3000 -f 25/1 -gop 25");

	CFFmpegWriter ffwriter;
	ffwriter.initialize(paramter);
	ffwriter.create_video_file("e:\\1.mp4");

	FILE* source_fp = NULL;
	char* source_name = "trailer_1080p.yuv";
	source_fp = fopen(source_name, "rb");

	CEncodeThread encode;

	encode.init(paramter.data());
	encode.start(ReadNextFrame, source_fp, WriteNextFrame, &ffwriter);
	encode.join();

	fseek(source_fp, 0, SEEK_SET);
	encode.init(paramter.data());
	encode.start(ReadNextFrame, source_fp, WriteNextFrame, &ffwriter);
	encode.join();


	ffwriter.close_video_file();
	ffwriter.uninitialize();
	fclose(source_fp);
	return 0;
}