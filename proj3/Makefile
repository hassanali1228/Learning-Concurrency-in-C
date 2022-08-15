# Makefile, ECE252  
# Yiqing Huang 

CC = gcc
CFLAGS = -Wall -g -std=gnu99 
LD = gcc
LDFLAGS = -g -std=gnu99
LDLIBS = -lz -lcurl -lpthread

LIB_UTIL = zutil.o crc.o shm_stack.o
SRCS   = paster2.c crc.c zutil.c shm_stack.c
OBJS_PASTER2  = paster2.o $(LIB_UTIL)
TARGETS= paster2

all: ${TARGETS}

paster2: $(OBJS_PASTER2) 
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS)

%.o: %.c 
	$(CC) $(CFLAGS) -c $< 

%.d: %.c
	gcc -MM -MF $@ $<

-include $(SRCS:.c=.d)

.PHONY: clean
clean:
	rm -f *~ *.d *.o *.out $(TARGETS)
