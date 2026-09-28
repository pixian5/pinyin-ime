// emoji_list.h — 内置「安全中性」常用表情（CLI 与 Fcitx5 addon 共用）。
//
// 用法：交互式输入法按 Ctrl-E 进入表情选择，1-9 选表情、[,]/[.] 翻页、Esc/Ctrl-E 返回。
// 码点用 \U/\u 转义写死，不依赖源码编码；仅 27 个，无外部数据文件。
// 2016 年 6 月后发布的中文字体（Noto/思源黑体等）均已覆盖这些 Emoji 图形。
#pragma once

#include <string>
#include <vector>

struct EmojiEntry {
    std::string glyph;
    std::string label;
};

inline const std::vector<EmojiEntry> &emoji_list() {
    static const std::vector<EmojiEntry> list = {
        // 表情
        {"\U0001F600", "大笑"}, {"\U0001F602", "笑哭"}, {"\U0001F60A", "微笑"},
        {"\U0001F60D", "喜欢"}, {"\U0001F62D", "哭"},   {"\U0001F621", "生气"},
        {"\U0001F914", "思考"}, {"\U0001F634", "困"},   {"\U0001F970", "爱慕"},
        // 手势 / 常用
        {"\U0001F44D", "赞"},   {"\U0001F44E", "踩"},   {"\U0001F44F", "鼓掌"},
        {"\U0001F64F", "感谢"}, {"\U0001F4AA", "加油"}, {"\U0001F91D", "握手"},
        {"\U0001F44C", "好的"}, {"\u2705", "完成"},     {"\u274C", "错误"},
        // 其他
        {"\u2600", "太阳"},     {"\u2B50", "星星"},     {"\u26A1", "闪电"},
        {"\U0001F525", "火"},   {"\U0001F4A7", "水滴"}, {"\U0001F389", "庆祝"},
        {"\U0001F381", "礼物"}, {"\U0001F3B5", "音乐"}, {"\U0001F339", "玫瑰"},
    };
    return list;
}