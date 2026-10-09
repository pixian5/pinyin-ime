#!/usr/bin/env python3
"""
pinyin-ime 配置加载器
支持 YAML 配置文件，用户配置优先于全局配置，支持热重载。

配置搜索路径（优先级从高到低）:
  1. ~/.config/pinyin-ime/config.yaml (用户)
  2. /etc/pinyin-ime/config.yaml (全局)
  3. <project>/config/pinyin-ime.yaml (默认)
"""
import os, yaml, copy, time

# 默认配置（内嵌，无配置文件时使用）
DEFAULT_CONFIG = {
    'basic': {
        'nbest': 45,
        'page_size': 9,
        'data_dir': '/usr/share/libime',
        'user_data_dir': '~/.local/share/pinyin-ime',
    },
    'fuzzy': {
        'enabled': False,
        'z_zh': True, 'c_ch': True, 's_sh': True,
        'an_ang': True, 'en_eng': True, 'in_ing': True,
        'ian_iang': False, 'uan_uang': False,
        'n_l': False, 'f_h': False, 'l_r': False,
    },
    'correction': {
        'enabled': False,
        'keyboard_adjacent': True,
        'common_typos': True,
    },
    'shuangpin': {
        'enabled': False,
        'scheme': 'xiaohe',
    },
    'abbreviation': {
        'enabled': True,
        'boost': True,
    },
    'learning': {
        'enabled': True,
        'history_weight': 1.0,
        'auto_learn_threshold': 3,
        'max_user_dict_entries': 10000,
    },
    'frequency': {
        'enabled': True,
        'freq_file': '~/.local/share/pinyin-ime/user.freq',
        'decay_factor': 0.95,
        'boost_weight': 2.0,
    },
    'post_correction': {
        'enabled': True,
        'homophone_fix': True,
        'measure_word_fix': True,
        'common_mistake_fix': True,
    },
    'performance': {
        'preload': True,
        'batch_timeout': 120,
        'request_timeout': 10,
    },
}

CONFIG_SEARCH_PATHS = [
    os.path.expanduser("~/.config/pinyin-ime/config.yaml"),
    "/etc/pinyin-ime/config.yaml",
    os.path.join(os.path.dirname(__file__), '..', 'config', 'pinyin-ime.yaml'),
]

class Config:
    """pinyin-ime 配置管理器"""
    
    def __init__(self, config_path=None):
        self._config = copy.deepcopy(DEFAULT_CONFIG)
        self._config_path = config_path
        self._mtime = 0
        self._load_time = 0
        self.load()
    
    def _find_config_file(self):
        """查找配置文件"""
        if self._config_path and os.path.isfile(self._config_path):
            return self._config_path
        for path in CONFIG_SEARCH_PATHS:
            if os.path.isfile(path):
                return path
        return None
    
    def load(self):
        """加载配置文件"""
        config_file = self._find_config_file()
        if config_file:
            try:
                with open(config_file) as f:
                    user_config = yaml.safe_load(f) or {}
                self._merge(self._config, user_config)
                self._config_path = config_file
                self._mtime = os.path.getmtime(config_file)
                self._load_time = time.time()
            except Exception as e:
                print(f"Warning: Failed to load config {config_file}: {e}")
    
    def reload_if_changed(self):
        """如果配置文件修改则重新加载"""
        if self._config_path and os.path.isfile(self._config_path):
            mtime = os.path.getmtime(self._config_path)
            if mtime > self._mtime:
                self._config = copy.deepcopy(DEFAULT_CONFIG)
                self.load()
                return True
        return False
    
    def _merge(self, base, override):
        """深度合并字典"""
        for k, v in override.items():
            if k in base and isinstance(base[k], dict) and isinstance(v, dict):
                self._merge(base[k], v)
            else:
                base[k] = v
    
    def get(self, key, default=None):
        """获取配置值，支持点号分隔路径: get('fuzzy.enabled')"""
        keys = key.split('.')
        value = self._config
        for k in keys:
            if isinstance(value, dict) and k in value:
                value = value[k]
            else:
                return default
        return value
    
    def set(self, key, value):
        """设置配置值"""
        keys = key.split('.')
        config = self._config
        for k in keys[:-1]:
            if k not in config:
                config[k] = {}
            config = config[k]
        config[keys[-1]] = value
    
    def save(self, path=None):
        """保存配置到文件"""
        save_path = path or self._config_path or CONFIG_SEARCH_PATHS[0]
        os.makedirs(os.path.dirname(save_path), exist_ok=True)
        with open(save_path, 'w') as f:
            yaml.dump(self._config, f, default_flow_style=False, allow_unicode=True)
    
    @property
    def fuzzy_enabled(self):
        return self.get('fuzzy.enabled', False)
    
    @property
    def correction_enabled(self):
        return self.get('correction.enabled', False)
    
    @property
    def shuangpin_enabled(self):
        return self.get('shuangpin.enabled', False)
    
    @property
    def shuangpin_scheme(self):
        return self.get('shuangpin.scheme', 'xiaohe')
    
    @property
    def learning_enabled(self):
        return self.get('learning.enabled', True)
    
    @property
    def history_weight(self):
        return self.get('learning.history_weight', 1.0)
    
    @property
    def frequency_enabled(self):
        return self.get('frequency.enabled', True)
    
    @property
    def post_correction_enabled(self):
        return self.get('post_correction.enabled', True)
    
    def to_cli_args(self):
        """转换为CLI命令行参数"""
        args = []
        if self.fuzzy_enabled:
            args.append('--fuzzy')
        if self.correction_enabled:
            args.append('--correct')
        if self.shuangpin_enabled:
            args.extend(['--shuangpin', '--sp-scheme', self.shuangpin_scheme])
        hw = self.history_weight
        if hw != 1.0:
            args.extend(['--history-weight', str(hw)])
        return args
    
    def __repr__(self):
        return f"Config(fuzzy={self.fuzzy_enabled}, correction={self.correction_enabled}, shuangpin={self.shuangpin_enabled})"

# Singleton
_config = None

def get_config(config_path=None):
    """获取全局配置实例"""
    global _config
    if _config is None or config_path:
        _config = Config(config_path)
    return _config

if __name__ == '__main__':
    config = Config()
    print("=== pinyin-ime 配置 ===")
    print(yaml.dump(config._config, default_flow_style=False, allow_unicode=True))
    print(f"CLI args: {config.to_cli_args()}")
    print(f"Config repr: {config}")
