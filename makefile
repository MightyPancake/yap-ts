CC := gcc
CFLAGS := -Wall
CYAN := [96m
PURPLE := [94m
GREEN := [92m
RESET := [0m

RM := rm -fr
CP := cp -r
MV := mv

YAP_PATH := $(shell pwd)

debug ?= false
ifeq ($(debug),true)
    CFLAGS += -g -O0 -fno-omit-frame-pointer
endif

log := $(debug)
ifeq ($(log),true)
    CFLAGS += -DYAP_LOG
endif

# Tree-sitter configuration
TS_LIB_SRC := ./tree-sitter/lib/src/
TS_LIB := ./lib/libtree-sitter.a

YAP_CFLAGS := $(shell yap --cflags)

# ts_yap flags
YAP_TS_FLAGS := $(YAP_CFLAGS) -L./lib/ -I./include -I./tree-sitter/lib/include -lyap $(CFLAGS)
YAP_TS_LIB := ./libyap_ts.so

.ONESHELL:

.PHONY: tree-sitter grammar yap_ts default clean test

default: all

all: lib/libtree-sitter.a grammar yap_ts

yap_ts:
	@echo $(PURPLE)Generating yap-ts module$(RESET)
	@echo $(CYAN)"log: $(log)"$(RESET)
	@echo $(CYAN)Buildig objects$(RESET)
	@echo $(CC) -fPIC $(YAP_TS_FLAGS) src/*.c -c $(CFLAGS)
	$(CC) -fPIC $(YAP_TS_FLAGS) src/*.c -c $(CFLAGS)
	@echo $(CYAN)Buildig dynamic lib$(RESET)
	@echo $(CC) -shared -o $(YAP_TS_LIB) ./*.o ./lib/libtree-sitter.a -Wl,-rpath,$(YAP_PATH)/lib $(CFLAGS)
	$(CC) -shared -o $(YAP_TS_LIB) ./*.o ./lib/libtree-sitter.a -Wl,-rpath,$(YAP_PATH)/lib $(CFLAGS)
	rm ./*.o
	@echo $(GREEN)Done!$(RESET)

grammar:
	@echo $(PURPLE)Generating grammar$(RESET)
	@cd grammar
	tree-sitter generate
	$(MV) ./src/parser.c ../src/tree_sitter_yap.c
	$(RM) Cargo.toml binding.gyp bindings package.json src
	@echo $(GREEN)Done!$(RESET)


# tree-sitter:
lib/libtree-sitter.a:
	@echo $(CYAN)Building tree-sitter lib...$(RESET)
	@echo $(CC) -fPIC -c $(TS_LIB_SRC)/*.c -I$(TS_LIB_SRC) -I$(TS_LIB_SRC)/../include $(CFLAGS)
	$(CC) -fPIC -c $(TS_LIB_SRC)/*.c -I$(TS_LIB_SRC) -I$(TS_LIB_SRC)/../include $(CFLAGS)
	@echo ar rcs ./lib/libtree-sitter.a *.o
	ar rcs ./lib/libtree-sitter.a *.o
	rm ./*.o
	@echo $(GREEN)Done!$(RESET)

test:
	@make debug=true
	@make copy
	@cd $(shell yap --modules).. && make test


clean:
	$(RM) $(TS_LIB) $(YAP_TS_LIB)

copy:
	rsync -a --exclude='.git' ./ $(shell yap -m)/yap-ts/

