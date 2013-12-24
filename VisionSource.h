/********************************************************************************
 Copyright (C) 2012 Hugh Bailey <obs.jim@gmail.com>
 Copyright (C) 2012 Muf <muf@mindflyer.net>

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
	Texture* pTexture;
	bool bChanged;
};

enum SignalState
{
	active,
	inactive,
	invalid
};

struct SharedVisionInfo
{
	bool *pCapturing;
	bool *pUseDMA;
	Texture *(*pTextures)[NUM_BUFFERS];
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
	Texture				*texture;
    bool				bCapturing;
	bool				bUseDMA;
	bool				bPointFilter;
	LPBITMAPINFO		lpBitmapInfo[NUM_BUFFERS];
	PVOID				pBitmapBits[NUM_BUFFERS];
	HBITMAP				hBitmaps[NUM_BUFFERS];
	Texture				*pTextures[NUM_BUFFERS];
	Texture				*noSignalTex;
	Texture				*invalidSignalTex;
	unsigned long		Buffers;
	SharedVisionInfo	sharedInfo;
	unsigned long		lastTex;
	unsigned long		dropTex;
	SignalState			signal;
	SamplerState		*sampler;

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

