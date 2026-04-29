GLSC2_PATH = $(realpath ../..)

SRC_PATH = $(GLSC2_PATH)/src

INCLUDES = -I$(SRC_PATH) -I$(GLSC2_PATH)/include

CFLAGS = -fPIC -O3

# header files from ./  ../ ../middleware
HEADER_FILES = $(wildcard *.h) $(wildcard $(SRC_PATH)/*.h) $(wildcard $(SRC_PATH)/middleware/*.h)

# c files from . 
C_FILES = $(wildcard *.c)

# object files to be built
OBJECT_FILES = $(C_FILES:.c=.o)

.PHONY: all clean clean-all

%.o: %.c $(HEADER_FILES)
	$(CC) $(CFLAGS) -o $@ -c $< $(INCLUDES)

all: $(OBJECT_FILES)

clean:
	rm -f *.o

clean-all: clean