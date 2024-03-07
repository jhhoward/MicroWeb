bin = MicroWeb.exe
SRC_PATH = ..\..\src
OBJDIR=obj
objects = MicroWeb.obj App.obj Parser.obj Tags.obj Platform.obj Colour.obj Hercules.obj BIOSVid.obj VidModes.obj Font.obj Interface.obj DOSInput.obj DOSNet.obj Page.obj Layout.obj Node.obj Text.obj Table.obj Section.obj ImgNode.obj Block.obj StyNode.obj LinkNode.obj Break.obj Render.obj Button.obj Field.obj DataPack.obj Surf1bpp.obj Surf4bpp.obj Surf8bpp.obj Form.obj Status.obj Scroll.obj HTTP.obj Decoder.obj Gif.obj MemBlock.obj Memory.obj EMS.obj
memory_model = -ml
CC = wpp
CFLAGS = -zq -0 -ot -bt=DOS -w2 $(memory_model) -fi=$(SRC_PATH)\Defines.h
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

Render.obj: $(SRC_PATH)\Render.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Tags.obj: $(SRC_PATH)\Tags.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Page.obj: $(SRC_PATH)\Page.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Platform.obj: $(SRC_PATH)\DOS\Platform.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

HTTP.obj: $(SRC_PATH)\HTTP.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Font.obj: $(SRC_PATH)\Font.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Interface.obj: $(SRC_PATH)\Interface.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Layout.obj: $(SRC_PATH)\Layout.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Node.obj: $(SRC_PATH)\Node.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

ImgNode.obj: $(SRC_PATH)\Nodes\ImgNode.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

StyNode.obj: $(SRC_PATH)\Nodes\StyNode.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

LinkNode.obj: $(SRC_PATH)\Nodes\LinkNode.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Block.obj: $(SRC_PATH)\Nodes\Block.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Break.obj: $(SRC_PATH)\Nodes\Break.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Section.obj: $(SRC_PATH)\Nodes\Section.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Text.obj: $(SRC_PATH)\Nodes\Text.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Form.obj: $(SRC_PATH)\Nodes\Form.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Status.obj: $(SRC_PATH)\Nodes\Status.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Scroll.obj: $(SRC_PATH)\Nodes\Scroll.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Field.obj: $(SRC_PATH)\Nodes\Field.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Table.obj: $(SRC_PATH)\Nodes\Table.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Button.obj: $(SRC_PATH)\Nodes\Button.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

DataPack.obj: $(SRC_PATH)\DataPack.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Decoder.obj: $(SRC_PATH)\Image\Decoder.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Gif.obj: $(SRC_PATH)\Image\Gif.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

MemBlock.obj: $(SRC_PATH)\Memory\MemBlock.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Memory.obj: $(SRC_PATH)\Memory\Memory.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

EMS.obj: $(SRC_PATH)\DOS\EMS.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

VidModes.obj: $(SRC_PATH)\VidModes.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Colour.obj: $(SRC_PATH)\Colour.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

BIOSVid.obj: $(SRC_PATH)\DOS\BIOSVid.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Hercules.obj: $(SRC_PATH)\DOS\Hercules.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

DOSInput.obj: $(SRC_PATH)\DOS\DOSInput.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

DOSNet.obj: $(SRC_PATH)\DOS\DOSNet.cpp
	 $(CC) -fo=$@ $(CFLAGS) -i=$(tcp_h_dir) -DCFG_H="tcp.cfg" $<

Surf1bpp.obj: $(SRC_PATH)\Draw\Surf1bpp.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Surf4bpp.obj: $(SRC_PATH)\DOS\Surf4bpp.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

Surf8bpp.obj: $(SRC_PATH)\Draw\Surf8bpp.cpp
	 $(CC) -fo=$@ $(CFLAGS) $<

clean: .symbolic
    del *.obj
    del $(bin)