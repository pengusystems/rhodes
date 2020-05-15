
CC := g++
PLX_OPT := $(PLX_OPT) -DPCI_CODE -DPLX_LITTLE_ENDIAN -DPLX_LINUX
# DEBUG := 1
ifdef DEBUG
   CC_OPT := $(CC_OPT) -g3
   PLX_OPT := $(PLX_OPT) -DPLX_DEBUG
else
   CC_OPT := $(CC_OPT) -O2
endif
INC_DIR := -I../../../include
CFLAGS := $(CFLAGS) -c -Wall $(CC_OPT) $(PLX_OPT) $(INC_DIR)
LIBS := $(LIBS) -lATSApi -L/usr/local/AlazarTech/lib

PROG = AcqToDisk
OBJS = AcqToDisk.o

$(PROG) : $(OBJS)
	$(CC) -o $(PROG) $(LIBS) $(OBJS)

AcqToDisk.o : AcqToDisk.cpp
	$(CC) -c $(CFLAGS) AcqToDisk.cpp

clean :
	rm -f $(PROG) *.o *~