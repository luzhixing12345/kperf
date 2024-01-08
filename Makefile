

CC = gcc
CFLAGS = -Wall -Wunused -Werror -Wformat-security -Wshadow -Wpedantic -Wstrict-aliasing -Wuninitialized -Wnull-dereference -Wformat=2

# -Wshadow 局部变量和全局变量同名
# -Wpedantic 严格的ISO C 标准警告
# -Wstrict-aliasing 警告可能会引发严格别名规则的别名问题
# -Wunnitialized 未初始化局部变量
# -Wnull-dereference 警告可能的空指针解引用
# -Wformat=2 字符串格式化

TARGET = kperf
SRC_PATH = src
# 搜索的后缀(.cpp -> .h)
SRC_EXT = c
# 头文件
INCLUDE_PATH = include
LDFLAGS = 
# 测试文件夹
TEST_PATH = test
# 项目名字(库)
RELEASE = $(TARGET)
# 编译得到的静态库的名字(如果需要)
LIB = lib$(TARGET).a
# ------------------------- #

rwildcard = $(foreach d, $(wildcard $1*), $(call rwildcard,$d/,$2) \
						$(filter $2, $d))

SRC = $(call rwildcard, $(SRC_PATH), %.$(SRC_EXT))
OBJ = $(SRC:$(SRC_EXT)=o)

THIRD_LIB = $(call rwildcard, $(INCLUDE_PATH), %.$(SRC_EXT))
THIRD_LIB_OBJ = $(THIRD_LIB:c=o)

CFLAGS += -I$(INCLUDE_PATH)

ifeq ($(MAKECMDGOALS),debug)
CFLAGS+=-g
endif

all: $(TARGET)

debug: all

$(TARGET): $(OBJ) $(THIRD_LIB_OBJ)
	$(CC) $^ $(LDFLAGS) -o $(SRC_PATH)/$@

%.o: %.c $(HEADER)
	$(CC) $(CFLAGS) -c $< -o $@

# ------------------------- #
#          使用方法
# ------------------------- #
.PHONY: clean clean_all lib release tar all test

# make : 编译
# make clean: 清除编译的中间文件
# make clean_all: 清除所有编译结果
# make lib: 将所有obj整合到一个.a
# make release: 导出release库
# make tar: 打包release
# make install: 安装release库
# make uninstall: 卸载release库

# Define variables for formatting
CP_FORMAT = "[cp]\t%-20s -> %s\n"
MV_FORMAT = "[mv]\t%-20s -> %s\n"

test:
	$(MAKE) clean
	$(MAKE) debug -j4
	python test.py

clean:
	rm -f $(OBJ) $(SRC_PATH)/$(TARGET) $(THIRD_LIB_OBJ)
release:
	$(MAKE) -j4
	mkdir $(RELEASE)
	@cp $(EXE) $(RELEASE)/ 
	tar -cvf $(TARGET).tar $(RELEASE)/

clean_all:
	rm -r $(RELEASE) $(RELEASE).tar
