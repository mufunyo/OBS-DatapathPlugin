/********************************************************************************
 Copyright (C) 2013 Muf <muf@mindflyer.net>

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

#include "DX9Crap.h"

void D3D9Texture::Map(void *lpData, INT pitch)
{
	HRESULT hr;
	D3DLOCKED_RECT lockedRect;
	if FAILED(hr = surface->LockRect(&lockedRect, NULL, NULL))
		AppWarning(TEXT("D3D9Texture::Map: LockRect failed, result = 0x%08lX"), hr);
	lpData = lockedRect.pBits;
	pitch = lockedRect.Pitch;
}

void D3D9Texture::Unmap()
{
	HRESULT hr;
	if FAILED(hr = surface->UnlockRect())
		AppWarning(TEXT("D3D9Texture::Unmap: UnlockRect failed, result = 0x%08lX"), hr);
}

D3D9Context::D3D9Context()
{
	HRESULT hr;
	if FAILED(hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &pD3D9))
		AppWarning(TEXT("D3D9Context: Direct3DCreate9Ex failed, result = 0x%08lX"), hr);

	HWND hwnd = CreateWindow(NULL, NULL, NULL, 0, 0, 32, 32, NULL, NULL, NULL, NULL);
	D3DPRESENT_PARAMETERS params;
	zero(&params, sizeof(D3DPRESENT_PARAMETERS));
	params.BackBufferWidth = 32;
	params.BackBufferHeight = 32;
	params.BackBufferCount = 1;
	params.SwapEffect = D3DSWAPEFFECT_DISCARD;
	params.Windowed = TRUE;
	if FAILED(hr = pD3D9->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED | D3DCREATE_NOWINDOWCHANGES, &params, NULL, &pD3D9Device))
		AppWarning(TEXT("D3D9Context: CreateDeviceEx failed, result = 0x%08lX"), hr);
}
D3D9Context::~D3D9Context()
{

}

D3D9Texture* D3D9Context::CreateTextureFromBuffer(unsigned int width, unsigned int height, D3DFORMAT format, void *buffer, size_t bufSize)
{
	HRESULT hr;
	D3D9Texture *texture = new D3D9Texture(width, height);
	void *pMap;
	INT pitch;

	hr = pD3D9Device->CreateOffscreenPlainSurface(width, height, format, D3DPOOL_DEFAULT, &texture->surface, NULL);
	texture->Map(pMap, pitch);
	memcpy(pMap, buffer, bufSize);
	texture->Unmap();

	return texture;
}

D3D9Texture* D3D9Context::CreateTextureFromRawFile(unsigned int width, unsigned int height, D3DFORMAT format, CTSTR lpFile)
{
	HANDLE hFile = NULL;
	HANDLE hFileMap = NULL;
	DWORD fileSize = 0;
	char *fileContents = NULL;
	D3D9Texture *texture = NULL;

	hFile = CreateFile(lpFile, GENERIC_READ, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hFile == INVALID_HANDLE_VALUE)
		AppWarning(TEXT("CreateTextureFromRawFile: Couldn't open file"));
	hFileMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
	fileContents = (char*)MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 0);
	fileSize = GetFileSize(hFile, NULL);
	
	texture = CreateTextureFromBuffer(width, height, format, fileContents, fileSize);

	CloseHandle(hFileMap);
	CloseHandle(hFile);

	return texture;
}

D3D9Texture* D3D9Context::CreateTextureFromFile(CTSTR lpFile)
{
	HRESULT hr;
	D3DSURFACE_DESC desc;
	D3D9Texture *texture = new D3D9Texture();

	if FAILED(hr = D3DXCreateTextureFromFileEx(pD3D9Device, lpFile, D3DX_DEFAULT_NONPOW2, D3DX_DEFAULT_NONPOW2, 1, 0, D3DFMT_UNKNOWN, D3DPOOL_DEFAULT, D3DX_FILTER_POINT, D3DX_FILTER_POINT, 0, NULL, NULL, &texture->texture))
		AppWarning(TEXT("CreateDX9TextureFromFile: D3DXCreateTextureFromFile failed, result = 0x%08lX"), hr);

	if FAILED(hr = texture->texture->GetSurfaceLevel(0, &texture->surface))
		AppWarning(TEXT("CreateDX9TextureFromFile: GetSurfaceLevel failed, result = 0x%08lX"), hr);

	if FAILED(hr = texture->surface->GetDesc(&desc))
		AppWarning(TEXT("CreateDX9TextureFromFile: GetDesc failed, result = 0x%08lX"), hr);
	texture->width = desc.Width;
	texture->height = desc.Height;

	return texture;
}

D3D9SharedTexture* D3D9Context::CreateSharedTexture(unsigned int width, unsigned int height)
{
	HRESULT hr;
	HANDLE sharedHandle = NULL;
	D3D9SharedTexture *sharedTexture = new D3D9SharedTexture(width, height);

	if FAILED(hr = pD3D9Device->CreateRenderTarget(width, height, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &sharedTexture->surface, &sharedHandle))
		 AppWarning(TEXT("CreateDX9SharedTexture: CreateRenderTarget failed, result = 0x%08lX"), hr);

	Texture* GSTexture = GS->CreateTextureFromSharedHandle(width, height, sharedHandle);
	sharedTexture->GSTexture = GSTexture;

	return sharedTexture;
}

void D3D9Context::BlitTexture(D3D9Texture *source, D3D9Texture *destination)
{
	HRESULT hr;
	if FAILED(hr = pD3D9Device->StretchRect(source->surface, NULL, destination->surface, NULL, D3DTEXF_NONE))
		AppWarning(TEXT("BlitTexture: StretchRect failed, result = 0x%08lX"), hr);
}

void D3D9Context::Flush()
{
	pD3D9Device->Present(NULL, NULL, NULL, NULL);
}

D3D9Texture::D3D9Texture(unsigned int width, unsigned int height)
	: width(width), height(height), texture(NULL), surface(NULL)
{

}

D3D9Texture::D3D9Texture()
	: texture(NULL), surface(NULL)
{

}

D3D9Texture::~D3D9Texture()
{

}

D3D9SharedTexture::D3D9SharedTexture(unsigned int width, unsigned int height)
	: D3D9Texture(width, height)
{

}