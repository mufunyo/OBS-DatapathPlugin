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

#include "DatapathPlugin.h"

/*Texture			*VisionSource::texture;
bool			VisionSource::bCapturing;
HRGB			VisionSource::hRGB;
LPBITMAPINFO	VisionSource::lpBitmapInfo;
PVOID			VisionSource::pBitmapBits[NUM_BUFFERS];
HBITMAP			VisionSource::hBitmaps[NUM_BUFFERS];
unsigned long	VisionSource::Buffers;*/

void VisionSource::CreateBitmapInformation(BITMAPINFO *pBitmapInfo, int width, int height, int bitCount)
{
   pBitmapInfo->bmiHeader.biWidth          = width;
   pBitmapInfo->bmiHeader.biHeight         = height;
   pBitmapInfo->bmiHeader.biBitCount       = bitCount;
   pBitmapInfo->bmiHeader.biSize           = sizeof(BITMAPINFOHEADER);
   pBitmapInfo->bmiHeader.biPlanes         = 1;
   pBitmapInfo->bmiHeader.biCompression    = BI_BITFIELDS;
   pBitmapInfo->bmiHeader.biSizeImage      = 0;
   pBitmapInfo->bmiHeader.biXPelsPerMeter  = 3000;
   pBitmapInfo->bmiHeader.biYPelsPerMeter  = 3000;
   pBitmapInfo->bmiHeader.biClrUsed        = 0;
   pBitmapInfo->bmiHeader.biClrImportant   = 0;
   pBitmapInfo->bmiHeader.biSizeImage      = width * height * bitCount / 8 ;

   switch ( bitCount )
   {
      case 16:
      {
         memcpy ( &pBitmapInfo->bmiColors, &ColourMasks[RGB_565], 
               sizeof(ColourMasks[RGB_565]) );
         break;
      }
      case 32:
      {
         memcpy ( &pBitmapInfo->bmiColors, &ColourMasks[RGB_888], 
               sizeof(ColourMasks[RGB_888]) );
         break;
      }
      default:
      {
         memcpy ( &pBitmapInfo->bmiColors, &ColourMasks[RGB_UNKNOWN], 
               sizeof(ColourMasks[RGB_UNKNOWN]) );
      }
   }
}

bool VisionSource::Init(XElement *data)
{
    traceIn(VisionSource::Init);
	//HDC hDC;


    this->data = data;
    UpdateSettings();

	lpBitmapInfo = (LPBITMAPINFO)Allocate(sizeof(BITMAPINFOHEADER)+(3*sizeof(DWORD)));

	if ( lpBitmapInfo )
	CreateBitmapInformation(lpBitmapInfo, data->GetInt(TEXT("resolutionWidth"), 640), data->GetInt(TEXT("resolutionHeight"), 480), 32); // bpp hardcoded
	
	return true;

    traceOut;
}

VisionSource::VisionSource()
{
	lpBitmapInfo = NULL;
	for(int i=0; i<NUM_BUFFERS; ++i) pBitmapBits[i] = NULL;
	for(int i=0; i<NUM_BUFFERS; ++i) hBitmaps[i] = NULL;
	for(int i=0; i<NUM_BUFFERS; ++i) pTextures[i] = NULL;
	Buffers = 0;
	hRGB = 0;
	bFlipVertical = true;
	sharedInfo.pCapturing = &bCapturing;
	sharedInfo.pTextures = &pTextures;
	sharedInfo.pBitmapBits = &pBitmapBits;
	sharedInfo.pBuffers = &Buffers;
	sharedInfo.hMutex = OSCreateMutex();
	sharedInfo.hBitmaps = &hBitmaps;
}

VisionSource::~VisionSource()
{
    traceIn(VisionSource::~VisionSource);

    Stop();

	if (Buffers)
	{
		unsigned long i;
		for (i = 0; i < Buffers; i++)
		{
			pTextures[i]->Unmap();
			//DeleteObject(hBitmaps[i]);
		}
	}
	//texture->Unmap();
	Free(lpBitmapInfo);
	OSCloseMutex(sharedInfo.hMutex);

    traceOut;
}

void VisionSource::Preprocess() { return; }

void VisionSource::Start()
{
    traceIn(VisionSource::Start);

    if(bCapturing)
        return;

	if (RGBERROR_NO_ERROR == RGBOpenInput(0, &hRGB)) // hardcode input # for now
	{
		if (RGBERROR_NO_ERROR == RGBSetFrameDropping(hRGB, 0))
		{
			if (RGBERROR_NO_ERROR == RGBSetFrameCapturedFn(hRGB, &Receive, (ULONG_PTR)&sharedInfo))
			{
				if (Buffers)
				{
				   for (unsigned long i = 0; i < Buffers; i++)
				   {
					  if (RGBERROR_NO_ERROR != RGBChainOutputBuffer(hRGB, lpBitmapInfo, pBitmapBits[i]))
						  break;
				   }

				   if (RGBERROR_NO_ERROR == RGBUseOutputBuffers(hRGB, TRUE))
						bCapturing = true;
				}

				if (RGBERROR_NO_ERROR == RGBStartCapture(hRGB))
					return;
			}
		}
	}
	AppWarning(TEXT("Could not open Vision input 0"));
    traceOut;
}

void VisionSource::Stop()
{
    traceIn(VisionSource::Stop);

    if(!bCapturing)
        return;

	if (0 == RGBUseOutputBuffers( hRGB, FALSE ))
		bCapturing = false;

	RGBStopCapture(hRGB);
	RGBCloseInput(hRGB);

	hRGB = 0;

    traceOut;
}

void VisionSource::BeginScene()
{
    traceIn(VisionSource::BeginScene);
	//UINT pitch;
	
	HDC hDC = GetDC(NULL);
		
	if (hDC)
	{
	// allocate textures
	while(Buffers < NUM_BUFFERS)
	{
		hBitmaps[Buffers] = CreateDIBSection(hDC, lpBitmapInfo, DIB_RGB_COLORS, &pBitmapBits[Buffers], NULL, 0);
		pTextures[Buffers] = CreateTexture(lpBitmapInfo->bmiHeader.biWidth, lpBitmapInfo->bmiHeader.biHeight, GS_BGR, NULL, FALSE, FALSE);
		//pTextures[Buffers]->Map((BYTE*&)pBitmapBits[Buffers], pitch);

		if (pTextures[Buffers])
		{
			Buffers++;
		}
		else
			AppWarning(TEXT("VisionSource: could not create texture"));
	}
	ReleaseDC (NULL, hDC);
	}
	

	//lpBitmapInfo->bmiHeader.biSizeImage = pitch*abs(lpBitmapInfo->bmiHeader.biHeight);

    Start();

    traceOut;
}

void VisionSource::EndScene()
{
    traceIn(VisionSource::EndScene);

    Stop();

    traceOut;
}

void RGBCBKAPI VisionSource::Receive(HWND hWnd, HRGB hRGB, LPBITMAPINFOHEADER pBitmapInfo, void *pBitmapBits, ULONG_PTR userData)
{
	SharedVisionInfo *sharedInfo = (SharedVisionInfo*)userData;
	OSEnterMutex(sharedInfo->hMutex);
	if(*sharedInfo->pCapturing)
    {
		//API->EnterSceneMutex();
		//(*sharedInfo->pTexture)->SetImage(pBitmapBits, GS_IMAGEFORMAT_BGRX, pBitmapInfo->biWidth*4);

		for (unsigned long i = 0; i < *sharedInfo->pBuffers; i++)
		{
			if ((*sharedInfo->pBitmapBits)[i] == pBitmapBits)
			{
				UINT pitch;
				BYTE* cruft;
				//(*sharedInfo->pTextures)[i]->Unmap();

				(*sharedInfo->pTextures)[i]->Map(cruft, pitch);
				SSECopy(cruft, pBitmapBits, pBitmapInfo->biHeight*pitch);
				(*sharedInfo->pTextures)[i]->Unmap();

				//(*sharedInfo->pTextures)[i]->SetImage(pBitmapBits, GS_IMAGEFORMAT_BGRX, pBitmapInfo->biWidth*4);
				(sharedInfo->qTexture).push((*sharedInfo->pTextures)[i]);
				break;
			}
		}

		//API->LeaveSceneMutex();

		//RGBChainOutputBuffer (hRGB, lpBitmapInfo, pBitmapBits);
    }
	OSLeaveMutex(sharedInfo->hMutex);
}

void VisionSource::Render(const Vect2 &pos, const Vect2 &size)
{
    traceIn(VisionSource::Render);

	OSEnterMutex(sharedInfo.hMutex);
	if (!sharedInfo.qTexture.empty())
	{
		//sharedInfo.qTexture.front()->Unmap();
		
		for (unsigned long i = 0; i < Buffers; i++)
		{
			if (pTextures[i] == sharedInfo.qTexture.front())
			{
				unsigned long ret;
				//UINT pitch;
				//BYTE* cruft;
				//API->EnterSceneMutex();
				//pTextures[i]->Unmap();
				DrawSprite(pTextures[i], 0xFFFFFFFF, pos.x, pos.y, pos.x+size.x, pos.y+size.y); // sharedInfo.qTexture.front()
				//pTextures[i]->Map(cruft, pitch);
				//API->LeaveSceneMutex();
				//(BYTE*&)pBitmapBits[i] = cruft;
				ret = RGBChainOutputBuffer(hRGB, lpBitmapInfo, pBitmapBits[i]);
				lastBuf = i;
				break;
			}
		}
		sharedInfo.qTexture.pop();
	}
	else
		DrawSprite(pTextures[lastBuf], 0xFFFFFFFF, pos.x, pos.y, pos.x+size.x, pos.y+size.y);
	OSLeaveMutex(sharedInfo.hMutex);

    /*if (NULL == texture)
		{
			// create the texture regardless, will just show up as red to indicate failure
			unsigned int size = lpBitmapInfo->bmiHeader.biWidth*lpBitmapInfo->bmiHeader.biHeight*4;
			BYTE *textureData = (BYTE*)Allocate(size);

			msetd(textureData, 0xFFFF0000, size);
			texture = CreateTexture(lpBitmapInfo->bmiHeader.biWidth, lpBitmapInfo->bmiHeader.biHeight, GS_BGR, textureData, FALSE, FALSE);
		
			Free(textureData);
		}
	else
    {
        if(bFlipVertical)
            DrawSprite(texture, 0xFFFFFFFF, pos.x, pos.y, pos.x+size.x, pos.y+size.y);
        else
            DrawSprite(texture, 0xFFFFFFFF, pos.x, pos.y+size.y, pos.x+size.x, pos.y);
    }*/

    traceOut;
}

void VisionSource::UpdateSettings()
{
    traceIn(VisionSource::UpdateSettings);

	renderCX = data->GetInt(TEXT("resolutionWidth"));
	renderCY = data->GetInt(TEXT("resolutionHeight"));

    //bool bWasCapturing = bCapturing;

    //if(bWasCapturing) Stop();

    //if(bWasCapturing) Start();

    traceOut;
}


