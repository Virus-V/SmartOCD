ifeq ($(ROOT_DIR),)
$(error Do not use this file directly to build)
endif

TEST_INC_PATHS += $(ROOT_DIR)/test
# TEST_SRC_FILES += $(wildcard $(ROOT_DIR)/test/*.c)
TEST_SRC_FILES += $(ROOT_DIR)/test/misc.c
