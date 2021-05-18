bin = MicroWeb.exe
SRC_PATH = ..\..\src
OBJDIR=obj
objects = MicroWeb.obj App.obj Parser.obj Renderer.obj Tags.obj Platform.obj CGA.obj Font.obj DOSInput.obj DOSNet.obj Page.obj
memory_model = -ml
CC = wpp
CFLAGS = -zq -0 -ot -bt=DOS -w2 $(memory_model)
LD = wlink

# begin mTCP stuff
tcp_h_dir = ..\..\lib\mTCP\TCPINC\
tcp_c_dir = ..\..\lib\mTCP\TCPLIB\

tcpobjs = packet.obj arp.obj eth.obj ip.obj tcp.obj tcpsockm.obj udp.obj utils.obj dns.obj timer.obj ipasm.obj trace.obj

tcp_compile_options = -0 $(memory_model) -DCFG_H="tcp.cfg" -oh -ok -ot -s -oa -ei -zp2 -zpw -we -ob -ol+ -oi+
tcp_compile_options += -i=$(tcp_h_dir)

.cpp : $(tcp_c_dir)

.asm : $(tcp_c_dir)

.asm.obj :
  wasm -0 $(memory_model) $[*

.cpp.obj :
  wpp $[* $(tcp_compile_options)
# end mTCP stuff

$(bin): $(objects) $(tcpobjs)
    $(LD) system dos name $@ file { $(objects) $(tcpobjs) }


MicroWeb.obj: $(SRC_PATH)\MicroWeb.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

App.obj: $(SRC_PATH)\App.cpp
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

DOSInput.obj: $(SRC_PATH)\DOS\DOSInput.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

DOSNet.obj: $(SRC_PATH)\DOS\DOSNet.cpp
	 $(CC) -fo=$@ $(CFLAGS) -i=$(tcp_h_dir) -DCFG_H="tcp.cfg" $<

clean: .symbolic
    del *.obj
    del $(bin)