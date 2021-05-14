
all : clean ptach.exe

clean : .symbolic
  @del ptach.exe
  @del *.obj
  @del *.map

ptach.exe : patch.cpp
  wcl386 patch.cpp
  mv patch.exe ptach.exe
  chmod 755 ptach.exe
