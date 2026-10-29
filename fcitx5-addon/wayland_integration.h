// v0.9.134: Wayland text-input-v3 协议适配
// fcitx5 的 wayland module 已内置 text-input-v3 支持，
// 本文件提供 pinyin-ime addon 的 Wayland 集成配置和适配代码。
#pragma once

#include <string>
#include <vector>

namespace pinyin_ime::wayland {

// Wayland 显示服务器信息
struct WaylandDisplay {
    std::string display_name;    // WAYLAND_DISPLAY 环境变量
    std::string socket_path;     // 套接字路径
    bool is_running = false;     // Wayland 是否在运行
};

// 检测 Wayland 环境
inline WaylandDisplay detect_wayland() {
    WaylandDisplay wd;
    const char *display = std::getenv("WAYLAND_DISPLAY");
    const char *xdg_runtime = std::getenv("XDG_RUNTIME_DIR");
    if (display) {
        wd.display_name = display;
        wd.is_running = true;
        if (xdg_runtime) {
            wd.socket_path = std::string(xdg_runtime) + "/" + display;
        }
    }
    return wd;
}

// text-input-v3 状态
enum class TextInputState {
    Inactive,       // 未激活
    PendingCommit, // 待提交
    Active          // 激活中
};

// Wayland 输入上下文适配
struct WaylandInputContext {
    TextInputState state = TextInputState::Inactive;
    std::string surrounding_text;  // 周围文本
    int cursor_pos = 0;            // 光标位置
    int anchor_pos = 0;            // 选区锚点
    std::string preedit_string;    // 预编辑文本
    int preedit_cursor = 0;        // 预编辑光标
    std::string commit_string;     // 提交文本

    // 激活输入
    void activate() { state = TextInputState::Active; }

    // 停用输入
    void deactivate() {
        state = TextInputState::Inactive;
        preedit_string.clear();
        commit_string.clear();
    }

    // 设置预编辑
    void set_preedit(const std::string &text, int cursor) {
        preedit_string = text;
        preedit_cursor = cursor;
    }

    // 提交文本
    void commit(const std::string &text) {
        commit_string = text;
        state = TextInputState::PendingCommit;
    }

    // 设置周围文本（用于上下文感知）
    void set_surrounding(const std::string &text, int cursor, int anchor) {
        surrounding_text = text;
        cursor_pos = cursor;
        anchor_pos = anchor;
    }
};

// Wayland 模块配置
struct WaylandModuleConfig {
    bool enable_text_input_v3 = true;   // text-input-v3 协议
    bool enable_text_input_v1 = false;  // text-input-v1 兼容
    bool enable_virtual_keyboard = true; // 虚拟键盘
    int repeat_rate = 25;                // 重复速率
    int repeat_delay = 300;              // 重复延迟(ms)
};

// content type hint（对应 zwp_text_input_v3_content_hint）
enum class ContentHint {
    None        = 0,
    Default     = 0x1,
    Password    = 0x2,
    AutoSuggestion = 0x4,
    AutoCompletion = 0x8,
    LineFeed   = 0x10,
    Multiline  = 0x20,
    SensitiveData = 0x40,
    Latin      = 0x80,
    MultilineNoNewline = 0x100
};

// content purpose（对应 zwp_text_input_v3_content_purpose）
enum class ContentPurpose {
    Normal     = 0,
    Password   = 1,
    Pin        = 2,
    Phone      = 3,
    Number     = 4,
    Calculator = 5,
    Url        = 6,
    Email      = 7,
    Name       = 8,
    Date       = 9,
    Time       = 10,
    Datetime   = 11,
    Terminal   = 12
};

// 根据内容目的调整输入法行为
inline std::string purpose_to_hint(ContentPurpose purpose) {
    switch (purpose) {
        case ContentPurpose::Password:
        case ContentPurpose::Pin:
            return "password";  // 禁用预编辑，直接上屏
        case ContentPurpose::Number:
        case ContentPurpose::Calculator:
            return "number";    // 仅数字模式
        case ContentPurpose::Url:
            return "url";       // URL 模式，优先英文
        case ContentPurpose::Email:
            return "email";     // 邮箱模式
        case ContentPurpose::Terminal:
            return "terminal";  // 终端模式
        default:
            return "normal";
    }
}

} // namespace pinyin_ime::wayland
