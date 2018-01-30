ifeq ($(ROOT_DIR),)
$(error Do not use this file directly to build)
endif

# 把当前目录加到头文件目录集合中
ALL_SRC_FILES += $(wildcard $(ROOT_DIR)/src/misc/*.c)
#宏定义
DEFINES += LOG_USE_COLOR