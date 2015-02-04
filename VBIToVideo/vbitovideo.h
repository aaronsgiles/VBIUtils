//------------------------------------------------------------------------------
// File: VBIToVideo.h
//
// Copyright (c) Aaron Giles.  All rights reserved.
// Based from Microsoft DirectShow sample code.
//------------------------------------------------------------------------------

#include <stdio.h>

class CVBIToVideo : public CTransformFilter
{

public:

    DECLARE_IUNKNOWN;
    static CUnknown * WINAPI CreateInstance(LPUNKNOWN punk, HRESULT *phr);

    // Overrriden from CTransformFilter base class

    HRESULT Transform(IMediaSample *pIn, IMediaSample *pOut);
    HRESULT CheckInputType(const CMediaType *mtIn);
    HRESULT CheckTransform(const CMediaType *mtIn, const CMediaType *mtOut);
    HRESULT DecideBufferSize(IMemAllocator *pAlloc,
                             ALLOCATOR_PROPERTIES *pProperties);
    HRESULT GetMediaType(int iPosition, CMediaType *pMediaType);

private:

    // Constructor
    CVBIToVideo(TCHAR *tszName, LPUNKNOWN punk, HRESULT *phr);
	virtual ~CVBIToVideo();

	CMediaType m_sourceMediaType;
	CMediaType m_destMediaType;

	FILE *m_logFile;
};

