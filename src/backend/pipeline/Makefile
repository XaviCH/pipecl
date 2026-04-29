PROJECT_PATH = $(realpath ../../..)
SRC_PATH = $(PROJECT_PATH)/src 

UTILS_CL = $(wildcard ../utils/*.cl)
BACKEND_CL = $(wildcard ../*.cl)
HEADER_FILES = $(wildcard ../../*.h)
KERNELS_CL = $(wildcard *.cl)
KERNELS_O_C = $(KERNELS_CL:.cl=.o.c)

CL_FLAGS += -D__COMPILER_RELATIVE_PATH__ -I$(SRC_PATH) # -Werror -cl-nv-verbose

.PHONY: all clean clean-all

CLC = $(PROJECT_PATH)/clc 

%.o: %.cl $(UTILS_CL) $(BACKEND_CL) $(HEADER_FILES)
	$(CLC) $< $@ -DC_OPENCL_HOST $(CL_FLAGS) $(OPT_FLAGS)

%.o.c: %.o
	xxd -i $< > $@

all: $(KERNELS_O_C)

clean:
	rm -f *.o

clean-all: clean
	rm  -f *.o.c
