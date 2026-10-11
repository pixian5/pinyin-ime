# Wayland 支持评估报告

## 当前状态

- fcitx5 v5.1.23 二进制 **未编译 Wayland 支持**
- 系统已安装 wayland 1.19.91 + wayland-protocols 1.24
- fcitx5-wayland addon 不存在于当前 bundle 中

## 添加 Wayland 支持的步骤

### 1. 依赖检查

| 依赖 | 状态 | 版本 |
|------|------|------|
| wayland-devel | ✅ 已安装 | 1.19.91 |
| wayland-protocols-devel | ✅ 已安装 | 1.24 |
| libxkbcommon-devel | 需检查 | - |
| wlroots | 需安装 | - |
| fcitx5 源码 | 需克隆 | - |

### 2. 构建步骤

```bash
# 1. 克隆 fcitx5 源码
git clone https://github.com/fcitx/fcitx5.git
cd fcitx5

# 2. 配置构建（启用 Wayland）
mkdir build && cd build
cmake .. \
  -DENABLE_WAYLAND=On \
  -DENABLE_X11=On \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/root/.local/pinyin-ime/opt/fcitx5-x11

# 3. 编译
ninja -j$(nproc)

# 4. 安装 wayland addon
cp lib/fcitx5/libwayland.so /root/.local/pinyin-ime/opt/fcitx5-x11/lib/fcitx5/
```

### 3. 运行时配置

```bash
# Wayland 环境变量
export WAYLAND_DISPLAY=wayland-0
export XDG_RUNTIME_DIR=/run/user/$(id -u)

# fcitx5 会自动检测 Wayland 并加载 wayland addon
fcitx5 -d
```

### 4. 测试方法

```bash
# 使用 Weston (Wayland 参考合成器) 在无头环境中测试
dnf install -y weston
weston --backend=drm-backend.so --tty=1  # 或使用 headless backend

# 或使用 cage (更轻量的合成器)
cage -- fcitx5 -d
```

## 评估结论

| 项目 | 评估 |
|------|------|
| 可行性 | ✅ 可行，依赖基本满足 |
| 工作量 | 中等（需重新编译 fcitx5） |
| 优先级 | 低（X11 已完全可用） |
| 风险 | 低（不影响现有 X11 功能） |

## 建议

1. **短期**：保持 X11 支持，已完全满足需求
2. **中期**：在有 Wayland 桌面环境时重新编译 fcitx5
3. **长期**：考虑同时支持 X11 + Wayland（fcitx5 原生支持双前端）
