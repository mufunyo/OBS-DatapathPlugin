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

#define NUM_BUFFERS 2

struct CapturedFrame
{
	Texture* pTexture;
	bool bChanged;
};

struct SharedVisionInfo
{
	bool *pCapturing;
	Texture *(*pTextures)[NUM_BUFFERS];
	LPBITMAPINFO (*lpBitmapInfo)[NUM_BUFFERS];
	HBITMAP (*hBitmaps)[NUM_BUFFERS];
	PVOID (*pBitmapBits)[NUM_BUFFERS];
	unsigned long *pBuffers;
	std::queue<CapturedFrame> qFrames;
	HANDLE hMutex;
};

class VisionSource : public ImageSource
{
    //---------------------------------

    bool					bFlipVertical;
    UINT					renderCX, renderCY;

    XElement				*data;
	Texture			*texture;
    bool				bCapturing;
	HRGB				hRGB;
	LPBITMAPINFO		lpBitmapInfo[NUM_BUFFERS];
	PVOID			pBitmapBits[NUM_BUFFERS];
	HBITMAP			hBitmaps[NUM_BUFFERS];
	Texture					*pTextures[NUM_BUFFERS];
	unsigned long	Buffers;
	SharedVisionInfo		sharedInfo;
	unsigned long	lastTex;

    //---------------------------------

    void Start();
    void Stop();
	static void CreateBitmapInformation(BITMAPINFO *pBitmapInfo, int width, int height, int bitCount);

public:
    bool Init(XElement *data);
	VisionSource();
	~VisionSource();

    void UpdateSettings();

    void Preprocess();
    void Render(const Vect2 &pos, const Vect2 &size);

    void BeginScene();
    void EndScene();

	static RGBFRAMECAPTUREDFNEX Receive;

    Vect2 GetSize() const {return Vect2(float(renderCX), float(renderCY));}
};

