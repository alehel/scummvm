MODULE := engines/flaaklypa

MODULE_OBJS = \
	anim.o \
	archive.o \
	console.o \
	cursor.o \
	flaaklypa.o \
	font.o \
	gem3d.o \
	menu.o \
	metaengine.o \
	music.o \
	puzzle.o \
	puzzledata.o \
	resources.o \
	scene.o \
	scenedata.o \
	sockdrawer.o \
	textinvader.o \
	yard.o

# This module can be built as a plugin
ifeq ($(ENABLE_FLAAKLYPA), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

# Include common rules
include $(srcdir)/rules.mk

# Detection objects
DETECT_OBJS += $(MODULE)/detection.o
