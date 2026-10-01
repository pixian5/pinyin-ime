# pinyin-ime 测试报告

**版本**: v0.9.26 | **日期**: 2026-10-01 10:56:11 | **平台**: aarch64 5.10.0-182.0.0.95.r3582_286.hce2.aarch64

## 1. 冒烟测试

```
== 全拼 ==
  PASS  nihao                -> 你好
  PASS  zhongguo             -> 中国
  PASS  woshiyige            -> 我是一个
  PASS  beijingdaxue         -> 北京大学
  PASS  zhongguoruanjian     -> 中国软件
== 简拼 ==
  PASS  bjdx                 -> 北京大学
  PASS  zg                   -> 中国
  PASS  js                   -> 技术
== 模糊音 ==
  PASS  cifan                -> 吃饭
  PASS  senme                -> 什么
  PASS  zeli                 -> 这里
  PASS  lanfang              -> 南方
== 双拼（小鹤）==
  PASS  nihc                 -> 你好
  PASS  vsgo                 -> 中国
  PASS  bwjkdaxt             -> 北京大学
  PASS  ufme                 -> 什么
  PASS  veli                 -> 这里
  PASS  iifj                 -> 吃饭
  PASS  jiuu                 -> 技术
  PASS  wouiyige             -> 我是一个
== CLI 功能 ==
  PASS  --version             -> pinyin_ime_cli 0.9.26
  PASS  --stats               -> JSON OK
  PASS  --json --quiet        -> ["你好","拟好","你毫","你号","你耗"]
  PASS  --correct --fuzzy     -> 什么
==== 结果：24 通过 / 0 失败 ====
```

## 2. 交互测试

```
[PASS] ['「你好」']
[PASS] ['hello']
[PASS] ['，', '。', '？']
[PASS] ['「中国」', '！']
[PASS] ['“', '”', '“']
[PASS] ['「你好」', '“']
[PASS] ['「我是一个」']
[PASS] ['「我是一个」']
[PASS] ['「我是一个」']
[PASS] ['「我是一个」']
[PASS] ['‘', '’', '‘']
[PASS] ['「西安」']
[PASS] ['😀']
[PASS] ['👍']
[PASS] ['「什么」']
[PASS] ['「中国」']
[PASS] ['「中国」']
[PASS] ['「亿」']
[PASS] ['「亿」']
[PASS] ['「一」']
[PASS] ['「你好」', 'ni hao', '已撤回上屏']
[PASS] ['「你好」', '已撤回上屏', '「你好」']
[PASS] ['「中」']
[PASS] ['「以」']
[PASS] ['「已」']
[PASS] ['「中」']
==== 交互回归：26/26 通过 ====
```

## 3. Daily 评测

```
=== Daily Life Sentence Evaluation ===
Total: 40
Sentence accuracy: 40/40 = 100.0%
Character accuracy: 265/265 = 100.0%
```

## 4. 预热验证

```
=== pinyin-ime 预热脚本 ===
预置数据: /root/workspace/pinyin-ime/data/preset_learning.txt
用户数据: /root/.local/share/pinyin-ime

阶段1: 98 个普通短语 (3x权重)
pinyin_ime_cli: 批量学习完成 288/294 句已学习（用户数据已保存到 /root/.local/share/pinyin-ime）
阶段2: 6 个关键消歧短语 (10x权重)
pinyin_ime_cli: 批量学习完成 60/60 句已学习（用户数据已保存到 /root/.local/share/pinyin-ime）
✅ 预热完成

=== 验证关键短语 ===
  ✅ shanghaishi → 上海是
  ✅ xianbeijing → 西安北京
  ✅ nanjingshiliuchaogudu → 南京是六朝古都
  ✅ woxiangdingyigefangjian → 我想订一个房间
  ✅ rengongzhineng → 人工智能
  ✅ nihaoshijie → 你好世界
  ✅ xuexishiwokuaile → 学习使我快乐

验证: 7/7 通过
```

## 5. E2E 端到端测试

```
[PASS] landmark: changcheng → 长城
[PASS] landmark: gugong → 故宫
[PASS] culture: zhonghua → 中华
[PASS] common noun: renmin → 人民
[PASS] sentence: woxiangqulvyou → 我想去旅游
[PASS] sentence: woshixuesheng → 我是学生
[PASS] phrase: zhongguorenmin → 中国人民
[PASS] phrase: shurufashijian → 输入法时间
[PASS] common noun: mubiao → 目标
[PASS] common noun: hexin → 核心
[PASS] disambiguation: xianbeijing → 西安北京
[PASS] disambiguation: xianyou → 西安有
[PASS] disambiguation: xianchengshi → 西安城市
[PASS] no-split: xiandai → 现代
[PASS] no-split: xianshi → 显示
[PASS] no-split: xianzai → 现在

========== Summary ==========
Total: 138 | Pass: 138 | Fail: 0
Accuracy: 100%
```

## 6. 性能基准

```
╔══════════════════════════════════════════════════════╗
║  pinyin-ime 性能基准测试                              ║
╚══════════════════════════════════════════════════════╝

==================================================
  冷启动时间
==================================================
  平均: 32.07 ms (进程启动 + LM 加载 + 解码)

==================================================
  短拼音 (2-3 音节)
==================================================
  nihao                                 31.97 ms
  shijie                                37.89 ms
  zhongguo                              41.55 ms
  woxihuan                              36.86 ms
  xianzai                               36.42 ms
  jintian                               35.41 ms
  ────────────────────────────────────────
  平均: 36.69 ms | 最小: 31.97 ms | 最大: 41.55 ms

==================================================
  中等拼音 (4-5 音节)
==================================================
  beijingdaxue                          42.05 ms
  woaixuexi                             35.38 ms
  rengongzhineng                        38.79 ms
  shenduxuexi                           40.63 ms
  jiqixuexi                             42.95 ms
  ────────────────────────────────────────
  平均: 39.96 ms | 最小: 35.38 ms | 最大: 42.95 ms

==================================================
  长拼音 (7+ 音节)
==================================================
  qingwenyixiaruzhushenmefangjian       63.06 ms
  woxiangdingyigefangjian               53.39 ms
  jintiandetianqizenmeyang              51.55 ms
  ────────────────────────────────────────
  平均: 56.00 ms | 最小: 51.55 ms | 最大: 63.06 ms

==================================================
  吞吐量
==================================================
  总查询: 140 次
  总耗时: 5.92 s
  QPS:    23.6 queries/sec

==================================================
  引擎统计
==================================================
  {
    "lm_path": "/root/workspace/native-build/opt/libime/lib/libime/zh_CN.lm",
    "dict_path": "/root/workspace/native-build/opt/libime/share/libime/sc.dict",
    "nbest": 5,
    "fuzzy": false,
    "shuangpin": false
  }

==================================================
  总结
==================================================
  冷启动:     32.07 ms
  短拼音延迟: 36.69 ms
  中等延迟:   39.96 ms
  长拼音延迟: 56.00 ms
  吞吐量:     23.6 QPS
  交互态延迟: ~1 ms (进程常驻，无冷启动)
```

## 7. 纠错能力

| 输入 | 无纠错 | 有纠错 | 纠错生效 |
|---|---|---|---|
| senme | 森么 | 什么 | ✅ |
| zongguo | 宗国 | 中国 | ✅ |
| sengri | 僧日 | 生日 | ✅ |
| nihou | 你后 | 你好 | ✅ |
| shengri | 生日 | 生日 | - |

## 8. 模糊音

| 输入 | 无模糊 | 有模糊 | 模糊生效 |
|---|---|---|---|
| cifan | 此番 | 吃饭 | ✅ |
| zeli | 啫喱 | 这里 | ✅ |
| lanfang | 兰芳 | 南方 | ✅ |
| sijiao | 死角 | 视角 | ✅ |

## 总结

- **版本**: v0.9.26
- **生成时间**: 2026-10-01 10:56:11
- **平台**: aarch64 5.10.0-182.0.0.95.r3582_286.hce2.aarch64

