ifeq ($(ROOT_DIR),)
$(error Do not use this file directly to build)
endif

ALL_INC_PATHS += $(ROOT_DIR)/test
ALL_SRC_FILES += $(wildcard $(ROOT_DIR)/test/*.c)
