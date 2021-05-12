bin = MicroWeb.exe
SRC_PATH = ..\..\src
OBJDIR=obj
objects = MicroWeb.obj Parser.obj Renderer.obj Tags.obj Platform.obj CGA.obj Font.obj DOSMouse.obj Page.obj

CC = wpp
CFLAGS = -zq -0 -ot -bt=DOS -w2 -mc
LD = wlink

$(bin): $(objects)
    $(LD) system dos name $@ file { $(objects) }


MicroWeb.obj: $(SRC_PATH)\MicroWeb.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Parser.obj: $(SRC_PATH)\Parser.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Renderer.obj: $(SRC_PATH)\Renderer.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Tags.obj: $(SRC_PATH)\Tags.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Page.obj: $(SRC_PATH)\Page.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Platform.obj: $(SRC_PATH)\DOS\Platform.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Font.obj: $(SRC_PATH)\Font.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

CGA.obj: $(SRC_PATH)\DOS\CGA.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

DOSMouse.obj: $(SRC_PATH)\DOS\DOSMouse.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

clean: .symbolic
    del *.obj
    del $(bin)