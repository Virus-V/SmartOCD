#
# Author: Virus.V
# Date: 2020-05-18 12:29:29
# LastEditors: Virus.V
# LastEditTime: 2020-05-18 18:50:58
# Description: file content
# Email: virusv@live.com
#

# 构建Adapter

DIR_OBJ_FILES = $(CURR_DIR)/adapter.o \
								$(CURR_DIR)/cmsis-dap/cmsis-dap.o	\
								$(CURR_DIR)/ftdi/ftdi.o	\
								
include $(ROOT_DIR)/common.mk

all: $(CURR_DIR)/libAdapter.a

$(CURR_DIR)/libAdapter.a: $(DIR_OBJ_FILES)
	$(AR) rcs $@ $^

clean:
	$(RM) $(CURR_DIR)/libAdapter.a
	$(RM) $(DIR_OBJ_FILES)

.PHONY: all clean

