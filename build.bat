set path=%path%;c:\mingw\bin
del basic.exe
gcc.exe -Wno-unused-parameter -Wno-parentheses -Wno-unused-result -Wno-deprecated-declarations -O2 -IC:\sdl\include\SDL2 -c basic.c -o basic.o -DSDL
gcc.exe -LC:\sdl\lib -o basic.exe basic.o  -lmingw32 -lSDL2main -lSDL2 -s 
pause