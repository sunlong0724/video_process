#include "msdk_decode.h"

#include <signal.h>
#include <iostream>


#define MSDK_FOPEN(FH, FN, M)           { FH=fopen(FN,M); }
// Helper macro definitions...
#define MSDK_PRINT_RET_MSG(ERR)         {PrintErrString(ERR, __FILE__, __LINE__);}
#define MSDK_CHECK_RESULT(P, X, ERR)    {if ((X) > (P)) {MSDK_PRINT_RET_MSG(ERR); return ERR;}}
#define MSDK_CHECK_POINTER(P, ERR)      {if (!(P)) {MSDK_PRINT_RET_MSG(ERR); return ERR;}}
#define MSDK_CHECK_ERROR(P, X, ERR)     {if ((X) == (P)) {MSDK_PRINT_RET_MSG(ERR); return ERR;}}
#define MSDK_IGNORE_MFX_STS(P, X)       {if ((X) == (P)) {P = MFX_ERR_NONE;}}
#define MSDK_BREAK_ON_ERROR(P)          {if (MFX_ERR_NONE != (P)) break;}
#define MSDK_SAFE_DELETE_ARRAY(P)       {if (P) {delete[] P; P = NULL;}}
#define MSDK_ALIGN32(X)                 (((mfxU32)((X)+31)) & (~ (mfxU32)31))
#define MSDK_ALIGN16(value)             (((value + 15) >> 4) << 4)
#define MSDK_SAFE_RELEASE(X)            {if (X) { X->Release(); X = NULL; }}
#define MSDK_MAX(A, B)                  (((A) > (B)) ? (A) : (B))

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
	MFX_ERR_NONE = 0,    /* no error */

						 /* reserved for unexpected errors */
	MFX_ERR_UNKNOWN = -1,   /* unknown error. */

							/* error codes <0 */
	MFX_ERR_NULL_PTR = -2,   /* null pointer */
	MFX_ERR_UNSUPPORTED = -3,   /* undeveloped feature */
	MFX_ERR_MEMORY_ALLOC = -4,   /* failed to allocate memory */
	MFX_ERR_NOT_ENOUGH_BUFFER = -5,   /* insufficient buffer at input/output */
	MFX_ERR_INVALID_HANDLE = -6,   /* invalid handle */
	MFX_ERR_LOCK_MEMORY = -7,   /* failed to lock the memory block */
	MFX_ERR_NOT_INITIALIZED = -8,   /* member function called before initialization */
	MFX_ERR_NOT_FOUND = -9,   /* the specified object is not found */
	MFX_ERR_MORE_DATA = -10,  /* expect more data at input */
	MFX_ERR_MORE_SURFACE = -11,  /* expect more surface at output */
	MFX_ERR_ABORTED = -12,  /* operation aborted */
	MFX_ERR_DEVICE_LOST = -13,  /* lose the HW acceleration device */
	MFX_ERR_INCOMPATIBLE_VIDEO_PARAM = -14,  /* incompatible video parameters */
	MFX_ERR_INVALID_VIDEO_PARAM = -15,  /* invalid video parameters */
	MFX_ERR_UNDEFINED_BEHAVIOR = -16,  /* undefined behavior */
	MFX_ERR_DEVICE_FAILED = -17,  /* device operation failure */
	MFX_ERR_MORE_BITSTREAM = -18,  /* expect more bitstream buffers at output */
	MFX_ERR_INCOMPATIBLE_AUDIO_PARAM = -19,  /* incompatible audio parameters */
	MFX_ERR_INVALID_AUDIO_PARAM = -20,  /* invalid audio parameters */

										/* warnings >0 */
	MFX_WRN_IN_EXECUTION = 1,    /* the previous asynchronous operation is in execution */
	MFX_WRN_DEVICE_BUSY = 2,    /* the HW acceleration device is busy */
	MFX_WRN_VIDEO_PARAM_CHANGED = 3,    /* the video parameters are changed during decoding */
	MFX_WRN_PARTIAL_ACCELERATION = 4,    /* SW is used */
	MFX_WRN_INCOMPATIBLE_VIDEO_PARAM = 5,    /* incompatible video parameters */
	MFX_WRN_VALUE_NOT_CHANGED = 6,    /* the value is saturated based on its valid range */
	MFX_WRN_OUT_OF_RANGE = 7,    /* the value is out of valid range */
	MFX_WRN_FILTER_SKIPPED = 10,   /* one of requested filters has been skipped */
	MFX_WRN_INCOMPATIBLE_AUDIO_PARAM = 11,   /* incompatible audio parameters */

											 /* threading statuses */
	MFX_TASK_DONE = MFX_ERR_NONE,               /* task has been completed */
	MFX_TASK_WORKING = 8,    /* there is some more work to do */
	MFX_TASK_BUSY = 9     /* task is waiting for resources */

} mfxStatus;

int32_t ReadNextFrame(unsigned char* buffer, int32_t buffer_len, void* ctx) {
	size_t nBytesRead = 0;
	FILE* fp = (FILE*)ctx;
	
	MSDK_CHECK_POINTER(fp, MFX_ERR_NULL_PTR);
	MSDK_CHECK_POINTER(buffer, MFX_ERR_NULL_PTR);

	nBytesRead = fread(buffer, 1, buffer_len, fp);
	return (int32_t)nBytesRead;
}

int32_t WriteNextFrame(unsigned char* buffer, int32_t buffer_len, void*  ctx) {
	size_t nBytesWritten = 0;
	FILE* fp = (FILE*)ctx;

	MSDK_CHECK_POINTER(fp, MFX_ERR_NULL_PTR);
	MSDK_CHECK_POINTER(buffer, MFX_ERR_NULL_PTR);
	

	// FIXME: DON'T WRITE !!! 
	//nBytesWritten = fwrite(buffer, 1, buffer_len, fp);
	//return (int32_t)nBytesWritten;
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
	signal(SIGINT, sig_cb);  /*ע��ctrl+c�źŲ�����*/


	if (argc < 2) {
		fprintf(stderr, "two paramteters are needed!\n");
		exit(0);
	}


	char* source_name = argv[1];// "out_1280x720p.264";
	char* sink_name = "out.yuv";

	//thread 1
	FILE* source_fp = NULL;
	FILE* sink_fp = NULL;
	source_fp = fopen(source_name, "rb");
	sink_fp = fopen(sink_name, "wb");

	CDecodeThread decode;
	std::string paramter("decode ");
	decode.init(paramter.data());
	decode.start(std::bind(ReadNextFrame, std::placeholders::_1, std::placeholders::_2, source_fp), std::bind(WriteNextFrame, std::placeholders::_1, std::placeholders::_2, sink_fp));

	std::this_thread::sleep_for(std::chrono::seconds(10));

	//thread 2
	//FILE* source_fp1 = NULL;
	//FILE* sink_fp1 = NULL;
	//source_fp1 = fopen(source_name, "rb");
	//sink_fp1 = fopen(sink_name, "wb");

	//CDecodeThread decode2;
	//std::string paramter2("decode ");
	//decode2.init(paramter2.data());
	//decode2.start(std::bind(ReadNextFrame, std::placeholders::_1, std::placeholders::_2, source_fp1), std::bind(WriteNextFrame, std::placeholders::_1, std::placeholders::_2, sink_fp1));

	
	while (g_running_flag)
		std::this_thread::sleep_for(std::chrono::milliseconds(500));

	decode.join();
	//decode2.join();

	fclose(sink_fp);
	fclose(source_fp);

	//fclose(sink_fp1);
	//fclose(source_fp1);
	return 0;
}