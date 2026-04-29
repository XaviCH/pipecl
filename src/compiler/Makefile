CC = gcc

SRC_PATH = ../

INCLUDE_PATH = ../../include/

INSTALL_PATH = $(realpath ../..)

HEADER_FILES = $(wildcard *.h)

INCLUDES += -I$(SRC_PATH) -I$(INCLUDE_PATH)
LIBS += -lOpenCL

.PHONY: all clean clean-all

all: clc.o glslc.o

clc.o: clc.c $(HEADER_FILES)
	$(CC) -c $@ $< $(INCLUDES)

glslc.o: glslc.c $(HEADER_FILES)
	$(CC) -c $@ $< $(INCLUDES) -DINSTALL_PATH="$(INSTALL_PATH)"

clean:

clean-all: clean
	rm -f clc glslc