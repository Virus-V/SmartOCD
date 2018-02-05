# SmartOCD源码文件
ifeq ($(ROOT_DIR),)
$(error Do not use this file directly to build)
endif

include $(ROOT_DIR)/src/debugger/debugger.mk
include $(ROOT_DIR)/src/misc/misc.mk
# 编译主函数
ALL_SRC_FILES += $(wildcard $(ROOT_DIR)/src/*.c)
