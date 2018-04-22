ifeq ($(ROOT_DIR),)
$(error Do not use this file directly to build)
endif
# 选择编译目录
_SUB_DIRS := ARM/ADI
SMARTOCD_SRC_FILES := $(foreach _sub_dir,$(_SUB_DIRS),$(wildcard $(ROOT_DIR)/src/arch/$(_sub_dir)/*.c))

