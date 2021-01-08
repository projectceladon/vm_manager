################################################
# Copyright (c) 2020 Intel Corporation.
# All rights reserved.
#
# SPDX-License-Identidfier: Apache-2.0
#
################################################
SRCS := vm_manager.c
SRCS += utils.c
SRCS += guest/guest.c
SRCS += guest/start.c
SRCS += guest/flash.c
SRCS += guest/tui.c
SRCS += guest/qmp.c
SRCS += guest/rpmb.c
SRCS += guest/vtpm.c
SRCS += guest/aaf.c

INCLUDE = -I./include -I./include/guest

INCLUDE += -I/usr/include \
	   -I/usr/include/glib-2.0 \
	   -I/usr/lib/x86_64-linux-gnu/glib-2.0/include \
	   -I/usr/include/json-c

CFLAGS = $(INCLUDE)
CFLAGS += -g -O2 -std=gnu99
CFLAGS += -Wall -Wextra -Wbad-function-cast \
	  -Winit-self -Wswitch-default \
	  -Wredundant-decls -Wnested-externs -Winline -Wstack-protector \
	  -Woverlength-strings -Waggregate-return \
	  -Wmissing-field-initializers -Wpointer-arith -Wcast-qual \
	  -Wcast-align -Wwrite-strings \
	  -fno-delete-null-pointer-checks -fstack-protector-strong \
	  -D_FORTIFY_SOURCE=2 \
	  -fno-strict-overflow -fwrapv

LDFLAGS = -lglib-2.0 -luuid -lmenu -lform -lncurses -ljson-c

BUILD_DIR ?= $(CURDIR)
OBJDIR ?= $(BUILD_DIR)/obj

OBJS := $(patsubst %.c,$(OBJDIR)/%.o,$(SRCS))

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

PROG_NAME := vm-manager
TARGET := $(BUILD_DIR)/$(PROG_NAME)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) $(TARGET)
	$(RM) -r $(OBJDIR)

install:
	install -d $(DESTDIR)$(prefix)/bin/
	install -t $(DESTDIR)$(prefix)/bin/ $(TARGET)

uninstall:
	rm -f $(DESTDIR)$(prefix)/bin/$(PROG_NAME)

all: vm-manager
