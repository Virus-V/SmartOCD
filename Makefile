#
# Author: Virus.V
# Date: 2020-05-19 08:58:05
# LastEditors: Virus.V
# LastEditTime: 2020-05-25 19:02:18
# Description: file content
# Email: virusv@live.com
#


ROOT_DIR := $(shell pwd)
# 版本号
VERSION := 0.2.0
COMPILE_TIME := $(shell date +%FT%T%z)

# 额外库文件
EXTRA_LIBS = usb-1.0 m dl

# 定义当前版本和构建时间
DEFINES += VERSION=\"$(VERSION)\" COMPILE_TIME=\"$(COMPILE_TIME)\"

# Optimization level [0,1,2,3,s]
OPT = 0
#DEBUG =
DEBUG = -ggdb

include config.mk

# parse config
# ...

CFLAGS = $(DEBUG)
#开发过程中使用-Werror，所有警告都当做错误来终止编译
CFLAGS += -O$(OPT) -Werror
CFLAGS += -I$(ROOT_DIR)/src
CFLAGS += $(addprefix -D,$(DEFINES))
# 所有库
LDFLAGS += $(addprefix -l,$(EXTRA_LIBS))

export ROOT_DIR CFLAGS LDFLAGS

all:
	$(MAKE) -C src $@

%_test: all $(ROOT_DIR)/test/%_test.o
	$(MAKE) -C test $@

clean:
	$(MAKE) -C src clean
	$(MAKE) -C test clean

.PHONY: all clean %_test