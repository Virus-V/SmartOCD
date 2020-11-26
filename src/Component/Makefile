#
# Author: Virus.V
# Date: 2020-05-18 17:20:36
# LastEditors: Virus.V
# LastEditTime: 2020-05-18 19:01:57
# Description: file content
# Email: virusv@live.com
#

# 构建Component

DIR_SRC_FILES = $(wildcard $(CURR_DIR)/*.c)	\
	$(wildcard $(CURR_DIR)/adapter/*.c)	\
	$(wildcard $(CURR_DIR)/ADI/*.c)

DIR_OBJ_FILES = ${patsubst %.c,%.o,$(DIR_SRC_FILES)}

include $(ROOT_DIR)/common.mk

all: $(CURR_DIR)/libComponent.a

$(CURR_DIR)/libComponent.a: $(DIR_OBJ_FILES)
	$(AR) rcs $@ $^

clean:
	$(RM) $(DIR_OBJ_FILES)
	$(RM) $(CURR_DIR)/libComponent.a

.PHONY: all clean

