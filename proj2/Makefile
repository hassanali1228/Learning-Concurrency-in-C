
# Makefile, ECE252  
# Yiqing Huang 

CC = gcc
CFLAGS = -Wall -g -std=gnu99 
LD = gcc
LDFLAGS = -g -std=gnu99 
LDLIBS = -pthread -lcurl -lz

SRCS   = paster.c pastersingle.c
OBJS1  = paster.o
OBJS2  = pastersingle.o
TARGETS= paster pastersingle

all: ${TARGETS}

paster: $(OBJS1) 
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) 

pastersingle: $(OBJS2)
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS)

%.o: %.c 
	$(CC) $(CFLAGS) -c $< 

%.d: %.c
	gcc -MM -MF $@ $<

.PHONY: clean
clean:
	rm -f *~ *.d *.o *.out paster pastersingle $(TARGETS) 
