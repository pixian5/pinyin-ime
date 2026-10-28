# pinyin-ime v0.9.125 发布说明

> 发布日期：2026-10-28
> 上一版本：v0.9.124（P0-P3 全部实现）
> GitCode 提交：`6970e14`

---

## 一、新增功能

### 1. i 模式快捷短语（搜狗式快速短语）

**触发方式**：空闲状态下按 `i` 键进入 i 模式，随后输入字母缩写。

**内置短语表**（50+ 条，按使用频率排序）：

| 类别 | 缩写 → 短语 | 说明 |
|------|------------|------|
| 问候 | `ih`→你好 `ix`→谢谢 `iz`→再见 `izw`→早上好 `ixw`→下午好 `iww`→晚上好 `iwa`→晚安 `ihj`→好久不见 | 最常用社交短语 |
| 礼貌 | `iq`→请 `id`→对的 `idy`→当然 `ibd`→不对 `iby`→不用 `imy`→没有 `ikl`→客气了 `ibk`→不客气 `isg`→应该的 | 礼貌用语 |
| 道歉/回应 | `isq`→对不起 `ihl`→好了 `iok`→好的 `ixk`→辛苦了 `ilx`→麻烦了 | 道歉与回应 |
| 工作/日常 | `isy`→收到 `izl`→知道了 `iml`→明白了 `idl`→到了 `iwl`→完了 `iks`→开始 `ijs`→结束 `icx`→继续 `imt`→没问题 | 工作场景 |
| 情感 | `igx`→高兴 `ikk`→开心 `ins`→难受 `isl`→算了 | 情感表达 |
| 常用句 | `iwm`→我们 `inm`→你们 `itm`→他们 `ish`→是的 `ibs`→不是 `izy`→这样 `iny`→那样 | 高频句式 |
| 趣味 | `iha`→哈哈 | — |

**交互模式操作**：
- 按 `i` 进入 i 模式（空闲状态下）
- 输入字母累积缩写，实时显示匹配短语
- 数字键 `1-9` 选择对应短语上屏
- 空格/回车选择首选短语上屏
- 退格删除最后输入的字母，缓冲空时退回正常模式
- `Esc` 退出 i 模式

**行模式操作**：
- 直接输入 `ih`、`ix` 等，输出对应短语

### 2. 联想词（搜狗式关联词推荐）

**触发方式**：候选词上屏后自动进入联想词模式，显示与刚上屏文本关联的候选词。

**联想规则表**（20+ 条）：

| 触发词 | 联想候选 | 说明 |
|--------|---------|------|
| 你好 | 啊、吗 | 问候后续 |
| 我 | 的、们 | 第一人称后续 |
| 谢谢 | 你、啊 | 致谢后续 |
| 想 | 要、到 | 意愿后续 |
| 今天 | 天气、是 | 时间话题后续 |
| 怎么 | 了、样 | 疑问后续 |
| 什么 | 的、吗 | 疑问后续 |
| 可以 | 的、吗 | 可能性后续 |
| 知道 | 了、吗 | 认知后续 |
| 喜欢 | 的、吗 | 喜好后续 |
| 需要 | 的、吗 | 需求后续 |
| 觉得 | 的、吗 | 感受后续 |
| 现在 | 的、吗 | 时间后续 |
| 已经 | 了、的 | 完成态后续 |
| 不要 | 的、紧 | 否定后续 |
| 没有 | 的、了 | 否定后续 |
| 不是 | 的、吗 | 否定后续 |
| 那个 | 的、人 | 指代后续 |
| 这个 | 的、人 | 指代后续 |
| 好的 | 、呀 | 应答后续 |

**交互模式操作**：
- 上屏后自动显示联想候选（如：上屏"你好"后显示 `联想: 1.啊 2.吗`）
- 数字键 `1-9` 选择联想词上屏，选后继续联想（链式联想）
- 任意字母键退出联想模式，进入正常拼音输入
- `Esc` 取消联想模式

**匹配算法**：按触发词长度从长到短排序匹配，避免短触发词误匹配（如"你好"不被"好"抢先匹配）。

---

## 二、Bug 修复

### Bug 1：render 函数大括号嵌套错误

**问题**：i 模式和联想词的渲染代码被误嵌套在 `if (emojiMode)` 块内部，导致这两个功能的渲染永远不会执行。

**原因**：在 `render` lambda 中，`if (emojiMode)` 块的 `return;` 语句后直接插入了 i 模式渲染代码，但缺少 `if (emojiMode)` 块的闭合 `}`。

**修复**：在 `return;` 后添加 `}` 闭合 emojiMode 块，并删除末尾多余的 `}`。

**影响文件**：`cpp/pinyin_ime_cli.cpp` 第 2389 行附近

### Bug 2：commit lambda 联想词代码位置

**问题**：联想词设置代码在 `if (select_and_commit(ctx, idx))` 块外部，即使提交失败也会设置联想词。

**修复**：将联想词代码移入 `if (select_and_commit)` 块内部，仅在实际成功上屏后才触发联想。

**影响文件**：`cpp/pinyin_ime_cli.cpp` 第 1925 行附近

---

## 三、实现细节

### 文件变更

| 文件 | 变更 | 行数 |
|------|------|------|
| `cpp/quick_input.h` | 新增 `QuickPhrase` 结构体、`builtin_quick_phrases()`、`is_i_mode()`、`i_mode_lookup()`、`association_lookup()` | +262 行 |
| `cpp/pinyin_ime_cli.cpp` | 行模式 + 交互模式集成 i 模式和联想词；修复 render 大括号和 commit lambda | +133 行 |
| `cpp/quick_input_test.cpp` | 新增 i 模式 7 项 + 联想词 8 项测试 | +70 行 |

### 新增函数签名

```cpp
// quick_input.h

// i 模式判断
bool is_i_mode(const std::string &input);

// i 模式查找：输入缩写，返回匹配的短语列表
std::vector<std::string> i_mode_lookup(const std::string &abbr);

// 联想词查找：输入上屏文本，返回关联候选词列表
std::vector<std::string> association_lookup(const std::string &committed);
```

### 测试结果

```
=== Results: 119 passed, 0 failed ===
```

测试覆盖：
- v 模式符号 29 组
- 货币 4 种、单位 12 种、百分比 3 个
- 统一入口 7 个、扩展日期 5 个
- **i 模式 7 项**（新增）
- **联想词 8 项**（新增）
- 其他原有测试

---

## 四、S3 产物清单

远端路径：`cst:xbz/pinyin-ime/v0.9.125/`

| 文件 | 大小 | 说明 |
|------|------|------|
| `pinyin-ime-src-v0.9.125.tar.gz` | 1.6 MB | 完整源码包（cpp + fcitx5-addon + scripts + docs） |
| `pinyin-ime-src-v0.9.125.tar.gz.sha256` | 65 B | 源码包校验和 |
| `pinyin-ime-ro-runtime-v0.9.125.tar.gz` | 18 KB | 只读 /usr 沙箱运行脚本包 |
| `pinyin-ime-ro-runtime-v0.9.125.tar.gz.sha256` | 104 B | 运行时包校验和 |
| `quick_input_test_v0.9.125` | 152 KB | 编译好的单元测试二进制（aarch64） |
| `quick_input.h` | 51 KB | 快速输入头文件（含 i 模式 + 联想词） |
| `quick_input_test.cpp` | 13 KB | 单元测试源码 |
| `pinyin_ime_cli.cpp` | 136 KB | CLI 主程序源码 |
| `pinyinime.cpp` | — | fcitx5 addon 源码 |
| `pinyinime.h` | — | fcitx5 addon 头文件 |
| `README.md` | — | 项目说明文档 |
| `VERSION` | 8 B | 版本号文件 |
| `SHA256SUMS_v0.9.125` | — | 全部文件 SHA256 校验和 |

---

## 五、构建环境

| 组件 | 版本 | 来源 |
|------|------|------|
| OS | openEuler 22.03 LTS aarch64 | 本机 |
| GCC | 10.3.1 | 系统自带 |
| libime | 1.1.17 | GitHub clone（头文件） |
| fcitx5 | 5.1.23 | GitHub clone（fcitx-utils 头文件） |
| boost | 1.78.0 | dnf install boost-devel |
| rclone | 1.75.1 | S3 上传工具 |

**已知限制**：GCC 10.3 不支持 `<source_location>`（需 GCC 11+），完整 CLI 编译需要升级编译器或使用兼容层。单元测试（`quick_input_test`）不依赖 libime，可独立编译验证。

---

## 六、后续待办

1. **完整 CLI 编译**：升级 GCC 至 11+ 或创建 source_location 兼容层，编译含 i 模式 + 联想词的完整 CLI 二进制
2. **fcitx5 addon 集成**：将 i 模式和联想词功能集成到 fcitx5 addon（`pinyinime.cpp`）
3. **自定义短语**：支持用户自定义 i 模式短语文件（`~/.config/pinyin-ime/quick_phrases.txt`）
4. **联想词学习**：基于用户输入历史动态扩展联想规则
5. **更多搜狗式功能**：Tab 英文补全、皮肤/主题、云拼音
