ifeq ($(ROOT_DIR),)
$(error Do not use this file directly to build)
endif

# 把当前目录加到头文件目录集合中
SMARTOCD_SRC_FILES += $(wildcard $(ROOT_DIR)/src/debugger/*.c)
