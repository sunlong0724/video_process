/*****************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or
nondisclosure agreement with Intel Corporation and may not be copied
or disclosed except in accordance with the terms of that agreement.
Copyright(c) 2005-2014 Intel Corporation. All Rights Reserved.

*****************************************************************************/

#include "common_utils.h"
#include "cmd_options.h"
#include "msdk_encode.h"

#define __UNUSED__(A) (void*)(A);

static void usage(CmdOptionsCtx* ctx)
{
    printf(
        "Encodes INPUT and optionally writes OUTPUT. If INPUT is not specified\n"
        "simulates input with empty frames filled with the color.\n"
        "\n"
        "Usage: %s [options] [INPUT] [OUTPUT]\n", ctx->program);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


FILE* g_reader = NULL;
FILE* g_writer = NULL;
char* g_source_name = "trailer_1080p.yuv";
char* g_sink_name = "out.264";

int32_t ReadNextFrame(unsigned char* buffer, int32_t buffer_len, void* ctx) {
	__UNUSED__(ctx);
	size_t nBytesRead = 0;

	if (!g_reader) {
		MSDK_FOPEN(g_reader, g_source_name, "rb");
		MSDK_CHECK_POINTER(g_reader, MFX_ERR_NULL_PTR);
	}
	MSDK_CHECK_POINTER(buffer, MFX_ERR_NULL_PTR);

	nBytesRead = fread(buffer, 1, buffer_len, g_reader);
	if (nBytesRead == 0) {
		fclose(g_reader);
		g_reader = NULL;
	}
	return (int32_t)nBytesRead;
}

int32_t WriteNextFrame(unsigned char* buffer, int32_t buffer_len, void*  ctx) {
	__UNUSED__(ctx);
	size_t nBytesWritten = 0;

	if (!g_writer) {
		MSDK_FOPEN(g_writer, g_sink_name, "wb");
		MSDK_CHECK_POINTER(g_writer, MFX_ERR_NULL_PTR);
	}
	MSDK_CHECK_POINTER(buffer, MFX_ERR_NULL_PTR);

	nBytesWritten = fwrite(buffer, 1, buffer_len, g_writer);
	if (nBytesWritten != buffer_len) {
	/*	fclose(g_writer);
		g_writer = NULL;*/
	}
	return (int32_t)nBytesWritten;
}



void CEncodeThread::start(_SourceDataCallback ReadNextFrameCallback, _SinkDataCallback WriteFrameCallback) {
	m_exited = false;
	m_source_callback = ReadNextFrameCallback;
	m_sink_callback = WriteFrameCallback;
	m_impl = std::thread(&CEncodeThread::run, this);
}

void CEncodeThread::stop() {
	printf("1%s\n", __FUNCTION__);
	m_exited = true;
	m_impl.join();
	printf("%s\n", __FUNCTION__);
}

void CEncodeThread::join() {
	m_impl.join();
}

int32_t CEncodeThread::run() {

	bool bEnableOutput = true; // if true, removes all output bitsteam file writing and printing the progress

	// Read options from the command line (if any is given)
	CmdOptions options;
	memset(&options, 0, sizeof(CmdOptions));
	options.ctx.options = OPTIONS_ENCODE2;
	options.ctx.usage = usage;
	// Set default values:
	options.values.impl = MFX_IMPL_AUTO_ANY;

	char* parameters[20];
	char params_buf[1024] = "encode ";
	strcat(params_buf, m_parameter_buf);

	char* p = strtok(params_buf, " ");
	int j = 0;
	while (p) {
		parameters[j] = new char[strlen(p) + 1];
		strcpy(parameters[j++], p);
		p = strtok(NULL, " ");
	}
	ParseOptions(j, parameters, &options);
	while (j > 0) {
		delete[] parameters[--j];
	}

	mfxStatus sts = MFX_ERR_NONE;
	// Initialize Intel Media SDK session
	// - MFX_IMPL_AUTO_ANY selects HW acceleration if available (on any adapter)
	// - Version 1.0 is selected for greatest backwards compatibility.
	// OS specific notes
	// - On Windows both SW and HW libraries may present
	// - On Linux only HW library only is available
	//   If more recent API features are needed, change the version accordingly
	mfxIMPL impl = MFX_IMPL_AUTO_ANY;
	mfxVersion ver = { { 0, 1 } };
	MFXVideoSession session;

	sts = Initialize(impl, ver, &session, NULL);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	// Create Media SDK encoder
	MFXVideoENCODE mfxENC(session);

	// Set required video parameters for encode
	// - In this example we are encoding an AVC (H.264) stream
	mfxVideoParam mfxEncParams;
	memset(&mfxEncParams, 0, sizeof(mfxEncParams));

	mfxEncParams.mfx.CodecId = MFX_CODEC_AVC;
	mfxEncParams.mfx.TargetUsage = MFX_TARGETUSAGE_BALANCED;
	if (options.values.GopPicSize != 0)
		mfxEncParams.mfx.GopPicSize = options.values.GopPicSize;
	mfxEncParams.mfx.TargetKbps = options.values.Bitrate;
	mfxEncParams.mfx.RateControlMethod = MFX_RATECONTROL_VBR;
	mfxEncParams.mfx.FrameInfo.FrameRateExtN = options.values.FrameRateN;
	mfxEncParams.mfx.FrameInfo.FrameRateExtD = options.values.FrameRateD;
	mfxEncParams.mfx.FrameInfo.FourCC = MFX_FOURCC_NV12;
	mfxEncParams.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
	mfxEncParams.mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
	mfxEncParams.mfx.FrameInfo.CropX = 0;
	mfxEncParams.mfx.FrameInfo.CropY = 0;
	mfxEncParams.mfx.FrameInfo.CropW = options.values.Width;
	mfxEncParams.mfx.FrameInfo.CropH = options.values.Height;
	// Width must be a multiple of 16
	// Height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
	mfxEncParams.mfx.FrameInfo.Width = MSDK_ALIGN16(options.values.Width);
	mfxEncParams.mfx.FrameInfo.Height =
		(MFX_PICSTRUCT_PROGRESSIVE == mfxEncParams.mfx.FrameInfo.PicStruct) ?
		MSDK_ALIGN16(options.values.Height) :
		MSDK_ALIGN32(options.values.Height);

	mfxEncParams.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

	// Validate video encode parameters (optional)
	// - In this example the validation result is written to same structure
	// - MFX_WRN_INCOMPATIBLE_VIDEO_PARAM is returned if some of the video parameters are not supported,
	//   instead the encoder will select suitable parameters closest matching the requested configuration
	sts = mfxENC.Query(&mfxEncParams, &mfxEncParams);
	MSDK_IGNORE_MFX_STS(sts, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	// Query number of required surfaces for encoder
	mfxFrameAllocRequest EncRequest;
	memset(&EncRequest, 0, sizeof(EncRequest));
	sts = mfxENC.QueryIOSurf(&mfxEncParams, &EncRequest);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	mfxU16 nEncSurfNum = EncRequest.NumFrameSuggested;

	// Allocate surfaces for encoder
	// - Width and height of buffer must be aligned, a multiple of 32
	// - Frame surface array keeps pointers all surface planes and general frame info
	mfxU16 width = (mfxU16)MSDK_ALIGN32(EncRequest.Info.Width);
	mfxU16 height = (mfxU16)MSDK_ALIGN32(EncRequest.Info.Height);
	mfxU8 bitsPerPixel = 12;        // NV12 format is a 12 bits per pixel format
	mfxU32 surfaceSize = width * height * bitsPerPixel / 8;
	mfxU8* surfaceBuffers = (mfxU8*) new mfxU8[surfaceSize * nEncSurfNum];

	// Allocate surface headers (mfxFrameSurface1) for encoder
	mfxFrameSurface1** pEncSurfaces = new mfxFrameSurface1 *[nEncSurfNum];
	MSDK_CHECK_POINTER(pEncSurfaces, MFX_ERR_MEMORY_ALLOC);
	for (int i = 0; i < nEncSurfNum; i++) {
		pEncSurfaces[i] = new mfxFrameSurface1;
		memset(pEncSurfaces[i], 0, sizeof(mfxFrameSurface1));
		memcpy(&(pEncSurfaces[i]->Info), &(mfxEncParams.mfx.FrameInfo), sizeof(mfxFrameInfo));
		pEncSurfaces[i]->Data.Y = &surfaceBuffers[surfaceSize * i];
		pEncSurfaces[i]->Data.U = pEncSurfaces[i]->Data.Y + width * height;
		pEncSurfaces[i]->Data.V = pEncSurfaces[i]->Data.U + 1;
		pEncSurfaces[i]->Data.Pitch = width;
		//if (!bEnableInput) {
		//	ClearYUVSurfaceSysMem(pEncSurfaces[i], width, height);
		//}
	}

	// Initialize the Media SDK encoder
	sts = mfxENC.Init(&mfxEncParams);
	MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	// Retrieve video parameters selected by encoder.
	// - BufferSizeInKB parameter is required to set bit stream buffer size
	mfxVideoParam par;
	memset(&par, 0, sizeof(par));
	sts = mfxENC.GetVideoParam(&par);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	// Prepare Media SDK bit stream buffer
	mfxBitstream mfxBS;
	memset(&mfxBS, 0, sizeof(mfxBS));
	mfxBS.MaxLength = par.mfx.BufferSizeInKB * 1000;
	mfxBS.Data = new mfxU8[mfxBS.MaxLength];
	MSDK_CHECK_POINTER(mfxBS.Data, MFX_ERR_MEMORY_ALLOC);

	//alloc cpu memory	
	mfxU32  swap_buffer_len = options.values.Width*options.values.Height * 3 / 2;//for yv12
	mfxU8* swap_buffer = new mfxU8[swap_buffer_len];
	memset(swap_buffer, 0x00, swap_buffer_len);
	// ===================================
	// Start encoding the frames
	//

	mfxTime tStart, tEnd;
	mfxGetTime(&tStart);

	int nEncSurfIdx = 0;
	mfxSyncPoint syncp;
	mfxU32 nFrame = 0;

	//
	// Stage 1: Main encoding loop
	//
	while (!this->m_exited /*|| MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts*/) {
		nEncSurfIdx = GetFreeSurfaceIndex(pEncSurfaces, nEncSurfNum);   // Find free frame surface
		MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, nEncSurfIdx, MFX_ERR_MEMORY_ALLOC);

		mfxI32 nBytesRead =  this->m_source_callback(swap_buffer, swap_buffer_len);
		if (-1 == nBytesRead) {
			MSDK_SLEEP(3);
			continue;
		}

		sts = LoadRawFrame(pEncSurfaces[nEncSurfIdx], swap_buffer, nBytesRead);
		MSDK_BREAK_ON_ERROR(sts);

		for (;;) {
			// Encode a frame asychronously (returns immediately)
			sts = mfxENC.EncodeFrameAsync(NULL, pEncSurfaces[nEncSurfIdx], &mfxBS, &syncp);

			if (MFX_ERR_NONE < sts && !syncp) {     // Repeat the call if warning and no output
				if (MFX_WRN_DEVICE_BUSY == sts)
					MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call
			}
			else if (MFX_ERR_NONE < sts && syncp) {
				sts = MFX_ERR_NONE;     // Ignore warnings if output is available
				break;
			}
			else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts) {
				// Allocate more bitstream buffer memory here if needed...
				break;
			}
			else
				break;
		}

		if (MFX_ERR_NONE == sts) {
			sts = session.SyncOperation(syncp, 60000);      // Synchronize. Wait until encoded frame is ready
			MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

			mfxU32 nBytesWritten = m_sink_callback(mfxBS.Data + mfxBS.DataOffset, mfxBS.DataLength);
			if (nBytesWritten != mfxBS.DataLength)
				sts = MFX_ERR_UNDEFINED_BEHAVIOR;
			MSDK_BREAK_ON_ERROR(sts);
			mfxBS.DataLength = 0;

			++nFrame;
			if (bEnableOutput) {
				printf("Frame number: %d\r", nFrame);
				fflush(stdout);
			}
		}
	}

	// MFX_ERR_MORE_DATA means that the input file has ended, need to go to buffering loop, exit in case of other errors
	MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	//
	// Stage 2: Retrieve the buffered encoded frames
	//
	while (MFX_ERR_NONE <= sts) {
		for (;;) {
			// Encode a frame asychronously (returns immediately)
			sts = mfxENC.EncodeFrameAsync(NULL, NULL, &mfxBS, &syncp);

			if (MFX_ERR_NONE < sts && !syncp) {     // Repeat the call if warning and no output
				if (MFX_WRN_DEVICE_BUSY == sts)
					MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call
			}
			else if (MFX_ERR_NONE < sts && syncp) {
				sts = MFX_ERR_NONE;     // Ignore warnings if output is available
				break;
			}
			else
				break;
		}

		if (MFX_ERR_NONE == sts) {
			sts = session.SyncOperation(syncp, 60000);      // Synchronize. Wait until encoded frame is ready
			MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

			mfxU32 nBytesWritten = m_sink_callback(mfxBS.Data + mfxBS.DataOffset, mfxBS.DataLength);
			if (nBytesWritten != mfxBS.DataLength)
				sts = MFX_ERR_UNDEFINED_BEHAVIOR;
			MSDK_BREAK_ON_ERROR(sts);
			mfxBS.DataLength = 0;

			++nFrame;
			if (bEnableOutput) {
				printf("Frame number: %d\r", nFrame);
				fflush(stdout);
			}
		}
	}

	// MFX_ERR_MORE_DATA indicates that there are no more buffered frames, exit in case of other errors
	MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	mfxGetTime(&tEnd);
	double elapsed = TimeDiffMsec(tEnd, tStart) / 1000;
	double fps = ((double)nFrame / elapsed);
	printf("\nExecution time: %3.2f s (%3.2f fps)\n", elapsed, fps);

	// ===================================================================
	// Clean up resources
	//  - It is recommended to close Media SDK components first, before releasing allocated surfaces, since
	//    some surfaces may still be locked by internal Media SDK resources.

	mfxENC.Close();
	// session closed automatically on destruction

	for (int i = 0; i < nEncSurfNum; i++)
		delete pEncSurfaces[i];
	MSDK_SAFE_DELETE_ARRAY(pEncSurfaces);
	MSDK_SAFE_DELETE_ARRAY(mfxBS.Data);

	MSDK_SAFE_DELETE_ARRAY(surfaceBuffers);

	delete[] swap_buffer;

	Release();
	return MFX_ERR_NONE;
}

bool CEncodeThread::init(const char* params){
	if (strlen(params) >= sizeof m_parameter_buf)
		return false;

	strcpy(m_parameter_buf, params);
	return true;
}


int main(int /*argc*/, char** /*argv*/) {

	CEncodeThread encode;
	std::string paramter("-g 1920x1080 -b 3000 -f 30/1 -gop 30");
	encode.init(paramter.data());
	encode.start(std::bind(ReadNextFrame, std::placeholders::_1,std::placeholders::_2, g_reader), std::bind(WriteNextFrame, std::placeholders::_1, std::placeholders::_2, g_writer));
	encode.join();

	return 0;
}
