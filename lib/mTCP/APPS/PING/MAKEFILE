#
# Ping makefile
#
#
# Possible optimizations for 8088 class processors
#
# -oa   Relax alias checking
# -ob   Try to generate straight line code
# -oe - expand user functions inline (-oe=20 is default)
# -oh   Enable repeated optimizations
# -oi   generate certain lib funcs inline
# -oi+  Set max inline depth (C++ only, use -oi for C)
# -ok   Flowing of register save into function flow graph
# -ol   loop optimizations
# -ol+  loop optimizations plus unrolling
# -or   Reorder for pipelined (486+ procs); not sure if good to use
# -os   Favor space over time
# -ot   Favor time over space
# -ei   Allocate an "int" for all enum types
# -zp2  Allow compiler to add padding to structs
# -zpw  Use with above; make sure you are warning free!
# -0    8088/8086 class code generation
# -s    disable stack overflow checking
# -zmf  put each function in a new code segment; helps with linking

# For this code performance is not an issue.  Make it small.

tcp_h_dir = ..\..\TCPINC\
tcp_c_dir = ..\..\TCPLIB\
common_h_dir = ..\..\INCLUDE

memory_model = -ms
compile_options = -0 $(memory_model) -DCFG_H="ping.cfg" -s -oh -ok -os -oa -ei -zp2 -zpw -we
compile_options += -i=$(tcp_h_dir) -i=$(common_h_dir)

tcpobjs = packet.obj arp.obj eth.obj ip.obj utils.obj timer.obj ipasm.obj udp.obj dns.obj trace.obj
objs = ping.obj

all : clean ping.exe

clean : .symbolic
  @del ping.exe
  @del *.obj
  @del *.map

patch : .symbolic
  ..\..\utils\ptach ping.exe ping.map $(memory_model)

.asm : $(tcp_c_dir)

.cpp : $(tcp_c_dir)

.asm.obj :
  wasm -0 $(memory_model) $[*

.cpp.obj :
  wpp $[* $(compile_options)

ping.exe : $(tcpobjs) $(objs)
  wlink system dos option map option eliminate option stack=4096 name $@ file *.obj
