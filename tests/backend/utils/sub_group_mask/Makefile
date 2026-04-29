PROJECT_PATH ?= $(realpath ../../../..)

PROJECT = sync

SRCS = main.cpp

LDFLAGS = -lOpenCL

LD_PROJECT_FLAGS ?=

OPTS ?= 

OBJS := $(addsuffix .o, $(notdir $(SRCS)))

KERNELS_CL = $(wildcard *.cl)
C_OBJS = $(KERNELS_CL:.cl=.o.c)

INCLUDES = -I$(PROJECT_PATH) -I$(PROJECT_PATH)/src

%.o: %.cl
	$(PROJECT_PATH)/clc $< $@ $(INCLUDES) -D__COMPILER_RELATIVE_PATH__ $(CL_FLAGS) $(OPT_FLAGS)

%.o.c: %.o
	xxd -i $< > $@

%.cpp.o: %.cpp $(C_OBJS) 
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(PROJECT): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LD_PROJECT_FLAGS) -o $@

clean:
	rm -f *.o *.o.c $(PROJECT)

run: $(PROJECT)
	./$(PROJECT)