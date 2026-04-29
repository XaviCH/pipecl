PROJECT_PATH ?= $(realpath ../../../..)

SRC_PATH = $(GLSC_PATH)/src
TESTS_PATH = $(PROJECT_PATH)/tests

PROJECT = main

SRCS = main.cpp

LDFLAGS = -lOpenCL

LD_PROJECT_FLAGS ?=

OPTS ?= 

PIPELINE_PATH = $(SRC_PATH)/backend/pipeline

KERNEL_OBJ_FILES = $(wildcard $(PIPELINE_PATH)/*.o.c)

OBJS := $(addsuffix .o, $(notdir $(SRCS)))

INCLUDES = -I$(PROJECT_PATH) -I$(PROJECT_PATH)/src -I$(PROJECT_PATH)/include

%.o: %.cl
	$(PROJECT_PATH)/src/compiler/clc $< $@ $(INCLUDES) -D__COMPILER_RELATIVE_PATH__ $(CL_FLAGS) $(OPT_FLAGS)

%.o.c: %.o
	xxd -i $< > $@

%.cpp.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(PROJECT): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ $(KERNEL_OBJ_FILES) $(LDFLAGS) $(LD_PROJECT_FLAGS) -o $@

run: $(PROJECT)
	./$(PROJECT)

clean:
	rm *.o