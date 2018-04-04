ifeq ($(ROOT_DIR),)
$(error Do not use this file directly to build)
endif

# 把当前目录加到头文件目录集合中
_SUB_DIRS = list
SMARTOCD_SRC_FILES += $(foreach _sub_dir,$(_SUB_DIRS),$(wildcard $(ROOT_DIR)/src/misc/$(_sub_dir)/*.c))
SMARTOCD_SRC_FILES += $(wildcard $(ROOT_DIR)/src/misc/*.c)
#宏定义
DEFINES += LOG_USE_COLOR