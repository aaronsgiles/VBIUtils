//------------------------------------------------------------------------------
// File: VBIAttach.cpp
//
// Copyright (c) Aaron Giles.  All rights reserved.
// Based on Microsoft DirectShow sample code.
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
#include "vbiattach.h"
#include "vbifont.h"
extern "C" {
#include "vbiparse.h"
};

#define VBI_LINES_PER_FIELD		22
#define VBI_LINES_PER_FRAME		(2 * VBI_LINES_PER_FIELD)

static const DWORD FOURCC_YUY2 = mmioFOURCC('Y','U','Y','2');   // uncompressed YUY2


// Setup information

const AMOVIESETUP_MEDIATYPE sudPinTypeVideo =
{
	&MEDIATYPE_Video,			// Major type
	&MEDIASUBTYPE_YUY2			// Minor type
};

const AMOVIESETUP_MEDIATYPE sudPinTypeVBI =
{
    &KSDATAFORMAT_TYPE_VBI,     // Major type
	&KSDATAFORMAT_SUBTYPE_RAW8  // Minor type
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
      &sudPinTypeVideo      // Pin information
    },
    { L"Output",            // Pins string name
      FALSE,                // Is it rendered
      TRUE,                 // Is it an output
      FALSE,                // Are we allowed none
      FALSE,                // And allowed many
      &CLSID_NULL,          // Connects to filter
      NULL,                 // Connects to pin
      1,                    // Number of types
      &sudPinTypeVideo      // Pin information
    },
    { L"VBI",               // Pins string name
      FALSE,                // Is it rendered
      FALSE,                // Is it an output
      FALSE,                // Are we allowed none
      FALSE,                // And allowed many
      &CLSID_NULL,          // Connects to filter
      NULL,                 // Connects to pin
      1,                    // Number of types
      &sudPinTypeVBI        // Pin information
    }
};

const AMOVIESETUP_FILTER sudVBIAttach =
{
    &CLSID_VBIAttach,       // Filter CLSID
    L"VBI Attach",          // String name
    MERIT_DO_NOT_USE,       // Filter merit
    3,                      // Number of pins
    sudpPins                // Pin information
};


// List of class IDs and creator functions for the class factory. This
// provides the link between the OLE entry point in the DLL and an object
// being created. The class factory will call the static CreateInstance

CFactoryTemplate g_Templates[] = {
    { L"VBI Attach"
    , &CLSID_VBIAttach
    , CVBIAttach::CreateInstance
    , NULL
    , &sudVBIAttach }
};
int g_cTemplates = sizeof(g_Templates) / sizeof(g_Templates[0]);


////////////////////////////////////////////////////////////////////////
//
// Exported entry points for registration and unregistration 
// (in this case they only call through to default implementations).
//
////////////////////////////////////////////////////////////////////////

//-------------------------------------------------------
// DllRegisterServer
//
// Handle registration of our component via regsvr32.exe
//-------------------------------------------------------

STDAPI DllRegisterServer()
{
    return AMovieDllRegisterServer2(TRUE);
}


//-------------------------------------------------------
// DllUnregisterServer
//
// Handle unregistration of our component via 
// regsvr32.exe
//-------------------------------------------------------

STDAPI DllUnregisterServer()
{
    return AMovieDllRegisterServer2(FALSE);
}


//-------------------------------------------------------
// DllEntryPoint
//-------------------------------------------------------

extern "C" BOOL WINAPI DllEntryPoint(HINSTANCE, ULONG, LPVOID);
BOOL APIENTRY DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
{
	return DllEntryPoint((HINSTANCE)(hModule), dwReason, lpReserved);
}


//-------------------------------------------------------
// CVBIAttach::CVBIAttach
//
// Construct a CVBIAttach object
//-------------------------------------------------------

CVBIAttach::CVBIAttach(TCHAR *tszName, LPUNKNOWN pUnknown, HRESULT *pResult) :
    CTransformFilter(tszName, pUnknown, CLSID_VBIAttach),
    m_pVBIInput(NULL),
    m_pVideoSampleList(NULL),
    m_pVBISampleList(NULL),
    m_videoCount(0),
    m_vbiCount(0),
    m_firstTime(0),
    m_displayVbi(false),
    m_logFile(NULL)
{
	// get the path to the current user's documents folder
	WCHAR folderPath[MAX_PATH + 20];
	HRESULT result = SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, folderPath);
	if (SUCCEEDED(result))
	{
		// create a log file there
		if (folderPath[wcslen(folderPath) - 1] != '\\')
			wcscat(folderPath, L"\\");
		wcscat(folderPath, L"vbiattach.log");
		m_logFile = _wfopen(folderPath, L"w");
	}
}


//-------------------------------------------------------
// CVBIAttach::~CVBIAttach
//
// Destruct the CVBIAttach object
//-------------------------------------------------------

CVBIAttach::~CVBIAttach()
{
	Log(L"In destructor\n");
	Log(L"Total video frames = %d\n", m_videoCount);
	Log(L"Total VBI frames = %d\n", m_vbiCount);
	Log(L"Total time = %d ms\n", GetTickCount() - m_firstTime);

	// clear out any pending samples	
	DeletePending();
	
	// delete the VBI input
    delete m_pVBIInput;

	// close the log
	Log(L"Closing log\n");
	if (m_logFile != NULL)
		fclose(m_logFile);
}


//-------------------------------------------------------
// CVBIAttach::CreateInstance
//
// Provide the way for COM to create a CVBIAttach object
//-------------------------------------------------------

CUnknown *CVBIAttach::CreateInstance(LPUNKNOWN pUnknown, HRESULT *pResult)
{
    ASSERT(pResult != NULL);
    
    CVBIAttach *pNewObject = new CVBIAttach(NAME("VBI Attach"), pUnknown, pResult);
    if (pNewObject == NULL && pResult != NULL)
        *pResult = E_OUTOFMEMORY;

	return pNewObject;
}


//-------------------------------------------------------
// CVBIAttach::CheckInputType
//
// Check the input type is OK - return an error otherwise
//-------------------------------------------------------

HRESULT CVBIAttach::CheckInputType(const CMediaType *pSourceType)
{
	// get the format information of the source type
    VIDEOINFOHEADER *pSourceInfo = reinterpret_cast<VIDEOINFOHEADER *>(pSourceType->Format());
	Log(L"CheckInputType: %s %s %s %p\n", 
		GUIDString(&pSourceType->majortype), GUIDString(&pSourceType->subtype), 
		GUIDString(&pSourceType->formattype), pSourceInfo);

	// verify that the source type is a proper video stream
    if (pSourceType->majortype != MEDIATYPE_Video)
        return E_FAIL;
    if (pSourceType->subtype != MEDIASUBTYPE_YUY2)
    	return E_FAIL;
    if (pSourceType->formattype != FORMAT_VideoInfo)
        return E_INVALIDARG;
    if (pSourceInfo == NULL)
        return E_INVALIDARG;

	// log the actual information about the source data
	Log(L"\tbiSize = %d\n", pSourceInfo->bmiHeader.biSize);
	Log(L"\tbiWidth = %d\n", pSourceInfo->bmiHeader.biWidth);
	Log(L"\tbiHeight = %d\n", pSourceInfo->bmiHeader.biHeight);
	Log(L"\tbiBitCount = %d\n", pSourceInfo->bmiHeader.biBitCount);
	Log(L"\tbiCompression = %s\n", FourCCString(pSourceInfo->bmiHeader.biCompression));
	Log(L"\tbiPlanes = %d\n", pSourceInfo->bmiHeader.biPlanes);
	Log(L"\tbiSizeImage = %d\n", pSourceInfo->bmiHeader.biSizeImage);
	Log(L"\tbiClrImportant = %d\n", pSourceInfo->bmiHeader.biClrImportant);
	
	// additional checks just to be sure
	if (pSourceInfo->bmiHeader.biBitCount != 16 || pSourceInfo->bmiHeader.biCompression != FOURCC_YUY2)
		return E_INVALIDARG;
	return NOERROR;
}


//-------------------------------------------------------
// CVBIAttach::CheckVBIInputType
//
// Check the VBI pin's input type is OK - return an 
// error otherwise
//-------------------------------------------------------

HRESULT CVBIAttach::CheckVBIInputType(const CMediaType *pSourceType)
{
	// get the format information of the source type
    PKS_VBIINFOHEADER pSourceInfo = reinterpret_cast<PKS_VBIINFOHEADER>(pSourceType->Format());
	Log(L"CheckVBIInputType: SourceType = %s %s %s %p\n", 
		GUIDString(&pSourceType->majortype), GUIDString(&pSourceType->subtype), 
		GUIDString(&pSourceType->formattype), pSourceInfo);

	// verify that the source type is a proper VBI stream
    if (pSourceType->majortype != KSDATAFORMAT_TYPE_VBI)
        return E_INVALIDARG;
    if (pSourceType->subtype != KSDATAFORMAT_SUBTYPE_RAW8)
        return E_INVALIDARG;
    if (pSourceType->formattype != KSDATAFORMAT_SPECIFIER_VBI)
        return E_INVALIDARG;
    if (pSourceInfo == NULL)
        return E_INVALIDARG;

	// log the actual information about the VBI data
	Log(L"CheckVBIInputType: success\n");
	Log(L"\tStartLine = %d\n", pSourceInfo->StartLine);
	Log(L"\tEndLine = %d\n", pSourceInfo->EndLine);
	Log(L"\tSamplingFrequency = %d\n", pSourceInfo->SamplingFrequency);
	Log(L"\tMinLineStartTime = %d\n", pSourceInfo->MinLineStartTime);
	Log(L"\tMaxLineStartTime = %d\n", pSourceInfo->MaxLineStartTime);
	Log(L"\tActualLineStartTime = %d\n", pSourceInfo->ActualLineStartTime);
	Log(L"\tActualLineEndTime = %d\n", pSourceInfo->ActualLineEndTime);
	Log(L"\tVideoStandard = %d\n", pSourceInfo->VideoStandard);
	Log(L"\tSamplesPerLine = %d\n", pSourceInfo->SamplesPerLine);
	Log(L"\tStrideInBytes = %d\n", pSourceInfo->StrideInBytes);
	Log(L"\tBufferSize = %d\n", pSourceInfo->BufferSize);

	return NOERROR;
}


//-------------------------------------------------------
// CVBIAttach::CheckTransform
//
// Check a transform can be done between these formats
//-------------------------------------------------------

HRESULT CVBIAttach::CheckTransform(const CMediaType *pSourceType, const CMediaType *pDestType)
{
	// get the format information of the source type
	VIDEOINFOHEADER *pSourceInfo = reinterpret_cast<VIDEOINFOHEADER *>(pSourceType->Format());
	Log(L"CheckTransform: SourceType = %s %s %s %p\n", 
		GUIDString(&pSourceType->majortype), GUIDString(&pSourceType->subtype), 
		GUIDString(&pSourceType->formattype), pSourceInfo);

	// make sure the source type is still valid
    if (pSourceType->formattype != FORMAT_VideoInfo)
        return E_INVALIDARG;
	if (pSourceInfo == NULL)
		return E_FAIL;
	if (pSourceInfo->bmiHeader.biBitCount != 16 || pSourceInfo->bmiHeader.biCompression != FOURCC_YUY2)
		return E_INVALIDARG;

	// get the format information of the destiniation type
	VIDEOINFOHEADER *pDestInfo = reinterpret_cast<VIDEOINFOHEADER *>(pDestType->Format());
	Log(L"CheckTransform: DestType = %s %s %s %p\n", 
		GUIDString(&pDestType->majortype), GUIDString(&pDestType->subtype), 
		GUIDString(&pDestType->formattype), pDestType->Format());

	// make sure the destiniation type is valid
    if (pDestType->majortype != MEDIATYPE_Video)
        return E_FAIL;
    if (pDestType->subtype != MEDIASUBTYPE_YUY2)
    	return E_FAIL;
	if (pDestInfo == NULL)
		return E_FAIL;

	// log information about the source and destination formats
	Log(L"CheckTransform: Width ... Source = %d, Dest = %d\n", pSourceInfo->bmiHeader.biWidth, pDestInfo->bmiHeader.biWidth);
	Log(L"CheckTransform: Height ... Source = %d, Dest = %d\n", pSourceInfo->bmiHeader.biHeight, pDestInfo->bmiHeader.biHeight);
	Log(L"CheckTransform: Depth ... Source = %d, Dest = %d\n", pSourceInfo->bmiHeader.biBitCount, pDestInfo->bmiHeader.biBitCount);
	Log(L"CheckTransform: Compression ... Source = %s, Dest = %s\n", FourCCString(pSourceInfo->bmiHeader.biCompression), FourCCString(pDestInfo->bmiHeader.biCompression));

	// if we don't match sizes, fail
	if (pDestInfo->bmiHeader.biWidth != pSourceInfo->bmiHeader.biWidth)
		return E_FAIL;
	if (pDestInfo->bmiHeader.biHeight != pSourceInfo->bmiHeader.biHeight + VBI_LINES_PER_FRAME)
		return E_FAIL;
	if (pDestInfo->bmiHeader.biBitCount != 16 || pDestInfo->bmiHeader.biCompression != FOURCC_YUY2)
		return E_FAIL;
	Log(L"CheckTransform: Success\n");

	// remember the source and destination types here
	m_sourceMediaType = *pSourceType;
	m_destMediaType = *pDestType;
	return NOERROR;
}


//-------------------------------------------------------
// CVBIAttach::DecideBufferSize
//
// Tell the output pin's allocator what size buffers we
// require. Can only do this when the input is connected
//-------------------------------------------------------

HRESULT CVBIAttach::DecideBufferSize(IMemAllocator *pAllocator, ALLOCATOR_PROPERTIES *pProperties)
{
	// verify that the input pin is connected
    if (!m_pInput->IsConnected())
	{
		Log(L"DecideBufferSize: Failure -- input not connected\n");
        return E_UNEXPECTED;
	}

	// get the info header for the source media type
	VIDEOINFOHEADER *pSourceInfo = reinterpret_cast<VIDEOINFOHEADER *>(m_pInput->CurrentMediaType().Format());
	if (pSourceInfo == NULL)
	{
		Log(L"DecideBufferSize: pSourceInfo->Format() returned NULL\n");
		return E_UNEXPECTED;
	}

	// request 4 buffers of the necessary size
    pProperties->cBuffers = 4;
    pProperties->cbBuffer = sizeof(UINT16) * pSourceInfo->bmiHeader.biWidth * (pSourceInfo->bmiHeader.biHeight + VBI_LINES_PER_FRAME);

    // Ask the allocator to reserve us some sample memory
    ALLOCATOR_PROPERTIES actualProperties;
    HRESULT result = pAllocator->SetProperties(pProperties, &actualProperties);
    if (FAILED(result))
	{
		Log(L"DecideBufferSize: SetProperties failed (%08X)\n", result);
        return result;
	}

	// Note that SetProperties can return NOERROR but still not have allocated
	// the memory we requested, so we must check we got whatever we wanted
    if (pProperties->cBuffers > actualProperties.cBuffers || pProperties->cbBuffer > actualProperties.cbBuffer)
	{
		Log(L"DecideBufferSize: SetProperties failed: requested %dx%d, got %dx%d\n", 
			pProperties->cBuffers, pProperties->cbBuffer, 
			actualProperties.cBuffers, actualProperties.cbBuffer);
		return E_FAIL;
	}
	return NOERROR;
}


//-------------------------------------------------------
// CVBIAttach::GetMediaType
//
// Iterator through all our supported media types.
//-------------------------------------------------------

HRESULT CVBIAttach::GetMediaType(int iPosition, CMediaType *pMediaType)
{
	// verify that the input pin is connected
    if (!m_pInput->IsConnected())
	{
		Log(L"GetMediaType: Failure -- input not connected\n");
        return E_UNEXPECTED;
	}

	// only one format supported
	if (iPosition < 0)
		return E_INVALIDARG;
	else if (iPosition > 0)
		return VFW_S_NO_MORE_ITEMS;

	// get the info header for the source media type
	*pMediaType = m_pInput->CurrentMediaType();
	
	// output is the same as the input, but with VBI_LINES_PER_FRAME extra height
	VIDEOINFOHEADER *pDestInfo = reinterpret_cast<VIDEOINFOHEADER *>(pMediaType->Format());
	pDestInfo->bmiHeader.biHeight += VBI_LINES_PER_FRAME;
	return NOERROR;
}


//-------------------------------------------------------
// CVBIAttach::Transform
//
// Perform the actual transformation. This code relies
// on the various Receive() functions to have determined
// a proper trio of samples to combine.
//-------------------------------------------------------

HRESULT CVBIAttach::Transform(CLinkedSample *pSourceSample, CLinkedSample *pVBISample1, CLinkedSample *pVBISample2, IMediaSample *pDestSample)
{
	CheckPointer(pSourceSample,E_POINTER);   
	CheckPointer(pDestSample,E_POINTER);   

	// log information about the required video source frame
	Log(L"Transform:\n");
	Log(L"\tVideo: f=%02X t=%12I64d-%12I64d%s\n", 
		pSourceSample->FieldFlags(), pSourceSample->StartTime(), pSourceSample->EndTime(), 
		(pSourceSample->Sample()->IsDiscontinuity() == S_OK) ? L" (discontinuity)" : L"");

	// log information about VBI sample 1
	if (pVBISample1 != NULL)
		Log(L"\tVBI1:  f=%02X t=%12I64d-%12I64d%s\n", 
			pVBISample1->FieldFlags(), pVBISample1->StartTime(), pVBISample1->EndTime(), 
			(pVBISample1->Sample()->IsDiscontinuity() == S_OK) ? L" (discontinuity)" : L"");
	else
		Log(L"\tVBI1:  (none)\n");

	// log information about VBI sample 2
	if (pVBISample2 != NULL)
		Log(L"\tVBI2:  f=%02X t=%12I64d-%12I64d%s\n", 
			pVBISample2->FieldFlags(), pVBISample2->StartTime(), pVBISample2->EndTime(), 
			(pVBISample2->Sample()->IsDiscontinuity() == S_OK) ? L" (discontinuity)" : L"");
	else
		Log(L"\tVBI2:  (none)\n");
	
	// get the format data from the media types
	VIDEOINFOHEADER *pSourceInfo = reinterpret_cast<VIDEOINFOHEADER *>(m_sourceMediaType.Format());
	ASSERT(pSourceInfo != NULL);
	PKS_VBIINFOHEADER pVBIInfo = reinterpret_cast<PKS_VBIINFOHEADER>(m_vbiMediaType.Format());
	ASSERT(pVBIInfo != NULL);
	VIDEOINFOHEADER *pDestInfo = reinterpret_cast<VIDEOINFOHEADER *>(m_destMediaType.Format());
	ASSERT(pDestInfo != NULL);

	// get a source pointer from the source sample
	UINT16 *pSourceBase;
	pSourceSample->Sample()->GetPointer(reinterpret_cast<BYTE **>(&pSourceBase));
	ASSERT(pSourceBase != NULL);
	
	// get the VBI1 source pointer (or NULL if not present)
	BYTE *pVBI1 = NULL;
	if (pVBISample1 != NULL)
	{
		pVBISample1->Sample()->GetPointer(&pVBI1);
		ASSERT(pVBI1 != NULL);
	}

	// get the VBI2 source pointer (or NULL if not present)
	BYTE *pVBI2 = NULL;
	if (pVBISample2 != NULL)
	{
		pVBISample2->Sample()->GetPointer(&pVBI2);
		ASSERT(pVBI2 != NULL);
	}

	// get the destination pointer
	UINT16 *pDestBase;
	pDestSample->GetPointer(reinterpret_cast<BYTE **>(&pDestBase));
	ASSERT(pDestBase != NULL);

	// copy VBI data by iterating over both fields
	for (int fieldIndex = 0; fieldIndex < 2; fieldIndex++)
	{
		BYTE *pVBI = (fieldIndex == 0) ? pVBI1 : pVBI2;
		
		// iterate over all possible lines in the field
		for (DWORD vbiLine = 0; vbiLine < VBI_LINES_PER_FIELD; vbiLine++)
		{
			UINT16 *pDest = pDestBase + (vbiLine * 2 + fieldIndex) * pDestInfo->bmiHeader.biWidth;
			
			// if the VBI data provides this line, downsample to the target
			if (pVBI != NULL && vbiLine >= pVBIInfo->StartLine && vbiLine <= pVBIInfo->EndLine)
				DownsampleVBIData(pDest, pDestInfo->bmiHeader.biWidth, pVBI, pVBIInfo, vbiLine);

			// otherwise, just fill with 0
			else
				memset(pDest, 0, sizeof(*pDest) * pDestInfo->bmiHeader.biWidth);
		}
	}
	
	// copy the source lines
	for (int scanline = 0; scanline < pSourceInfo->bmiHeader.biHeight; scanline++)
	{
		const UINT16 *pSource = pSourceBase + scanline * pSourceInfo->bmiHeader.biWidth;
		UINT16 *pDest = pDestBase + (scanline + VBI_LINES_PER_FRAME) * pDestInfo->bmiHeader.biWidth;
		memcpy(pDest, pSource, pDestInfo->bmiHeader.biWidth * sizeof(*pDest));
	}
	
	// optionally render the VBI data
	Log(L"\tField 1 VBI:\n");
	ComputeAndRenderVBI(pDestBase, pDestInfo->bmiHeader.biWidth * 2, 10);
	Log(L"\tField 2 VBI:\n");
	ComputeAndRenderVBI(pDestBase + pDestInfo->bmiHeader.biWidth, pDestInfo->bmiHeader.biWidth * 2, pDestInfo->bmiHeader.biWidth / 2);

    // set the actual data length
	pDestSample->SetActualDataLength(sizeof(*pDestBase) * pDestInfo->bmiHeader.biHeight * pDestInfo->bmiHeader.biWidth);
    return NOERROR;
}


//-------------------------------------------------------
// CVBIAttach::StartStreaming
//
// Called when streaming begins. We override this to
// check for the appropriate key that displays the
// VBI overlay.
//-------------------------------------------------------

HRESULT CVBIAttach::StartStreaming()
{
	m_displayVbi = (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;
	return CTransformFilter::StartStreaming();
}


//-------------------------------------------------------
// CVBIAttach::BeginFlush
//
// Called when flushing starts. We throw away any
// accumulated samples in response.
//-------------------------------------------------------

HRESULT CVBIAttach::BeginFlush()
{
	Log(L"BeginFlush ... deleting pending samples\n");
	DeletePending();
	return CTransformFilter::BeginFlush();
}


//-------------------------------------------------------
// CVBIAttach::GetPinCount
//
// We override this to increase the number of pins on
// the filter. A standard transform filter has 2 pins.
//-------------------------------------------------------

int CVBIAttach::GetPinCount()
{
    return 3;
}


//-------------------------------------------------------
// CVBIAttach::GetPin
//
// Return a CBasePin object for each pin. The first two
// pins (video in and output) are handled by the base
// transform class, but we must override this to provide
// access to the VBI input pin. Like the parent class,
// we create the pins dynamically when they are asked
// for.
//-------------------------------------------------------

CBasePin *CVBIAttach::GetPin(int n)
{
    // Create the pin if necessary
    if (m_pVBIInput == NULL)
    {
	    HRESULT result = S_OK;
        m_pVBIInput = new CVBIInputPin(NAME("VBI input pin"),
                                       this,              // Owner filter
                                       &result,           // Result code
                                       L"VBI In");        // Pin name

        //  Can't fail
        ASSERT(SUCCEEDED(result));
        if (m_pVBIInput == NULL)
            return NULL;
    }

    // Return the appropriate pin
    if (n == 2)
    	return m_pVBIInput;
    else
    	return CTransformFilter::GetPin(n);
}


//-------------------------------------------------------
// CVBIAttach::FindPin
//
// If the Id is "Vbi", return a referenced pointer to
// our input pin; otherwise, let the base class handle
// any other pins.
//-------------------------------------------------------

STDMETHODIMP CVBIAttach::FindPin(LPCWSTR Id, __deref_out IPin **ppPin)
{
    CheckPointer(ppPin, E_POINTER);
    ValidateReadWritePtr(ppPin, sizeof(IPin *));
    
    if (wcscmp(Id, L"Vbi") == 0)
    {
    	*ppPin = GetPin(2);
        (*ppPin)->AddRef();
        return NOERROR;
    }
    return CTransformFilter::FindPin(Id, ppPin);
}


//-------------------------------------------------------
// CVBIAttach::Stop
//
// When stopping, we make the VBI input inactive and
// delete any pending samples. We also call the base
// class to let it do the same for the input and output
// pins.
//-------------------------------------------------------

STDMETHODIMP CVBIAttach::Stop()
{
	Log(L"Stop ... deleting pending samples\n");
	{
	    CAutoLock lock(&m_csFilter);
	    
	    if (m_State != State_Stopped && m_pVBIInput != NULL)
		    m_pVBIInput->Inactive();
		DeletePending();
	}
	return CTransformFilter::Stop();
}


//-------------------------------------------------------
// CVBIAttach::Receive
//
// We override this from the base class and handle our
// own receive logic. This function simply tracks
// samples and adds them to the linked list for future
// processing. We then call ReceiveCommon which checks
// to see if we have enough accumulated to process.
//-------------------------------------------------------

HRESULT CVBIAttach::Receive(IMediaSample *pSample)
{
	// note the time of the first sample
	if (m_firstTime == 0)
		m_firstTime = GetTickCount();
	
	// increment the count of video samples
	m_videoCount++;

	// check for other streams and pass them on
    AM_SAMPLE2_PROPERTIES * const pProps = m_pInput->SampleProps();
    if (pProps->dwStreamId != AM_STREAM_MEDIA)
        return m_pOutput->Deliver(pSample);

    ASSERT(pSample != NULL);
    ASSERT(m_pOutput != NULL);

	// allocate a new sample for the video sample list
    new CLinkedSample(m_pVideoSampleList, pSample);
    
    // see if we can process anything
   	return ReceiveCommon();
}


//-------------------------------------------------------
// CVBIAttach::ReceiveVBI
//
// Equivalent logic to CVBIAttach::Receive, except for
// incoming VBI samples.
//-------------------------------------------------------

HRESULT CVBIAttach::ReceiveVBI(IMediaSample *pSample)
{
	// note the time of the first sample
	if (m_firstTime == 0)
		m_firstTime = GetTickCount();
	
	// increment the count of video samples
	m_vbiCount++;

    ASSERT(pSample != NULL);
    ASSERT(m_pOutput != NULL);

	// allocate a new sample for the VBI sample list
    new CLinkedSample(m_pVBISampleList, pSample);
    
    // see if we can process anything
   	return ReceiveCommon();
}


//-------------------------------------------------------
// CVBIAttach::ReceiveCommon
//
// Look at all the queued samples to see if we have a
// video frame and matching VBI samples. If so, process
// them as a group via the Transform method.
//-------------------------------------------------------

HRESULT CVBIAttach::ReceiveCommon()
{
	Log(L"ReceiveCommon:\n");
	
	// if no video sample, we're done
	CLinkedSample *videoSample = m_pVideoSampleList;
	if (videoSample == NULL)
	{
		Log(L"\tNo video samples\n");
		return NOERROR;
	}
	Log(L"\tProcessing video sample %I64d-%I64d\n", videoSample->StartTime(), videoSample->EndTime());
	
	// throw away any VBI samples that are incomplete
tryAgain:
	while (m_pVBISampleList != NULL && (m_pVBISampleList->FieldFlags() & 3) > 1)
	{
		Log(L"\tThrowing away VBI sample due to bad field #1 %I64d-%I64d (%d)\n", 
			m_pVBISampleList->StartTime(), m_pVBISampleList->EndTime(), m_pVBISampleList->FieldFlags() & 3);
		delete m_pVBISampleList;
	}
	
	// if we don't have two samples to check, stop
	if (m_pVBISampleList == NULL || m_pVBISampleList->Next() == NULL)
	{
		Log(L"\tNot enough VBI samples left\n");
		return NOERROR;
	}
	
	// if the two samples are not a pair, delete and try again
	if ((m_pVBISampleList->Next()->FieldFlags() & 3) != 2)
	{
		Log(L"\tThrowing away VBI sample due to bad field #2 %I64d-%I64d (%d)\n", 
			m_pVBISampleList->Next()->StartTime(), m_pVBISampleList->Next()->EndTime(), m_pVBISampleList->Next()->FieldFlags() & 3);
		delete m_pVBISampleList;
		goto tryAgain;
	}

	// the second one cannot be earlier than the video frame
	if (m_pVBISampleList->Next()->CenterOverlaps(videoSample) < 0)
	{
		Log(L"\tThrowing away VBI sample pair due to being too early %I64d-%I64d (%d)/%I64d-%I64d (%d)\n", 
			m_pVBISampleList->StartTime(), m_pVBISampleList->EndTime(), m_pVBISampleList->FieldFlags() & 3,
			m_pVBISampleList->Next()->StartTime(), m_pVBISampleList->Next()->EndTime(), m_pVBISampleList->Next()->FieldFlags() & 3);
		delete m_pVBISampleList;
		delete m_pVBISampleList;
		goto tryAgain;
	}

	// if the the first one overlaps the frame, we have a match
	CLinkedSample *vbiSample1 = NULL;
	CLinkedSample *vbiSample2 = NULL;
	Log(L"\tFirst VBI %I64d-%I64d: match=%d\n", m_pVBISampleList->StartTime(), m_pVBISampleList->EndTime(), m_pVBISampleList->CenterOverlaps(videoSample));
	Log(L"\tSecond VBI %I64d-%I64d: match=%d\n", m_pVBISampleList->Next()->StartTime(), m_pVBISampleList->Next()->EndTime(), m_pVBISampleList->Next()->CenterOverlaps(videoSample));
	if (m_pVBISampleList->CenterOverlaps(videoSample) == 0)
	{
		vbiSample1 = m_pVBISampleList;
		vbiSample2 = m_pVBISampleList->Next();
		Log(L"\tMatch accepted\n");
	}
	else
		Log(L"\tMatch denied\n");

    // Set up the output sample
    IMediaSample *pOutSample;
    HRESULT result = InitializeOutputSample(videoSample->Sample(), &pOutSample);
    if (FAILED(result))
    	goto exit;

    // transform the data
    result = Transform(videoSample, vbiSample1, vbiSample2, pOutSample);

	// this logic is copy/paste from the base class
    if (FAILED(result))
		DbgLog((LOG_TRACE,1,TEXT("Error from transform")));
    else
    {
        // the Transform() function can return S_FALSE to indicate that the
        // sample should not be delivered; we only deliver the sample if it's
        // really S_OK (same as NOERROR, of course.)
        if (result == NOERROR)
        {
    	    result = m_pOutput->Deliver(pOutSample);
            m_bSampleSkipped = FALSE;	// last thing no longer dropped
        }
        else
        {
            // S_FALSE returned from Transform is a PRIVATE agreement
            // We should return NOERROR from Receive() in this cause because returning S_FALSE
            // from Receive() means that this is the end of the stream and no more data should
            // be sent.
            if (S_FALSE == result)
            {
                //  Release the sample before calling notify to avoid
                //  deadlocks if the sample holds a lock on the system
                //  such as DirectDraw buffers do
                pOutSample->Release();
                m_bSampleSkipped = TRUE;
                if (!m_bQualityChanged)
                {
                    NotifyEvent(EC_QUALITY_CHANGE,0,0);
                    m_bQualityChanged = TRUE;
                }
                result = NOERROR;
		    	goto exit;
            }
        }
    }

    // release the output buffer. If the connected pin still needs it,
    // it will have addrefed it itself.
    pOutSample->Release();

exit:
	delete videoSample;
	delete vbiSample1;
	delete vbiSample2;

    return result;
}


//-------------------------------------------------------
// CVBIAttach::DownsampleVBIData
//
// Downsample VBI data to the destination at the given
// width
//-------------------------------------------------------

void CVBIAttach::DownsampleVBIData(UINT16 *pDest, int destWidth, const BYTE *pVBI, PKS_VBIINFOHEADER pVBIInfo, int vbiLine)
{
	const UINT8 *vbi = (UINT8 *)pVBI + (vbiLine - pVBIInfo->StartLine) * pVBIInfo->StrideInBytes;
	UINT32 vbiSourceStep = (pVBIInfo->SamplesPerLine << 16) / destWidth;

	// iterate over destination pixels
	for (int x = 0; x < destWidth; x++)
	{
		UINT32 start = x * vbiSourceStep;
		UINT32 stop = start + vbiSourceStep;
		UINT32 sum = 0;

		// if we're starting on a pixel other than the target, handle the partial pixel
		if ((start >> 16) != (stop >> 16))
		{
			UINT32 frac = 0x10000 - (start & 0xffff);
			sum = frac * vbi[start >> 16];
			start += frac;
		}
		
		// while we're still waiting to hit the stop pixel, count whole pixels
		while ((start >> 16) != (stop >> 16))
		{
			sum += vbi[start >> 16] << 16;
			start += 0x10000;
		}
		
		// count the last partial pixel
		if (start != stop)
			sum += (stop - start) * vbi[start >> 16];

		// set the Cr/Cb component to 0x80 and put the scaled final result in Y
		*pDest++ = 0x8000 | (UINT8)(sum / vbiSourceStep);
	}
}
			

//-------------------------------------------------------
// CVBIAttach::ComputeAndRenderVBI
//
// Compute the raw VBI data and render it as an overlay
// on top of the video stream.
//-------------------------------------------------------

void CVBIAttach::ComputeAndRenderVBI(UINT16 *pDest, UINT32 destWidth, int x)
{
	WCHAR text[80];
	UINT8 bits[24];
	
	// line 11 contains the white flag
	bool white = (vbi_parse_white_flag(pDest + 11*destWidth, destWidth/2, 0) != 0);
	swprintf(text, 80, L"LINE11 %s", white ? L"WHITE" : L"");
	Log(L"\t\t%s\n", text);
	if (m_displayVbi)
		PrintToVideo(pDest, destWidth, 200, x, text);

	// line 16 contains stop code and other interesting bits
	UINT32 line16 = 0;
	if (vbi_parse_manchester_code(pDest + 16*destWidth, destWidth/2, 0, 24, bits) == 24)
		for (int i = 0; i < 24; i++)
			line16 = (line16 << 1) | bits[i];
	if (line16 == 0x82cfff)
		swprintf(text, 80, L"LINE16 %06X STOP", line16);
	else
		swprintf(text, 80, L"LINE16 %06X", line16);
	Log(L"\t\t%s\n", text);
	if (m_displayVbi)
		PrintToVideo(pDest, destWidth, 210, x, text);

	/* line 17 and 18 contain frame/chapter/lead in/out encodings */
	for (int lineNum = 17; lineNum <= 18; lineNum++)
	{
		UINT32 line1718 = 0;
		if (vbi_parse_manchester_code(pDest + lineNum*destWidth, destWidth/2, 0, 24, bits) == 24)
			for (int i = 0; i < 24; i++)
				line1718 = (line1718 << 1) | bits[i];
		if (line1718 == 0x88ffff)
			swprintf(text, 80, L"LINE%d %06X LEADIN", lineNum, line1718);
		else if (line1718 == 0x80eeee)
			swprintf(text, 80, L"LINE%d %06X LEADOUT", lineNum, line1718);
		else if ((line1718 & 0xf00000) == 0xf00000)
			swprintf(text, 80, L"LINE%d %06X FRAME %05X", lineNum, line1718, line1718 & 0x7ffff);
		else if ((line1718 & 0xf00fff) == 0x800ddd)
			swprintf(text, 80, L"LINE%d %06X CHAPTER %02X", lineNum, line1718, (line1718 >> 12) & 0x7f);
		else
			swprintf(text, 80, L"LINE%d %06X", lineNum, line1718);
		Log(L"\t\t%s\n", text);
		if (m_displayVbi)
			PrintToVideo(pDest, destWidth, 220 + 10 * (lineNum - 17), x, text);
	}
}


//-------------------------------------------------------
// CVBIAttach::PrintToVideo
//
// Print a string of text to the destination on
// alternate scanlines.
//-------------------------------------------------------

void CVBIAttach::PrintToVideo(UINT16 *pDest, UINT32 destWidth, int y, int x, const WCHAR *text)
{
	// advance the destination pointer to the appropriate starting pixel
	pDest += y * destWidth + x;
	
	// iterate over the string
	for ( ; *text != 0; text++)
	{
		// find the current character in the font string; if not found, skip it
		const WCHAR *pos = wcschr(FontChars, *text);
		if (pos == NULL)
			continue;
		
		// get a pointer to the font character data
		const BYTE *pData = &Font[pos - FontChars][0];
		
		// iterate over the 5 rows of pixels
		for (int fontRow = 0; fontRow < 5; fontRow++)
		{
			UINT8 data = *pData++;
			pDest[fontRow * destWidth + 0] = (data & 8) ? 0x80ff : 0x8000;
			pDest[fontRow * destWidth + 1] = (data & 8) ? 0x80ff : 0x8000;
			pDest[fontRow * destWidth + 2] = (data & 4) ? 0x80ff : 0x8000;
			pDest[fontRow * destWidth + 3] = (data & 4) ? 0x80ff : 0x8000;
			pDest[fontRow * destWidth + 4] = (data & 2) ? 0x80ff : 0x8000;
			pDest[fontRow * destWidth + 5] = (data & 2) ? 0x80ff : 0x8000;
			pDest[fontRow * destWidth + 6] = (data & 1) ? 0x80ff : 0x8000;
			pDest[fontRow * destWidth + 7] = (data & 1) ? 0x80ff : 0x8000;
			pDest[fontRow * destWidth + 8] = 0x8000;
			pDest[fontRow * destWidth + 9] = 0x8000;

			pDest[fontRow * destWidth + destWidth / 2 + 0] = 0x8000;
			pDest[fontRow * destWidth + destWidth / 2 + 1] = 0x8000;
			pDest[fontRow * destWidth + destWidth / 2 + 2] = 0x8000;
			pDest[fontRow * destWidth + destWidth / 2 + 3] = 0x8000;
			pDest[fontRow * destWidth + destWidth / 2 + 4] = 0x8000;
			pDest[fontRow * destWidth + destWidth / 2 + 5] = 0x8000;
			pDest[fontRow * destWidth + destWidth / 2 + 6] = 0x8000;
			pDest[fontRow * destWidth + destWidth / 2 + 7] = 0x8000;
			pDest[fontRow * destWidth + destWidth / 2 + 8] = 0x8000;
			pDest[fontRow * destWidth + destWidth / 2 + 9] = 0x8000;
		}
		
		// advance to the next character
		pDest += 10;
	}
}


//-------------------------------------------------------
// CVBIAttach::Log
//
// Output to the log file if it exists
//-------------------------------------------------------

void CVBIAttach::Log(WCHAR *pFormat, ...)
{
	if (m_logFile != NULL)
	{
		va_list argptr;
		va_start(argptr, pFormat);
		vfwprintf(m_logFile, pFormat, argptr);
		va_end(argptr);
		fflush(m_logFile);
	}
}


//-------------------------------------------------------
// CVBIAttach::FourCCString
//
// Convert a FOURCC to a string and return a pointer 
// to it
//-------------------------------------------------------

const WCHAR *CVBIAttach::FourCCString(DWORD fourcc)
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


//-------------------------------------------------------
// CVBIAttach::GUIDString
//
// Convert a GUID to a string and return a pointer to it
//-------------------------------------------------------

const WCHAR *CVBIAttach::GUIDString(const GUID *pGUID)
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


//-------------------------------------------------------
// CVBIAttach::DeletePending
//
// Delete any pending samples we have queued up
//-------------------------------------------------------

void CVBIAttach::DeletePending()
{
    CAutoLock lck(&m_csReceive);
	while (m_pVideoSampleList != NULL)
		delete m_pVideoSampleList;
	while (m_pVBISampleList != NULL)
		delete m_pVBISampleList;
}


//-------------------------------------------------------
// CVBIInputPin::CVBIInputPin
//
// Constructor for the VBI input pin object. 
//-------------------------------------------------------

CVBIInputPin::CVBIInputPin(
    __in_opt LPCTSTR pObjectName,
    __inout CVBIAttach *pTransformFilter,
    __inout HRESULT * pResult,
    __in_opt LPCWSTR pName)
    : CBaseInputPin(pObjectName, pTransformFilter, &pTransformFilter->m_csFilter, pResult, pName)
{
    DbgLog((LOG_TRACE,2,TEXT("CVBIInputPin::CVBIInputPin")));
    m_pTransformFilter = pTransformFilter;
}

#ifdef UNICODE
CVBIInputPin::CVBIInputPin(
    __in_opt LPCSTR pObjectName,
    __inout CVBIAttach *pTransformFilter,
    __inout HRESULT * pResult,
    __in_opt LPCWSTR pName)
    : CBaseInputPin(pObjectName, pTransformFilter, &pTransformFilter->m_csFilter, pResult, pName)
{
    DbgLog((LOG_TRACE,2,TEXT("CVBIInputPin::CVBIInputPin")));
    m_pTransformFilter = pTransformFilter;
}
#endif


//-------------------------------------------------------
// CVBIInputPin::CheckMediaType
//
// Pass this to the owning CVBIAttach filter. 
//-------------------------------------------------------

HRESULT CVBIInputPin::CheckMediaType(const CMediaType* pmt)
{
	return m_pTransformFilter->CheckVBIInputType(pmt);
}


//-------------------------------------------------------
// CVBIInputPin::SetMediaType
//
// Ensure all is ok, and set the VBI media type on the
// transform filter for future reference. 
//-------------------------------------------------------

HRESULT CVBIInputPin::SetMediaType(const CMediaType* mt)
{
    // Set the base class media type (should always succeed)
    HRESULT result = CBasePin::SetMediaType(mt);
    if (FAILED(result))
        return result;

    // check the transform can be done (should always succeed)
    ASSERT(SUCCEEDED(m_pTransformFilter->CheckVBIInputType(mt)));
	m_pTransformFilter->m_vbiMediaType = *mt;
    return result;
}


//-------------------------------------------------------
// CVBIInputPin::Receive
//
// Receive new data. Again, we pass this through to the
// owning CVBIAttach filter. 
//-------------------------------------------------------

HRESULT CVBIInputPin::Receive(IMediaSample *pSample)
{
    CAutoLock lck(&m_pTransformFilter->m_csReceive);
    ASSERT(pSample != NULL);

    // check all is well with the base class
    HRESULT result = CBaseInputPin::Receive(pSample);
    if (result == S_OK)
        result = m_pTransformFilter->ReceiveVBI(pSample);
    return result;
}


//-------------------------------------------------------
// CVBIInputPin::CheckStreaming
//
// Check status against the owning CVBIAttach filter. 
//-------------------------------------------------------

HRESULT CVBIInputPin::CheckStreaming()
{
	ASSERT(m_pTransformFilter->m_pOutput != NULL);

	if (!m_pTransformFilter->m_pOutput->IsConnected())
		return VFW_E_NOT_CONNECTED;

	//  Shouldn't be able to get any data if we're not connected!
	ASSERT(IsConnected());

	//  we're flushing
	if (m_bFlushing)
		return S_FALSE;

	//  Don't process stuff in Stopped state
	if (IsStopped())
		return VFW_E_WRONG_STATE;
	if (m_bRunTimeError)
		return VFW_E_RUNTIME_ERROR;

	return S_OK;
}
