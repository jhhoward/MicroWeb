//
// Copyright (C) 2021 James Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

#ifndef _EGA_H_
#define _EGA_H_

#include "../Platform.h"

struct Image;

class EGADriver : public VideoDriver
{
public:
	EGADriver();

	virtual void Init();
	virtual void Shutdown();

	virtual void ScaleImageDimensions(int& width, int& height);

private:
	int GetScreenMode();
	void SetScreenMode(int screenMode);

	int startingScreenMode;

protected:
	void SetupVars(const char* inAssetPackToUse, int inScreenMode, int inScreenHeight);

	int screenModeToUse;
	const char* assetPackToUse;
};

class VGADriver : public EGADriver
{
public:
	VGADriver() 
	{
		SetupVars("DEFAULT.DAT", 0x11, 480);
	}
	virtual void ScaleImageDimensions(int& width, int& height) {}
};

#endif
