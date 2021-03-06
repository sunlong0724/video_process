/*****************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or
nondisclosure agreement with Intel Corporation and may not be copied
or disclosed except in accordance with the terms of that agreement.
Copyright(c) 2005-2014 Intel Corporation. All Rights Reserved.

*****************************************************************************/

#include "common_utils.h"
#include <nmmintrin.h>

// =================================================================
// Utility functions, not directly tied to Intel Media SDK functionality
//


void PrintErrString(int err,const char* filestr,int line)
{
    switch (err) {
    case   0:
        printf("\n No error.\n");
        break;
    case  -1:
        printf("\n Unknown error: %s %d\n",filestr,line);
        break;
    case  -2:
        printf("\n Null pointer.  Check filename/path + permissions? %s %d\n",filestr,line);
        break;
    case  -3:
        printf("\n Unsupported feature/library load error. %s %d\n",filestr,line);
        break;
    case  -4:
        printf("\n Could not allocate memory. %s %d\n",filestr,line);
        break;
    case  -5:
        printf("\n Insufficient IO buffers. %s %d\n",filestr,line);
        break;
    case  -6:
        printf("\n Invalid handle. %s %d\n",filestr,line);
        break;
    case  -7:
        printf("\n Memory lock failure. %s %d\n",filestr,line);
        break;
    case  -8:
        printf("\n Function called before initialization. %s %d\n",filestr,line);
        break;
    case  -9:
        printf("\n Specified object not found. %s %d\n",filestr,line);
        break;
    case -10:
        printf("\n More input data expected. %s %d\n",filestr,line);
        break;
    case -11:
        printf("\n More output surfaces expected. %s %d\n",filestr,line);
        break;
    case -12:
        printf("\n Operation aborted. %s %d\n",filestr,line);
        break;
    case -13:
        printf("\n HW device lost. %s %d\n",filestr,line);
        break;
    case -14:
        printf("\n Incompatible video parameters. %s %d\n",filestr,line);
        break;
    case -15:
        printf("\n Invalid video parameters. %s %d\n",filestr,line);
        break;
    case -16:
        printf("\n Undefined behavior. %s %d\n",filestr,line);
        break;
    case -17:
        printf("\n Device operation failure. %s %d\n",filestr,line);
        break;
    case -18:
        printf("\n More bitstream data expected. %s %d\n",filestr,line);
        break;
    case -19:
        printf("\n Incompatible audio parameters. %s %d\n",filestr,line);
        break;
    case -20:
        printf("\n Invalid audio parameters. %s %d\n",filestr,line);
        break;
    default:
        printf("\nError code %d,\t%s\t%d\n\n", err, filestr, line);
    }
}

mfxStatus ReadPlaneData(mfxU16 w, mfxU16 h, mfxU8* buf, mfxU8* ptr,
                        mfxU16 pitch, mfxU16 offset, FILE* fSource)
{
    mfxU32 nBytesRead;
    for (mfxU16 i = 0; i < h; i++) {
        nBytesRead = (mfxU32) fread(buf, 1, w, fSource);
        if (w != nBytesRead)
            return MFX_ERR_MORE_DATA;
        for (mfxU16 j = 0; j < w; j++)
            ptr[i * pitch + j * 2 + offset] = buf[j];
    }
    return MFX_ERR_NONE;
}

mfxStatus ReadPlaneData(mfxU16 w, mfxU16 h,/* mfxU8* buf,*/ mfxU8* ptr,
	mfxU16 pitch, mfxU16 offset, mfxU8* buffer, mfxU32 data_len)
{
//	mfxU32 nBytesRead;
	if (data_len <= 0)
		return MFX_ERR_MORE_DATA;

	for (mfxU16 i = 0; i < h; i++) {
		//nBytesRead = (mfxU32)fread(buf, 1, w, fSource);
		//if (w != nBytesRead)
		//	return MFX_ERR_MORE_DATA;
		for (mfxU16 j = 0; j < w; j++)
			ptr[i * pitch + j * 2 + offset] = buffer[i*w+j];
	}
	return MFX_ERR_NONE;
}

mfxStatus LoadRawFrame(mfxFrameSurface1* pSurface, mfxU8* buffer, mfxU32 data_len) {
	mfxStatus sts = MFX_ERR_NONE;
//	mfxU32 nBytesRead;
	mfxU16 w, h, i, pitch;
	mfxU8* ptr;
	mfxFrameInfo* pInfo = &pSurface->Info;
	mfxFrameData* pData = &pSurface->Data;

	if (data_len <= 0) {
		return MFX_ERR_MORE_DATA;
	}

	if (pInfo->CropH > 0 && pInfo->CropW > 0) {
		w = pInfo->CropW;
		h = pInfo->CropH;
	}
	else {
		w = pInfo->Width;
		h = pInfo->Height;
	}

	pitch = pData->Pitch;
	ptr = pData->Y + pInfo->CropX + pInfo->CropY * pData->Pitch;

	// read luminance plane
	for (i = 0; i < h; i++) {
		//nBytesRead = (mfxU32)fread(ptr + i * pitch, 1, w, fSource);
		//if (w != nBytesRead)
		//	return MFX_ERR_MORE_DATA;
		memcpy(ptr + i * pitch, buffer + i * w, w);//maybe has a bug!
	}

	buffer += w*h;
	data_len -= w*h;
//	mfxU8 buf[2048];        // maximum supported chroma width for nv12
	w /= 2;
	h /= 2;
	ptr = pData->UV + pInfo->CropX + (pInfo->CropY / 2) * pitch;
	if (w > 2048)
		return MFX_ERR_UNSUPPORTED;

	// load U
	sts = ReadPlaneData(w, h, ptr, pitch, 0, buffer, data_len);
	if (MFX_ERR_NONE != sts)
		return sts;

	data_len -= w*h;
	buffer += w*h;
	// load V
	ReadPlaneData(w, h,  ptr, pitch, 1, buffer, data_len);
	if (MFX_ERR_NONE != sts)
		return sts;

	return MFX_ERR_NONE;

}

mfxStatus LoadRawFrame(mfxFrameSurface1* pSurface, FILE* fSource)
{
    if (!fSource) {
        // Simulate instantaneous access to 1000 "empty" frames.
        static int frameCount = 0;
        if (1000 == frameCount++)
            return MFX_ERR_MORE_DATA;
        else
            return MFX_ERR_NONE;
    }

    mfxStatus sts = MFX_ERR_NONE;
    mfxU32 nBytesRead;
    mfxU16 w, h, i, pitch;
    mfxU8* ptr;
    mfxFrameInfo* pInfo = &pSurface->Info;
    mfxFrameData* pData = &pSurface->Data;

    if (pInfo->CropH > 0 && pInfo->CropW > 0) {
        w = pInfo->CropW;
        h = pInfo->CropH;
    } else {
        w = pInfo->Width;
        h = pInfo->Height;
    }

    pitch = pData->Pitch;
    ptr = pData->Y + pInfo->CropX + pInfo->CropY * pData->Pitch;

    // read luminance plane
    for (i = 0; i < h; i++) {
        nBytesRead = (mfxU32) fread(ptr + i * pitch, 1, w, fSource);
        if (w != nBytesRead)
            return MFX_ERR_MORE_DATA;
    }

    mfxU8 buf[2048];        // maximum supported chroma width for nv12
    w /= 2;
    h /= 2;
    ptr = pData->UV + pInfo->CropX + (pInfo->CropY / 2) * pitch;
    if (w > 2048)
        return MFX_ERR_UNSUPPORTED;

    // load U
    sts = ReadPlaneData(w, h, buf, ptr, pitch, 0, fSource);
    if (MFX_ERR_NONE != sts)
        return sts;
    // load V
    ReadPlaneData(w, h, buf, ptr, pitch, 1, fSource);
    if (MFX_ERR_NONE != sts)
        return sts;

    return MFX_ERR_NONE;
}

mfxStatus LoadRawRGBFrame(mfxFrameSurface1* pSurface, FILE* fSource)
{
    if (!fSource) {
        // Simulate instantaneous access to 1000 "empty" frames.
        static int frameCount = 0;
        if (1000 == frameCount++)
            return MFX_ERR_MORE_DATA;
        else
            return MFX_ERR_NONE;
    }

    size_t nBytesRead;
    mfxU16 w, h;
    mfxFrameInfo* pInfo = &pSurface->Info;

    if (pInfo->CropH > 0 && pInfo->CropW > 0) {
        w = pInfo->CropW;
        h = pInfo->CropH;
    } else {
        w = pInfo->Width;
        h = pInfo->Height;
    }

    for (mfxU16 i = 0; i < h; i++) {
        nBytesRead = fread(pSurface->Data.B + i * pSurface->Data.Pitch,
                           1, w * 4, fSource);
        if ((size_t)(w * 4) != nBytesRead)
            return MFX_ERR_MORE_DATA;
    }

    return MFX_ERR_NONE;
}

mfxStatus WriteBitStreamFrame(mfxBitstream* pMfxBitstream, FILE* fSink)
{
    mfxU32 nBytesWritten =
        (mfxU32) fwrite(pMfxBitstream->Data + pMfxBitstream->DataOffset, 1,
                        pMfxBitstream->DataLength, fSink);



    if (nBytesWritten != pMfxBitstream->DataLength)
        return MFX_ERR_UNDEFINED_BEHAVIOR;

    pMfxBitstream->DataLength = 0;

    return MFX_ERR_NONE;
}

mfxStatus ReadBitStreamData(mfxBitstream* pBS, FILE* fSource)
{
    memmove(pBS->Data, pBS->Data + pBS->DataOffset, pBS->DataLength);
    pBS->DataOffset = 0;

    mfxU32 nBytesRead = (mfxU32) fread(pBS->Data + pBS->DataLength, 1,
                                       pBS->MaxLength - pBS->DataLength,
                                       fSource);

    if (0 == nBytesRead)
        return MFX_ERR_MORE_DATA;

    pBS->DataLength += nBytesRead;

    return MFX_ERR_NONE;
}



mfxStatus WriteSection(mfxU8* plane, mfxU16 factor, mfxU16 chunksize,
                       mfxFrameInfo* pInfo, mfxFrameData* pData, mfxU32 i,
                       mfxU32 j, FILE* fSink)
{
    if (chunksize !=
        fwrite(plane +
               (pInfo->CropY * pData->Pitch / factor + pInfo->CropX) +
               i * pData->Pitch + j, 1, chunksize, fSink))
        return MFX_ERR_UNDEFINED_BEHAVIOR;
    return MFX_ERR_NONE;
}

mfxStatus WriteRawFrame(mfxFrameSurface1* pSurface, FILE* fSink)
{
    mfxFrameInfo* pInfo = &pSurface->Info;
    mfxFrameData* pData = &pSurface->Data;
    mfxU32 i, j, h, w;
    mfxStatus sts = MFX_ERR_NONE;

    for (i = 0; i < pInfo->CropH; i++)
        sts =
            WriteSection(pData->Y, 1, pInfo->CropW, pInfo, pData, i, 0,
                         fSink);

    h = pInfo->CropH / 2;
    w = pInfo->CropW;
    for (i = 0; i < h; i++)
        for (j = 0; j < w; j += 2)
            sts =
                WriteSection(pData->UV, 2, 1, pInfo, pData, i, j,
                             fSink);
    for (i = 0; i < h; i++)
        for (j = 1; j < w; j += 2)
            sts =
                WriteSection(pData->UV, 2, 1, pInfo, pData, i, j,
                             fSink);

    return sts;
}


mfxStatus WriteSection(mfxU8* plane, mfxU16 factor, mfxU16 chunksize,
	mfxFrameInfo* pInfo, mfxFrameData* pData, mfxU32 i,
	mfxU32 j, mfxU8* buffer, mfxU32 buffer_size, mfxU32& data_size)
{
	//if (chunksize !=
	//	fwrite(plane +
	//		(pInfo->CropY * pData->Pitch / factor + pInfo->CropX) +
	//		i * pData->Pitch + j, 1, chunksize, fSink))
	//	return MFX_ERR_UNDEFINED_BEHAVIOR;

	memcpy(buffer + data_size, plane + (pInfo->CropY * pData->Pitch / factor + pInfo->CropX) + i * pData->Pitch + j, chunksize);
	data_size += chunksize;

	return MFX_ERR_NONE;
}

//  CopyFrame( )
//
//  COPIES VIDEO FRAMES FROM USWC MEMORY TO WB SYSTEM MEMORY VIA CACHED BUFFER
//    ASSUMES PITCH IS A MULTIPLE OF 64B CACHE LINE SIZE, WIDTH MAY NOT BE

typedef		unsigned int		UINT;
#define		CACHED_BUFFER_SIZE	4096	

void	CopyFrame(void * pSrc, void * pDest, void * pCacheBlock,
	UINT width, UINT height, UINT pitch)
{
	__m128i		x0, x1, x2, x3;
	__m128i		*pLoad;
	__m128i		*pStore;
	__m128i		*pCache;
	UINT		x, y, yLoad, yStore;
	UINT		rowsPerBlock;
	UINT		width64;
	UINT		extraPitch;


	rowsPerBlock = CACHED_BUFFER_SIZE / pitch;
	width64 = (width + 63) & ~0x03f;
	extraPitch = (pitch - width64) / 16;

	pLoad = (__m128i *)pSrc;
	pStore = (__m128i *)pDest;

	//  COPY THROUGH 4KB CACHED BUFFER
	for (y = 0; y < height; y += rowsPerBlock)
	{
		//  ROWS LEFT TO COPY AT END
		if (y + rowsPerBlock > height)
			rowsPerBlock = height - y;

		pCache = (__m128i *)pCacheBlock;

		_mm_mfence();

		// LOAD ROWS OF PITCH WIDTH INTO CACHED BLOCK
		for (yLoad = 0; yLoad < rowsPerBlock; yLoad++)
		{
			// COPY A ROW, CACHE LINE AT A TIME
			for (x = 0; x < pitch; x += 64)
			{
				x0 = _mm_stream_load_si128(pLoad + 0);
				x1 = _mm_stream_load_si128(pLoad + 1);
				x2 = _mm_stream_load_si128(pLoad + 2);
				x3 = _mm_stream_load_si128(pLoad + 3);

				_mm_store_si128(pCache + 0, x0);
				_mm_store_si128(pCache + 1, x1);
				_mm_store_si128(pCache + 2, x2);
				_mm_store_si128(pCache + 3, x3);

				pCache += 4;
				pLoad += 4;
			}
		}

		_mm_mfence();

		pCache = (__m128i *)pCacheBlock;

		// STORE ROWS OF FRAME WIDTH FROM CACHED BLOCK
		for (yStore = 0; yStore < rowsPerBlock; yStore++)
		{
			// copy a row, cache line at a time
			for (x = 0; x < width64; x += 64)
			{
				x0 = _mm_load_si128(pCache);
				x1 = _mm_load_si128(pCache + 1);
				x2 = _mm_load_si128(pCache + 2);
				x3 = _mm_load_si128(pCache + 3);

				_mm_stream_si128(pStore, x0);
				_mm_stream_si128(pStore + 1, x1);
				_mm_stream_si128(pStore + 2, x2);
				_mm_stream_si128(pStore + 3, x3);

				pCache += 4;
				pStore += 4;
			}

			pCache += extraPitch;
			pStore += extraPitch;
		}
	}
}

#define		CACHED_BUFFER_SIZE	4096	
mfxU8*  g_swap_buffer2 = new mfxU8[CACHED_BUFFER_SIZE];

mfxStatus WriteRawFrame(mfxFrameSurface1* pSurface, mfxU8* buffer, mfxU32 buffer_size)
{
	mfxFrameInfo* pInfo = &pSurface->Info;
	mfxFrameData* pData = &pSurface->Data;
	mfxU32 i, j, h, w;
	mfxStatus sts = MFX_ERR_NONE;

	mfxU32 data_size = 0;

	CopyFrame(pData->Y, buffer + data_size, g_swap_buffer2, pInfo->CropW, pInfo->CropH, pData->Pitch);
	data_size += pData->Pitch*pInfo->CropH;
	CopyFrame(pData->UV, buffer + data_size, g_swap_buffer2, pInfo->CropW, pInfo->CropH/2, pData->Pitch);

	return sts;
}

int GetFreeTaskIndex(Task* pTaskPool, mfxU16 nPoolSize)
{
    if (pTaskPool)
        for (int i = 0; i < nPoolSize; i++)
            if (!pTaskPool[i].syncp)
                return i;
    return MFX_ERR_NOT_FOUND;
}

void ClearYUVSurfaceSysMem(mfxFrameSurface1* pSfc, mfxU16 width, mfxU16 height)
{
    // In case simulating direct access to frames we initialize the allocated surfaces with default pattern
    memset(pSfc->Data.Y, 100, width * height);  // Y plane
    memset(pSfc->Data.U, 50, (width * height)/2);  // UV plane
}


// Get free raw frame surface
int GetFreeSurfaceIndex(mfxFrameSurface1** pSurfacesPool, mfxU16 nPoolSize)
{
    if (pSurfacesPool)
        for (mfxU16 i = 0; i < nPoolSize; i++)
            if (0 == pSurfacesPool[i]->Data.Locked)
                return i;
    return MFX_ERR_NOT_FOUND;
}

char mfxFrameTypeString(mfxU16 FrameType)
{
    mfxU8 FrameTmp = FrameType & 0xF;
    char FrameTypeOut;
    switch (FrameTmp) {
    case MFX_FRAMETYPE_I:
        FrameTypeOut = 'I';
        break;
    case MFX_FRAMETYPE_P:
        FrameTypeOut = 'P';
        break;
    case MFX_FRAMETYPE_B:
        FrameTypeOut = 'B';
        break;
    default:
        FrameTypeOut = '*';
    }
    return FrameTypeOut;
}
