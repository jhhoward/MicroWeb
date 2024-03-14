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

#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include <stdlib.h>
#include "Font.h"
#include "Colour.h"
#include "Cursor.h"

struct Image;
class DrawSurface;
struct VideoModeInfo;

class VideoDriver
{
public:
	virtual void Init(VideoModeInfo* videoMode) = 0;
	virtual void Shutdown() = 0;

	virtual void ScaleImageDimensions(int& width, int& height) {}

	void InvertVideoOutput();

	int screenWidth;
	int screenHeight;

	DrawSurface* drawSurface;
	ColourScheme colourScheme;
	uint8_t* paletteLUT;
};

typedef uint8_t NetworkAddress[4];   // An IPv4 address is 4 bytes
class HTTPRequest;

class NetworkTCPSocket
{
public:
	virtual int Send(uint8_t* data, int length) = 0;
	virtual int Receive(uint8_t* buffer, int length) = 0;
	virtual int Connect(NetworkAddress address, int port) = 0;
	virtual bool IsConnectComplete() = 0;
	virtual bool IsClosed() = 0;
	virtual void Close() = 0;
};

class NetworkDriver
{
public:
	virtual void Init() {}
	virtual void Shutdown() {}
	virtual void Update() {}

	virtual bool IsConnected() { return false; }

	// Returns zero on success, negative number is error
	virtual int ResolveAddress(const char* name, NetworkAddress address, bool sendRequest) { return false; }

	virtual NetworkTCPSocket* CreateSocket() { return NULL; }
	virtual void DestroySocket(NetworkTCPSocket* socket) {}

	virtual HTTPRequest* CreateRequest(char* url) { return NULL; }
	virtual void DestroyRequest(HTTPRequest* request) {}
};

typedef uint16_t InputButtonCode;

class InputDriver
{
public:
	virtual void Init() {}
	virtual void Shutdown() {}
	virtual void Update() {}

	virtual void HideMouse() = 0;
	virtual void ShowMouse() = 0;
	virtual void SetMouseCursor(MouseCursor::Type type) = 0;
	virtual void GetMouseStatus(int& buttons, int& x, int& y) = 0;
	virtual void SetMousePosition(int x, int y) = 0;
	virtual InputButtonCode GetKeyPress() { return 0; }
};

class Platform
{
public:
	static bool Init(int argc, char* argv[]);
	static void Shutdown();
	static void Update();

	static VideoDriver* video;
	static NetworkDriver* network;
	static InputDriver* input;
};

#endif
