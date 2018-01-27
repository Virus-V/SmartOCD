# SmartOCD
# 2018年01月26日20:13:09
# Virus.V <virusv@live.com>
# GPLv3

ROOT_DIR := $(shell pwd)
TARGET := smartocd
#==========可变参数区===========
# 所有源码文件
ALL_SRC_FILES = 
# 所有静态库目录
ALL_LIB_PATHS =
# 所有头文件目录
ALL_INC_PATHS = $(ROOT_DIR)/src
# 所有库文件
ALL_LIBS = usb-1.0
# 所有对象文件
ALL_OBJ_FILES = $(subst .c,.o,$(ALL_SRC_FILES))
#宏定义
DEFINES = 
#==========可变参数区===========

include $(ROOT_DIR)/src/source.mk
include $(ROOT_DIR)/test/test.mk

# Optimization level [0,1,2,3,s]
OPT ?= 0
#DEBUG = 
DEBUG = -ggdb

CFLAGS = $(DEBUG)
CFLAGS += -O$(OPT)
# 条件编译，选择dsp库和器件类型
CFLAGS += $(addprefix -D,$(DEFINES)) $(addprefix -I,$(ALL_INC_PATHS))
# Linker Flags
LDFLAGS += $(addprefix -L,$(ALL_LIB_PATH))
LDFLAGS += $(addprefix -l,$(ALL_LIBS))

.PRECIOUS : $(ALL_OBJ_FILE)

all: $(TARGET)

$(TARGET): $(ALL_OBJ_FILES)
	@$(CC) $^ -o $@ $(LDFLAGS) 
	@echo "Build Complete!"

%_test: $(ALL_OBJ_FILES)
	$(CC) $^ -o $(ROOT_DIR)/test/$@ -e $*_main $(LDFLAGS) -nostartfiles 

#%.o : %.c
.PHONY: reset all clean

clean:
	@rm $(ALL_OBJ_FILES)
	
# 重置项目，删除所有动态生成的文件
reset:
	@rm $(subst .c,.d,$(ALL_SRC_FILES))

-include ${patsubst %.c,%.d,$(ALL_SRC_FILES)}

%.d: %.c
	@$(CC) -MM $(CFLAGS) $< > $@.$$$$;	\
	sed 's,\($(notdir $*)\)\.o[ :]*,$(dir $*)\1.o $@ : ,g' < $@.$$$$ > $@;	\
	rm -f $@.$$$$