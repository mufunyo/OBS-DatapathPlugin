/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>
 Copyright (C) 2012-2013 Muf <muf@mindflyer.net>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
********************************************************************************/


#pragma once

#include <rgb.h>
#include <rgbapi.h>
#include <rgberror.h>
#include <queue>

#include "DX9Crap.h"

#define NUM_BUFFERS 3

struct CapturedFrame
{
	D3D9SharedTexture *pTexture;
	bool bChanged;
};

enum SignalState
{
	active,
	inactive,
	invalid
};

enum PixelFmt
{
	RGB32,
	RGB24,
	RGB16,
	YUY2,
	NV12, // unused for now
	Y8
};

const int pixFmtBpp[] = { 32, 24, 16, 16, 12, 8 };
const DWORD pixFmtFCC[] = { BI_BITFIELDS, BI_BITFIELDS, BI_BITFIELDS, '2YUY', '21VN', '008Y' };
const D3DFORMAT pixFmtD3D[] = { D3DFMT_X8R8G8B8, D3DFMT_R8G8B8, D3DFMT_R5G6B5, D3DFMT_YUY2, (D3DFORMAT)'21VN', D3DFMT_L8 };

struct SharedVisionInfo
{
	bool *pCapturing;
	D3D9Texture *(*pTextures)[NUM_BUFFERS];
	D3D9SharedTexture *(*pSharedTextures)[NUM_BUFFERS];
	LPBITMAPINFO (*lpBitmapInfo)[NUM_BUFFERS];
	HBITMAP (*hBitmaps)[NUM_BUFFERS];
	PVOID (*pBitmapBits)[NUM_BUFFERS];
	unsigned long *pBuffers;
	std::queue<CapturedFrame> qFrames;
	SignalState *pSignal;
	HANDLE hMutex;
	HANDLE hDataMutex;
	XElement **data;
};



class VisionSource : public ImageSource
{
    bool				bFlipVertical;
    UINT				renderCX, renderCY;

    XElement			*data;
    bool				bCapturing;
	bool				bPointFilter;
	LPBITMAPINFO		lpBitmapInfo[NUM_BUFFERS];
	PVOID				pBitmapBits[NUM_BUFFERS];
	HBITMAP				hBitmaps[NUM_BUFFERS];
	D3D9Texture			*pTextures[NUM_BUFFERS];
	D3D9SharedTexture	*pSharedTextures[NUM_BUFFERS];
	Texture				*noSignalTex;
	Texture				*invalidSignalTex;
	unsigned long		Buffers;
	SharedVisionInfo	sharedInfo;
	unsigned long		lastTex;
	unsigned long		dropTex;
	SignalState			signal;
	SamplerState		*sampler;
	PixelFmt			pixFormat;

public:
    bool Init(XElement *data);
	VisionSource();
	~VisionSource();

    void UpdateSettings();

    void Preprocess();
    void Render(const Vect2 &pos, const Vect2 &size);

    void BeginScene();
    void EndScene();

	void Start();
    void Stop();

	static RGBFRAMECAPTUREDFNEX Receive;
	static RGBMODECHANGEDFN ResolutionSwitch;
	static RGBNOSIGNALFN NoSignal;
	static RGBINVALIDSIGNALFN InvalidSignal;
	
	HRGB hRGB;
	static HWND hConfigWnd;

    Vect2 GetSize() const {return Vect2(float(renderCX), float(renderCY));}
};

