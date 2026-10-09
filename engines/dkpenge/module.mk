MODULE := engines/dkpenge

MODULE_OBJS = \
	ani.o \
	dkpenge.o \
	collage.o \
	database.o \
	metaengine.o \
	page.o \
	quest.o \
	quiz.o \
	resources.o \
	vm.o

# This module can be built as a plugin
ifeq ($(ENABLE_DKPENGE), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

# Include common rules
include $(srcdir)/rules.mk

# Detection objects
DETECT_OBJS += $(MODULE)/detection.o
