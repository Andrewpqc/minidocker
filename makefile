C = gcc
CXX = g++
C_LIB = network.c nl.c
LINK = network.o nl.o minidocker.o
MAIN = main.cpp
LD = -std=c++11
OUT = minidocker

all:
	$(C) -c $(C_LIB)
	$(CXX) $(LD) -c minidocker.cpp 
	$(CXX) $(LD) -o $(OUT) $(MAIN) $(LINK)

clean:
	rm *.o $(OUT)

install:
	sudo cp ./minidocker /usr/local/bin/