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

#include "DatapathPlugin.h"

HWND VisionSource::hConfigWnd = NULL;

bool VisionSource::Init(XElement *data)
{
    traceIn(VisionSource::Init);

    this->data = data;
    UpdateSettings();

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
	sharedInfo.pTextures = &pTextures;
	sharedInfo.pSharedTextures = &pSharedTextures;
	sharedInfo.lpBitmapInfo = &lpBitmapInfo;
	sharedInfo.pBitmapBits = &pBitmapBits;
	sharedInfo.pBuffers = &Buffers;
	sharedInfo.hMutex = OSCreateMutex();
	sharedInfo.hDataMutex = OSCreateMutex();
	sharedInfo.data = &data;
	sharedInfo.hBitmaps = &hBitmaps;
	sharedInfo.pSignal = &signal;
	lastTex = 0;
	dropTex = 0;
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
	StringCchPrintf(warning, 255, TEXT("Could not open Vision input %i"), input+1);
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

	HDC hDC = GetDC(NULL);

	for (int i = 0; i < NUM_BUFFERS; i++)
	{
		lpBitmapInfo[i] = (LPBITMAPINFO)Allocate(sizeof(BITMAPINFOHEADER)+(3*sizeof(DWORD)));
		if (lpBitmapInfo[i])
			CreateBitmapInformation(lpBitmapInfo[i], renderCX, renderCY, pixFmtBpp[pixFormat], pixFmtFCC[pixFormat]);
	}

	if (hDC)
	{
	// allocate textures
	UINT pitch = 0;
	for(int i = 0; i < NUM_BUFFERS; i++)
	{
		pSharedTextures[i] = gpD3D9->CreateSharedTexture(lpBitmapInfo[i]->bmiHeader.biWidth, lpBitmapInfo[i]->bmiHeader.biHeight);
		pTextures[i] = gpD3D9->CreateTexture(lpBitmapInfo[i]->bmiHeader.biWidth, lpBitmapInfo[i]->bmiHeader.biHeight, pixFmtD3D[pixFormat]);
		pTextures[i]->Map(pBitmapBits[i], pitch);
		lpBitmapInfo[i]->bmiHeader.biSizeImage = pitch*abs(lpBitmapInfo[i]->bmiHeader.biHeight);

		if (pTextures[i] && pSharedTextures[i])
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

				(*sharedInfo->pTextures)[i]->Unmap();
				gpD3D9->BlitTexture((*sharedInfo->pTextures)[i], (*sharedInfo->pSharedTextures)[i]);

				frame.pTexture = (*sharedInfo->pSharedTextures)[i];
				if ((pFrameData->PBitmapInfo->biWidth != (*sharedInfo->pTextures)[i]->Width()) || (pFrameData->PBitmapInfo->biHeight != (*sharedInfo->pTextures)[i]->Height()))
				{		
					(*sharedInfo->lpBitmapInfo)[i]->bmiHeader = *pFrameData->PBitmapInfo;
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
			if (pSharedTextures[i] == sharedInfo.qFrames.front().pTexture)
			{
				UINT pitch = 0;

				if(bPointFilter)
					LoadSamplerState(sampler, 0);

				gpD3D9->Flush();
				BlendFunction(GS_BLEND_ONE, GS_BLEND_ZERO);
				DrawSprite(pSharedTextures[lastTex]->GSTexture, 0xFFFFFFFF, pos.x, pos.y, pos.x+size.x, pos.y+size.y);
				BlendFunction(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA);

				LoadSamplerState(NULL, 0);

				if (sharedInfo.qFrames.front().bChanged)
				{
					delete pTextures[i];
					delete pSharedTextures[i];
					pSharedTextures[i] = gpD3D9->CreateSharedTexture(lpBitmapInfo[i]->bmiHeader.biWidth, lpBitmapInfo[i]->bmiHeader.biHeight);
					pTextures[i] = gpD3D9->CreateTexture(lpBitmapInfo[i]->bmiHeader.biWidth, lpBitmapInfo[i]->bmiHeader.biHeight, pixFmtD3D[pixFormat]);
				}

				pTextures[i]->Map(pBitmapBits[i], pitch);

				RGBChainOutputBuffer(hRGB, lpBitmapInfo[i], pBitmapBits[i]);
				dropTex = lastTex;
				lastTex = i;
				break;
			}
		}
		sharedInfo.qFrames.pop();
	}
	else switch(signal)
	{
	case active:
		{
			DrawSprite(pSharedTextures[dropTex]->GSTexture, 0xFFFFFFFF, pos.x, pos.y, pos.x+size.x, pos.y+size.y);
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
	pixFormat = (PixelFmt)data->GetInt(TEXT("pixFormat"), 0);

    traceOut;
}


