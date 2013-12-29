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

extern "C" __declspec(dllexport) bool LoadPlugin();
extern "C" __declspec(dllexport) void UnloadPlugin();
extern "C" __declspec(dllexport) CTSTR GetPluginName();
extern "C" __declspec(dllexport) CTSTR GetPluginDescription();

LocaleStringLookup *pluginLocale = NULL;
HINSTANCE hinstMain = NULL;
D3D9Context *gpD3D9 = NULL;

#define DATAPATH_CLASSNAME TEXT("DatapathCapture")

void CreateBitmapInformation(BITMAPINFO *pBitmapInfo, int width, int height, PixelFmt pixFormat)
{
	pBitmapInfo->bmiHeader.biWidth				= width;
	pBitmapInfo->bmiHeader.biHeight				= height;
	pBitmapInfo->bmiHeader.biBitCount			= pixFmtBpp[pixFormat];
	pBitmapInfo->bmiHeader.biSize				= sizeof(BITMAPINFOHEADER);
	pBitmapInfo->bmiHeader.biPlanes				= 1;
	pBitmapInfo->bmiHeader.biCompression		= pixFmtFCC[pixFormat];
	pBitmapInfo->bmiHeader.biSizeImage			= 0;
	pBitmapInfo->bmiHeader.biXPelsPerMeter		= 3000;
	pBitmapInfo->bmiHeader.biYPelsPerMeter		= 3000;
	pBitmapInfo->bmiHeader.biClrUsed			= 0;
	pBitmapInfo->bmiHeader.biClrImportant		= 0;
	pBitmapInfo->bmiHeader.biSizeImage			= width * height * pixFmtBpp[pixFormat] / 8 ;

	memcpy(&pBitmapInfo->bmiColors, &pixFmtMask[pixFormat], sizeof(pixFmtMask[pixFormat]));
}

void SetCropping(HRGB hRGB, int* pLeft, int* pTop, int* pWidth, int* pHeight)
{
	int left, top, width, height;

	if (!pLeft || !pTop || !pWidth || !pHeight) // fill in missing values
	{
		RGBGetCropping(hRGB, (signed long*)&top, (signed long*)&left, (unsigned long*)&width, (unsigned long*)&height);
		if (!pLeft)
			pLeft = &left;
		if (!pTop)
			pTop = &top;
		if (!pWidth)
			pWidth = &width;
		if (!pHeight)
			pHeight = &height;
	}

	RGBSetCropping(hRGB, (signed long)*pTop, (signed long)*pLeft, (unsigned long)*pWidth, (unsigned long)*pHeight);
}

void GetModeText(unsigned long input, TSTR string, size_t size)
{
	unsigned long signalWidth = 0;
	unsigned long signalHeight = 0;
	unsigned long signalHz = 0;
	SIGNALTYPE signalType;
	CTSTR signalText;

	RGBGetInputSignalType(input, &signalType, &signalWidth, &signalHeight, &signalHz);

	if (RGB_SIGNALTYPE_NOSIGNAL != signalType)
	{
		switch(signalType)
		{
		case RGB_SIGNALTYPE_COMPOSITE:
			{
				signalText = TEXT("Composite");
				break;
			}
		case RGB_SIGNALTYPE_DLDVI:
			{
				signalText = TEXT("DVI-DL");
				break;
			}
		case RGB_SIGNALTYPE_DVI:
			{
				signalText = TEXT("DVI");
				break;
			}
		case RGB_SIGNALTYPE_SDI:
			{
				signalText = TEXT("SDI");
				break;
			}
		case RGB_SIGNALTYPE_SVIDEO:
			{
				signalText = TEXT("S-Video");
				break;
			}
		case RGB_SIGNALTYPE_VGA:
			{
				signalText = TEXT("VGA");
				break;
			}
		case RGB_SIGNALTYPE_YPRPB:
			{
				signalText = TEXT("YPbPr");
			}
		default:
			break;
		}

		StringCchPrintf(string, size, TEXT("Detected mode:\n%s %ix%i %.5gHz"), signalText, signalWidth, signalHeight, ((float)signalHz/1000.0f));
	}
	else
	{
		switch(signalType)
		{
		case RGB_SIGNALTYPE_NOSIGNAL:
			{
				StringCchCopy(string, size, TEXT("\nNo signal detected"));
				break;
			}
		case RGB_SIGNALTYPE_OUTOFRANGE:
			StringCchCopy(string, size, TEXT("\nSignal out of range"));
		default:
			break;
		}
	}
}

void RGBCBKAPI ModeChanged(HWND hWnd, HRGB hRGB, PRGBMODECHANGEDINFO pModeChangedInfo, ULONG_PTR userData)
{
	ConfigVisionInfo *configInfo = (ConfigVisionInfo*)userData;
	if (configInfo->hConfigWnd)
	{
		TCHAR modeText[255];
		GetModeText(configInfo->data->GetInt(TEXT("input")), modeText, 255);
		SetWindowText(GetDlgItem(configInfo->hConfigWnd, IDC_DETECTEDMODE), modeText);
	}
}

// This is where I roll around in my own poo like a content little piggy
INT_PTR CALLBACK ConfigureDialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message)
    {
        case WM_INITDIALOG:
            {
				TCHAR modeText[255];

                ConfigVisionInfo *configInfo = (ConfigVisionInfo*)lParam;
                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)lParam);
				configInfo->source->hConfigWnd = hwnd;
                LocalizeWindow(hwnd);

				configInfo->widthCur = 640;
				configInfo->widthMin = 160;
				configInfo->widthMax = 4096;
				configInfo->heightCur = 480;
				configInfo->heightMin = 120;
				configInfo->heightMax = 4096;
				configInfo->cropTopCur = 0;
				configInfo->cropTopMin = 0;
				configInfo->cropLeftCur = 0;
				configInfo->cropLeftMin = 0;
				configInfo->cropWidthCur = 0;
				configInfo->cropWidthMin = 0;
				configInfo->cropHeightCur = 0;
				configInfo->cropHeightMin = 0;
				configInfo->hRGB = 0;

				signed long horOffsetCur = 0, horOffsetMin = 0, horOffsetMax = 0, phaseCur = 0, phaseMin = 0, phaseMax = 0, verOffsetCur = 0, verOffsetMin = 0, verOffsetMax = 0, blackLevelCur = 0, blackLevelMin = 0, blackLevelMax = 0, gainRedCur = 0, gainRedMin = 0, gainRedMax = 0, gainGreenCur = 0, gainGreenMin = 0, gainGreenMax = 0, gainBlueCur = 0, gainBlueMin = 0, gainBlueMax = 0, brightCur = 0, brightMin = 0, brightMax = 0, contCur = 0, contMin = 0, contMax = 0;
				unsigned long horScaleCur = 0, horScaleMin = 0, horScaleMax = 0;

				RGBGetNumberOfInputs(&configInfo->inputs);
				configInfo->input = configInfo->data->GetInt(TEXT("input"));
				configInfo->input = min((int)configInfo->inputs, configInfo->input);

				GetModeText(configInfo->input, modeText, 255);
				SetWindowText(GetDlgItem(hwnd, IDC_DETECTEDMODE), modeText);

				if (configInfo->source)
					configInfo->hRGB = configInfo->source->hRGB;
				if (!configInfo->hRGB)
					RGBOpenInput((unsigned long)configInfo->input, &configInfo->hRGB);

				if (configInfo->hRGB)
				{
					signed long dummy;
					RGBGetCaptureWidthDefault(configInfo->hRGB, &configInfo->widthCur);
					RGBGetCaptureWidthMinimum(configInfo->hRGB, &configInfo->widthMin);
					RGBGetCaptureWidthMaximum(configInfo->hRGB, &configInfo->widthMax);
					RGBGetCaptureHeightDefault(configInfo->hRGB, &configInfo->heightCur);
					RGBGetCaptureHeightMinimum(configInfo->hRGB, &configInfo->heightMin);
					RGBGetCaptureHeightMaximum(configInfo->hRGB, &configInfo->heightMax);
					RGBGetCropping(configInfo->hRGB, &configInfo->cropTopCur, &configInfo->cropLeftCur, &configInfo->cropWidthCur, &configInfo->cropHeightCur);
					RGBGetCroppingMinimum(configInfo->hRGB, &configInfo->cropTopMin, &configInfo->cropLeftMin, &configInfo->cropWidthMin, &configInfo->cropHeightMin);

					RGBGetHorPositionDefault(configInfo->hRGB, &horOffsetCur);
					RGBGetHorPositionMinimum(configInfo->hRGB, &horOffsetMin);
					RGBGetHorPositionMaximum(configInfo->hRGB, &horOffsetMax);
					RGBGetHorScaleDefault(configInfo->hRGB, &horScaleCur);
					RGBGetHorScaleMinimum(configInfo->hRGB, &horScaleMin);
					RGBGetHorScaleMaximum(configInfo->hRGB, &horScaleMax);
					RGBGetPhaseDefault(configInfo->hRGB, &phaseCur);
					RGBGetPhaseMinimum(configInfo->hRGB, &phaseMin);
					RGBGetPhaseMaximum(configInfo->hRGB, &phaseMax);
					RGBGetVerPositionDefault(configInfo->hRGB, &verOffsetCur);
					RGBGetVerPositionMinimum(configInfo->hRGB, &verOffsetMin);
					RGBGetVerPositionMaximum(configInfo->hRGB, &verOffsetMax);
					RGBGetBlackLevelDefault(configInfo->hRGB, &blackLevelCur);
					RGBGetBlackLevelMinimum(configInfo->hRGB, &blackLevelMin);
					RGBGetBlackLevelMaximum(configInfo->hRGB, &blackLevelMax);
					RGBGetBrightnessDefault(configInfo->hRGB, &brightCur);
					RGBGetBrightnessMinimum(configInfo->hRGB, &brightMin);
					RGBGetBrightnessMaximum(configInfo->hRGB, &brightMax);
					brightMin = brightMin - brightCur;
					brightMax = brightMax - brightCur;
					brightCur = brightCur + brightMin;
					RGBGetContrastDefault(configInfo->hRGB, &contCur);
					RGBGetContrastMinimum(configInfo->hRGB, &contMin);
					RGBGetContrastMaximum(configInfo->hRGB, &contMax);
					contMin = contMin - contCur;
					contMax = contMax - contCur;
					contCur = contCur + contMin;
					RGBGetColourBalanceDefault(configInfo->hRGB, &dummy, &dummy, &dummy, &gainRedCur, &gainGreenCur, &gainBlueCur);
					RGBGetColourBalanceMinimum(configInfo->hRGB, &dummy, &dummy, &dummy, &gainRedMin, &gainGreenMin, &gainBlueMin);
					RGBGetColourBalanceMaximum(configInfo->hRGB, &dummy, &dummy, &dummy, &gainRedMax, &gainGreenMax, &gainBlueMax);
					gainRedMin = gainRedMin - gainRedCur;
					gainRedMax = gainRedMax - gainRedCur;
					gainRedCur = gainRedCur + gainRedMin;
					gainGreenMin = gainGreenMin - gainGreenCur;
					gainGreenMax = gainGreenMax - gainGreenCur;
					gainGreenCur = gainGreenCur + gainGreenMin;
					gainBlueMin = gainBlueMin - gainBlueCur;
					gainBlueMax = gainBlueMax - gainBlueCur;
					gainBlueCur = gainBlueCur + gainBlueMin;
				}
				
				if (configInfo->source && configInfo->hRGB != configInfo->source->hRGB)
					RGBSetModeChangedFn(configInfo->hRGB, &ModeChanged, (ULONG_PTR)&configInfo);

				for (unsigned long i = 0; i < configInfo->inputs; i++)
				{
					TCHAR  text[32];
					
					StringCchPrintf (text, 32, TEXT("%d"), i+1);

					SendMessage(GetDlgItem(hwnd, IDC_INPUTSOURCE), CB_ADDSTRING, (WPARAM)0, (LPARAM)&text);
					SendMessage(GetDlgItem(hwnd, IDC_INPUTSOURCE), CB_SETITEMDATA , i, (LPARAM)i);
				}
				SendMessage(GetDlgItem(hwnd, IDC_INPUTSOURCE), CB_SETCURSEL, configInfo->input, 0);

				configInfo->pixFormat = (PixelFmt)configInfo->data->GetInt(TEXT("pixFormat"), 0);
				int index = 0;
				if (gpD3D9->CheckFormat(pixFmtD3D[RGB32]))
				{
					SendMessage(GetDlgItem(hwnd, IDC_PIXFORMAT), CB_INSERTSTRING, index, (LPARAM)TEXT("Aligned RGB (32 bits)"));
					SendMessage(GetDlgItem(hwnd, IDC_PIXFORMAT), CB_SETITEMDATA, index, 0);
					index++;
				}
				if (gpD3D9->CheckFormat(pixFmtD3D[RGB24]))
				{
					SendMessage(GetDlgItem(hwnd, IDC_PIXFORMAT), CB_INSERTSTRING, index, (LPARAM)TEXT("Unaligned RGB (24 bits)"));
					SendMessage(GetDlgItem(hwnd, IDC_PIXFORMAT), CB_SETITEMDATA, index, 1);
					index++;
				}
				if (gpD3D9->CheckFormat(pixFmtD3D[RGB16]))
				{
					SendMessage(GetDlgItem(hwnd, IDC_PIXFORMAT), CB_INSERTSTRING, index, (LPARAM)TEXT("Reduced-depth RGB (16 bits)"));
					SendMessage(GetDlgItem(hwnd, IDC_PIXFORMAT), CB_SETITEMDATA, index, 2);
					index++;
				}
				if (gpD3D9->CheckFormat(pixFmtD3D[YUY2]))
				{
					SendMessage(GetDlgItem(hwnd, IDC_PIXFORMAT), CB_INSERTSTRING, index, (LPARAM)TEXT("4:2:2 sampled YUV (16 bits)"));
					SendMessage(GetDlgItem(hwnd, IDC_PIXFORMAT), CB_SETITEMDATA, index, 3);
					index++;
				}
				if (gpD3D9->CheckFormat(pixFmtD3D[Y8]))
				{
					SendMessage(GetDlgItem(hwnd, IDC_PIXFORMAT), CB_INSERTSTRING, index, (LPARAM)TEXT("Greyscale (8 bits)"));
					SendMessage(GetDlgItem(hwnd, IDC_PIXFORMAT), CB_SETITEMDATA, index, 5);
					index++;
				}
				SendMessage(GetDlgItem(hwnd, IDC_PIXFORMAT), CB_SETCURSEL, configInfo->pixFormat, 0);

				configInfo->width = configInfo->data->GetInt(TEXT("resolutionWidth"), (int)configInfo->widthCur);
				configInfo->height = configInfo->data->GetInt(TEXT("resolutionHeight"), (int)configInfo->heightCur);
				configInfo->cropTop = configInfo->data->GetInt(TEXT("cropTop"), configInfo->cropTopCur);
				configInfo->cropLeft = configInfo->data->GetInt(TEXT("cropLeft"), configInfo->cropLeftCur);
				configInfo->cropWidth = configInfo->data->GetInt(TEXT("cropWidth"), (int)configInfo->cropWidthCur);
				configInfo->cropHeight = configInfo->data->GetInt(TEXT("cropHeight"), (int)configInfo->cropHeightCur);
				configInfo->cropping = configInfo->data->GetInt(TEXT("cropping"), FALSE);
				configInfo->customRes = configInfo->data->GetInt(TEXT("customRes"), FALSE);
				configInfo->pointFilter = configInfo->data->GetInt(TEXT("pointFilter"), FALSE);

				configInfo->cropTop = min(configInfo->cropTop, (int)configInfo->heightCur);
				configInfo->cropLeft = min(configInfo->cropLeft, (int)configInfo->widthCur);
				unsigned long cropHeightMax = max(configInfo->cropHeightMin, configInfo->heightCur - configInfo->cropTop);
				unsigned long cropWidthMax = max(configInfo->cropWidthMin, configInfo->widthCur - configInfo->cropLeft);
				configInfo->cropHeight = min(configInfo->cropHeight, (int)cropHeightMax);
				configInfo->cropWidth = min(configInfo->cropWidth, (int)cropWidthMax);

				int brightness = configInfo->data->GetInt(TEXT("brightness"), brightCur);
				int contrast = configInfo->data->GetInt(TEXT("contrast"), contCur);
				int horOffset = configInfo->data->GetInt(TEXT("horOffset"), horOffsetCur);
				int horScale = configInfo->data->GetInt(TEXT("horScale"), horScaleCur);
				int phase = configInfo->data->GetInt(TEXT("phase"), phaseCur);
				int verOffset = configInfo->data->GetInt(TEXT("verOffset"), verOffsetCur);
				int blackLevel = configInfo->data->GetInt(TEXT("blackLevel"), blackLevelCur);
				int gainRed = configInfo->data->GetInt(TEXT("gainRed"), gainRedCur);
				int gainGreen = configInfo->data->GetInt(TEXT("gainGreen"), gainGreenCur);
				int gainBlue = configInfo->data->GetInt(TEXT("gainBlue"), gainBlueCur);

				SCROLLINFO scrollInfo;
				scrollInfo.cbSize = sizeof(SCROLLINFO);
				scrollInfo.fMask = SIF_ALL;
				scrollInfo.nPage = 1;

				scrollInfo.nMin = brightMin;
				scrollInfo.nMax = brightMax;
				scrollInfo.nPos = brightness;
				SetScrollInfo(GetDlgItem(hwnd, IDC_BRIGHTNESSSCROLL), SB_CTL, &scrollInfo, TRUE);
				scrollInfo.nMin = contMin;
				scrollInfo.nMax = contMax;
				scrollInfo.nPos = contrast;
				SetScrollInfo(GetDlgItem(hwnd, IDC_CONTRASTSCROLL), SB_CTL, &scrollInfo, TRUE);
				scrollInfo.nMin = horOffsetMin;
				scrollInfo.nMax = horOffsetMax;
				scrollInfo.nPos = horOffset;
				SetScrollInfo(GetDlgItem(hwnd, IDC_HOROFFSETSCROLL), SB_CTL, &scrollInfo, TRUE);
				scrollInfo.nMin = horScaleMin;
				scrollInfo.nMax = horScaleMax;
				scrollInfo.nPos = horScale;
				SetScrollInfo(GetDlgItem(hwnd, IDC_HORSCALESCROLL), SB_CTL, &scrollInfo, TRUE);
				scrollInfo.nMin = phaseMin;
				scrollInfo.nMax = phaseMax;
				scrollInfo.nPos = phase;
				SetScrollInfo(GetDlgItem(hwnd, IDC_PHASESCROLL), SB_CTL, &scrollInfo, TRUE);
				scrollInfo.nMin = verOffsetMin;
				scrollInfo.nMax = verOffsetMax;
				scrollInfo.nPos = verOffset;
				SetScrollInfo(GetDlgItem(hwnd, IDC_VEROFFSETSCROLL), SB_CTL, &scrollInfo, TRUE);
				scrollInfo.nMin = blackLevelMin;
				scrollInfo.nMax = blackLevelMax;
				scrollInfo.nPos = blackLevel;
				SetScrollInfo(GetDlgItem(hwnd, IDC_BLACKLEVELSCROLL), SB_CTL, &scrollInfo, TRUE);
				scrollInfo.nMin = gainRedMin;
				scrollInfo.nMax = gainRedMax;
				scrollInfo.nPos = gainRed;
				SetScrollInfo(GetDlgItem(hwnd, IDC_GAINREDSCROLL), SB_CTL, &scrollInfo, TRUE);
				scrollInfo.nMin = gainGreenMin;
				scrollInfo.nMax = gainGreenMax;
				scrollInfo.nPos = gainGreen;
				SetScrollInfo(GetDlgItem(hwnd, IDC_GAINGREENSCROLL), SB_CTL, &scrollInfo, TRUE);
				scrollInfo.nMin = gainBlueMin;
				scrollInfo.nMax = gainBlueMax;
				scrollInfo.nPos = gainBlue;
				SetScrollInfo(GetDlgItem(hwnd, IDC_GAINBLUESCROLL), SB_CTL, &scrollInfo, TRUE);


				SendMessage(GetDlgItem(hwnd, IDC_ADDRWIDTHSPIN), UDM_SETRANGE32, (int)configInfo->widthMin, (int)configInfo->widthMax);
				SendMessage(GetDlgItem(hwnd, IDC_ADDRHEIGHTSPIN), UDM_SETRANGE32, (int)configInfo->heightMin, (int)configInfo->heightMax);
				SendMessage(GetDlgItem(hwnd, IDC_TOPSPIN), UDM_SETRANGE32, configInfo->cropTopMin, configInfo->heightCur);
				SendMessage(GetDlgItem(hwnd, IDC_LEFTSPIN), UDM_SETRANGE32, configInfo->cropLeftMin, configInfo->widthCur);
				SendMessage(GetDlgItem(hwnd, IDC_WIDTHSPIN), UDM_SETRANGE32, (int)configInfo->cropWidthMin, (int)cropWidthMax);
				SendMessage(GetDlgItem(hwnd, IDC_HEIGHTSPIN), UDM_SETRANGE32, (int)configInfo->cropHeightMin, (int)cropHeightMax);
				SendMessage(GetDlgItem(hwnd, IDC_BRIGHTNESSSPIN), UDM_SETRANGE32, brightMin, brightMax);
				SendMessage(GetDlgItem(hwnd, IDC_CONTRASTSPIN), UDM_SETRANGE32, contMin, contMax);
				SendMessage(GetDlgItem(hwnd, IDC_HOROFFSETSPIN), UDM_SETRANGE32, horOffsetMin, horOffsetMax);
				SendMessage(GetDlgItem(hwnd, IDC_HORSCALESPIN), UDM_SETRANGE32, horScaleMin, horScaleMax);
				SendMessage(GetDlgItem(hwnd, IDC_PHASESPIN), UDM_SETRANGE32, phaseMin, phaseMax);
				SendMessage(GetDlgItem(hwnd, IDC_VEROFFSETSPIN), UDM_SETRANGE32, verOffsetMin, verOffsetMax);
				SendMessage(GetDlgItem(hwnd, IDC_BLACKLEVELSPIN), UDM_SETRANGE32, blackLevelMin, blackLevelMax);
				SendMessage(GetDlgItem(hwnd, IDC_GAINREDSPIN), UDM_SETRANGE32, gainRedMin, gainRedMax);
				SendMessage(GetDlgItem(hwnd, IDC_GAINGREENSPIN), UDM_SETRANGE32, gainGreenMin, gainGreenMax);
				SendMessage(GetDlgItem(hwnd, IDC_GAINBLUESPIN), UDM_SETRANGE32, gainBlueMin, gainBlueMax);
				SendMessage(GetDlgItem(hwnd, IDC_ADDRWIDTHSPIN), UDM_SETPOS32, 0, configInfo->width);
				SendMessage(GetDlgItem(hwnd, IDC_ADDRHEIGHTSPIN), UDM_SETPOS32, 0, configInfo->height);
				SendMessage(GetDlgItem(hwnd, IDC_TOPSPIN), UDM_SETPOS32, 0, configInfo->cropTop);
				SendMessage(GetDlgItem(hwnd, IDC_LEFTSPIN), UDM_SETPOS32, 0, configInfo->cropLeft);
				SendMessage(GetDlgItem(hwnd, IDC_WIDTHSPIN), UDM_SETPOS32, 0, configInfo->cropWidth);
				SendMessage(GetDlgItem(hwnd, IDC_HEIGHTSPIN), UDM_SETPOS32, 0, configInfo->cropHeight);
				SendMessage(GetDlgItem(hwnd, IDC_BRIGHTNESSSPIN), UDM_SETPOS32, 0, brightness);
				SendMessage(GetDlgItem(hwnd, IDC_CONTRASTSPIN), UDM_SETPOS32, 0, contrast);
				SendMessage(GetDlgItem(hwnd, IDC_HOROFFSETSPIN), UDM_SETPOS32, 0, horOffset);
				SendMessage(GetDlgItem(hwnd, IDC_HORSCALESPIN), UDM_SETPOS32, 0, horScale);
				SendMessage(GetDlgItem(hwnd, IDC_PHASESPIN), UDM_SETPOS32, 0, phase);
				SendMessage(GetDlgItem(hwnd, IDC_VEROFFSETSPIN), UDM_SETPOS32, 0, blackLevel);
				SendMessage(GetDlgItem(hwnd, IDC_BLACKLEVELSPIN), UDM_SETPOS32, 0, verOffset);
				SendMessage(GetDlgItem(hwnd, IDC_GAINREDSPIN), UDM_SETPOS32, 0, gainRed);
				SendMessage(GetDlgItem(hwnd, IDC_GAINGREENSPIN), UDM_SETPOS32, 0, gainGreen);
				SendMessage(GetDlgItem(hwnd, IDC_GAINBLUESPIN), UDM_SETPOS32, 0, gainBlue);
				SendMessage(GetDlgItem(hwnd, IDC_BRIGHTNESSSCROLL), SBM_SETPOS, brightness, TRUE);
				SendMessage(GetDlgItem(hwnd, IDC_HOROFFSETSCROLL), SBM_SETPOS, horOffset, TRUE);
				SendMessage(GetDlgItem(hwnd, IDC_HORSCALESCROLL), SBM_SETPOS, horScale, TRUE);
				SendMessage(GetDlgItem(hwnd, IDC_PHASESCROLL), SBM_SETPOS, phase, TRUE);
				SendMessage(GetDlgItem(hwnd, IDC_VEROFFSETSCROLL), SBM_SETPOS, blackLevel, TRUE);
				SendMessage(GetDlgItem(hwnd, IDC_BLACKLEVELSCROLL), SBM_SETPOS, verOffset, TRUE);
				SendMessage(GetDlgItem(hwnd, IDC_GAINREDSCROLL), SBM_SETPOS, gainRed, TRUE);
				SendMessage(GetDlgItem(hwnd, IDC_GAINGREENSCROLL), SBM_SETPOS, gainGreen, TRUE);
				SendMessage(GetDlgItem(hwnd, IDC_GAINBLUESCROLL), SBM_SETPOS, gainBlue, TRUE);
				SendMessage(GetDlgItem(hwnd, IDC_CONTRASTSCROLL), SBM_SETPOS, contrast, TRUE);
				SendMessage(GetDlgItem(hwnd, IDC_CROPPING), BM_SETCHECK, configInfo->cropping, 0);
				SendMessage(GetDlgItem(hwnd, IDC_CUSTOMRES), BM_SETCHECK, configInfo->customRes, 0);
				SendMessage(GetDlgItem(hwnd, IDC_POINTFILTER), BM_SETCHECK, configInfo->pointFilter, 0);
				EnableWindow(GetDlgItem(hwnd, IDC_ADDRWIDTH), configInfo->customRes);
				EnableWindow(GetDlgItem(hwnd, IDC_ADDRWIDTHSTATIC), configInfo->customRes);
				EnableWindow(GetDlgItem(hwnd, IDC_ADDRWIDTHSPIN), configInfo->customRes);
				EnableWindow(GetDlgItem(hwnd, IDC_ADDRHEIGHT), configInfo->customRes);
				EnableWindow(GetDlgItem(hwnd, IDC_ADDRHEIGHTSTATIC), configInfo->customRes);
				EnableWindow(GetDlgItem(hwnd, IDC_ADDRHEIGHTSPIN), configInfo->customRes);
				EnableWindow(GetDlgItem(hwnd, IDC_TOP), configInfo->cropping);
				EnableWindow(GetDlgItem(hwnd, IDC_TOPSTATIC), configInfo->cropping);
				EnableWindow(GetDlgItem(hwnd, IDC_TOPSPIN), configInfo->cropping);
				EnableWindow(GetDlgItem(hwnd, IDC_LEFT), configInfo->cropping);
				EnableWindow(GetDlgItem(hwnd, IDC_LEFTSTATIC), configInfo->cropping);
				EnableWindow(GetDlgItem(hwnd, IDC_LEFTSPIN), configInfo->cropping);
				EnableWindow(GetDlgItem(hwnd, IDC_WIDTH), configInfo->cropping);
				EnableWindow(GetDlgItem(hwnd, IDC_WIDTHSTATIC), configInfo->cropping);
				EnableWindow(GetDlgItem(hwnd, IDC_WIDTHSPIN), configInfo->cropping);
				EnableWindow(GetDlgItem(hwnd, IDC_HEIGHT), configInfo->cropping);
				EnableWindow(GetDlgItem(hwnd, IDC_HEIGHTSTATIC), configInfo->cropping);
				EnableWindow(GetDlgItem(hwnd, IDC_HEIGHTSPIN), configInfo->cropping);

                return TRUE;
            }

		case WM_HSCROLL: // this code is mainly horrible because scrollbars aren't meant to be used as slider controls ^_^
			{
				int minimum, maximum, spinner;
				int ctrl = GetDlgCtrlID((HWND)lParam);
				int value = GetScrollPos((HWND)lParam, SB_CTL);
				GetScrollRange((HWND)lParam, SB_CTL, &minimum, &maximum);

				switch(ctrl) // LOOK UP ALL THE THINGS!
				{
					case IDC_HOROFFSETSCROLL:
					{
						spinner = IDC_HOROFFSETSPIN;
						break;
					}
					case IDC_HORSCALESCROLL:
					{
						spinner = IDC_HORSCALESPIN;
						break;
					}
					case IDC_PHASESCROLL:
					{
						spinner = IDC_PHASESPIN;
						break;
					}
					case IDC_VEROFFSETSCROLL:
					{
						spinner = IDC_VEROFFSETSPIN;
						break;
					}
					case IDC_BLACKLEVELSCROLL:
					{
						spinner = IDC_BLACKLEVELSPIN;
						break;
					}
					case IDC_BRIGHTNESSSCROLL:
					{
						spinner = IDC_BRIGHTNESSSPIN;
						break;
					}
					case IDC_CONTRASTSCROLL:
					{
						spinner = IDC_CONTRASTSPIN;
						break;
					}
					case IDC_GAINREDSCROLL:
					{
						spinner = IDC_GAINREDSPIN;
						break;
					}
					case IDC_GAINGREENSCROLL:
					{
						spinner = IDC_GAINGREENSPIN;
						break;
					}
					case IDC_GAINBLUESCROLL:
					{
						spinner = IDC_GAINBLUESPIN;
						break;
					}
				}
				switch(LOWORD(wParam))
				{
				case SB_THUMBTRACK:
				case SB_THUMBPOSITION:
					{
						value = (signed short)HIWORD(wParam);
						break;
					}
				case SB_LINELEFT:
					{
						value = max(minimum, value--);
						break;
					}
				case SB_LINERIGHT:
					{
						value = min(maximum, value++);
						break;
					}
				case SB_PAGELEFT:
					{
						value = max(minimum, value - 5);
						break;
					}
				case SB_PAGERIGHT:
					{
						value = min(maximum, value + 5);
						break;
					}
				case SB_ENDSCROLL:
					{
						value = (int)SendMessage(GetDlgItem(hwnd, spinner), UDM_GETPOS32, 0, 0);
						SendMessage(GetDlgItem(hwnd, ctrl), SBM_SETPOS, value, TRUE);
						return 0;
					}
				}
				SendMessage(GetDlgItem(hwnd, spinner), UDM_SETPOS32, NULL, value);
				break;
			}

		case WM_NOTIFY:
			{
				LPNMHDR nmh = (LPNMHDR)lParam;
				if (nmh->code == UDN_DELTAPOS)
				{
					ConfigVisionInfo *configInfo = (ConfigVisionInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
					if ((LONG)configInfo > DWLP_USER)
					{
						HWND hOpposingCtrl;
						int *pVal;
						int *pOpposingVal;
						int *pOpposingMinVal;
						int *pOpposingMaxVal;
						int *pOpposingCurVal;
						switch (nmh->idFrom)
						{
							case IDC_LEFTSPIN:
								{
									hOpposingCtrl = GetDlgItem(hwnd, IDC_WIDTHSPIN);
									pVal = &configInfo->cropLeft;
									pOpposingVal = &configInfo->width;
									pOpposingMinVal = (int*)&configInfo->widthMin;
									pOpposingMaxVal = (int*)&configInfo->widthMax;
									pOpposingCurVal = (int*)&configInfo->widthCur;
									break;
								}
							case IDC_TOPSPIN:
								{
									hOpposingCtrl = GetDlgItem(hwnd, IDC_HEIGHTSPIN);
									pVal = &configInfo->cropTop;
									pOpposingVal = &configInfo->height;
									pOpposingMinVal = (int*)&configInfo->heightMin;
									pOpposingMaxVal = (int*)&configInfo->heightMax;
									pOpposingCurVal = (int*)&configInfo->heightCur;
									break;
								}
							case IDC_WIDTHSPIN:
								{
									hOpposingCtrl = GetDlgItem(hwnd, IDC_LEFTSPIN);
									pVal = &configInfo->width;
									pOpposingVal = &configInfo->cropLeft;
									break;
								}
							case IDC_HEIGHTSPIN:
								{
									hOpposingCtrl = GetDlgItem(hwnd, IDC_TOPSPIN);
									pVal = &configInfo->height;
									pOpposingVal = &configInfo->cropTop;
									break;
								}
							default:
								return 0;
						}

						*pVal = (int)SendMessage(nmh->hwndFrom, UDM_GETPOS32, 0, 0);
						*pOpposingVal = (int)SendMessage(hOpposingCtrl, UDM_GETPOS32, 0, 0);

						if (configInfo->source && configInfo->source->hRGB)
							SetCropping(configInfo->source->hRGB, &configInfo->cropLeft, &configInfo->cropTop, &configInfo->cropWidth, &configInfo->cropHeight);
						
						if (nmh->idFrom == IDC_LEFTSPIN || nmh->idFrom == IDC_TOPSPIN)
						{
							*pOpposingMaxVal = max(*pOpposingMinVal, *pOpposingCurVal - *pVal);
							if (*pOpposingVal > *pOpposingMaxVal)
									SendMessage(hOpposingCtrl, UDM_SETPOS32, NULL, *pOpposingMaxVal);
							SendMessage(hOpposingCtrl, UDM_SETRANGE32, NULL, *pOpposingMaxVal);
						}
					}
				}
			break;
			}

		case WM_COMMAND:
            switch(LOWORD(wParam))
            {
				case IDC_INPUTSOURCE:
					{
						if (HIWORD(wParam) == CBN_SELCHANGE)
						{
							int input = (int)SendMessage(GetDlgItem(hwnd, IDC_INPUTSOURCE), CB_GETCURSEL, 0, 0);
							LONG user = GetWindowLongPtr(hwnd, DWLP_USER);
							if (user > DWLP_USER)
							{
								TCHAR modeText[255];
								ConfigVisionInfo *configInfo = (ConfigVisionInfo*)user;
								GetModeText(input, modeText, 255);
								SetWindowText(GetDlgItem(hwnd, IDC_DETECTEDMODE), modeText);

								if (configInfo->source)
									if (configInfo->source->hRGB)
									{
										configInfo->source->Stop();
										configInfo->data->SetInt(TEXT("input"), input);
										configInfo->source->Start();
									}
							}

						}
					}
				case IDC_LEFT:
					{
						if (HIWORD(wParam) == EN_CHANGE || HIWORD(wParam) == EN_KILLFOCUS)
						{
							ConfigVisionInfo *configInfo = (ConfigVisionInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
							if ((LONG)configInfo > DWLP_USER)
							{
								configInfo->cropLeft = (int)SendMessage(GetDlgItem(hwnd, IDC_LEFTSPIN), UDM_GETPOS32, 0, 0);
								configInfo->width = (int)SendMessage(GetDlgItem(hwnd, IDC_WIDTHSPIN), UDM_GETPOS32, 0, 0);
								if (GetDlgCtrlID(GetFocus()) != IDC_LEFT) // wait for user to finish typing
									SendMessage(GetDlgItem(hwnd, IDC_LEFTSPIN), UDM_SETPOS32, NULL, configInfo->cropLeft);

								if (configInfo->source && configInfo->source->hRGB)
									SetCropping(configInfo->source->hRGB, &configInfo->cropLeft, NULL, NULL, NULL);

								configInfo->widthMax = max((int)configInfo->widthMin, configInfo->widthCur - configInfo->cropLeft);
								if (configInfo->width > (int)configInfo->widthMax)
									SendMessage(GetDlgItem(hwnd, IDC_WIDTHSPIN), UDM_SETPOS32, NULL, configInfo->widthMax);
								SendMessage(GetDlgItem(hwnd, IDC_WIDTHSPIN), UDM_SETRANGE32, NULL, configInfo->widthMax);
							}
							break;
						}
					}
				case IDC_TOP:
					{
						if (HIWORD(wParam) == EN_CHANGE || HIWORD(wParam) == EN_KILLFOCUS)
						{
							ConfigVisionInfo *configInfo = (ConfigVisionInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
							if ((LONG)configInfo > DWLP_USER)
							{
								configInfo->cropTop = (int)SendMessage(GetDlgItem(hwnd, IDC_TOPSPIN), UDM_GETPOS32, 0, 0);
								configInfo->height = (int)SendMessage(GetDlgItem(hwnd, IDC_HEIGHTSPIN), UDM_GETPOS32, 0, 0);
								if (GetDlgCtrlID(GetFocus()) != IDC_TOP) // wait for user to finish typing
									SendMessage(GetDlgItem(hwnd, IDC_TOPSPIN), UDM_SETPOS32, NULL, configInfo->cropTop);

								if (configInfo->source && configInfo->source->hRGB)
									SetCropping(configInfo->source->hRGB, NULL, &configInfo->cropTop, NULL, NULL);

								configInfo->heightMax = max((int)configInfo->heightMin, configInfo->heightCur - configInfo->cropTop);
								if (configInfo->height > (int)configInfo->heightMax)
									SendMessage(GetDlgItem(hwnd, IDC_HEIGHTSPIN), UDM_SETPOS32, NULL, configInfo->heightMax);
								SendMessage(GetDlgItem(hwnd, IDC_HEIGHTSPIN), UDM_SETRANGE32, NULL, configInfo->heightMax);
							}
							break;
						}
					}
				case IDC_WIDTH:
					{
						if (HIWORD(wParam) == EN_CHANGE || HIWORD(wParam) == EN_KILLFOCUS)
						{
							ConfigVisionInfo *configInfo = (ConfigVisionInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
							if ((LONG)configInfo > DWLP_USER)
							{
								configInfo->cropWidth = (int)SendMessage(GetDlgItem(hwnd, IDC_WIDTHSPIN), UDM_GETPOS32, 0, 0);
								if (GetDlgCtrlID(GetFocus()) != IDC_WIDTH) // wait for user to finish typing
									SendMessage(GetDlgItem(hwnd, IDC_WIDTHSPIN), UDM_SETPOS32, NULL, configInfo->cropWidth);
								if (configInfo->source && configInfo->source->hRGB)
									SetCropping(configInfo->source->hRGB, NULL, NULL, &configInfo->cropWidth, NULL);
							}
							break;
						}
					}
				case IDC_HEIGHT:
					{
						if (HIWORD(wParam) == EN_CHANGE || HIWORD(wParam) == EN_KILLFOCUS)
						{
							ConfigVisionInfo *configInfo = (ConfigVisionInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
							if ((LONG)configInfo > DWLP_USER)
							{
								configInfo->cropHeight = (int)SendMessage(GetDlgItem(hwnd, IDC_HEIGHTSPIN), UDM_GETPOS32, 0, 0);
								if (GetDlgCtrlID(GetFocus()) != IDC_HEIGHT) // wait for user to finish typing
									SendMessage(GetDlgItem(hwnd, IDC_HEIGHTSPIN), UDM_SETPOS32, NULL, configInfo->cropHeight);
								if (configInfo->source && configInfo->source->hRGB)
									SetCropping(configInfo->source->hRGB, NULL, NULL, NULL, &configInfo->cropHeight);
							}
							break;
						}
					}
				case IDC_CROPPING:
					{
						BOOL cropping = SendMessage(GetDlgItem(hwnd, IDC_CROPPING), BM_GETCHECK, 0, 0);

						EnableWindow(GetDlgItem(hwnd, IDC_TOP), cropping);
						EnableWindow(GetDlgItem(hwnd, IDC_TOPSTATIC), cropping);
						EnableWindow(GetDlgItem(hwnd, IDC_TOPSPIN), cropping);
						EnableWindow(GetDlgItem(hwnd, IDC_LEFT), cropping);
						EnableWindow(GetDlgItem(hwnd, IDC_LEFTSTATIC), cropping);
						EnableWindow(GetDlgItem(hwnd, IDC_LEFTSPIN), cropping);
						EnableWindow(GetDlgItem(hwnd, IDC_WIDTH), cropping);
						EnableWindow(GetDlgItem(hwnd, IDC_WIDTHSTATIC), cropping);
						EnableWindow(GetDlgItem(hwnd, IDC_WIDTHSPIN), cropping);
						EnableWindow(GetDlgItem(hwnd, IDC_HEIGHT), cropping);
						EnableWindow(GetDlgItem(hwnd, IDC_HEIGHTSTATIC), cropping);
						EnableWindow(GetDlgItem(hwnd, IDC_HEIGHTSPIN), cropping);

						ConfigVisionInfo *configInfo = (ConfigVisionInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
						if ((LONG)configInfo > DWLP_USER && configInfo->source && configInfo->source->hRGB)
							RGBEnableCropping(configInfo->source->hRGB, cropping);
						
						break;
					}
				case IDC_CUSTOMRES:
					{
						BOOL customRes = SendMessage(GetDlgItem(hwnd, IDC_CUSTOMRES), BM_GETCHECK, 0, 0);

						EnableWindow(GetDlgItem(hwnd, IDC_ADDRWIDTH), customRes);
						EnableWindow(GetDlgItem(hwnd, IDC_ADDRWIDTHSTATIC), customRes);
						EnableWindow(GetDlgItem(hwnd, IDC_ADDRWIDTHSPIN), customRes);
						EnableWindow(GetDlgItem(hwnd, IDC_ADDRHEIGHT), customRes);
						EnableWindow(GetDlgItem(hwnd, IDC_ADDRHEIGHTSTATIC), customRes);
						EnableWindow(GetDlgItem(hwnd, IDC_ADDRHEIGHTSPIN), customRes);

						ConfigVisionInfo *configInfo = (ConfigVisionInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
						if ((LONG)configInfo > DWLP_USER && configInfo->source && configInfo->source->hRGB)
						{
							unsigned long width, height;
							if (!customRes)
							{
								RGBGetCaptureWidthDefault(configInfo->source->hRGB, &width);
								RGBGetCaptureHeightDefault(configInfo->source->hRGB, &height);
							}
							else
							{
								width = (int)SendMessage(GetDlgItem(hwnd, IDC_ADDRWIDTHSPIN), UDM_GETPOS32, 0, 0);
								height = (int)SendMessage(GetDlgItem(hwnd, IDC_ADDRHEIGHTSPIN), UDM_GETPOS32, 0, 0);
							}
							RGBSetCaptureWidth(configInfo->source->hRGB, width);
							RGBSetCaptureHeight(configInfo->source->hRGB, height);
						}
						break;
					}
				case IDC_HOROFFSET:
					{
						int horoffset = (int)SendMessage(GetDlgItem(hwnd, IDC_HOROFFSETSPIN), UDM_GETPOS32, 0, 0);
						SendMessage(GetDlgItem(hwnd, IDC_HOROFFSETSPIN), UDM_SETPOS32, NULL, horoffset);
						SendMessage(GetDlgItem(hwnd, IDC_HOROFFSETSCROLL), SBM_SETPOS, horoffset, TRUE);
						break;
					}
				case IDC_HORSCALE:
					{
						int horscale = (int)SendMessage(GetDlgItem(hwnd, IDC_HORSCALESPIN), UDM_GETPOS32, 0, 0);
						SendMessage(GetDlgItem(hwnd, IDC_HORSCALESPIN), UDM_SETPOS32, NULL, horscale);
						SendMessage(GetDlgItem(hwnd, IDC_HORSCALESCROLL), SBM_SETPOS, horscale, TRUE);
						break;
					}
				case IDC_PHASE:
					{
						int phase = (int)SendMessage(GetDlgItem(hwnd, IDC_PHASESPIN), UDM_GETPOS32, 0, 0);
						SendMessage(GetDlgItem(hwnd, IDC_PHASESPIN), UDM_SETPOS32, NULL, phase);
						SendMessage(GetDlgItem(hwnd, IDC_PHASESCROLL), SBM_SETPOS, phase, TRUE);
						break;
					}
				case IDC_VEROFFSET:
					{
						int veroffset = (int)SendMessage(GetDlgItem(hwnd, IDC_VEROFFSETSPIN), UDM_GETPOS32, 0, 0);
						SendMessage(GetDlgItem(hwnd, IDC_VEROFFSETSPIN), UDM_SETPOS32, NULL, veroffset);
						SendMessage(GetDlgItem(hwnd, IDC_VEROFFSETSCROLL), SBM_SETPOS, veroffset, TRUE);
						break;
					}
				case IDC_BLACKLEVEL:
					{
						int blacklevel = (int)SendMessage(GetDlgItem(hwnd, IDC_BLACKLEVELSPIN), UDM_GETPOS32, 0, 0);
						SendMessage(GetDlgItem(hwnd, IDC_BLACKLEVELSPIN), UDM_SETPOS32, NULL, blacklevel);
						SendMessage(GetDlgItem(hwnd, IDC_BLACKLEVELSCROLL), SBM_SETPOS, blacklevel, TRUE);
						break;
					}
				case IDC_BRIGHTNESS:
					{
						int brightness = (int)SendMessage(GetDlgItem(hwnd, IDC_BRIGHTNESSSPIN), UDM_GETPOS32, 0, 0);
						SendMessage(GetDlgItem(hwnd, IDC_BRIGHTNESSSPIN), UDM_SETPOS32, NULL, brightness);
						SendMessage(GetDlgItem(hwnd, IDC_BRIGHTNESSSCROLL), SBM_SETPOS, brightness, TRUE);
						break;
					}
				case IDC_CONTRAST:
					{
						int contrast = (int)SendMessage(GetDlgItem(hwnd, IDC_CONTRASTSPIN), UDM_GETPOS32, 0, 0);
						SendMessage(GetDlgItem(hwnd, IDC_CONTRASTSPIN), UDM_SETPOS32, NULL, contrast);
						SendMessage(GetDlgItem(hwnd, IDC_CONTRASTSCROLL), SBM_SETPOS, contrast, TRUE);
						break;
					}
				case IDC_GAINRED:
					{
						int gainred = (int)SendMessage(GetDlgItem(hwnd, IDC_GAINREDSPIN), UDM_GETPOS32, 0, 0);
						SendMessage(GetDlgItem(hwnd, IDC_GAINREDSPIN), UDM_SETPOS32, NULL, gainred);
						SendMessage(GetDlgItem(hwnd, IDC_GAINREDSCROLL), SBM_SETPOS, gainred, TRUE);
						break;
					}
				case IDC_GAINGREEN:
					{
						int gaingreen = (int)SendMessage(GetDlgItem(hwnd, IDC_GAINGREENSPIN), UDM_GETPOS32, 0, 0);
						SendMessage(GetDlgItem(hwnd, IDC_GAINGREENSPIN), UDM_SETPOS32, NULL, gaingreen);
						SendMessage(GetDlgItem(hwnd, IDC_GAINGREENSCROLL), SBM_SETPOS, gaingreen, TRUE);
						break;
					}
				case IDC_GAINBLUE:
					{
						int gainblue = (int)SendMessage(GetDlgItem(hwnd, IDC_GAINBLUESPIN), UDM_GETPOS32, 0, 0);
						SendMessage(GetDlgItem(hwnd, IDC_GAINBLUESPIN), UDM_SETPOS32, NULL, gainblue);
						SendMessage(GetDlgItem(hwnd, IDC_GAINBLUESCROLL), SBM_SETPOS, gainblue, TRUE);
						break;
					}
                case IDOK:
                    {
						ConfigVisionInfo *configInfo = (ConfigVisionInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                        BOOL bFailed;
						
						int input = (int)SendMessage(GetDlgItem(hwnd, IDC_INPUTSOURCE), CB_GETCURSEL, 0, 0);
						// TODO: check input
                        configInfo->data->SetInt(TEXT("input"), input);

						int pixFormat = (int)SendMessage(GetDlgItem(hwnd, IDC_PIXFORMAT), CB_GETCURSEL, 0, 0);
						// TODO: check input
                        configInfo->data->SetInt(TEXT("pixFormat"), pixFormat);

						BOOL customRes = (BOOL)SendMessage(GetDlgItem(hwnd, IDC_CUSTOMRES), BM_GETCHECK, 0, 0);
						configInfo->data->SetInt(TEXT("customRes"), customRes);

						if (customRes)
						{
							int width = (int)SendMessage(GetDlgItem(hwnd, IDC_ADDRWIDTHSPIN), UDM_GETPOS32, 0, (LPARAM)&bFailed);
							// TODO: check input
							configInfo->data->SetInt(TEXT("resolutionWidth"), bFailed ? 640 : width);

							int height = (int)SendMessage(GetDlgItem(hwnd, IDC_ADDRHEIGHTSPIN), UDM_GETPOS32, 0, (LPARAM)&bFailed);
							// TODO: check input
							configInfo->data->SetInt(TEXT("resolutionHeight"), bFailed ? 480 : height);
						}

						int cropTop = (int)SendMessage(GetDlgItem(hwnd, IDC_TOPSPIN), UDM_GETPOS32, 0, (LPARAM)&bFailed);
                        // TODO: check input
                        configInfo->data->SetInt(TEXT("cropTop"), bFailed ? 0 : cropTop);

						int cropLeft = (int)SendMessage(GetDlgItem(hwnd, IDC_LEFTSPIN), UDM_GETPOS32, 0, (LPARAM)&bFailed);
                        // TODO: check input
                        configInfo->data->SetInt(TEXT("cropLeft"), bFailed ? 0 : cropLeft);

						int cropWidth = (int)SendMessage(GetDlgItem(hwnd, IDC_WIDTHSPIN), UDM_GETPOS32, 0, (LPARAM)&bFailed);
                        // TODO: check input
                        configInfo->data->SetInt(TEXT("cropWidth"), bFailed ? 640 : cropWidth);

						int cropHeight = (int)SendMessage(GetDlgItem(hwnd, IDC_HEIGHTSPIN), UDM_GETPOS32, 0, (LPARAM)&bFailed);
                        // TODO: check input
                        configInfo->data->SetInt(TEXT("cropHeight"), bFailed ? 480 : cropHeight);

						BOOL cropping = (BOOL)SendMessage(GetDlgItem(hwnd, IDC_CROPPING), BM_GETCHECK, 0, 0);
						configInfo->data->SetInt(TEXT("cropping"), cropping);

						BOOL pointFilter = (BOOL)SendMessage(GetDlgItem(hwnd, IDC_POINTFILTER), BM_GETCHECK, 0, 0);
						configInfo->data->SetInt(TEXT("pointFilter"), pointFilter);
                    }

                case IDCANCEL:
					ConfigVisionInfo *configInfo = (ConfigVisionInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
					configInfo->source->hConfigWnd = NULL;
					if (!configInfo->source || configInfo->hRGB != configInfo->source->hRGB)
						RGBCloseInput(configInfo->hRGB);
                    EndDialog(hwnd, LOWORD(wParam));
                    break;
            }
            break;
    }

    return 0;
}

bool STDCALL ConfigureVisionSource(XElement *element, bool bCreating)
{
    if(!element)
    {
        AppWarning(TEXT("ConfigureVisionSource: NULL element"));
        return false;
    }

    XElement *data = element->GetElement(TEXT("data"));
    if(!data)
        data = element->CreateElement(TEXT("data"));

    ConfigVisionInfo configInfo;
    configInfo.data = data;
	configInfo.source = (VisionSource*)OBSGetSceneImageSource(element->GetName());

	configInfo.hMutex = OSCreateMutex();
    if(DialogBoxParam(hinstMain, MAKEINTRESOURCE(IDD_CONFIG), OBSGetMainWindow(), ConfigureDialogProc, (LPARAM)&configInfo) == IDOK)
    {
		int input = data->GetInt(TEXT("input"));
		HRGB hRGB;
		int width = 640;
		int height = 480;
		bool openInput = ((configInfo.source) && (hRGB = configInfo.source->hRGB)); // use existing handle if available
		
		OSCloseMutex(configInfo.hMutex);
		if (openInput || RGBERROR_NO_ERROR == RGBOpenInput(input, &hRGB))
		{
			RGBGetCaptureWidthDefault(hRGB, (unsigned long*)&width);
			RGBGetCaptureHeightDefault(hRGB, (unsigned long*)&height);
			if (!openInput) // don't close a handle we didn't open ourselves
				RGBCloseInput(hRGB);
		}
		if (data->GetInt(TEXT("customRes")))
		{
			element->SetInt(TEXT("cx"), data->GetInt(TEXT("resolutionWidth"), width));
			element->SetInt(TEXT("cy"), data->GetInt(TEXT("resolutionHeight"), height));
		}
		else
		{
			element->SetInt(TEXT("cx"), width);
			element->SetInt(TEXT("cy"), height);
		}

        return true;
    }

    return false;
}

ImageSource* STDCALL CreateVisionSource(XElement *data)
{
    VisionSource *source = new VisionSource;
    if(!source->Init(data))
    {
        delete source;
        return NULL;
    }

    return source;
}


bool LoadPlugin()
{
    traceIn(DatapathPluginLoadPlugin);

	if (RGBERROR_NO_ERROR != RGBLoad(&gHRGBDLL))
		return false;
	gpD3D9 = new D3D9Context();

    pluginLocale = new LocaleStringLookup;

    if(!pluginLocale->LoadStringFile(TEXT("plugins/DatapathPlugin/locale/en.txt")))
        AppWarning(TEXT("Could not open locale string file '%s'"), TEXT("plugins/DatapathPlugin/locale/en.txt"));

    if(scmpi(OBSGetLanguage(), TEXT("en")) != 0)
    {
        String pluginStringFile;
        pluginStringFile << TEXT("plugins/DatapathPlugin/locale/") << OBSGetLanguage() << TEXT(".txt");
        if(!pluginLocale->LoadStringFile(pluginStringFile))
            AppWarning(TEXT("Could not open locale string file '%s'"), pluginStringFile.Array());
    }

    OBSRegisterImageSourceClass(DATAPATH_CLASSNAME, PluginStr("ClassName"), (OBSCREATEPROC)CreateVisionSource, (OBSCONFIGPROC)ConfigureVisionSource);

    return true;

    traceOut;
}

void UnloadPlugin()
{
    delete pluginLocale;
	RGBFree(gHRGBDLL);
}

CTSTR GetPluginName()
{
    return PluginStr("Plugin.Name");
}

CTSTR GetPluginDescription()
{
    return PluginStr("Plugin.Description");
}


BOOL CALLBACK DllMain(HINSTANCE hInst, DWORD dwReason, LPVOID lpBla)
{
    if(dwReason == DLL_PROCESS_ATTACH)
        hinstMain = hInst;

    return TRUE;
}

