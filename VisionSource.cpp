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
	int cropping = data->GetInt(TEXT("cropping"));
	int width = data->GetInt(cropping ? TEXT("cropWidth") : TEXT("resolutionWidth"), 640);
	int height = data->GetInt(cropping ? TEXT("cropHeight") : TEXT("resolutionHeight"), 480);

	for (int i = 0; i < NUM_BUFFERS; i++)
	{
		lpBitmapInfo[i] = (LPBITMAPINFO)Allocate(sizeof(BITMAPINFOHEADER)+(3*sizeof(DWORD)));
		if (lpBitmapInfo[i])
			CreateBitmapInformation(lpBitmapInfo[i], width, height, 32); // bpp hardcoded
	}
	
	return true;

    traceOut;
}

VisionSource::VisionSource()
{
	bFlipVertical = true;
	sharedInfo.pCapturing = &bCapturing;
	sharedInfo.pTextures = &pTextures;
	sharedInfo.lpBitmapInfo = &lpBitmapInfo;
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
			delete pTextures[i];
			DeleteObject(hBitmaps[i]);
			Free(lpBitmapInfo[i]);
		}
	}
	OSCloseMutex(sharedInfo.hMutex);

    traceOut;
}

void VisionSource::Preprocess() { return; }

void VisionSource::Start()
{
    traceIn(VisionSource::Start);

    if(bCapturing)
        return;

	int input = data->GetInt(TEXT("input"));

	if (RGBERROR_NO_ERROR == RGBOpenInput(input, &hRGB)) // hardcode input # for now
	{
		if (RGBERROR_NO_ERROR == RGBSetFrameDropping(hRGB, 0))
		{
			if(data->GetInt(TEXT("cropping")))
			{
				RGBEnableCropping(hRGB, 1);
				RGBSetCropping(hRGB, data->GetInt(TEXT("cropTop")), data->GetInt(TEXT("cropLeft")), data->GetInt(TEXT("cropWidth"), 640), data->GetInt(TEXT("cropHeight"), 480));
			}

			if (RGBERROR_NO_ERROR == RGBSetFrameCapturedFnEx(hRGB, &Receive, (ULONG_PTR)&sharedInfo))
			{
				if (Buffers)
				{
				   for (unsigned long i = 0; i < Buffers; i++)
				   {
					  if (RGBERROR_NO_ERROR != RGBChainOutputBuffer(hRGB, lpBitmapInfo[i], pBitmapBits[i]))
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
	TCHAR warning[255];
	StringCchPrintf(warning, 255, TEXT("Could not open Vision input %i"), input);
	AppWarning(warning);
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
		hBitmaps[Buffers] = CreateDIBSection(hDC, lpBitmapInfo[Buffers], DIB_RGB_COLORS, &pBitmapBits[Buffers], NULL, 0);
		pTextures[Buffers] = CreateTexture(lpBitmapInfo[Buffers]->bmiHeader.biWidth, lpBitmapInfo[Buffers]->bmiHeader.biHeight, GS_BGR, NULL, FALSE, FALSE);
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

void RGBCBKAPI VisionSource::Receive(HWND hWnd, HRGB hRGB, PRGBFRAMEDATA pFrameData, ULONG_PTR userData)
{
	SharedVisionInfo *sharedInfo = (SharedVisionInfo*)userData;
	OSEnterMutex(sharedInfo->hMutex);
	if(*sharedInfo->pCapturing)
    {
		//API->EnterSceneMutex();
		//(*sharedInfo->pTexture)->SetImage(pBitmapBits, GS_IMAGEFORMAT_BGRX, pBitmapInfo->biWidth*4);

		for (unsigned long i = 0; i < *sharedInfo->pBuffers; i++) // find which buffer we have been passed
		{
			if ((*sharedInfo->pBitmapBits)[i] == pFrameData->PBitmapBits)
			{
				UINT pitch;
				BYTE* cruft;
				CapturedFrame frame;
				//(*sharedInfo->pTextures)[i]->Unmap();

				(*sharedInfo->pTextures)[i]->Map(cruft, pitch);
				SSECopy(cruft, pFrameData->PBitmapBits, (*sharedInfo->pTextures)[i]->Height()*pitch);
				(*sharedInfo->pTextures)[i]->Unmap();

				frame.pTexture = (*sharedInfo->pTextures)[i];
				if ((pFrameData->PBitmapInfo->biWidth != (*sharedInfo->pTextures)[i]->Width()) || (pFrameData->PBitmapInfo->biHeight != (*sharedInfo->pTextures)[i]->Height()))
				{					
					HDC hDC = GetDC(NULL);
					if (hDC)
					{
						DeleteObject((*sharedInfo->hBitmaps)[i]);
						CreateBitmapInformation((*sharedInfo->lpBitmapInfo)[i], pFrameData->PBitmapInfo->biWidth, pFrameData->PBitmapInfo->biHeight, 32); // bpp hardcoded
						(*sharedInfo->hBitmaps)[i] = CreateDIBSection(hDC, (*sharedInfo->lpBitmapInfo)[i], DIB_RGB_COLORS, &(*sharedInfo->pBitmapBits)[i], NULL, 0);
						ReleaseDC(NULL, hDC);
						frame.bChanged = true;
					}
				}
				else
					frame.bChanged = false;

				//(*sharedInfo->pTextures)[i]->SetImage(pBitmapBits, GS_IMAGEFORMAT_BGRX, pBitmapInfo->biWidth*4);
				(sharedInfo->qFrames).push(frame);
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
	if (!sharedInfo.qFrames.empty())
	{
		//sharedInfo.qTexture.front()->Unmap();
		
		for (unsigned long i = 0; i < Buffers; i++) // find which texture we have been passed
		{
			if (pTextures[i] == sharedInfo.qFrames.front().pTexture)
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

				if (sharedInfo.qFrames.front().bChanged)
				{
					delete pTextures[i];
					pTextures[i] = CreateTexture(lpBitmapInfo[i]->bmiHeader.biWidth, lpBitmapInfo[i]->bmiHeader.biHeight, GS_BGR, NULL, FALSE, FALSE);
				}

				ret = RGBChainOutputBuffer(hRGB, lpBitmapInfo[i], pBitmapBits[i]);
				lastTex = i;
				break;
			}
		}
		sharedInfo.qFrames.pop();
	}
	else
		DrawSprite(pTextures[lastTex], 0xFFFFFFFF, pos.x, pos.y, pos.x+size.x, pos.y+size.y);
	OSLeaveMutex(sharedInfo.hMutex);

    traceOut;
}

void VisionSource::UpdateSettings()
{
    traceIn(VisionSource::UpdateSettings);

	int cropping = data->GetInt(TEXT("cropping"));
	renderCX = data->GetInt(cropping ? TEXT("cropWidth") : TEXT("resolutionWidth"), 640);
	renderCY = data->GetInt(cropping ? TEXT("cropHeight") : TEXT("resolutionHeight"), 480);

	RGBEnableCropping(hRGB, cropping);

	if(cropping)
		RGBSetCropping(hRGB, data->GetInt(TEXT("cropTop")), data->GetInt(TEXT("cropLeft")), data->GetInt(TEXT("cropWidth"), 640), data->GetInt(TEXT("cropHeight"), 480));

    //bool bWasCapturing = bCapturing;

    //if(bWasCapturing) Stop();

    //if(bWasCapturing) Start();

    traceOut;
}


