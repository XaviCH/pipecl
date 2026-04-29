# only include if is one directory deeep
PROJECT_PATH = $(realpath ../../..)

TEST_PATH 	?= $(PROJECT_PATH)/tests

CXXFLAGS += -std=c++11 -Wall -Wextra -Wfatal-errors
CXXFLAGS += -Wno-deprecated-declarations -Wno-unused-parameter -Wno-narrowing
CXXFLAGS += -I$(PROJECT_PATH)/include
CXXFLAGS += -I$(PROJECT_PATH)/src
CXXFLAGS += -I$(PROJECT_PATH)

LDFLAGS += 	$(TEST_PATH)/apps/parameters.so

# driver selection logic

driver ?= opencl

ifeq ($(driver), opencl)
	CXXFLAGS += -DC_OPENCL_HOST
	LDFLAGS += $(PROJECT_PATH)/libGLSC2.so $(PROJECT_PATH)/libEGL.so 
else
ifeq ($(driver), gles)
	CXXFLAGS += -DC_OPENGL_HOST
	LDFLAGS += -lGLESv2 -lEGL
else
	ERROR_MSG = ERROR: driver=$(driver) is not a valid driver, driver=[opencl|gles]
-include error
endif
endif

# Debugigng
ifdef DEBUG
	CXXFLAGS += -g -O0
else    
	CXXFLAGS += -O3 -DNDEBUG
endif

GLSLC = $(PROJECT_PATH)/glslc

OBJS := $(addsuffix .o, $(notdir $(SRCS)))
CSHADERS := $(addsuffix .o, $(notdir $(SHADERS)))

all: $(PROJECT) $(OBJS)
 
# build objects

%.cl.o: %.cl
	$(GLSLC) $< $@

%.cc.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.cpp.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.c.o: %.c
	$(CC) $(CXXFLAGS) -c $< -o $@

# build executable

$(PROJECT): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) $(LD_PROJECT_FLAGS) -o $@

# commands

run: parameters $(PROJECT) $(CSHADERS)
	./$(PROJECT) $(OPTS)

display:
	feh -Z -F --force-aliasing -Y image.ppm

clean:
	rm -rf $(PROJECT) *.o .depend

clean-all: clean
	rm -rf *.dump *.pocl *.ocl

ifneq ($(width),)
PARAMETERS_CFLAGS += -DWIDTH=$(width)
endif

ifneq ($(height),)
PARAMETERS_CFLAGS += -DHEIGHT=$(height)
endif

ifneq ($(size),)
PARAMETERS_CFLAGS += -DSIZE=$(size)
endif

ifneq ($(offline),)
PARAMETERS_CFLAGS += -DOFFLINE=$(offline)
endif

parameters:
	$(MAKE) -C $(TEST_PATH)/apps parameters.so CFLAGS="$(PARAMETERS_CFLAGS)"

error:
	$(error $(ERROR_MSG))