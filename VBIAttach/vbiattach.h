//------------------------------------------------------------------------------
// File: VBIAttach.h
//
// Copyright (c) Aaron Giles.  All rights reserved.
// Based from Microsoft DirectShow sample code.
//------------------------------------------------------------------------------

#include <stdio.h>

class CVBIInputPin : public CBaseInputPin
{
    friend class CVBIAttach;

protected:
    CVBIAttach *m_pTransformFilter;


public:

    CVBIInputPin(
        __in_opt LPCTSTR pObjectName,
        __inout CVBIAttach *pTransformFilter,
        __inout HRESULT * phr,
        __in_opt LPCWSTR pName);
#ifdef UNICODE
    CVBIInputPin(
        __in_opt LPCSTR pObjectName,
        __inout CVBIAttach *pTransformFilter,
        __inout HRESULT * phr,
        __in_opt LPCWSTR pName);
#endif

    STDMETHODIMP QueryId(__deref_out LPWSTR * Id)
    {
        return AMGetWideString(L"Vbi", Id);
    }

    // check that we can support this output type
    HRESULT CheckMediaType(const CMediaType* mtIn);

    // set the connection media type
    HRESULT SetMediaType(const CMediaType* mt);

    // --- IMemInputPin -----

    // here's the next block of data from the stream.
    // AddRef it yourself if you need to hold it beyond the end
    // of this call.
    STDMETHODIMP Receive(IMediaSample * pSample);

    // Check if it's OK to process samples
    virtual HRESULT CheckStreaming();

    // Media type
public:
    CMediaType& CurrentMediaType() { return m_mt; };

};


class CLinkedSample
{
public:
	CLinkedSample(CLinkedSample *& pHead, IMediaSample *pSample) :
		m_pSample(pSample),
		m_pHead(pHead),
		m_pNext(NULL),
		m_Start(-1),
		m_End(-1),
		m_FieldFlags(0)
	{
		m_pSample->AddRef();
	    m_pSample->GetTime(&m_Start, &m_End);

	    IMediaSample2 *pSample2;
	    if (SUCCEEDED(pSample->QueryInterface(__uuidof(IMediaSample2), (void **)&pSample2)))
	    {
   		    AM_SAMPLE2_PROPERTIES properties;
	        if (SUCCEEDED(pSample2->GetProperties(sizeof(properties), (PBYTE)&properties)))
	        	m_FieldFlags = properties.dwTypeSpecificFlags;
	        pSample2->Release();
	    }
	    
		if (pHead == NULL)
			pHead = this;
		else
		{
			CLinkedSample *curSample;
			for (curSample = pHead; curSample->m_pNext != NULL; curSample = curSample->m_pNext) ;
			curSample->m_pNext = this;
		}
	}
	
	virtual ~CLinkedSample()
	{
		ASSERT(m_pHead == this);
		m_pHead = m_pNext;
		m_pSample->Release();
	}
	
	IMediaSample *Sample() { return m_pSample; }
	CLinkedSample *Next() { return m_pNext; }
	REFERENCE_TIME StartTime() { return m_Start; }
	REFERENCE_TIME EndTime() { return m_End; }
	DWORD FieldFlags() { return m_FieldFlags; }

	int CenterOverlaps(CLinkedSample *pOtherSample)
	{
		REFERENCE_TIME mean = (m_Start + m_End) / 2;
		if (mean < pOtherSample->m_Start)
			return -1;
		else if (mean <= pOtherSample->m_End)
			return 0;
		else
			return 1;
	}
	
private:
	CLinkedSample *&m_pHead;
	CLinkedSample *m_pNext;
	IMediaSample *m_pSample;
	REFERENCE_TIME m_Start, m_End;
	DWORD m_FieldFlags;
};


class CVBIAttach : public CTransformFilter
{
public:

    DECLARE_IUNKNOWN;
    static CUnknown * WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);

    // Overrriden from CTransformFilter base class

    HRESULT Transform(CLinkedSample *pSourceSample, CLinkedSample *pVBISample1, CLinkedSample *pVBISample2, IMediaSample *pDestSample);
    HRESULT CheckInputType(const CMediaType *mtIn);
    HRESULT CheckTransform(const CMediaType *mtIn, const CMediaType *mtOut);
    HRESULT DecideBufferSize(IMemAllocator *pAlloc,
                             ALLOCATOR_PROPERTIES *pProperties);
    HRESULT GetMediaType(int iPosition, CMediaType *pMediaType);

	// Non-standard overrides

    virtual HRESULT StartStreaming();
    virtual int GetPinCount();
    virtual CBasePin * GetPin(int n);
    STDMETHODIMP FindPin(LPCWSTR Id, __deref_out IPin **ppPin);
    virtual HRESULT Receive(IMediaSample *pSample);
    virtual HRESULT ReceiveVBI(IMediaSample *pSample);
    virtual HRESULT BeginFlush(void);
    STDMETHODIMP Stop();
    
    // VBI-specific stuff

    HRESULT CheckVBIInputType(const CMediaType *mtIn);

private:

    // Constructor
    CVBIAttach(TCHAR *tszName, LPUNKNOWN punk, HRESULT *phr);
	virtual ~CVBIAttach();

    HRESULT ReceiveCommon();
    

	void DownsampleVBIData(UINT16 *pDest, int destWidth, const BYTE *pVBI, PKS_VBIINFOHEADER pVBIInfo, int vbiLine);
	void ComputeAndRenderVBI(UINT16 *pDest, UINT32 destWidth, int x);
	void PrintToVideo(UINT16 *pDest, UINT32 destWidth, int y, int x, const WCHAR *text);
    void Log(WCHAR *pFormat, ...);
	const WCHAR *FourCCString(DWORD fourcc);
	const WCHAR *GUIDString(const GUID *pGUID);
    void DeletePending();

	// private variables
	CMediaType m_sourceMediaType;
	CMediaType m_destMediaType;
	CMediaType m_vbiMediaType;
	
    friend class CVBIInputPin;
	CVBIInputPin *m_pVBIInput;
	
	CLinkedSample *m_pVideoSampleList;
	CLinkedSample *m_pVBISampleList;
	
	UINT32 m_videoCount;
	UINT32 m_vbiCount;
	DWORD m_firstTime;
	
	bool m_displayVbi;

	FILE *m_logFile;

	static const WCHAR *CVBIAttach::FontChars;
	static const BYTE CVBIAttach::Font[][5]; 
};
