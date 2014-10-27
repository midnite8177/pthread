all : x86 x64
.PHONY : x86 x64

SRC := ../..
CFLAGS := -O3 -DNDEBUG -Wall -Werror

x86 : x86/pthread.dll
x64 : x64/pthread.dll

x86/pthread.dll : $(SRC)/pthread.c $(SRC)/pthread.h
	if not exist x86 mkdir x86
	gcc -m32 -shared $(CFLAGS) $< -o $@

x64/pthread.dll : $(SRC)/pthread.c $(SRC)/pthread.h
	if not exist x64 mkdir x64
	gcc -m64 -shared $(CFLAGS) $< -o $@

clean : 
	if exist x86\pthread.dll del x86\pthread.dll
	if exist x64\pthread.dll del x64\pthread.dll
