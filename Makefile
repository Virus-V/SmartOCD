# SmartOCD
# 2018年01月26日20:13:09
# Virus.V <virusv@live.com>
# GPLv3

ROOT_DIR := $(shell pwd)
TARGET := smartocd
# 版本号
VERSION := 1.0.0
COMPILE_TIME := $(shell date +%FT%T%z)

include $(ROOT_DIR)/src/source.mk
include $(ROOT_DIR)/test/test.mk
include $(ROOT_DIR)/lua-5.3.4/lua.mk
#==========可变参数区============ 
# 入口文件
SMARTOCD_ENTRY_SRC_FILE = $(ROOT_DIR)/src/smart_ocd.c
SMARTOCD_ENTRY_OBJ_FILE = $(subst .c,.o,$(SMARTOCD_ENTRY_SRC_FILE))
# SmartOCD 头文件搜索目录
SMARTOCD_INC_PATHS = $(ROOT_DIR)/src
# 所有库文件
ALL_LIBS = usb-1.0 lua m dl

# SmartOCD和TEST对象文件
SMARTOCD_OBJ_FILES = $(subst .c,.o,$(SMARTOCD_SRC_FILES))
TEST_OBJ_FILES = $(subst .c,.o,$(TEST_SRC_FILES))

#宏定义
DEFINES += VERSION=\"$(VERSION)\" COMPILE_TIME=\"$(COMPILE_TIME)\" 

# Optimization level [0,1,2,3,s]
OPT ?= 0
#DEBUG = 
DEBUG = -ggdb

CFLAGS = $(DEBUG)
CFLAGS += -O$(OPT)
CFLAGS += $(addprefix -D,$(DEFINES)) $(addprefix -I,$(SMARTOCD_INC_PATHS) $(TEST_INC_PATHS))
# Linker Flags
LDFLAGS += $(addprefix -L,$(ALL_LIB_PATH))
# 所有库
LDFLAGS += $(addprefix -l,$(ALL_LIBS))

.PRECIOUS : $(SMARTOCD_ENTRY_OBJ_FILE) $(SMARTOCD_OBJ_FILES) $(TEST_OBJ_FILES)

all: $(TARGET)

$(TARGET): $(SMARTOCD_OBJ_FILES) $(SMARTOCD_ENTRY_OBJ_FILE)
	$(CC) $^ -o $@ $(LDFLAGS) 
	@echo "Build Complete!"
 
%_test: $(SMARTOCD_OBJ_FILES) $(TEST_OBJ_FILES) $(ROOT_DIR)/test/%_test.o
	$(CC) $^ -ggdb -o $(ROOT_DIR)/test/$@ $(LDFLAGS)

#%.o : %.c
.PHONY: reset all clean

clean:
	$(RM) $(SMARTOCD_OBJ_FILES) $(SMARTOCD_ENTRY_OBJ_FILE) $(TEST_OBJ_FILES)
	
# 重置项目，删除所有动态生成的文件
reset:
	$(RM) $(subst .c,.d,$(SMARTOCD_SRC_FILES) $(SMARTOCD_ENTRY_SRC_FILE) $(TEST_SRC_FILES))

-include ${patsubst %.c,%.d,$(SMARTOCD_ENTRY_SRC_FILE) $(SMARTOCD_SRC_FILES) $(TEST_SRC_FILES)}

%.d: %.c
	$(CC) -MM $(CFLAGS) $< > $@.$$$$;	\
	sed 's,\($(notdir $*)\)\.o[ :]*,$(dir $*)\1.o $@ : ,g' < $@.$$$$ > $@;	\
	$(RM) -f $@.$$$$
