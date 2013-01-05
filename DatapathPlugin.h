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
};

extern LocaleStringLookup *pluginLocale;
#define PluginStr(text) pluginLocale->LookupString(TEXT2(text))
