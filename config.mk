# 版本号
VERSION := 0.2.0
OS := $(shell uname -s)

# 额外库文件
ifeq ($(OS),FreeBSD)
EXTRA_LIBS = usb m dl ftdi1
else ifeq ($(OS),Linux)
EXTRA_LIBS = usb-1.0 m dl ftdi1
else ifeq ($(OS),Darwin)
$(error "Not support Darwin yet")
else
$(error "Not support ${OS} yet")
endif

# 定义当前版本和构建时间
DEFINES += VERSION=\"$(VERSION)\" COMPILE_TIME=\"$(COMPILE_TIME)\"

# Optimization level [0,1,2,3,s]
OPT = 0
#DEBUG =
DEBUG = -ggdb

CFLAGS = $(DEBUG)
#开发过程中使用-Werror，所有警告都当做错误来终止编译
CFLAGS += -O$(OPT) -Werror
CFLAGS += -isystem /usr/local/include
CFLAGS += -I$(ROOT_DIR)/src -I$(CURR_DIR)
CFLAGS += $(addprefix -D,$(DEFINES))
# 所有库
LDFLAGS = $(addprefix -l,$(EXTRA_LIBS))
LDFLAGS += -L/usr/local/lib
LDFLAGS += -L$(ROOT_DIR)/src/Library/libuv
