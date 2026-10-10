MODULE := engines/flaaklypa

MODULE_OBJS = \
	anim.o \
	archive.o \
	audiopairs.o \
	beemaze.o \
	bugzzz.o \
	buildabike.o \
	butterfly.o \
	console.o \
	cursor.o \
	dialog.o \
	flaaklypa.o \
	font.o \
	garage.o \
	gem3d.o \
	hopscotch.o \
	house.o \
	hustle.o \
	intent.o \
	lettersort.o \
	menu.o \
	metaengine.o \
	music.o \
	pipeline.o \
	puzzle.o \
	puzzledata.o \
	resources.o \
	scene.o \
	scenedata.o \
	sockdrawer.o \
	sound.o \
	textinvader.o \
	whackamole.o \
	wheelbarrow.o \
	yard.o

# This module can be built as a plugin
ifeq ($(ENABLE_FLAAKLYPA), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

# Include common rules
include $(srcdir)/rules.mk

# Detection objects
DETECT_OBJS += $(MODULE)/detection.o
