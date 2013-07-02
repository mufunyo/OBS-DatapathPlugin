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

HWND VisionSource::hConfigWnd = NULL;

bool VisionSource::Init(XElement *data)
{
    traceIn(VisionSource::Init);

    this->data = data;
    UpdateSettings();

	for (int i = 0; i < NUM_BUFFERS; i++)
	{
		lpBitmapInfo[i] = (LPBITMAPINFO)Allocate(sizeof(BITMAPINFOHEADER)+(3*sizeof(DWORD)));
		if (lpBitmapInfo[i])
			CreateBitmapInformation(lpBitmapInfo[i], renderCX, renderCY, 32); // bpp hardcoded
	}
	XFile file;
	if (file.Open(OBSGetPluginDataPath() << TEXT("\\DatapathPlugin\\nosignal.png"), XFILE_READ, XFILE_OPENEXISTING)) // load user-defined image if available
	{
		file.Close();
		noSignalTex = CreateTextureFromFile(OBSGetPluginDataPath() << TEXT("\\DatapathPlugin\\nosignal.png"), TRUE);
	}
	
	if (!noSignalTex)
		noSignalTex = CreateTextureFromFile(TEXT("plugins\\DatapathPlugin\\nosignal.png"), TRUE); // fallback to default image

	if (file.Open(OBSGetPluginDataPath() << TEXT("\\DatapathPlugin\\invalidsignal.png"), XFILE_READ, XFILE_OPENEXISTING)) // load user-defined image if available
	{
		file.Close();
		invalidSignalTex = CreateTextureFromFile(OBSGetPluginDataPath() << TEXT("\\DatapathPlugin\\invalidsignal.png"), TRUE);
	}
	
	if (!invalidSignalTex)
		invalidSignalTex = CreateTextureFromFile(TEXT("plugins\\DatapathPlugin\\invalidsignal.png"), TRUE); // fallback to default image

	SamplerInfo samplerInfo;
	samplerInfo.filter = GS_FILTER_POINT;
	sampler = CreateSamplerState(samplerInfo);
	
	return true;

    traceOut;
}

VisionSource::VisionSource()
{
	bFlipVertical = true;
	sharedInfo.pCapturing = &bCapturing;
	sharedInfo.pUseDMA = &bUseDMA;
	sharedInfo.pTextures = &pTextures;
	sharedInfo.lpBitmapInfo = &lpBitmapInfo;
	sharedInfo.pBitmapBits = &pBitmapBits;
	sharedInfo.pBuffers = &Buffers;
	sharedInfo.hMutex = OSCreateMutex();
	sharedInfo.hDataMutex = OSCreateMutex();
	sharedInfo.data = &data;
	sharedInfo.hBitmaps = &hBitmaps;
	sharedInfo.pSignal = &signal;
	bUseDMA = true; // hardcode for now while toggle is horribly broken
}

VisionSource::~VisionSource()
{
    traceIn(VisionSource::~VisionSource);

    Stop();

	if (Buffers)
	{
		for (unsigned long i = 0; i < Buffers; i++)
		{
			delete pTextures[i];
			//delete noSignalTex;
			//delete invalidSignalTex; // crash if i try to do this
			if (!bUseDMA)
				DeleteObject(hBitmaps[i]);
			Free(lpBitmapInfo[i]);
		}
	}
	OSCloseMutex(sharedInfo.hMutex);
	OSCloseMutex(sharedInfo.hDataMutex);

    traceOut;
}

void VisionSource::Preprocess() { return; }

void VisionSource::Start()
{
    traceIn(VisionSource::Start);

    if(bCapturing)
        return;

	int input = data->GetInt(TEXT("input"));
	SIGNALTYPE signalType;
	unsigned long dummy;

	RGBGetInputSignalType(input, &signalType, &dummy, &dummy, &dummy); // no NULL checking? really Datapath, really?
	OSEnterMutex(sharedInfo.hMutex);
	switch(signalType)
	{
		case RGB_SIGNALTYPE_NOSIGNAL:
		{
			signal = inactive;
			break;
		}
		case RGB_SIGNALTYPE_OUTOFRANGE:
		{
			signal = invalid;
			break;
		}
		default:
			signal = active;
	}
	OSLeaveMutex(sharedInfo.hMutex);

	if (RGBERROR_NO_ERROR == RGBOpenInput(input, &hRGB))
	{
		if (RGBERROR_NO_ERROR == RGBSetFrameDropping(hRGB, 0))
		{
			if(data->GetInt(TEXT("cropping")))
			{
				RGBSetCropping(hRGB, data->GetInt(TEXT("cropTop")), data->GetInt(TEXT("cropLeft")), data->GetInt(TEXT("cropWidth"), 640), data->GetInt(TEXT("cropHeight"), 480));
				RGBEnableCropping(hRGB, 1);
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

				   RGBSetModeChangedFn(hRGB, &ResolutionSwitch, (ULONG_PTR)&sharedInfo);
				   RGBSetNoSignalFn(hRGB, &NoSignal, (ULONG_PTR)&sharedInfo);
				   RGBSetInvalidSignalFn(hRGB, &InvalidSignal, (ULONG_PTR)&sharedInfo);

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
	UINT pitch;
	
	HDC hDC = GetDC(NULL);
		
	if (hDC)
	{
	// allocate textures
	for(int i = 0; i < NUM_BUFFERS; i++)
	{
		pTextures[i] = CreateTexture(lpBitmapInfo[i]->bmiHeader.biWidth, lpBitmapInfo[i]->bmiHeader.biHeight, GS_BGR, NULL, FALSE, FALSE);
		if (bUseDMA)
		{
			pTextures[i]->Map((BYTE*&)pBitmapBits[i], pitch);
			lpBitmapInfo[i]->bmiHeader.biSizeImage = pitch*abs(lpBitmapInfo[i]->bmiHeader.biHeight);
		}
		else
			hBitmaps[i] = CreateDIBSection(hDC, lpBitmapInfo[i], DIB_RGB_COLORS, &pBitmapBits[i], NULL, 0);
		

		if (pTextures[i])
		{
			Buffers++;
		}
		else
			AppWarning(TEXT("VisionSource: could not create texture"));
	}
	ReleaseDC (NULL, hDC);
	}

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
		for (unsigned long i = 0; i < *sharedInfo->pBuffers; i++) // find which buffer we have been passed
		{
			if ((*sharedInfo->pBitmapBits)[i] == pFrameData->PBitmapBits)
			{
				CapturedFrame frame;

				if (!(*sharedInfo->pUseDMA))
				{
					BYTE* mappedBuf;
					UINT pitch;
					(*sharedInfo->pTextures)[i]->Map(mappedBuf, pitch);
					memcpy(mappedBuf, pFrameData->PBitmapBits, (*sharedInfo->pTextures)[i]->Height()*pitch);
				}
				(*sharedInfo->pTextures)[i]->Unmap();

				frame.pTexture = (*sharedInfo->pTextures)[i];
				if ((pFrameData->PBitmapInfo->biWidth != (*sharedInfo->pTextures)[i]->Width()) || (pFrameData->PBitmapInfo->biHeight != (*sharedInfo->pTextures)[i]->Height()))
				{		
					CreateBitmapInformation((*sharedInfo->lpBitmapInfo)[i], pFrameData->PBitmapInfo->biWidth, pFrameData->PBitmapInfo->biHeight, 32); // bpp hardcoded
					if (!(*sharedInfo->pUseDMA))
					{
						HDC hDC = GetDC(NULL);
						if (hDC)
						{
							DeleteObject((*sharedInfo->hBitmaps)[i]);
							(*sharedInfo->hBitmaps)[i] = CreateDIBSection(hDC, (*sharedInfo->lpBitmapInfo)[i], DIB_RGB_COLORS, &(*sharedInfo->pBitmapBits)[i], NULL, 0);
							ReleaseDC(NULL, hDC);
						}
					}
					frame.bChanged = true;
				}
				else
					frame.bChanged = false;

				(sharedInfo->qFrames).push(frame);
				break;
			}
		}
    }
	OSLeaveMutex(sharedInfo->hMutex);
}

void VisionSource::Render(const Vect2 &pos, const Vect2 &size)
{
    traceIn(VisionSource::Render);

	OSEnterMutex(sharedInfo.hMutex);

	if (sharedInfo.qFrames.size() > NUM_BUFFERS-1)
	{
		for (unsigned long i = 0; i < Buffers; i++) // find which texture we have been passed
		{
			if (pTextures[i] == sharedInfo.qFrames.front().pTexture)
			{
				UINT pitch;

				if(bPointFilter)
					LoadSamplerState(sampler, 0);

				DrawSprite(pTextures[lastTex], 0xFFFFFFFF, pos.x, pos.y, pos.x+size.x, pos.y+size.y);
				
				LoadSamplerState(NULL, 0);

				if (sharedInfo.qFrames.front().bChanged)
				{
					delete pTextures[i]; // but won't this cause the framedrop code to try to draw an uninitialised texture?
					pTextures[i] = CreateTexture(lpBitmapInfo[i]->bmiHeader.biWidth, lpBitmapInfo[i]->bmiHeader.biHeight, GS_BGR, NULL, FALSE, FALSE);
				}

				if (bUseDMA)
					pTextures[i]->Map((BYTE*&)pBitmapBits[i], pitch);

				RGBChainOutputBuffer(hRGB, lpBitmapInfo[i], pBitmapBits[i]);
				lastTex = i;
				break;
			}
		}
		sharedInfo.qFrames.pop();
	}
	else switch(signal)
	{
	case active:
		{ // I DUNNO MAN SEEMS TO WORK FINE
			DrawSprite(pTextures[lastTex], 0xFFFFFFFF, pos.x, pos.y, pos.x+size.x, pos.y+size.y);
			break;
		}
	case invalid:
		{
			DrawSprite(invalidSignalTex, 0xFFFFFFFF, pos.x, pos.y, pos.x+size.x, pos.y+size.y);
			break;
		}
	case inactive:
		{
			DrawSprite(noSignalTex, 0xFFFFFFFF, pos.x, pos.y, pos.x+size.x, pos.y+size.y);
		}
	}

	OSLeaveMutex(sharedInfo.hMutex);

    traceOut;
}

void RGBCBKAPI VisionSource::ResolutionSwitch(HWND hWnd, HRGB hRGB, PRGBMODECHANGEDINFO pModeChangedInfo, ULONG_PTR userData)
{
	SharedVisionInfo *sharedInfo = (SharedVisionInfo*)userData;
	unsigned long width;
	unsigned long height;

	OSEnterMutex(sharedInfo->hMutex);
	(*sharedInfo->pSignal) = active;
	OSLeaveMutex(sharedInfo->hMutex);

	if (hConfigWnd)
	{
		TCHAR modeText[255];
		GetModeText((*sharedInfo->data)->GetInt(TEXT("input")), modeText, 255);
		SetWindowText(GetDlgItem(hConfigWnd, IDC_DETECTEDMODE), modeText);
	}
	
	if ((RGBERROR_NO_ERROR == RGBGetCaptureWidthDefault(hRGB, &width)) && (RGBERROR_NO_ERROR == RGBGetCaptureHeightDefault(hRGB, &height)))
	{
		RGBSetCaptureWidth(hRGB, width);
		RGBSetCaptureHeight(hRGB, height);
		// TODO: set renderCX/CY ?
	}
	
}

void RGBCBKAPI VisionSource::NoSignal(HWND hWnd, HRGB hRGB, ULONG_PTR userData)
{
	SharedVisionInfo *sharedInfo = (SharedVisionInfo*)userData;
	OSEnterMutex(sharedInfo->hMutex);
	(*sharedInfo->pSignal) = inactive;
	OSLeaveMutex(sharedInfo->hMutex);
}

void RGBCBKAPI VisionSource::InvalidSignal(HWND hWnd, HRGB hRGB, unsigned long horClock, unsigned long verClock, ULONG_PTR userData)
{
	SharedVisionInfo *sharedInfo = (SharedVisionInfo*)userData;
	OSEnterMutex(sharedInfo->hMutex);
	(*sharedInfo->pSignal) = invalid;
	OSLeaveMutex(sharedInfo->hMutex);
	
}

void VisionSource::UpdateSettings()
{
    traceIn(VisionSource::UpdateSettings);

	/*bool bNewUseDMA = (data->GetInt(TEXT("useDMA"), 1)!=0);
	if(bCapturing && (bUseDMA != bNewUseDMA))
	{
		EndScene();
		for (int i = 0; i < NUM_BUFFERS; i++)
		{
			if (bUseDMA)
			{
				UINT pitch;
				DeleteObject(hBitmaps[i]);
				pTextures[i]->Map((BYTE*&)pBitmapBits[i], pitch);
				lpBitmapInfo[i]->bmiHeader.biSizeImage = pitch*abs(lpBitmapInfo[i]->bmiHeader.biHeight);
				Log(TEXT("VisionSource: Using DMA capture"));
			}
			else
			{
				HDC hDC = GetDC(NULL);
				if (hDC)
				{
					hBitmaps[i] = CreateDIBSection(hDC, lpBitmapInfo[i], DIB_RGB_COLORS, &pBitmapBits[i], NULL, 0);
					ReleaseDC(NULL, hDC);
					Log(TEXT("VisionSource: Using SSECopy capture"));
				}
			}				
			bUseDMA = bNewUseDMA;
			BeginScene();
		}
	}*/

	int input = data->GetInt(TEXT("input"));
	int cropping = data->GetInt(TEXT("cropping"));
	int width = 640;
	int height = 480;
	bool openInput = (hRGB!=0); // use existing handle if available
	
	if (openInput || RGBERROR_NO_ERROR == RGBOpenInput(input, &hRGB))
	{
		RGBGetCaptureWidthDefault(hRGB, (unsigned long*)&width);
		RGBGetCaptureHeightDefault(hRGB, (unsigned long*)&height);
		if (!openInput) // don't close a handle we didn't open ourselves
			RGBCloseInput(hRGB);
	}
	if (data->GetInt(TEXT("customRes")))
	{
		renderCX = data->GetInt(TEXT("resolutionWidth"), width);
		renderCY = data->GetInt(TEXT("resolutionHeight"), height);
	}
	else
	{
		renderCX = width;
		renderCY = height;
	}

	if (hRGB) // are we (still) capturing?
	{
		RGBEnableCropping(hRGB, cropping);

		if(cropping)
			RGBSetCropping(hRGB, data->GetInt(TEXT("cropTop")), data->GetInt(TEXT("cropLeft")), data->GetInt(TEXT("cropWidth"), 640), data->GetInt(TEXT("cropHeight"), 480));
	}
	
	bPointFilter = (data->GetInt(TEXT("pointFilter"), 1)!=0);

    traceOut;
}


