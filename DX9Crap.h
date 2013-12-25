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

#pragma once

#include "OBSApi.h"
#include "D3D9.h"
#include "D3D10.h"
#include "d3dx9.h"

// Meh, too lazy to design this properly

class D3D9Texture
{
public:
	D3D9Texture(unsigned int width, unsigned int height);
	D3D9Texture();
    ~D3D9Texture();
	void Map(void *lpData, INT pitch);
	void Unmap();
	DWORD Width() const;
	DWORD Height() const;

	LPDIRECT3DTEXTURE9		texture;
	LPDIRECT3DSURFACE9		surface;
	UINT					width, height;
};

class D3D9SharedTexture : public D3D9Texture
{
public:
	D3D9SharedTexture(unsigned int width, unsigned int height);

	Texture					*GSTexture;
};

class D3D9Context
{
public:
	D3D9Context();
	~D3D9Context();

	D3D9Texture				*CreateTexture(unsigned int width, unsigned int height, D3DFORMAT format);
	D3D9Texture				*CreateTextureFromFile(CTSTR lpFile);
	D3D9Texture				*CreateTextureFromBuffer(unsigned int width, unsigned int height, D3DFORMAT format, void *buffer, size_t bufSize);
	D3D9Texture				*CreateTextureFromRawFile(unsigned int width, unsigned int height, D3DFORMAT format, CTSTR lpFile);
	D3D9SharedTexture		*CreateSharedTexture(unsigned int width, unsigned int height);
	void BlitTexture(D3D9Texture *source, D3D9Texture *destination);
	void Flush();

	HWND					hWnd;
	LPDIRECT3D9EX			pD3D9;
	LPDIRECT3DDEVICE9EX		pD3D9Device;
};