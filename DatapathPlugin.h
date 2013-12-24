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

#include "OBSApi.h"

#include <strsafe.h>

#include "VisionSource.h"
#include "resource.h"

//-----------------------------------------------------------

extern HINSTANCE hinstMain;
static HRGBDLL   gHRGBDLL = 0;

#define RGB_UNKNOWN  0
#define RGB_565      1
#define RGB_888      2

static struct
{
   COLORREF Mask[3];
}  ColourMasks[] =
{
   { 0x00000000, 0x00000000, 0x00000000, },  /* Unknown */
   { 0x0000f800, 0x000007e0, 0x0000001f, },  /* RGB565 */
   { 0x00ff0000, 0x0000ff00, 0x000000ff, },  /* RGB888 */
};

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

void CreateBitmapInformation(BITMAPINFO *pBitmapInfo, int width, int height, int bitCount, DWORD fourCC = 0);
void SetCropping(HRGB hRGB, int* pLeft, int* pTop, int* pWidth, int* pHeight);
void GetModeText(unsigned long input, TSTR string, size_t size);
static RGBMODECHANGEDFN ModeChanged;

extern LocaleStringLookup *pluginLocale;
#define PluginStr(text) pluginLocale->LookupString(TEXT2(text))
