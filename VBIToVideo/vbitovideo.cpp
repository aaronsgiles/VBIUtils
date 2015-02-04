//------------------------------------------------------------------------------
// File: VBIToVideo.cpp
//
// Copyright (c) Aaron Giles.  All rights reserved.
// Based from Microsoft DirectShow sample code.
//------------------------------------------------------------------------------

#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <streams.h>
#include <initguid.h>
#include <ks.h>
#include <ksmedia.h>

#if (1100 > _MSC_VER)
#include <olectlid.h>
#else
#include <olectl.h>
#endif
#include <shlobj.h>
#include <stdio.h>

#include "uids.h"
#include "vbitovideo.h"
#include "resource.h"

static const DWORD FOURCC_YUY2 = mmioFOURCC('Y','U','Y','2');   // uncompressed YUY2


// Setup information

const AMOVIESETUP_MEDIATYPE sudPinTypes[] =
{
	{
	    &KSDATAFORMAT_TYPE_VBI,     // Major type
		&KSDATAFORMAT_SUBTYPE_RAW8  // Minor type
	},
	{
		&MEDIATYPE_Video,			// Major type
		&MEDIASUBTYPE_NULL			// Minor type
	}
};

const AMOVIESETUP_PIN sudpPins[] =
{
    { L"Input",             // Pins string name
      FALSE,                // Is it rendered
      FALSE,                // Is it an output
      FALSE,                // Are we allowed none
      FALSE,                // And allowed many
      &CLSID_NULL,          // Connects to filter
      NULL,                 // Connects to pin
      1,                    // Number of types
      &sudPinTypes[0]       // Pin information
    },
    { L"Output",            // Pins string name
      FALSE,                // Is it rendered
      TRUE,                 // Is it an output
      FALSE,                // Are we allowed none
      FALSE,                // And allowed many
      &CLSID_NULL,          // Connects to filter
      NULL,                 // Connects to pin
      1,                    // Number of types
      &sudPinTypes[1]       // Pin information
    }
};

const AMOVIESETUP_FILTER sudVBIToVideo =
{
    &CLSID_VBIToVideo,      // Filter CLSID
    L"VBI To Video",        // String name
    MERIT_DO_NOT_USE,       // Filter merit
    2,                      // Number of pins
    sudpPins                // Pin information
};


// List of class IDs and creator functions for the class factory. This
// provides the link between the OLE entry point in the DLL and an object
// being created. The class factory will call the static CreateInstance

CFactoryTemplate g_Templates[] = {
    { L"CVBI To Video"
    , &CLSID_VBIToVideo
    , CVBIToVideo::CreateInstance
    , NULL
    , &sudVBIToVideo }
};
int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);


////////////////////////////////////////////////////////////////////////
//
// Exported entry points for registration and unregistration 
// (in this case they only call through to default implementations).
//
////////////////////////////////////////////////////////////////////////

//
// DllRegisterServer
//
// Handles sample registry and unregistry
//
STDAPI DllRegisterServer()
{
    return AMovieDllRegisterServer2( TRUE );

} // DllRegisterServer


//
// DllUnregisterServer
//
STDAPI DllUnregisterServer()
{
    return AMovieDllRegisterServer2( FALSE );

} // DllUnregisterServer


//
// DllEntryPoint
//
extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);

BOOL APIENTRY DllMain(HANDLE hModule, 
                      DWORD  dwReason, 
                      LPVOID lpReserved)
{
	return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}


//
// Helpers
//
static const WCHAR *FourCCString(DWORD fourcc)
{
	static WCHAR buffer[16][5];
	static int bufIndex;
	WCHAR *curBuffer = &buffer[bufIndex++ % 16][0];

	curBuffer[0] = (UINT8)(fourcc >> 0);
	curBuffer[1] = (UINT8)(fourcc >> 8);
	curBuffer[2] = (UINT8)(fourcc >> 16);
	curBuffer[3] = (UINT8)(fourcc >> 24);
	curBuffer[4] = 0;

	return curBuffer;
}


static const WCHAR *GUIDString(const GUID *pGUID)
{
	static WCHAR buffer[16][100];
	static int bufIndex;
	WCHAR *curBuffer = &buffer[bufIndex++ % 16][0];

	swprintf(curBuffer, 100, L"{%08X-%04X-%04X-%02X%02X%02X%02X%02X%02X%02X%02X}",
		pGUID->Data1,
		pGUID->Data2,
		pGUID->Data3,
		pGUID->Data4[0], pGUID->Data4[1], pGUID->Data4[2], pGUID->Data4[3], 
		pGUID->Data4[4], pGUID->Data4[5], pGUID->Data4[6], pGUID->Data4[7]);

	return curBuffer;
}


//
// Constructor
//
CVBIToVideo::CVBIToVideo(TCHAR *tszName,
                       LPUNKNOWN punk,
                       HRESULT *phr) :
    CTransformFilter(tszName, punk, CLSID_VBIToVideo)
{
	WCHAR folderPath[MAX_PATH + 20];
	HRESULT result = SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, folderPath);
	if (SUCCEEDED(result))
	{
		if (folderPath[wcslen(folderPath) - 1] != '\\')
			wcscat(folderPath, L"\\");
		wcscat(folderPath, L"vbitovideo.log");
		m_logFile = _wfopen(folderPath, L"w");
	}

} // (Constructor)


//
// Destructor
//
CVBIToVideo::~CVBIToVideo()
{
	if (m_logFile != NULL)
		fclose(m_logFile);
}


//
// CreateInstance
//
// Provide the way for COM to create a EZrgb24 object
//
CUnknown *CVBIToVideo::CreateInstance(LPUNKNOWN pUnknown, HRESULT *pResult)
{
    ASSERT(pResult != NULL);
    
    CVBIToVideo *pNewObject = new CVBIToVideo(NAME("VBI To Video"), pUnknown, pResult);

    if (pNewObject == NULL && pResult != NULL)
        *pResult = E_OUTOFMEMORY;

	return pNewObject;
}


//
// CheckInputType
//
// Check the input type is OK - return an error otherwise
//
HRESULT CVBIToVideo::CheckInputType(const CMediaType *pSourceType)
{
	if (m_logFile != NULL)
		fwprintf(m_logFile, L"CheckInputType: %s %s %s %p\n", GUIDString(&pSourceType->majortype), GUIDString(&pSourceType->subtype), GUIDString(&pSourceType->formattype), pSourceType->Format());

    if (pSourceType->majortype != KSDATAFORMAT_TYPE_VBI)
        return E_INVALIDARG;
    if (pSourceType->subtype != KSDATAFORMAT_SUBTYPE_RAW8)
        return E_INVALIDARG;
    if (pSourceType->formattype != KSDATAFORMAT_SPECIFIER_VBI)
        return E_INVALIDARG;

    PKS_VBIINFOHEADER pInfo = (PKS_VBIINFOHEADER)pSourceType->Format();
    if (pSourceType == NULL)
        return E_INVALIDARG;

	if (m_logFile != NULL)
	{
		fwprintf(m_logFile, L"CheckInputType: success\n");
		fwprintf(m_logFile, L"   StartLine = %d\n", pInfo->StartLine);
		fwprintf(m_logFile, L"   EndLine = %d\n", pInfo->EndLine);
		fwprintf(m_logFile, L"   SamplingFrequency = %d\n", pInfo->SamplingFrequency);
		fwprintf(m_logFile, L"   MinLineStartTime = %d\n", pInfo->MinLineStartTime);
		fwprintf(m_logFile, L"   MaxLineStartTime = %d\n", pInfo->MaxLineStartTime);
		fwprintf(m_logFile, L"   ActualLineStartTime = %d\n", pInfo->ActualLineStartTime);
		fwprintf(m_logFile, L"   ActualLineEndTime = %d\n", pInfo->ActualLineEndTime);
		fwprintf(m_logFile, L"   VideoStandard = %d\n", pInfo->VideoStandard);
		fwprintf(m_logFile, L"   SamplesPerLine = %d\n", pInfo->SamplesPerLine);
		fwprintf(m_logFile, L"   StrideInBytes = %d\n", pInfo->StrideInBytes);
		fwprintf(m_logFile, L"   BufferSize = %d\n", pInfo->BufferSize);
	}

	return NOERROR;
}


//
// CheckTransform
//
// Check a transform can be done between these formats
//
HRESULT CVBIToVideo::CheckTransform(const CMediaType *pSourceType, const CMediaType *pDestType)
{
	PKS_VBIINFOHEADER pSourceInfo = (PKS_VBIINFOHEADER)pSourceType->Format();
	if (m_logFile != NULL)
		fwprintf(m_logFile, L"CheckTransform: SourceType = %s %s %s %p\n", GUIDString(&pSourceType->majortype), GUIDString(&pSourceType->subtype), GUIDString(&pSourceType->formattype), pSourceType->Format());

	// make sure the source type is still valid
    if (pSourceType->majortype != KSDATAFORMAT_TYPE_VBI)
        return E_FAIL;
    if (pSourceType->subtype != KSDATAFORMAT_SUBTYPE_RAW8)
        return E_FAIL;
    if (pSourceType->formattype != KSDATAFORMAT_SPECIFIER_VBI)
        return E_FAIL;
	if (pSourceInfo == NULL)
		return E_FAIL;

	VIDEOINFOHEADER *pDestInfo = (VIDEOINFOHEADER *)pDestType->Format();
	if (m_logFile != NULL)
		fwprintf(m_logFile, L"CheckTransform: DestType = %s %s %s %p\n", GUIDString(&pDestType->majortype), GUIDString(&pDestType->subtype), GUIDString(&pDestType->formattype), pDestType->Format());

	// make sure the dest type is valid
    if (pDestType->majortype != MEDIATYPE_Video)
        return E_FAIL;
	if (pDestInfo == NULL)
		return E_FAIL;

	if (m_logFile != NULL)
	{
		fwprintf(m_logFile, L"CheckTransform: Width ... Source = %d, Dest = %d\n", pSourceInfo->SamplesPerLine, pDestInfo->bmiHeader.biWidth);
		fwprintf(m_logFile, L"CheckTransform: Height ... Source = %d, Dest = %d\n", pSourceInfo->EndLine + 1, pDestInfo->bmiHeader.biHeight);
		fwprintf(m_logFile, L"CheckTransform: Depth ... Dest = %d\n", pDestInfo->bmiHeader.biBitCount);
		fwprintf(m_logFile, L"CheckTransform: Compression ... Dest = %s\n", FourCCString(pDestInfo->bmiHeader.biCompression));
	}

	// if we don't match sizes, fail
	if (pDestInfo->bmiHeader.biWidth != pSourceInfo->SamplesPerLine)
		return E_FAIL;
	if (pDestInfo->bmiHeader.biHeight != pSourceInfo->EndLine + 1)
		return E_FAIL;
	if (pDestInfo->bmiHeader.biBitCount != 16 || pDestInfo->bmiHeader.biCompression != FOURCC_YUY2)
		return E_FAIL;

	if (m_logFile != NULL)
		fwprintf(m_logFile, L"CheckTransform: Success\n");

	// note the source and destination types here
	m_sourceMediaType = *pSourceType;
	m_destMediaType = *pDestType;

	return NOERROR;
}


//
// DecideBufferSize
//
// Tell the output pin's allocator what size buffers we
// require. Can only do this when the input is connected
//
HRESULT CVBIToVideo::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pProperties)
{
	if (m_logFile != NULL)
		fwprintf(m_logFile, L"DecideBufferSize\n");

	// verify that the input pin is connected
    if (!m_pInput->IsConnected())
	{
		if (m_logFile != NULL)
			fwprintf(m_logFile, L"DecideBufferSize: Failure -- input not connected\n");
        return E_UNEXPECTED;
	}

	// get the info header for the source media type
	PKS_VBIINFOHEADER pSourceInfo = (PKS_VBIINFOHEADER)m_pInput->CurrentMediaType().Format();
	ASSERT(pSourceInfo != NULL);

	// set up for one buffer of the necessary size
    pProperties->cBuffers = 1;
    pProperties->cbBuffer = sizeof(UINT16) * pSourceInfo->SamplesPerLine * (pSourceInfo->EndLine + 1);
    ASSERT(pProperties->cbBuffer != 0);

    // Ask the allocator to reserve us some sample memory, NOTE the function
    // can succeed (that is return NOERROR) but still not have allocated the
    // memory that we requested, so we must check we got whatever we wanted
    ALLOCATOR_PROPERTIES Actual;
    HRESULT result = pAlloc->SetProperties(pProperties, &Actual);
    if (FAILED(result))
	{
		if (m_logFile != NULL)
			fwprintf(m_logFile, L"DecideBufferSize: SetProperties failed (%08X)\n", result);
        return result;
	}

	if (m_logFile != NULL)
		fwprintf(m_logFile, L"DecideBufferSize: requested %dx%d, got %dx%d\n", pProperties->cBuffers, pProperties->cbBuffer, Actual.cBuffers, Actual.cbBuffer);
    if (pProperties->cBuffers > Actual.cBuffers || pProperties->cbBuffer > Actual.cbBuffer)
	{
		if (m_logFile != NULL)
			fwprintf(m_logFile, L"DecideBufferSize: Failure\n");
		return E_FAIL;
	}

	if (m_logFile != NULL)
		fwprintf(m_logFile, L"DecideBufferSize: Success\n");
	return NOERROR;
}


//
// GetMediaType
//
// We support one type
//
HRESULT CVBIToVideo::GetMediaType(int iPosition, CMediaType *pMediaType)
{
	if (m_logFile != NULL)
		fwprintf(m_logFile, L"GetMediaType (%d)\n", iPosition);

	// verify that the input pin is connected
    if (!m_pInput->IsConnected())
	{
		if (m_logFile != NULL)
			fwprintf(m_logFile, L"GetMediaType: Failure -- input not connected\n");
        return E_UNEXPECTED;
	}

	// only one format supported
	if (iPosition < 0)
		return E_INVALIDARG;
	else if (iPosition > 0)
		return VFW_S_NO_MORE_ITEMS;

	// get the info header for the source media type
	PKS_VBIINFOHEADER pSourceInfo = (PKS_VBIINFOHEADER)m_pInput->CurrentMediaType().Format();
	ASSERT(pSourceInfo != NULL);

	// allocate space for the VIDEOINFO
    VIDEOINFO *pVideoInfo = (VIDEOINFO *)pMediaType->AllocFormatBuffer(sizeof(*pVideoInfo));
	if (pVideoInfo == NULL)
	{
		if (m_logFile != NULL)
			fwprintf(m_logFile, L"GetMediaType: Failure -- out of memory\n");
		return E_OUTOFMEMORY;
	}
	ZeroMemory(pVideoInfo, sizeof(*pVideoInfo));

    // fill in bitmap information
    pVideoInfo->bmiHeader.biSize         = sizeof(BITMAPINFOHEADER);
    pVideoInfo->bmiHeader.biWidth        = pSourceInfo->SamplesPerLine;
    pVideoInfo->bmiHeader.biHeight       = pSourceInfo->EndLine + 1;
	pVideoInfo->bmiHeader.biBitCount     = 16;
	pVideoInfo->bmiHeader.biCompression  = FOURCC_YUY2;
    pVideoInfo->bmiHeader.biPlanes       = 1;
    pVideoInfo->bmiHeader.biSizeImage    = pVideoInfo->bmiHeader.biWidth * pVideoInfo->bmiHeader.biHeight * pVideoInfo->bmiHeader.biBitCount / 8;
    pVideoInfo->bmiHeader.biClrImportant = 0;

	SetRectEmpty(&pVideoInfo->rcSource); // we want the whole image area rendered.
    SetRectEmpty(&pVideoInfo->rcTarget); // no particular destination rectangle

	pMediaType->SetType(&MEDIATYPE_Video);
	pMediaType->SetFormatType(&FORMAT_VideoInfo);
	pMediaType->SetTemporalCompression(FALSE);

	// Work out the GUID for the subtype from the header info.
	const GUID SubTypeGUID = GetBitmapSubtype(&pVideoInfo->bmiHeader);
	pMediaType->SetSubtype(&SubTypeGUID);
	pMediaType->SetSampleSize(pVideoInfo->bmiHeader.biSizeImage);

	if (m_logFile != NULL)
	{
		fwprintf(m_logFile, L"GetMediaType: Describing output format as\n");
		fwprintf(m_logFile, L"   type = %s\n", GUIDString(&pMediaType->majortype));
		fwprintf(m_logFile, L"   subtype = %s\n", GUIDString(&pMediaType->subtype));
		fwprintf(m_logFile, L"   format = %s\n", GUIDString(&pMediaType->formattype));
		fwprintf(m_logFile, L"   sampleSize = %d\n", pMediaType->GetSampleSize());
		fwprintf(m_logFile, L"   biSize = %d\n", pVideoInfo->bmiHeader.biSize);
		fwprintf(m_logFile, L"   biWidth = %d\n", pVideoInfo->bmiHeader.biWidth);
		fwprintf(m_logFile, L"   biHeight = %d\n", pVideoInfo->bmiHeader.biHeight);
		fwprintf(m_logFile, L"   biBitCount = %d\n", pVideoInfo->bmiHeader.biBitCount);
		fwprintf(m_logFile, L"   biCompression = %s\n", FourCCString(pVideoInfo->bmiHeader.biCompression));
		fwprintf(m_logFile, L"   biPlanes = %d\n", pVideoInfo->bmiHeader.biPlanes);
		fwprintf(m_logFile, L"   biSizeImage = %d\n", pVideoInfo->bmiHeader.biSizeImage);
		fwprintf(m_logFile, L"   biClrImportant = %d\n", pVideoInfo->bmiHeader.biClrImportant);
	}

	return NOERROR;
}


//
// Transform
//
// Copy the input sample into the output sample - then transform the output
// sample 'in place'. If we have all keyframes, then we shouldn't do a copy
// If we have cinepak or indeo and are decompressing frame N it needs frame
// decompressed frame N-1 available to calculate it, unless we are at a
// keyframe. So with keyframed codecs, you can't get away with applying the
// transform to change the frames in place, because you'll mess up the next
// frames decompression. The runtime MPEG decoder does not have keyframes in
// the same way so it can be done in place. We know if a sample is key frame
// as we transform because the sync point property will be set on the sample
//
HRESULT CVBIToVideo::Transform(IMediaSample *pSourceSample, IMediaSample *pDestSample)
{
	if (m_logFile != NULL)
	{
		fwprintf(m_logFile, L"Transform (%p->%p)\n", pSourceSample, pDestSample);
	    REFERENCE_TIME startTime, endTime;
	    if (pSourceSample->GetTime(&startTime, &endTime) == NOERROR)
			fwprintf(m_logFile, L"   Time=%I64d-%I64d\n", startTime, endTime);
	    LONGLONG startMediaTime, endMediaTime;
	    if (pSourceSample->GetMediaTime(&startMediaTime, &endMediaTime) == NOERROR)
			fwprintf(m_logFile, L"   MediaTime=%I64d-%I64d\n", startMediaTime, endMediaTime);
	    HRESULT result = pSourceSample->IsSyncPoint();
		fwprintf(m_logFile, L"   SyncPoint=%s", (result == S_OK) ? "yes" : "no");
		result = pSourceSample->IsPreroll();
		fwprintf(m_logFile, L"   Preroll=%s", (result == S_OK) ? "yes" : "no");
	    result = pSourceSample->IsDiscontinuity();
		fwprintf(m_logFile, L"   Discontinuity=%s", (result == S_OK) ? "yes" : "no");
	}

	CheckPointer(pSourceSample,E_POINTER);   
	CheckPointer(pDestSample,E_POINTER);   

	// get the format data from the media types
	PKS_VBIINFOHEADER pSourceInfo = (PKS_VBIINFOHEADER)m_sourceMediaType.Format();
	VIDEOINFOHEADER *pDestInfo = (VIDEOINFOHEADER *)m_destMediaType.Format();

	// get source pointer
	BYTE *pSource;
	pSourceSample->GetPointer(&pSource);
	ASSERT(pSource != NULL);

	// get dest pointer
	BYTE *pDest;
	pDestSample->GetPointer(&pDest);
	ASSERT(pDest != NULL);

	// copy to the destination
	for (int y = 0; y < pDestInfo->bmiHeader.biHeight; y++)
	{
		UINT16 *dest = (UINT16 *)pDest + y * pDestInfo->bmiHeader.biWidth;
		if (y >= (int)pSourceInfo->StartLine && y <= (int)pSourceInfo->EndLine)
		{
			const UINT8 *source = pSource + pSourceInfo->StrideInBytes * (y - pSourceInfo->StartLine);
			for (int x = 0; x < pDestInfo->bmiHeader.biWidth; x++)
				*dest++ = 0x8000 | *source++;
		}
		else
		{
			for (int x = 0; x < pDestInfo->bmiHeader.biWidth; x++)
				*dest++ = 0;
		}
	}

    // set the actual data length
	pDestSample->SetActualDataLength(sizeof(UINT16) * pDestInfo->bmiHeader.biHeight * pDestInfo->bmiHeader.biWidth);
/*
	// copy the sample times
    REFERENCE_TIME startTime, endTime;
    if (pSourceSample->GetTime(&startTime, &endTime) == NOERROR)
        pDestSample->SetTime(&startTime, &endTime);

    LONGLONG startMediaTime, endMediaTime;
    if (pSourceSample->GetMediaTime(&startMediaTime, &endMediaTime) == NOERROR)
        pDestSample->SetMediaTime(&startMediaTime, &endMediaTime);

	// copy the sync point property
    HRESULT result = pSourceSample->IsSyncPoint();
    if (result == S_OK)
        pDestSample->SetSyncPoint(TRUE);
    else if (result == S_FALSE)
		pDestSample->SetSyncPoint(FALSE);

    // copy the preroll property
	result = pSourceSample->IsPreroll();
    if (result == S_OK)
        pDestSample->SetPreroll(TRUE);
    else if (result == S_FALSE)
        pDestSample->SetPreroll(FALSE);

    // copy the discontinuity property
    result = pSourceSample->IsDiscontinuity();
    if (result == S_OK)
	    pDestSample->SetDiscontinuity(TRUE);
    else if (result == S_FALSE)
        pDestSample->SetDiscontinuity(FALSE);
*/
    return NOERROR;
}
