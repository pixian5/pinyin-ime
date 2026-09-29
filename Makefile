# pinyin-ime Makefile
# 用法: make [build|install|test|clean|bench]

PROJECT := pinyin-ime
VERSION := $(shell cat VERSION 2>/dev/null || echo unknown)

# 构建路径
BUILD_DIR := cpp/build
CLI_BIN := $(BUILD_DIR)/pinyin_ime_cli

# 安装路径
PREFIX ?= /usr/local

# 库路径（根据实际环境调整）
LIBIME_DIR ?= /root/workspace/native-build/opt/libime
ENV_DIR ?= /root/workspace/native-build/mamba-env

.PHONY: all build cli addon install test clean bench eval train help

all: build

build: cli
	@echo "构建完成: $(CLI_BIN)"

cli:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. -DCMAKE_BUILD_TYPE=Release 2>/dev/null || cmake ..
	@cmake --build $(BUILD_DIR)

addon:
	@echo "构建 fcitx5 addon..."
	@cd fcitx5-addon && mkdir -p build && cd build && \
		cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build .

install: build
	@bash scripts/install.sh --prefix $(PREFIX)

test: build
	@bash scripts/smoke_test.sh

bench: build
	@python3 scripts/latency_perkey.py

eval: build
	@python3 scripts/quick_eval.py bench/corpus/news.txt "" \
		$(CLI_BIN) \
		$(LIBIME_DIR)/lib/libime/zh_CN.lm \
		$(LIBIME_DIR)/share/libime/sc.dict

train: build
	@bash scripts/train_and_eval.sh bench/corpus/news.txt

clean:
	@rm -rf $(BUILD_DIR) fcitx5-addon/build
	@echo "已清理"

help:
	@echo "$(PROJECT) v$(VERSION)"
	@echo ""
	@echo "可用目标:"
	@echo "  make build    - 构建 CLI"
	@echo "  make addon    - 构建 fcitx5 addon"
	@echo "  make install  - 安装到 $(PREFIX)"
	@echo "  make test     - 运行冒烟测试"
	@echo "  make bench    - 运行延迟基准"
	@echo "  make eval     - 运行准确率评测"
	@echo "  make train    - 训练 + 评测"
	@echo "  make clean    - 清理构建"
	@echo "  make help     - 显示帮助"
