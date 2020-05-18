ifeq ($(ROOT_DIR),)
$(error Do not use this file directly to build)
endif

# 把当前目录加到头文件目录集合中
SMARTOCD_SRC_FILES += $(wildcard $(ROOT_DIR)/src/api/*.c)
SMARTOCD_SRC_FILES += $(wildcard $(ROOT_DIR)/src/api/adapter/*.c)
SMARTOCD_SRC_FILES += $(wildcard $(ROOT_DIR)/src/api/arch/ARM/ADI/*.c)