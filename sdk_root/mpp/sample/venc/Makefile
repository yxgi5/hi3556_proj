# Hisilicon Hi35xx sample Makefile

include ../Makefile.param
CFLAGS += -L../common/lib -lmp4v2
SRCS := $(wildcard *.c) 
#TARGET := $(SRCS:%.c=%)
TARGET = sample_$(notdir $(CURDIR))

# compile linux or HuaweiLite
include $(PWD)/../$(OSTYPE).mak
