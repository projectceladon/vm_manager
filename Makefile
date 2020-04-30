################################################
# Copyright (c) 2020 Intel Corporation.
# All rights reserved.
#
# SPDX-License-Identidfier: Apache-2.0
#
################################################

export CC ?= gcc
export LD ?= gcc

export BUILD_DIR ?= $(CURDIR)/build
export prefix ?= /usr

SOURCE_DIR=src

all:
	make -C $(SOURCE_DIR)

clean:
	make -C $(SOURCE_DIR) clean
	$(RM) -r $(BUILD_DIR)

install:
	make -C $(SOURCE_DIR) install

uninstall:
	make -C $(SOURCE_DIR) uninstall
