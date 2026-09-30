# 故障排查

## 常见问题

### 1. CLI 找不到语言模型或词典

**症状**: `pinyin_ime_cli: 找不到 language model (zh_CN.lm)`

**原因**: 数据文件路径不对。

**解决**:
```bash
# 检查数据文件是否存在
ls /root/workspace/native-build/opt/libime/lib/libime/zh_CN.lm
ls /root/workspace/native-build/opt/libime/share/libime/sc.dict

# 显式指定路径
source ./setup_env.sh
echo "nihao" | "$PINYIN_IME_CLI" "$PINYIN_IME_LM" "$PINYIN_IME_DICT"
```

### 2. setup_env.sh 找不到变量

**症状**: `$PINYIN_IME_CLI` 为空

**解决**:
```bash
cd /root/workspace/pinyin-ime
source ./setup_env.sh
echo $PINYIN_IME_CLI  # 应输出路径
```

### 3. E2E 测试失败：testim 加载警告

**症状**: `Failed to load input method addon: testim`

**说明**: 这是**正常警告**，testim 是 fcitx5 的测试 IM addon，不影响 pinyinime 测试。E2E 测试使用 testfrontend 而非 testim。

### 4. E2E 测试失败：候选不匹配

**症状**: `[FAIL] xxx → 实际结果 (expected: 期望结果)`

**排查步骤**:
1. 先在 CLI 中验证同一输入：
   ```bash
   echo "xxx" | "$PINYIN_IME_CLI" --quiet --top1 "$PINYIN_IME_LM" "$PINYIN_IME_DICT"
   ```
2. 如果 CLI 也错，是语言模型排序问题，用用户学习修复：
   ```bash
   echo -e "xxx\t期望结果" > /tmp/learn.txt
   echo "dummy" | "$PINYIN_IME_CLI" --learn-file /tmp/learn.txt "$PINYIN_IME_LM" "$PINYIN_IME_DICT"
   ```
3. 如果 CLI 对但 E2E 错，是 addon 差异（模糊音/纠错配置不同）

### 5. daily 准确率下降

**症状**: `eval_daily.py` 某句不通过

**排查**:
```bash
# 用正确的 python 路径
/root/workspace/native-build/mamba-env/bin/python3 scripts/eval_daily.py
# 或
/usr/local/bin/python3 scripts/eval_daily.py
```

### 6. 用户学习数据丢失

**位置**: `~/.local/share/pinyin-ime/`

**文件**:
- `user.history` — 语言模型历史
- `user.dict` — 用户词典

**恢复**:
```bash
# 重新运行预热脚本
bash scripts/warmup.sh
```

### 7. addon 编译失败

**症状**: `find_package(LibIMEPinyin REQUIRED)` 失败

**解决**:
```bash
source ./setup_env.sh
export PATH="$NB/mamba-env/bin:$PATH"
export LD_LIBRARY_PATH="$NB/mamba-env/lib:$NB/opt/fcitx5-server/lib:$NB/opt/libime/lib"

cmake -S fcitx5-addon -B fcitx5-addon/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$NB/opt/fcitx5-server;$NB/opt/libime;$NB/mamba-env"
cmake --build fcitx5-addon/build
```

### 8. 性能下降

**检查**:
```bash
/usr/local/bin/python3 scripts/benchmark.py
# 正常: 170+ 句/秒, P50 < 35ms
```

**原因**: 用户学习数据过大可能影响启动速度。清理：
```bash
rm ~/.local/share/pinyin-ime/user.history
rm ~/.local/share/pinyin-ime/user.dict
bash scripts/warmup.sh  # 重新预热
```

### 9. Web 演示无法访问

**端口冲突**:
```bash
# 查看端口占用
lsof -i:8080  # Web演示
lsof -i:8081  # 打字练习

# 修改端口：编辑脚本中的 PORT 变量
```

### 10. 消歧不生效

**CLI**: 消歧在输入完整拼音后应用
**addon**: 消歧在逐字输入时实时应用

**验证**:
```bash
# CLI
echo "xianbeijing" | "$PINYIN_IME_CLI" --quiet --top1 "$PINYIN_IME_LM" "$PINYIN_IME_DICT"
# 应输出: 西安北京

# 不应拆分的情况
echo "xiandai" | "$PINYIN_IME_CLI" --quiet --top1 "$PINYIN_IME_LM" "$PINYIN_IME_DICT"
# 应输出: 现代
```

## 获取帮助

```bash
# 健康检查
bash scripts/healthcheck.sh

# 生成测试报告
bash scripts/generate_report.sh

# 查看版本
echo "" | "$PINYIN_IME_CLI" --version
```
