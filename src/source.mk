# SmartOCD源码文件
ifeq ($(ROOT_DIR),)
$(error Do not use this file directly to build)
endif

include $(ROOT_DIR)/src/arch/arch.mk
include $(ROOT_DIR)/src/debugger/debugger.mk
include $(ROOT_DIR)/src/lib/lib.mk
include $(ROOT_DIR)/src/misc/misc.mk
#include $(ROOT_DIR)/src/layer/layer.mk
# 把main函数所在的文件踢出来，防止与单元测试的main函数冲突
SMARTOCD_SRC_FILES += $(filter-out $(ROOT_DIR)/src/smart_ocd.c, $(wildcard $(ROOT_DIR)/src/*.c))
