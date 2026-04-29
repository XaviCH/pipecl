PROJECT = composition

SRCS = main.cc

LD_PROJECT_FLAGS ?= 

OPTS ?=

include ../common.mk

ifdef HOSTGPU
run: clean run-hostgpu
	feh -Z -F --force-aliasing -Y image.ppm
else ifdef HOSTDRIVER
run: clean run-hostdriver
	feh -Z -F --force-aliasing -Y image.ppm
else
run: clean run-simx
	feh -Z -F --force-aliasing -Y image.ppm
endif