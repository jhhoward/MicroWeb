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

#ifndef _HP95LX_H_
#define _HP95LX_H_

#include "../Platform.h"

struct Image;

class HP95LXVideoDriver : public VideoDriver
{
public:
	HP95LXVideoDriver();

	virtual void Init();
	virtual void Shutdown();
	virtual void ScaleImageDimensions(int& width, int& height);

private:
	int GetScreenMode();
	void SetScreenMode(int screenMode);

	int startingScreenMode;
};

#endif
