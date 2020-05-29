#
# Author: Virus.V
# Date: 2020-05-15 16:33:34
# LastEditors: Virus.V
# LastEditTime: 2020-05-29 10:02:11
# Description: file content
# Email: virusv@live.com
#

CURR_DIR = $(shell pwd)
# SmartOCD入口以及初始化
DIR_OBJ_FILES = $(CURR_DIR)/smartocd.o

MODULE_PATH = Library Component Adapter
MODULE_LIST = $(foreach var,$(MODULE_PATH),$(CURR_DIR)/$(var)/lib$(var).a)

.PRECIOUS : $(DIR_OBJ_FILES)

all: $(DIR_OBJ_FILES) $(MODULE_LIST)
	$(CC) $(DIR_OBJ_FILES) -Wl,--whole-archive $(MODULE_LIST) -Wl,--no-whole-archive -o smartocd $(LDFLAGS)
	@echo "Build Complete!"

modules: $(MODULE_LIST)

clean:
	$(RM) $(DIR_OBJ_FILES)
	$(foreach var,$(MODULE_PATH),\
		echo "Clean $(var) module.."; \
		$(MAKE) -C $(var) clean;	\
	)

# 构建module
$(MODULE_LIST):
	@echo "Build modules..."
	$(foreach var,$(MODULE_PATH),\
		echo "building $(var) module.."; \
		$(MAKE) -C $(var);	\
	)
	@echo "Modules build finished..."

.PHONY: all clean $(MODULE_LIST) modules

include $(ROOT_DIR)/utils.mk
