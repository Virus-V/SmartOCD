#
# Author: Virus.V
# Date: 2020-05-15 16:06:50
# LastEditors: Virus.V
# LastEditTime: 2020-05-18 19:41:42
# Description: file content
# Email: virusv@live.com
#

# 构建Library

DIR_SRC_FILES = $(wildcard $(CURR_DIR)/jtag/*.c)	\
	$(wildcard $(CURR_DIR)/linenoise/*.c)	\
	$(wildcard $(CURR_DIR)/log/*.c)	\
	$(wildcard $(CURR_DIR)/usb/*.c)	\
	$(wildcard $(CURR_DIR)/lua_api/*.c)	\
	$(wildcard $(CURR_DIR)/misc/*.c)

DIR_OBJ_FILES = ${patsubst %.c,%.o,$(DIR_SRC_FILES)}

include $(ROOT_DIR)/common.mk

all: $(CURR_DIR)/libLibrary.a

$(CURR_DIR)/libLibrary.a: lua $(DIR_OBJ_FILES)
	mkdir obj.$$$$;	\
	cd obj.$$$$;	\
	$(AR) x $(CURR_DIR)/lua/src/liblua.a;	\
	cd .. ;\
	$(AR) rcs $@ $(DIR_OBJ_FILES) obj.$$$$/*.o;	\
	$(RM) -rf obj.$$$$	\

lua:
	$(MAKE) -C $(CURR_DIR)/lua linux ALL=a

clean:
	$(RM) $(DIR_OBJ_FILES)
	$(RM) $(CURR_DIR)/libLibrary.a
	$(MAKE) -C $(CURR_DIR)/lua clean

.PHONY: all clean lua

