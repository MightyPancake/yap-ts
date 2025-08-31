CC := gcc
CFLAGS := -Wall -g
CYAN := [96m
PURPLE := [94m
GREEN := [92m
RESET := [0m

RM := rm -fr
CP := cp -r
MV := mv

debug ?= false
log := $(debug)
ifeq ($(log),true)
    CFLAGS += -DYAP_LOG
endif

# Tree-sitter configuration
TS_LIB_SRC := ./tree-sitter/lib/src/
TS_LIB := ./lib/libtree-sitter.a

YAP_CFLAGS := $(shell yap --cflags)

# ts_yap flags
YAP_TS_FLAGS := $(YAP_CFLAGS) -L./lib/ -I./include -I./tree-sitter/lib/include -ltree-sitter -lyap $(CFLAGS) -rdynamic
YAP_TS_LIB := ./yap_ts.so

.ONESHELL:

.PHONY: tree-sitter grammar yap_ts default clean utils

default: all

all: tree-sitter grammar yap_ts

yap_ts:
	@echo $(PURPLE)Generating yap-ts module$(RESET)
	@echo $(CYAN)"log: $(log)"$(RESET)
	@echo $(CYAN)Buildig objects$(RESET)
	@echo $(CC) -fPIC $(YAP_TS_FLAGS) src/*.c -c $(CFLAGS)
	@$(CC) -fPIC $(YAP_TS_FLAGS) src/*.c -c $(CFLAGS)
	@echo $(CYAN)Buildig dynamic lib$(RESET)
	@$(CC) -shared -o $(YAP_TS_LIB) ./*.o -L./lib -ltree-sitter $(CFLAGS)
	@rm ./*.o
	@echo $(GREEN)Done!$(RESET)

grammar:
	@echo $(PURPLE)Generating grammar$(RESET)
	@cd grammar
	tree-sitter generate
	$(MV) ./src/parser.c ../src/tree_sitter_yap.c
	$(RM) Cargo.toml binding.gyp bindings package.json src
	@echo $(GREEN)Done!$(RESET)

# $(CC) -shared -o libtree-sitter.so ./lib/ts.o

tree-sitter:
	@echo $(CYAN)Building tree-sitter lib...$(RESET)
	@$(CC) -fPIC -c $(TS_LIB_SRC)/*.c -I$(TS_LIB_SRC) -I$(TS_LIB_SRC)/../include
	@ar rcs ./lib/libtree-sitter.a *.o
	@rm ./*.o
	@echo $(GREEN)Done!$(RESET)

clean:
	@$(RM) $(TS_LIB) $(YAP_TS_LIB)
