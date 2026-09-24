# 基线评测容器：libime 整句拼音引擎 + 评测脚本运行环境。
#
# 默认基础镜像 debian:12。若拉取 Docker Hub 失败（弱网/热点），可改用本机已有的
# Debian 12 镜像，省掉整个镜像下载：
#   docker build --build-arg BASE_IMAGE=xxsy-backend:latest -t pinyin-bench -f docker/Dockerfile .
#
# 用法（在项目根目录）:
#   docker run --rm -v "$PWD/bench":/bench -w /bench pinyin-bench \
#     python3 run_bench.py --corpus corpus/wiki.txt --cli /src/build/libime_bench --show 20
#
# 或者进容器交互式跑：
#   docker run --rm -it -v "$PWD/bench":/bench -w /bench pinyin-bench bash
ARG BASE_IMAGE=debian:12
FROM ${BASE_IMAGE}

ENV DEBIAN_FRONTEND=noninteractive

# libimepinyin-dev 提供头文件与 cmake 配置；libime-data / libime-data-language-model
# 提供 sc.dict 与 sc.lm。Debian 12 上是 libime 1.0.16。
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        cmake \
        g++ \
        pkg-config \
        libimepinyin-dev \
        libime-data \
        libime-data-language-model \
        python3 \
        python3-pip \
    && rm -rf /var/lib/apt/lists/*

COPY cpp /src
WORKDIR /src
# Debian 的 libimepinyin-dev 不带 core 头文件（lattice.h / userlanguagemodel.h），补装 libimecore-dev。
# 单独一层：避免改动上面的 apt 层导致整层重下。
RUN apt-get update && apt-get install -y --no-install-recommends \
        libimecore-dev libboost-dev \
    && rm -rf /var/lib/apt/lists/*

# PinyinIME 继承 fcitx::ConnectableObject，需要 fcitx-utils 头文件。
# 单独一层，避免上面的 boost 层被一起重新下载。
RUN apt-get update && apt-get install -y --no-install-recommends \
        libfcitx5utils-dev \
    && rm -rf /var/lib/apt/lists/*

# 诊断：确认 libime 头文件与数据文件的真实位置（不同发行版路径不一致）
RUN echo "--- historybigram.h ---"; find / -name "historybigram.h" 2>/dev/null | head -3; \
    echo "--- pinyinime.h ---";      find / -name "pinyinime.h"     2>/dev/null | head -3; \
    echo "--- 语言模型 *.lm ---";   find / -name "*.lm" 2>/dev/null | head -5; \
    echo "--- /usr/share/libime ---"; ls -R /usr/share/libime 2>/dev/null | head -20; \
    echo "--- 已装 libime 包 ---";    dpkg -l 2>/dev/null | grep -i libime | awk '{print $2, $3}'

# 直接 g++ 编译，不依赖 make/cmake 生成器。
# libime 的库名：libimepinyin / libimecore；PinyinIME 继承 fcitx::ConnectableObject，需链 Fcitx5Utils。
# 第二个分支是库名变动时的兜底。
# fcitx-utils 的头文件同样在非标准前缀下（Debian 的 Fcitx5/Utils 目录）
RUN echo "--- libime 库 ---"; find / -name "libime*.so*" 2>/dev/null | head -10; \
    echo "--- Fcitx5Utils 库 ---"; find / -name "libFcitx5Utils*" 2>/dev/null | head -3; \
    echo "--- ld 搜索路径 ---"; ldconfig -p 2>/dev/null | grep -iE "imepinyin|imecore|fcitx5utils" | head -5; \
    # 库名是 libIMEPinyin / libIMECore（IME 大写），不是 libimepinyin
    g++ -O2 -std=c++17 \
        -I/usr/include/LibIME -I/usr/include/Fcitx5/Utils -I/usr/include/Fcitx5 \
        -o libime_bench libime_bench.cpp -lIMEPinyin -lIMECore -lFcitx5Utils

# 构建期冒烟：能打出"你好"就说明库、sc.lm、sc.dict 都就位了
RUN echo "nihao" | ./libime_bench

# 评测脚本依赖（pypinyin 生成 gold、opencc 繁转简）
COPY bench/requirements.txt /bench_requirements.txt
RUN pip3 install --no-cache-dir --break-system-packages -r /bench_requirements.txt \
    || pip3 install --no-cache-dir -r /bench_requirements.txt

# 确认数据文件位置，便于排错
RUN ls /usr/share/libime/ 2>/dev/null || ls /usr/lib/aarch64-linux-gnu/libime/ 2>/dev/null || true

WORKDIR /bench
CMD ["/src/libime_bench"]
