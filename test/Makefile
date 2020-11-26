#
# SOME TEST
#
#

DIR_OBJ_FILES = $(CURR_DIR)/entry.o $(CURR_DIR)/$(MAKECMDGOALS).o

include $(ROOT_DIR)/common.mk

MODULE_PATH = Library Component Adapter
MODULE_LIST = $(foreach var,$(MODULE_PATH),$(ROOT_DIR)/src/$(var)/lib$(var).a)

.PRECIOUS : $(DIR_OBJ_FILES)

%_test: $(DIR_OBJ_FILES) $(MODULE_LIST)
	$(CC) $(DIR_OBJ_FILES) -Wl,--whole-archive $(MODULE_LIST) -Wl,--no-whole-archive -o $@ $(LDFLAGS)
	@echo "Build Complete!"

clean:
	$(RM) $(DIR_OBJ_FILES)

.PHONY: all clean $(MODULE_LIST) modules

