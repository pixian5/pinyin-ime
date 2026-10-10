#!/bin/bash
# pinyin-ime 系统级一键安装脚本
# 支持 openEuler / CentOS / RHEL / Ubuntu / Debian
#
# 用法:
#   bash scripts/install.sh
#
# 功能:
#   1. 检测系统环境和依赖
#   2. 下载预构建产物（S3 或本地 dist/）
#   3. 安装 fcitx5 + pinyinime addon
#   4. 配置输入法环境
#   5. 验证安装

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }
log_step() { echo -e "${BLUE}[STEP]${NC} $1"; }

# Sudo wrapper - use sudo if available, otherwise direct (already root)
SUDO=""
if command -v sudo &>/dev/null && sudo -n true 2>/dev/null; then
    SUDO="sudo"
elif [ "$(id -u)" = "0" ]; then
    SUDO=""
    log_info "Running as root, no sudo needed"
else
    log_warn "sudo not available and not root - may fail"
fi
log_step() { echo -e "${BLUE}[STEP]${NC} $1"; }

# Configuration
INSTALL_PREFIX="${INSTALL_PREFIX:-/usr/local}"
VERSION="${VERSION:-0.9.81}"
S3_BASE="cst:xbz/pinyin-ime"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Detect OS
detect_os() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        OS_ID="${ID}"
        OS_VERSION="${VERSION_ID}"
    elif [ -f /etc/redhat-release ]; then
        OS_ID="centos"
        OS_VERSION="7"
    else
        OS_ID="unknown"
        OS_VERSION="unknown"
    fi
    log_info "Detected OS: $OS_ID $OS_VERSION"
}

# Check dependencies
check_deps() {
    log_step "Checking dependencies..."
    
    local missing=()
    
    # Check for fcitx5 (either system or our build)
    local fcitx5_found=0
    if command -v fcitx5 &>/dev/null; then
        fcitx5_found=1
        log_info "fcitx5 found: $(command -v fcitx5)"
    elif [ -f "$PROJECT_ROOT/dist/fcitx5" ]; then
        fcitx5_found=1
        log_info "fcitx5 found in dist/ (pre-built)"
    fi
    
    # Check for DBus
    if ! command -v dbus-daemon &>/dev/null && ! command -v dbus-run-session &>/dev/null; then
        missing+=("dbus")
    fi
    
    # Check for X11 libraries
    if ! ldconfig -p 2>/dev/null | grep -q "libX11.so"; then
        missing+=("libX11")
    fi
    
    if [ ${#missing[@]} -gt 0 ]; then
        log_warn "Missing dependencies: ${missing[*]}"
        log_info "Attempting to install..."
        
        case "$OS_ID" in
            ubuntu|debian)
                $SUDO apt-get update
                $SUDO apt-get install -y libxcb1 libx11-6 libxkbcommon-x11-0 \
                    dbus-x11 fonts-noto-cjk
                ;;
            centos|rhel|fedora|openEuler)
                if command -v dnf &>/dev/null; then
                    $SUDO dnf install -y libX11 libxcb libxkbcommon-x11 \
                        dbus dconf
                else
                    $SUDO yum install -y libX11 libxcb libxkbcommon-x11 \
                        dbus dconf
                fi
                ;;
            *)
                log_error "Unsupported OS: $OS_ID"
                log_info "Please install X11 and DBus libraries manually"
                return 1
                ;;
        esac
    fi
    
    if [ $fcitx5_found -eq 0 ]; then
        log_warn "fcitx5 not found - will use pre-built binary from dist/"
    fi
    
    log_info "Dependencies OK"
}

# Download artifacts
download_artifacts() {
    log_step "Downloading pinyin-ime artifacts..."
    
    local dist_dir="$PROJECT_ROOT/dist"
    local temp_dir="/tmp/pinyin-ime-install"
    
    # Check if local dist exists
    if [ -f "$dist_dir/pinyin_ime_cli" ] && [ -f "$dist_dir/libpinyinime.so" ]; then
        log_info "Using local dist/ directory"
        return 0
    fi
    
    # Try S3 download
    mkdir -p "$temp_dir"
    
    if command -v rclone &>/dev/null; then
        log_info "Downloading from S3 (version: $VERSION)..."
        if rclone copy "$S3_BASE/v$VERSION/" "$temp_dir/" \
            --user-agent "rclone/1.75.1 pinyin-ime" 2>/dev/null; then
            
            # Verify checksums
            if [ -f "$temp_dir/SHA256SUMS_v$VERSION" ]; then
                cd "$temp_dir"
                sha256sum -c "SHA256SUMS_v$VERSION" 2>/dev/null || {
                    log_error "Checksum verification failed"
                    return 1
                }
            fi
            
            # Copy to dist
            mkdir -p "$dist_dir"
            cp "$temp_dir/pinyin_ime_cli_v$VERSION" "$dist_dir/pinyin_ime_cli" 2>/dev/null || true
            cp "$temp_dir/libpinyinime_v$VERSION.so" "$dist_dir/libpinyinime.so" 2>/dev/null || true
            cp "$temp_dir/fcitx5_v$VERSION" "$dist_dir/fcitx5" 2>/dev/null || true
            cp "$temp_dir/zh_CN.lm" "$dist_dir/" 2>/dev/null || true
            cp "$temp_dir/sc.dict" "$dist_dir/" 2>/dev/null || true
            
            log_info "Download complete"
            rm -rf "$temp_dir"
            return 0
        fi
    fi
    
    log_error "Cannot download artifacts"
    log_info "Please provide dist/ directory with pre-built binaries"
    return 1
}

# Install binaries
install_binaries() {
    log_step "Installing binaries..."
    
    local dist_dir="$PROJECT_ROOT/dist"
    
    # Create directories
    $SUDO mkdir -p "$INSTALL_PREFIX/bin"
    $SUDO mkdir -p "$INSTALL_PREFIX/lib/pinyin-ime"
    $SUDO mkdir -p "$INSTALL_PREFIX/share/pinyin-ime"
    $SUDO mkdir -p "$INSTALL_PREFIX/share/fcitx5/addon"
    $SUDO mkdir -p "$INSTALL_PREFIX/share/fcitx5/inputmethod"
    $SUDO mkdir -p "$INSTALL_PREFIX/lib/fcitx5"
    
    # Install fcitx5 binary (if available in dist/)
    if [ -f "$dist_dir/fcitx5" ]; then
        $SUDO install -m 755 "$dist_dir/fcitx5" "$INSTALL_PREFIX/bin/fcitx5"
        log_info "Installed: $INSTALL_PREFIX/bin/fcitx5 (pre-built)"
    fi
    
    # Install CLI
    if [ -f "$dist_dir/pinyin_ime_cli" ]; then
        $SUDO install -m 755 "$dist_dir/pinyin_ime_cli" "$INSTALL_PREFIX/bin/pinyin-ime"
        log_info "Installed: $INSTALL_PREFIX/bin/pinyin-ime"
    fi
    
    # Install addon
    if [ -f "$dist_dir/libpinyinime.so" ]; then
        $SUDO install -m 755 "$dist_dir/libpinyinime.so" "$INSTALL_PREFIX/lib/fcitx5/"
        log_info "Installed: $INSTALL_PREFIX/lib/fcitx5/libpinyinime.so"
    fi
    
    # Install data files
    if [ -f "$dist_dir/zh_CN.lm" ]; then
        $SUDO install -m 644 "$dist_dir/zh_CN.lm" "$INSTALL_PREFIX/share/pinyin-ime/"
        log_info "Installed: zh_CN.lm"
    fi
    
    if [ -f "$dist_dir/sc.dict" ]; then
        $SUDO install -m 644 "$dist_dir/sc.dict" "$INSTALL_PREFIX/share/pinyin-ime/"
        log_info "Installed: sc.dict"
    fi
    
    # Install addon config
    if [ -f "$SCRIPT_DIR/../fcitx5-addon/pinyinime.conf" ]; then
        $SUDO install -m 644 "$SCRIPT_DIR/../fcitx5-addon/pinyinime.conf" \
            "$INSTALL_PREFIX/share/fcitx5/addon/"
        log_info "Installed: pinyinime.conf"
    fi
    
    if [ -f "$SCRIPT_DIR/../fcitx5-addon/pinyinime.inputmethod" ]; then
        $SUDO install -m 644 "$SCRIPT_DIR/../fcitx5-addon/pinyinime.inputmethod" \
            "$INSTALL_PREFIX/share/fcitx5/inputmethod/"
        log_info "Installed: pinyinime.inputmethod"
    fi
    
    log_info "Binaries installed"
}

# Configure environment
configure_env() {
    log_step "Configuring environment..."
    
    # Set environment variables
    local env_file="/etc/profile.d/pinyin-ime.sh"
    
    # Try system location first, fallback to user location
    if ! $SUDO tee "$env_file" > /dev/null 2>/dev/null << EOF
# pinyin-ime environment
export PATH="\$PATH:$INSTALL_PREFIX/bin"
export FCITX_ADDON_DIRS="\$FCITX_ADDON_DIRS:$INSTALL_PREFIX/lib/fcitx5"
export FCITX_ADDON_CONFIG_DIRS="\$FCITX_ADDON_CONFIG_DIRS:$INSTALL_PREFIX/share/fcitx5/addon"
export FCITX_DATA_DIR="\$FCITX_DATA_DIR:$INSTALL_PREFIX/share/fcitx5"
export PINYIN_IME_DATA_DIR="$INSTALL_PREFIX/share/pinyin-ime"
EOF
    then
        # Fallback to user location
        env_file="$HOME/.pinyin-ime-env"
        cat > "$env_file" << EOF
# pinyin-ime environment
export PATH="\$PATH:$INSTALL_PREFIX/bin"
export FCITX_ADDON_DIRS="\$FCITX_ADDON_DIRS:$INSTALL_PREFIX/lib/fcitx5"
export FCITX_ADDON_CONFIG_DIRS="\$FCITX_ADDON_CONFIG_DIRS:$INSTALL_PREFIX/share/fcitx5/addon"
export FCITX_DATA_DIR="\$FCITX_DATA_DIR:$INSTALL_PREFIX/share/fcitx5"
export PINYIN_IME_DATA_DIR="$INSTALL_PREFIX/share/pinyin-ime"
EOF
        log_info "Environment configured: $env_file (user location)"
        log_info "Add 'source $env_file' to your ~/.bashrc"
    else
        log_info "Environment configured: $env_file"
    fi
    
    # Create fcitx5 profile
    local fcitx5_config="$HOME/.config/fcitx5"
    mkdir -p "$fcitx5_config"
    
    if [ ! -f "$fcitx5_config/profile" ]; then
        cat > "$fcitx5_config/profile" << 'EOF'
[Groups/0]
Name=默认
Default Layout=us
DefaultIM=pinyinime

[Groups/0/Items/0]
Name=keyboard-us
Layout=

[Groups/0/Items/1]
Name=pinyinime
Layout=

[GroupOrder]
0=默认
EOF
        log_info "Created fcitx5 profile"
    fi
    
    # Set input method environment
    local im_env="/etc/profile.d/input-method.sh"
    if ! $SUDO tee "$im_env" > /dev/null 2>/dev/null << 'EOF'
# Input method environment
export GTK_IM_MODULE=fcitx
export QT_IM_MODULE=fcitx
export XMODIFIERS=@im=fcitx
export INPUT_METHOD=fcitx
EOF
    then
        # Fallback to user location
        im_env="$HOME/.input-method-env"
        cat > "$im_env" << 'EOF'
# Input method environment
export GTK_IM_MODULE=fcitx
export QT_IM_MODULE=fcitx
export XMODIFIERS=@im=fcitx
export INPUT_METHOD=fcitx
EOF
        log_info "Input method environment: $im_env (user location)"
        log_info "Add 'source $im_env' to your ~/.bashrc"
    else
        log_info "Input method environment configured"
    fi
}

# Verify installation
verify_install() {
    log_step "Verifying installation..."
    
    # Check CLI
    local cli_path="$INSTALL_PREFIX/bin/pinyin-ime"
    if [ -x "$cli_path" ]; then
        local version=$("$cli_path" --version 2>&1 | head -1)
        log_info "CLI: $version"
    elif command -v pinyin-ime &>/dev/null; then
        local version=$(pinyin-ime --version 2>&1 | head -1)
        log_info "CLI: $version"
    else
        log_error "CLI not found"
        return 1
    fi
    
    # Check addon
    if [ -f "$INSTALL_PREFIX/lib/fcitx5/libpinyinime.so" ]; then
        log_info "Addon: installed"
    else
        log_error "Addon not found"
        return 1
    fi
    
    # Check data files
    if [ -f "$INSTALL_PREFIX/share/pinyin-ime/zh_CN.lm" ] && \
       [ -f "$INSTALL_PREFIX/share/pinyin-ime/sc.dict" ]; then
        log_info "Data files: installed"
    else
        log_warn "Data files not found (may need manual download)"
    fi
    
    log_info "Installation verified"
}

# Print usage instructions
print_usage() {
    echo ""
    echo "=========================================="
    echo "pinyin-ime Installation Complete!"
    echo "=========================================="
    echo ""
    echo "Usage:"
    echo "  1. Restart your session or run:"
    echo "     source /etc/profile.d/pinyin-ime.sh"
    echo "     source /etc/profile.d/input-method.sh"
    echo ""
    echo "  2. Start fcitx5:"
    echo "     fcitx5 -d"
    echo ""
    echo "  3. Add pinyinime to input method list:"
    echo "     fcitx5-configtool"
    echo ""
    echo "  4. Test CLI:"
    echo "     echo 'nihao' | pinyin-ime"
    echo ""
    echo "Environment variables:"
    echo "  GTK_IM_MODULE=fcitx"
    echo "  QT_IM_MODULE=fcitx"
    echo "  XMODIFIERS=@im=fcitx"
    echo ""
    echo "For X11 applications, set:"
    echo "  export XMODIFIERS=@im=fcitx"
    echo ""
    echo "For Wayland applications, set:"
    echo "  export GTK_IM_MODULE=fcitx"
    echo "  export QT_IM_MODULE=fcitx"
    echo ""
}

# Main
main() {
    echo "=========================================="
    echo "pinyin-ime Installer v$VERSION"
    echo "=========================================="
    echo ""
    
    detect_os
    
    # Parse options
    SKIP_DEPS=0
    for arg in "$@"; do
        case $arg in
            --skip-deps)
                SKIP_DEPS=1
                log_info "Skipping dependency installation"
                ;;
            --prefix=*)
                INSTALL_PREFIX="${arg#*=}"
                log_info "Install prefix: $INSTALL_PREFIX"
                ;;
            --version=*)
                VERSION="${arg#*=}"
                log_info "Version: $VERSION"
                ;;
        esac
    done
    
    if [ $SKIP_DEPS -eq 0 ]; then
        check_deps
    fi
    
    download_artifacts
    install_binaries
    configure_env
    verify_install
    print_usage
    
    log_info "Installation successful!"
}

# Run main
main "$@"
