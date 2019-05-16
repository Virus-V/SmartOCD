# SmartOCD源码文件
ifeq ($(ROOT_DIR),)
$(error Do not use this file directly to build)
endif

#include $(ROOT_DIR)/src/arch/arch.mk
#include $(ROOT_DIR)/src/debugger/debugger.mk

# Adapter
SMARTOCD_SRC_FILES += $(wildcard $(ROOT_DIR)/src/adapter/cmsis-dap/*.c)

# JTAG库
SMARTOCD_SRC_FILES += $(wildcard $(ROOT_DIR)/src/JTAG/*.c)

# USB库
SMARTOCD_SRC_FILES += $(wildcard $(ROOT_DIR)/src/USB/*.c)

include $(ROOT_DIR)/src/misc/misc.mk
include $(ROOT_DIR)/src/api/api.mk
# 把main函数所在的文件踢出来，防止与单元测试的main函数冲突
#SMARTOCD_SRC_FILES += $(filter-out $(ROOT_DIR)/src/smart_ocd.c, $(wildcard $(ROOT_DIR)/src/*.c))
