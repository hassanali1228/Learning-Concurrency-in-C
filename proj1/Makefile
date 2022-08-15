# ECE252 Lab1 Makefile
# Harsh Gandhi & Hassan Ali Amjad, 2022/05/23

CC=gcc
CFLAGS=-Wall -g
LD=gcc
LDFLAGS=-g
LDLIBS = -lz

OBJS1=pnginfo.o
OBJS2=findpng.o
OBJS3=catpng.o

all: pnginfo findpng catpng

pnginfo: $(OBJS1) 
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS)

findpng: $(OBJS2) 
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS)

catpng: $(OBJS3) 
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) 

.c.o:
	$(CC) $(CFLAGS) -c $<

%.d: %.c
	gcc -MM -MF $@ $<  


.PHONY: clean
clean:
	rm -f *.d *.o *.out pnginfo findpng catpng
