#
# Author: Virus.V
# Date: 2020-05-19 08:58:05
# LastEditors: Virus.V
# LastEditTime: 2020-05-29 10:21:35
# Description: file content
# Email: virusv@live.com
#


ROOT_DIR := $(shell pwd)
COMPILE_TIME := $(shell date +%FT%T%z)

# export all variable above
export

include common.mk

all:
	$(MAKE) -C src $@

%_test:
	$(MAKE) -C src modules
	$(MAKE) -C test $@

clean:
	$(MAKE) -C src clean
	$(MAKE) -C test clean

.PHONY: all clean %_test
