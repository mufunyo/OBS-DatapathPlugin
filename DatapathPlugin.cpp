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


#define DATAPATH_CLASSNAME TEXT("DatapathCapture")

void CreateBitmapInformation(BITMAPINFO *pBitmapInfo, int width, int height, int bitCount)
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
					RGBGetCaptureWidthDefault(configInfo->hRGB, &configInfo->widthCur);
					RGBGetCaptureWidthMinimum(configInfo->hRGB, &configInfo->widthMin);
					RGBGetCaptureWidthMaximum(configInfo->hRGB, &configInfo->widthMax);
					RGBGetCaptureHeightDefault(configInfo->hRGB, &configInfo->heightCur);
					RGBGetCaptureHeightMinimum(configInfo->hRGB, &configInfo->heightMin);
					RGBGetCaptureHeightMaximum(configInfo->hRGB, &configInfo->heightMax);
					RGBGetCropping(configInfo->hRGB, &configInfo->cropTopCur, &configInfo->cropLeftCur, &configInfo->cropWidthCur, &configInfo->cropHeightCur);
					RGBGetCroppingMinimum(configInfo->hRGB, &configInfo->cropTopMin, &configInfo->cropLeftMin, &configInfo->cropWidthMin, &configInfo->cropHeightMin);
				}
				
				if (configInfo->source && configInfo->hRGB != configInfo->source->hRGB)
					RGBSetModeChangedFn(configInfo->hRGB, &ModeChanged, (ULONG_PTR)&configInfo);

				for (unsigned long i = 0; i < configInfo->inputs; i++)
				{
					TCHAR  text[32];
					
					StringCchPrintf (text, 32, TEXT("%d"), i+1);

					SendMessage (GetDlgItem(hwnd, IDC_INPUTSOURCE), CB_ADDSTRING, (WPARAM)0, (LPARAM)&text);
					SendMessage (GetDlgItem(hwnd, IDC_INPUTSOURCE), CB_SETITEMDATA , i, (LPARAM)i);
				}
				SendMessage(GetDlgItem(hwnd, IDC_INPUTSOURCE), CB_SETCURSEL, configInfo->input, 0);

				configInfo->width = configInfo->data->GetInt(TEXT("resolutionWidth"), (int)configInfo->widthCur);
				configInfo->height = configInfo->data->GetInt(TEXT("resolutionHeight"), (int)configInfo->heightCur);
				configInfo->cropTop = configInfo->data->GetInt(TEXT("cropTop"), configInfo->cropTopCur);
				configInfo->cropLeft = configInfo->data->GetInt(TEXT("cropLeft"), configInfo->cropLeftCur);
				configInfo->cropWidth = configInfo->data->GetInt(TEXT("cropWidth"), (int)configInfo->cropWidthCur);
				configInfo->cropHeight = configInfo->data->GetInt(TEXT("cropHeight"), (int)configInfo->cropHeightCur);
				configInfo->cropping = configInfo->data->GetInt(TEXT("cropping"), FALSE);
				configInfo->customRes = configInfo->data->GetInt(TEXT("customRes"), FALSE);
				configInfo->useDMA = configInfo->data->GetInt(TEXT("useDMA"), TRUE);

				configInfo->cropTop = min(configInfo->cropTop, (int)configInfo->heightCur);
				configInfo->cropLeft = min(configInfo->cropLeft, (int)configInfo->widthCur);
				unsigned long cropHeightMax = max(configInfo->cropHeightMin, configInfo->heightCur - configInfo->cropTop);
				unsigned long cropWidthMax = max(configInfo->cropWidthMin, configInfo->widthCur - configInfo->cropLeft);
				configInfo->cropHeight = min(configInfo->cropHeight, (int)cropHeightMax);
				configInfo->cropWidth = min(configInfo->cropWidth, (int)cropWidthMax);

				SendMessage(GetDlgItem(hwnd, IDC_ADDRWIDTHSPIN), UDM_SETRANGE32, (int)configInfo->widthMin, (int)configInfo->widthMax);
				SendMessage(GetDlgItem(hwnd, IDC_ADDRHEIGHTSPIN), UDM_SETRANGE32, (int)configInfo->heightMin, (int)configInfo->heightMax);
				SendMessage(GetDlgItem(hwnd, IDC_TOPSPIN), UDM_SETRANGE32, configInfo->cropTopMin, configInfo->heightCur);
				SendMessage(GetDlgItem(hwnd, IDC_LEFTSPIN), UDM_SETRANGE32, configInfo->cropLeftMin, configInfo->widthCur);
				SendMessage(GetDlgItem(hwnd, IDC_WIDTHSPIN), UDM_SETRANGE32, (int)configInfo->cropWidthMin, (int)cropWidthMax);
				SendMessage(GetDlgItem(hwnd, IDC_HEIGHTSPIN), UDM_SETRANGE32, (int)configInfo->cropHeightMin, (int)cropHeightMax);
				SendMessage(GetDlgItem(hwnd, IDC_ADDRWIDTHSPIN), UDM_SETPOS32, 0, configInfo->width);
				SendMessage(GetDlgItem(hwnd, IDC_ADDRHEIGHTSPIN), UDM_SETPOS32, 0, configInfo->height);
				SendMessage(GetDlgItem(hwnd, IDC_TOPSPIN), UDM_SETPOS32, 0, configInfo->cropTop);
				SendMessage(GetDlgItem(hwnd, IDC_LEFTSPIN), UDM_SETPOS32, 0, configInfo->cropLeft);
				SendMessage(GetDlgItem(hwnd, IDC_WIDTHSPIN), UDM_SETPOS32, 0, configInfo->cropWidth);
				SendMessage(GetDlgItem(hwnd, IDC_HEIGHTSPIN), UDM_SETPOS32, 0, configInfo->cropHeight);
				SendMessage(GetDlgItem(hwnd, IDC_CROPPING), BM_SETCHECK, configInfo->cropping, 0);
				SendMessage(GetDlgItem(hwnd, IDC_CUSTOMRES), BM_SETCHECK, configInfo->customRes, 0);
				SendMessage(GetDlgItem(hwnd, IDC_USEDMA), BM_SETCHECK, configInfo->useDMA, 0);
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
						if (HIWORD(wParam) == EN_CHANGE)
						{
							ConfigVisionInfo *configInfo = (ConfigVisionInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
							if ((LONG)configInfo > DWLP_USER)
							{
								OSEnterMutex(configInfo->hMutex);
								configInfo->cropLeft = (int)SendMessage(GetDlgItem(hwnd, IDC_LEFTSPIN), UDM_GETPOS32, 0, 0);
								configInfo->width = (int)SendMessage(GetDlgItem(hwnd, IDC_WIDTHSPIN), UDM_GETPOS32, 0, 0);

								if (configInfo->source && configInfo->source->hRGB)
									SetCropping(configInfo->source->hRGB, &configInfo->cropLeft, NULL, NULL, NULL);

								configInfo->widthMax = max((int)configInfo->widthMin, configInfo->widthCur - configInfo->cropLeft);
								if (configInfo->width > (int)configInfo->widthMax)
									SendMessage(GetDlgItem(hwnd, IDC_WIDTHSPIN), UDM_SETPOS32, NULL, configInfo->widthMax);
								SendMessage(GetDlgItem(hwnd, IDC_WIDTHSPIN), UDM_SETRANGE32, NULL, configInfo->widthMax);
								OSLeaveMutex(configInfo->hMutex);
							}
							break;
						}
					}
				case IDC_TOP:
					{
						if (HIWORD(wParam) == EN_CHANGE)
						{
							ConfigVisionInfo *configInfo = (ConfigVisionInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
							if ((LONG)configInfo > DWLP_USER)
							{
								configInfo->cropTop = (int)SendMessage(GetDlgItem(hwnd, IDC_TOPSPIN), UDM_GETPOS32, 0, 0);
								configInfo->height = (int)SendMessage(GetDlgItem(hwnd, IDC_HEIGHTSPIN), UDM_GETPOS32, 0, 0);

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
						if (HIWORD(wParam) == EN_CHANGE)
						{
							ConfigVisionInfo *configInfo = (ConfigVisionInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
							if ((LONG)configInfo > DWLP_USER && configInfo->source && configInfo->source->hRGB)
							{
								configInfo->width = (int)SendMessage(GetDlgItem(hwnd, IDC_WIDTHSPIN), UDM_GETPOS32, 0, 0);
								SetCropping(configInfo->source->hRGB, NULL, NULL, &configInfo->width, NULL);
							}
							break;
						}
					}
				case IDC_HEIGHT:
					{
						if (HIWORD(wParam) == EN_CHANGE)
						{
							ConfigVisionInfo *configInfo = (ConfigVisionInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
							if ((LONG)configInfo > DWLP_USER && configInfo->source && configInfo->source->hRGB)
							{
								configInfo->height = (int)SendMessage(GetDlgItem(hwnd, IDC_HEIGHTSPIN), UDM_GETPOS32, 0, 0);
								SetCropping(configInfo->source->hRGB, NULL, NULL, NULL, &configInfo->height);
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
							if (customRes)
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
                case IDOK:
                    {
						ConfigVisionInfo *configInfo = (ConfigVisionInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
                        BOOL bFailed;
						
						int input = (int)SendMessage(GetDlgItem(hwnd, IDC_INPUTSOURCE), CB_GETCURSEL, 0, 0);
						// TODO: check input
                        configInfo->data->SetInt(TEXT("input"), input);

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

						BOOL useDMA = (BOOL)SendMessage(GetDlgItem(hwnd, IDC_USEDMA), BM_GETCHECK, 0, 0);
						configInfo->data->SetInt(TEXT("useDMA"), useDMA);
                    }

                case IDCANCEL:
					ConfigVisionInfo *configInfo = (ConfigVisionInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
					configInfo->source->hConfigWnd = NULL;
					if (configInfo->hRGB != configInfo->source->hRGB)
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
	configInfo.source = (VisionSource*)API->GetSceneImageSource(element->GetName());

	configInfo.hMutex = OSCreateMutex();
    if(DialogBoxParam(hinstMain, MAKEINTRESOURCE(IDD_CONFIG), API->GetMainWindow(), ConfigureDialogProc, (LPARAM)&configInfo) == IDOK)
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

    pluginLocale = new LocaleStringLookup;

    if(!pluginLocale->LoadStringFile(TEXT("plugins/DatapathPlugin/locale/en.txt")))
        AppWarning(TEXT("Could not open locale string file '%s'"), TEXT("plugins/DatapathPlugin/locale/en.txt"));

    if(scmpi(API->GetLanguage(), TEXT("en")) != 0)
    {
        String pluginStringFile;
        pluginStringFile << TEXT("plugins/DatapathPlugin/locale/") << API->GetLanguage() << TEXT(".txt");
        if(!pluginLocale->LoadStringFile(pluginStringFile))
            AppWarning(TEXT("Could not open locale string file '%s'"), pluginStringFile.Array());
    }

    API->RegisterImageSourceClass(DATAPATH_CLASSNAME, PluginStr("ClassName"), (OBSCREATEPROC)CreateVisionSource, (OBSCONFIGPROC)ConfigureVisionSource);

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

