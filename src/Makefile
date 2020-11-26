#
# Author: Virus.V
# Date: 2020-05-15 16:33:34
# LastEditors: Virus.V
# LastEditTime: 2020-05-29 10:02:11
# Description: file content
# Email: virusv@live.com
#

# SmartOCD入口以及初始化
DIR_OBJ_FILES = $(CURR_DIR)/smartocd.o

include $(ROOT_DIR)/common.mk

MODULE_PATH = Library Component Adapter
MODULE_LIST = $(foreach var,$(MODULE_PATH),$(CURR_DIR)/$(var)/lib$(var).a)

.PRECIOUS : $(DIR_OBJ_FILES)

all: $(DIR_OBJ_FILES) $(MODULE_LIST)
	$(CC) $(DIR_OBJ_FILES) -Wl,--whole-archive $(MODULE_LIST) -Wl,--no-whole-archive -o smartocd $(LDFLAGS)
	@echo "Build Complete!"

$(MODULE_LIST): modules

modules:
	@echo "Build modules..."
	$(foreach var,$(MODULE_PATH),\
		echo "building $(var) module.."; \
		$(MAKE) -C $(var) all;	\
	)
	@echo "Modules build finished..."

clean:
	$(RM) $(DIR_OBJ_FILES)
	$(foreach var,$(MODULE_PATH),\
		echo "Clean $(var) module.."; \
		$(MAKE) -C $(var) clean;	\
	)

.PHONY: all clean $(MODULE_LIST) modules

