/*****************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or
nondisclosure agreement with Intel Corporation and may not be copied
or disclosed except in accordance with the terms of that agreement.
Copyright(c) 2005-2014 Intel Corporation. All Rights Reserved.

*****************************************************************************/

#include "common_utils.h"
#include "cmd_options.h"

#include "msdk_decode.h"

static void usage(CmdOptionsCtx* ctx)
{
	printf(
		"Decodes INPUT and optionally writes OUTPUT.\n"
		"\n"
		"Usage: %s [options] INPUT [OUTPUT]\n", ctx->program);
}

bool CDecodeThread::init(const char* params) {
	if (strlen(params) >= sizeof m_parameter_buf)
		return false;

	strcpy(m_parameter_buf, params);
	return true;
}

int32_t CDecodeThread::run()
//int32_t CDecodeThread::run()
{
	//////////////////////////////////////////////////////////////////////////

	mfxStatus sts = MFX_ERR_NONE;
	bool bEnableOutput = true; // if true, removes all YUV file writing
	CmdOptions options;

	// =====================================================================
	// Intel Media SDK decode pipeline setup
	// - In this example we are decoding an AVC (H.264) stream
	// - Video memory surfaces are used to store the decoded frames
	//   (Note that when using HW acceleration video surfaces are prefered, for better performance)
	//

	// Read options from the command line (if any is given)
	memset(&options, 0, sizeof(CmdOptions));
	options.ctx.options = OPTIONS_DECODE;
	options.ctx.usage = usage;
	// Set default values:
	options.values.impl = MFX_IMPL_AUTO_ANY;

	char* parameters[20];
	char params_buf[1024];
	strcpy(params_buf, m_parameter_buf);

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

	// Initialize Intel Media SDK session
	// - MFX_IMPL_AUTO_ANY selects HW acceleration if available (on any adapter)
	// - Version 1.0 is selected for greatest backwards compatibility.
	// OS specific notes
	// - On Windows both SW and HW libraries may present
	// - On Linux only HW library only is available
	//   If more recent API features are needed, change the version accordingly
	mfxIMPL impl = options.values.impl;
	mfxVersion ver = { { 0, 1 } };
	MFXVideoSession session;

	mfxFrameAllocator mfxAllocator;
	sts = Initialize(impl, ver, &session, &mfxAllocator);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	// Create Media SDK decoder
	MFXVideoDECODE mfxDEC(session);

	// Set required video parameters for decode
	mfxVideoParam mfxVideoParams;
	memset(&mfxVideoParams, 0, sizeof(mfxVideoParams));
	mfxVideoParams.mfx.CodecId = MFX_CODEC_AVC;
	mfxVideoParams.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;

	// Prepare Media SDK bit stream buffer
	// - Arbitrary buffer size for this example
	mfxBitstream mfxBS;
	memset(&mfxBS, 0, sizeof(mfxBS));
	mfxBS.MaxLength = 1024 * 1024 * 50;
	mfxBS.Data = new mfxU8[mfxBS.MaxLength];
	MSDK_CHECK_POINTER(mfxBS.Data, MFX_ERR_MEMORY_ALLOC);

	mfxI32 nBytesRead = 0;
	// Read a chunk of data from stream file into bit stream buffer
	// - Parse bit stream, searching for header and fill video parameters structure
	// - Abort if bit stream header is not found in the first bit stream buffer chunk
again_read:
	memmove(mfxBS.Data, mfxBS.Data + mfxBS.DataOffset, mfxBS.DataLength);
	mfxBS.DataOffset = 0;

	nBytesRead = this->m_source_callback(mfxBS.Data + mfxBS.DataLength, mfxBS.MaxLength - mfxBS.DataLength);
	if (-1 == nBytesRead) {
		printf("No data!\n");
		MSDK_SLEEP(3);
		goto again_read;
	}
	mfxBS.DataLength += nBytesRead;

	//FILE* fSource;
	//MSDK_FOPEN(fSource, options.values.SourceName, "rb");
	//MSDK_CHECK_POINTER(fSource, MFX_ERR_NULL_PTR);

	//sts = ReadBitStreamData(&mfxBS, fSource);
	//MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	sts = mfxDEC.DecodeHeader(&mfxBS, &mfxVideoParams);
	MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	// Validate video decode parameters (optional)
	// sts = mfxDEC.Query(&mfxVideoParams, &mfxVideoParams);

	// Query number of required surfaces for decoder
	mfxFrameAllocRequest Request;
	memset(&Request, 0, sizeof(Request));
	sts = mfxDEC.QueryIOSurf(&mfxVideoParams, &Request);
	MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	mfxU16 numSurfaces = Request.NumFrameSuggested;

	Request.Type |= WILL_READ; // This line is only required for Windows DirectX11 to ensure that surfaces can be retrieved by the application

							   // Allocate surfaces for decoder
	mfxFrameAllocResponse mfxResponse;
	sts = mfxAllocator.Alloc(mfxAllocator.pthis, &Request, &mfxResponse);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	// Allocate surface headers (mfxFrameSurface1) for decoder
	mfxFrameSurface1** pmfxSurfaces = new mfxFrameSurface1 *[numSurfaces];
	MSDK_CHECK_POINTER(pmfxSurfaces, MFX_ERR_MEMORY_ALLOC);
	for (int i = 0; i < numSurfaces; i++) {
		pmfxSurfaces[i] = new mfxFrameSurface1;
		memset(pmfxSurfaces[i], 0, sizeof(mfxFrameSurface1));
		memcpy(&(pmfxSurfaces[i]->Info), &(mfxVideoParams.mfx.FrameInfo), sizeof(mfxFrameInfo));
		pmfxSurfaces[i]->Data.MemId = mfxResponse.mids[i];      // MID (memory id) represents one video NV12 surface
	}

	// Initialize the Media SDK decoder
	sts = mfxDEC.Init(&mfxVideoParams);
	MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	// ===============================================================
	// Start decoding the frames
	//

	mfxTime tStart, tEnd;
	mfxGetTime(&tStart);

	mfxSyncPoint syncp;
	mfxFrameSurface1* pmfxOutSurface = NULL;
	int nIndex = 0;
	mfxU32 nFrame = 0;

	//alloc cpu memory	
	options.values.Width = mfxVideoParams.mfx.FrameInfo.CropW;
	options.values.Height = mfxVideoParams.mfx.FrameInfo.CropH;

	//mfxU32  swap_buffer_len = options.values. *options.values.Height * 3 / 2;//for yv12
	mfxU8*  swap_buffer = NULL;
	mfxU32  swap_buffer_len = 0;

	std::chrono::time_point<std::chrono::high_resolution_clock> start_all, start, end;
	int64_t elipse = 0;
	int64_t elipse1 = 0;
	int64_t elipse_read_file = 0;
	//
	// Stage 1: Main decoding loop
	//
	while (MFX_ERR_NONE <= sts && !m_exited || MFX_ERR_MORE_SURFACE == sts  || MFX_ERR_MORE_DATA == sts) {
		start_all = start = std::chrono::high_resolution_clock::now();

		if (MFX_WRN_DEVICE_BUSY == sts)
			MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync

		if (MFX_ERR_MORE_DATA == sts) {

			memmove(mfxBS.Data, mfxBS.Data + mfxBS.DataOffset, mfxBS.DataLength);
			mfxBS.DataOffset = 0;

			nBytesRead = this->m_source_callback(mfxBS.Data + mfxBS.DataLength, mfxBS.MaxLength - mfxBS.DataLength);

			if (nBytesRead <= 0) {
				sts = MFX_ERR_MORE_DATA;
				printf("No data!\n");
			
				//FIXME: TEST!!!
				m_exited = true;
				break;

				MSDK_SLEEP(3);
				continue;
			}
			mfxBS.DataLength += nBytesRead;
		}

		end = std::chrono::high_resolution_clock::now();
		int elipse1 = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		start = end;


		if (MFX_ERR_MORE_SURFACE == sts || MFX_ERR_NONE == sts) {
			nIndex = GetFreeSurfaceIndex(pmfxSurfaces, numSurfaces);        // Find free frame surface
			MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, nIndex, MFX_ERR_MEMORY_ALLOC);
		}

		int elipse2 = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		start = end;

		// Decode a frame asychronously (returns immediately)
		//  - If input bitstream contains multiple frames DecodeFrameAsync will start decoding multiple frames, and remove them from bitstream
		sts = mfxDEC.DecodeFrameAsync(&mfxBS, pmfxSurfaces[nIndex], &pmfxOutSurface, &syncp);

		int elipse3 = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		start = end;

		// Ignore warnings if output is available,
		// if no output and no action required just repeat the DecodeFrameAsync call
		if (MFX_ERR_NONE < sts && syncp)
			sts = MFX_ERR_NONE;

		if (MFX_ERR_NONE == sts)
			sts = session.SyncOperation(syncp, 60000);      // Synchronize. Wait until decoded frame is ready

		int elipse4 = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
		start = end;

		if (MFX_ERR_NONE == sts) {
			++nFrame;
			if (bEnableOutput) {
				// Surface locking required when read/write video surfaces
				sts = mfxAllocator.Lock(mfxAllocator.pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
				MSDK_BREAK_ON_ERROR(sts);

				std::chrono::time_point<std::chrono::high_resolution_clock> start0, end0;
				start0 = std::chrono::high_resolution_clock::now();

				if (swap_buffer == NULL) {
#ifdef LINE_COPY_NV12
					swap_buffer_len =pmfxOutSurface->Info.CropW * pmfxOutSurface->Info.CropH * 3 / 2;
#else
					swap_buffer_len = pmfxOutSurface->Data.Pitch /*pmfxOutSurface->Info.CropW */* pmfxOutSurface->Info.CropH * 3 / 2;
#endif
					swap_buffer = new mfxU8[swap_buffer_len];
					memset(swap_buffer, 0x00, swap_buffer_len);
				}

				sts = WriteRawFrame(pmfxOutSurface, swap_buffer, swap_buffer_len);
				MSDK_BREAK_ON_ERROR(sts);

				end0 = std::chrono::high_resolution_clock::now();
				int64_t elipse0 = std::chrono::duration_cast<std::chrono::milliseconds>(end0 - start0).count();

				mfxU32 nBytesWritten = m_sink_callback(swap_buffer, swap_buffer_len);
				//if (nBytesWritten != swap_buffer_len)  //no need check the value of the return!!!
				//	sts = MFX_ERR_UNDEFINED_BEHAVIOR;
				//MSDK_BREAK_ON_ERROR(sts);

				sts = mfxAllocator.Unlock(mfxAllocator.pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
				MSDK_BREAK_ON_ERROR(sts);

				end = std::chrono::high_resolution_clock::now();
				int64_t elipse4= std::chrono::duration_cast<std::chrono::milliseconds>(end - start_all).count();

				//printf("Frame number: %03d,WriteRawFrame:%03lld, elipse1:%03lld,elipse2:%03lld,elipse3:%03lld,elipse4:%03lld, threadid:%d\r\n", nFrame,  elipse0,elipse1, elipse2, elipse3,elipse4, std::this_thread::get_id());
				//fflush(stdout);
			}
		}
	}

	// MFX_ERR_MORE_DATA means that file has ended, need to go to buffering loop, exit in case of other errors
	MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	//
	// Stage 2: Retrieve the buffered decoded frames
	//
	while (MFX_ERR_NONE <= sts  || MFX_ERR_MORE_SURFACE == sts ) {
		if (MFX_WRN_DEVICE_BUSY == sts)
			MSDK_SLEEP(1);  // Wait if device is busy, then repeat the same call to DecodeFrameAsync

		nIndex = GetFreeSurfaceIndex(pmfxSurfaces, numSurfaces);        // Find free frame surface
		MSDK_CHECK_ERROR(MFX_ERR_NOT_FOUND, nIndex, MFX_ERR_MEMORY_ALLOC);

		// Decode a frame asychronously (returns immediately)
		sts = mfxDEC.DecodeFrameAsync(NULL, pmfxSurfaces[nIndex], &pmfxOutSurface, &syncp);

		// Ignore warnings if output is available,
		// if no output and no action required just repeat the DecodeFrameAsync call
		if (MFX_ERR_NONE < sts && syncp)
			sts = MFX_ERR_NONE;

		if (MFX_ERR_NONE == sts)
			sts = session.SyncOperation(syncp, 60000);      // Synchronize. Waits until decoded frame is ready

		if (MFX_ERR_NONE == sts) {
			++nFrame;
			if (bEnableOutput) {
				// Surface locking required when read/write D3D surfaces
				sts = mfxAllocator.Lock(mfxAllocator.pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
				MSDK_BREAK_ON_ERROR(sts);

				if (swap_buffer == NULL) {
#ifdef LINE_COPY_NV12
					swap_buffer_len = pmfxOutSurface->Info.CropW * pmfxOutSurface->Info.CropH * 3 / 2;
#else
					swap_buffer_len = pmfxOutSurface->Data.Pitch /*pmfxOutSurface->Info.CropW */* pmfxOutSurface->Info.CropH * 3 / 2;
#endif
					swap_buffer = new mfxU8[swap_buffer_len];
					memset(swap_buffer, 0x00, swap_buffer_len);
				}

				sts = WriteRawFrame(pmfxOutSurface, swap_buffer, swap_buffer_len);
				MSDK_BREAK_ON_ERROR(sts);

				mfxU32 nBytesWritten = m_sink_callback(swap_buffer, swap_buffer_len);
				//if (nBytesWritten != swap_buffer_len)   //no need check the value of the return!!!
				//	sts = MFX_ERR_UNDEFINED_BEHAVIOR;
				//MSDK_BREAK_ON_ERROR(sts);

				sts = mfxAllocator.Unlock(mfxAllocator.pthis, pmfxOutSurface->Data.MemId, &(pmfxOutSurface->Data));
				MSDK_BREAK_ON_ERROR(sts);

				printf("Frame number: %d\r", nFrame);
				fflush(stdout);
			}
		}
	}

	// MFX_ERR_MORE_DATA indicates that all buffers has been fetched, exit in case of other errors
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

	mfxDEC.Close();
	// session closed automatically on destruction

	for (int i = 0; i < numSurfaces; i++)
		delete pmfxSurfaces[i];
	MSDK_SAFE_DELETE_ARRAY(pmfxSurfaces);
	MSDK_SAFE_DELETE_ARRAY(mfxBS.Data);

	mfxAllocator.Free(mfxAllocator.pthis, &mfxResponse);

	delete[] swap_buffer;

	Release();

	return 0;
}

void CDecodeThread::start(_SourceDataCallback ReadNextFrameCallback, _SinkDataCallback WriteFrameCallback) {
	m_exited = false;
	m_source_callback = ReadNextFrameCallback;
	m_sink_callback = WriteFrameCallback;
	m_impl = std::thread(&CDecodeThread::run, this);
}

void CDecodeThread::stop() {
	m_exited = true;
	m_impl.join();
}

void CDecodeThread::join() {
	m_impl.join();
}