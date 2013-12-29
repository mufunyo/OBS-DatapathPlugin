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

#include "OBSApi.h"

#include <strsafe.h>

#include "VisionSource.h"
#include "resource.h"

//-----------------------------------------------------------

extern HINSTANCE	hinstMain;
extern D3D9Context	*gpD3D9;
static HRGBDLL		gHRGBDLL = 0;

struct ConfigVisionInfo
{
    XElement *data;
	VisionSource *source;
	HWND hConfigWnd;
	HANDLE hMutex;
	HRGB hRGB;
	BOOL cropping;
	BOOL customRes;
	BOOL pointFilter;
	PixelFmt pixFormat;
	unsigned long inputs;
	int input;
	int width;
	unsigned long widthCur;
	unsigned long widthMin;
	unsigned long widthMax;
	int height;
	unsigned long heightCur;
	unsigned long heightMin;
	unsigned long heightMax;
	int cropTop;
	long cropTopCur;
	long cropTopMin;
	int cropLeft;
	long cropLeftCur;
	long cropLeftMin;
	int cropWidth;
	unsigned long cropWidthCur;
	unsigned long cropWidthMin;
	int cropHeight;
	unsigned long cropHeightCur;
	unsigned long cropHeightMin;
};

void CreateBitmapInformation(BITMAPINFO *pBitmapInfo, int width, int height, PixelFmt pixFormat);
void SetCropping(HRGB hRGB, int* pLeft, int* pTop, int* pWidth, int* pHeight);
void GetModeText(unsigned long input, TSTR string, size_t size);
static RGBMODECHANGEDFN ModeChanged;

extern LocaleStringLookup *pluginLocale;
#define PluginStr(text) pluginLocale->LookupString(TEXT2(text))
