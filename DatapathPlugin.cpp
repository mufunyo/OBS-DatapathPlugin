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

void SetCropping(LONG user, int* pLeft, int* pTop, int* pWidth, int* pHeight)
{
	if (user <= DWLP_USER)
		return;
	ConfigVisionInfo *configInfo = (ConfigVisionInfo*)user;

	if (!configInfo->source)
		return;
	if (!configInfo->source->hRGB)
		return;

	int left, top, width, height;

	if (!pLeft || !pTop || !pWidth || !pHeight) // fill in missing values
	{
		RGBGetCropping(configInfo->source->hRGB, (signed long*)&top, (signed long*)&left, (unsigned long*)&width, (unsigned long*)&height);
		if (!pLeft)
			pLeft = &left;
		if (!pTop)
			pTop = &top;
		if (!pWidth)
			pWidth = &width;
		if (!pHeight)
			pHeight = &height;
	}

	RGBSetCropping(configInfo->source->hRGB, (signed long)*pTop, (signed long)*pLeft, (unsigned long)*pWidth, (unsigned long)*pHeight);
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
				HRGB hRGB;
				unsigned long inputs;
				unsigned long widthCur  = 640;
				unsigned long widthMin  = 160;
				unsigned long widthMax  = 4096;
				unsigned long heightCur = 480;
				unsigned long heightMin = 120;
				unsigned long heightMax = 4096;
				long cropTopCur = 0;
				long cropTopMin = 0;
				long cropTopMax = 0;
				long cropLeftCur = 0;
				long cropLeftMin = 0;
				long cropLeftMax = 0;
				unsigned long cropWidthCur = 0;
				unsigned long cropWidthMin = 0;
				unsigned long cropWidthMax = 0;
				unsigned long cropHeightCur = 0;
				unsigned long cropHeightMin = 0;
				unsigned long cropHeightMax = 0;
				TCHAR modeText[255];

                ConfigVisionInfo *configInfo = (ConfigVisionInfo*)lParam;
                SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)lParam);
				configInfo->source->hConfigWnd = hwnd;
                LocalizeWindow(hwnd);
				
				SendMessage(GetDlgItem(hwnd, IDC_CUSTOMRES), BM_SETCHECK, 1, 0);

				RGBGetNumberOfInputs(&inputs);
				int input = configInfo->data->GetInt(TEXT("input"));
				input = min((int)inputs, input);

				GetModeText(input, modeText, 255);
				SetWindowText(GetDlgItem(hwnd, IDC_DETECTEDMODE), modeText);

				if (configInfo->source)
					hRGB = configInfo->source->hRGB;
				else
				{
					RGBOpenInput((unsigned long)input, &hRGB);
					RGBSetModeChangedFn(hRGB, &ModeChanged, (ULONG_PTR)&configInfo);
				}

				if (hRGB)
				{
					RGBGetCaptureWidthDefault(hRGB, &widthCur);
					RGBGetCaptureWidthMinimum(hRGB, &widthMin);
					RGBGetCaptureWidthMaximum(hRGB, &widthMax);
					RGBGetCaptureHeightDefault(hRGB, &heightCur);
					RGBGetCaptureHeightMinimum(hRGB, &heightMin);
					RGBGetCaptureHeightMaximum(hRGB, &heightMax);
					RGBGetCropping(hRGB, &cropTopCur, &cropLeftCur, &cropWidthCur, &cropHeightCur);
					RGBGetCroppingMinimum(hRGB, &cropTopMin, &cropLeftMin, &cropWidthMin, &cropHeightMin);
					RGBGetCroppingMaximum(hRGB, &cropTopMax, &cropLeftMax, &cropWidthMax, &cropHeightMax);
				}
				
				if (hRGB && !configInfo->source)
					RGBCloseInput(hRGB);

				for (unsigned long i = 0; i < inputs; i++)
				{
					TCHAR  text[32];
					
					StringCchPrintf (text, 32, TEXT("%d"), i+1);

					SendMessage (GetDlgItem(hwnd, IDC_INPUTSOURCE), CB_ADDSTRING, (WPARAM)0, (LPARAM)&text);
					SendMessage (GetDlgItem(hwnd, IDC_INPUTSOURCE), CB_SETITEMDATA , i, (LPARAM)i);
				}
				SendMessage(GetDlgItem(hwnd, IDC_INPUTSOURCE), CB_SETCURSEL, input, 0);

				int width = configInfo->data->GetInt(TEXT("resolutionWidth"), (int)widthCur);
				int height = configInfo->data->GetInt(TEXT("resolutionHeight"), (int)heightCur);
				int cropTop = configInfo->data->GetInt(TEXT("cropTop"), cropTopCur);
				int cropLeft = configInfo->data->GetInt(TEXT("cropLeft"), cropLeftCur);
				int cropWidth = configInfo->data->GetInt(TEXT("cropWidth"), (int)cropWidthCur);
				int cropHeight = configInfo->data->GetInt(TEXT("cropHeight"), (int)cropHeightCur);
				BOOL cropping = configInfo->data->GetInt(TEXT("cropping"), FALSE);


				SendMessage(GetDlgItem(hwnd, IDC_ADDRWIDTHSPIN), UDM_SETRANGE32, (int)widthMin, (int)widthMax);
				SendMessage(GetDlgItem(hwnd, IDC_ADDRHEIGHTSPIN), UDM_SETRANGE32, (int)heightMin, (int)heightMax);
				SendMessage(GetDlgItem(hwnd, IDC_TOPSPIN), UDM_SETRANGE32, cropTopMin, cropTopMax);
				SendMessage(GetDlgItem(hwnd, IDC_LEFTSPIN), UDM_SETRANGE32, cropLeftMin, cropLeftMax);
				SendMessage(GetDlgItem(hwnd, IDC_WIDTHSPIN), UDM_SETRANGE32, (int)cropWidthMin, (int)cropWidthMax);
				SendMessage(GetDlgItem(hwnd, IDC_HEIGHTSPIN), UDM_SETRANGE32, (int)cropHeightMin, (int)cropHeightMax);
				SendMessage(GetDlgItem(hwnd, IDC_ADDRWIDTHSPIN), UDM_SETPOS32, 0, width);
				SendMessage(GetDlgItem(hwnd, IDC_ADDRHEIGHTSPIN), UDM_SETPOS32, 0, height);
				SendMessage(GetDlgItem(hwnd, IDC_TOPSPIN), UDM_SETPOS32, 0, cropTop);
				SendMessage(GetDlgItem(hwnd, IDC_LEFTSPIN), UDM_SETPOS32, 0, cropLeft);
				SendMessage(GetDlgItem(hwnd, IDC_WIDTHSPIN), UDM_SETPOS32, 0, cropWidth);
				SendMessage(GetDlgItem(hwnd, IDC_HEIGHTSPIN), UDM_SETPOS32, 0, cropHeight);
				SendMessage(GetDlgItem(hwnd, IDC_CROPPING), BM_SETCHECK, cropping, 0);
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
							int left = (int)SendMessage(GetDlgItem(hwnd, IDC_LEFTSPIN), UDM_GETPOS32, 0, 0);
							SetCropping(GetWindowLongPtr(hwnd, DWLP_USER), &left, NULL, NULL, NULL);
							break;
						}
					}
				case IDC_TOP:
					{
						if (HIWORD(wParam) == EN_CHANGE)
						{
							int top = (int)SendMessage(GetDlgItem(hwnd, IDC_TOPSPIN), UDM_GETPOS32, 0, 0);
							SetCropping(GetWindowLongPtr(hwnd, DWLP_USER), NULL, &top, NULL, NULL);
							break;
						}
					}
				case IDC_WIDTH:
					{
						if (HIWORD(wParam) == EN_CHANGE)
						{
							int width = (int)SendMessage(GetDlgItem(hwnd, IDC_WIDTHSPIN), UDM_GETPOS32, 0, 0);
							SetCropping(GetWindowLongPtr(hwnd, DWLP_USER), NULL, NULL, &width, NULL);
							break;
						}
					}
				case IDC_HEIGHT:
						if (HIWORD(wParam) == EN_CHANGE)
						{
							int height = (int)SendMessage(GetDlgItem(hwnd, IDC_HEIGHTSPIN), UDM_GETPOS32, 0, 0);
							SetCropping(GetWindowLongPtr(hwnd, DWLP_USER), NULL, NULL, NULL, &height);
							break;
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

						LONG user = GetWindowLongPtr(hwnd, DWLP_USER);
						if (user > DWLP_USER)
						{
							ConfigVisionInfo *configInfo = (ConfigVisionInfo*)user;
							if (configInfo->source)
								if (configInfo->source->hRGB)
									RGBEnableCropping(configInfo->source->hRGB, cropping);
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

                        int width = (int)SendMessage(GetDlgItem(hwnd, IDC_ADDRWIDTHSPIN), UDM_GETPOS32, 0, (LPARAM)&bFailed);
						// TODO: check input
                        configInfo->data->SetInt(TEXT("resolutionWidth"), bFailed ? 640 : width);

						int height = (int)SendMessage(GetDlgItem(hwnd, IDC_ADDRHEIGHTSPIN), UDM_GETPOS32, 0, (LPARAM)&bFailed);
                        // TODO: check input
                        configInfo->data->SetInt(TEXT("resolutionHeight"), bFailed ? 480 : height);

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

						configInfo->source->hConfigWnd = NULL;
                    }

                case IDCANCEL:
					ConfigVisionInfo *configInfo = (ConfigVisionInfo*)GetWindowLongPtr(hwnd, DWLP_USER);
					configInfo->source->hConfigWnd = NULL;
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

    if(DialogBoxParam(hinstMain, MAKEINTRESOURCE(IDD_CONFIG), API->GetMainWindow(), ConfigureDialogProc, (LPARAM)&configInfo) == IDOK)
    {
        element->SetInt(TEXT("cx"), data->GetInt(TEXT("resolutionWidth")));
        element->SetInt(TEXT("cy"), data->GetInt(TEXT("resolutionHeight")));

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

