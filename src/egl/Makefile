GLSC2_PATH = $(realpath ../..)

INCLUDE = -I$(GLSC2_PATH)/include

all: egl.o

egl.o: egl.c
	$(CC) -o $@ -c $< -fPIC -O3 $(INCLUDE)

clean:
	rm -f *.o

clean-all: clean