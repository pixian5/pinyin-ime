#!/usr/bin/env python3
"""
pinyin-ime client library: Python API for the pinyin-ime daemon.
Can also work standalone (spawning CLI processes) when daemon is not running.

Usage:
  from pinyin_client import PinyinIME
  
  # Auto-detect daemon or fall back to CLI
  ime = PinyinIME()
  
  # Or connect to specific daemon
  ime = PinyinIME(socket="/tmp/pinyin-ime.sock")
  
  # Or use CLI directly
  ime = PinyinIME(use_daemon=False)
  
  # API
  result = ime.top1("nihao")          # "你好"
  candidates = ime.input("nihao")     # ["你好", "你毫", "拟好", ...]
  top5 = ime.top_k("nihao", 5)        # ["你好", "你毫", "拟好", "你号", "你耗"]
  next_words = ime.predict("你好")    # ["吗", "啊", "！"]
  ime.learn("pianzi", "片子")         # Add to user dict
"""
import os, socket, subprocess, shutil

DEFAULT_SOCKET = "/tmp/pinyin-ime.sock"

class PinyinIME:
    def __init__(self, socket=DEFAULT_SOCKET, use_daemon=True, cli_path=None, lm_path=None, dict_path=None):
        self.socket_path = socket
        self.cli_path = cli_path or self._find_cli()
        self.lm_path = lm_path
        self.dict_path = dict_path
        if not self.lm_path or not self.dict_path:
            self.lm_path, self.dict_path = self._find_model()
        
        # Try to connect to daemon
        self.daemon_available = False
        if use_daemon:
            try:
                s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                s.connect(self.socket_path)
                s.sendall(b"PING\n")
                resp = s.recv(1024).decode().strip()
                s.close()
                self.daemon_available = (resp == "PONG")
            except:
                self.daemon_available = False
        
        self._env = self._get_env()
    
    def _find_cli(self):
        candidates = [
            os.path.join(os.path.dirname(__file__), '..', 'cpp', 'build', 'pinyin_ime_cli'),
            '/usr/local/bin/pinyin_ime_cli',
            '/usr/bin/pinyin_ime_cli',
            shutil.which('pinyin_ime_cli'),
        ]
        for c in candidates:
            if c and os.path.isfile(c) and os.access(c, os.X_OK):
                return os.path.abspath(c)
        return None
    
    def _find_model(self):
        nb = "/root/workspace/native-build/opt"
        candidates = [
            (f"{nb}/libime/lib/libime/zh_CN.lm", f"{nb}/libime/share/libime/sc.dict"),
            ("/usr/share/libime/zh_CN.lm", "/usr/share/libime/sc.dict"),
        ]
        for lm, dp in candidates:
            if os.path.isfile(lm) and os.path.isfile(dp):
                return lm, dp
        return None, None
    
    def _get_env(self):
        nb = "/root/workspace/native-build"
        env = os.environ.copy()
        env['PATH'] = f"{nb}/mamba-env/bin:{env.get('PATH', '')}"
        env['LD_LIBRARY_PATH'] = f"{nb}/mamba-env/lib:{nb}/opt/fcitx5/lib:{nb}/opt/libime/lib:{env.get('LD_LIBRARY_PATH', '')}"
        return env
    
    def _daemon_cmd(self, cmd):
        """Send command to daemon and return response."""
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.connect(self.socket_path)
        s.sendall(f"{cmd}\n".encode())
        resp = s.recv(65536).decode().strip()
        s.close()
        return resp
    
    def _cli_cmd(self, pinyin, extra_args, timeout=10):
        """Run CLI directly."""
        result = subprocess.run(
            [self.cli_path] + extra_args + [self.lm_path, self.dict_path],
            input=pinyin, capture_output=True, text=True, timeout=timeout, env=self._env
        )
        return result.stdout.strip()
    
    def top1(self, pinyin):
        """Get top-1 candidate for pinyin input."""
        if self.daemon_available:
            return self._daemon_cmd(f"TOP1 {pinyin}")
        return self._cli_cmd(pinyin, ['--quiet', '--top1'])
    
    def input(self, pinyin):
        """Get all candidates for pinyin input."""
        if self.daemon_available:
            result = self._daemon_cmd(f"INPUT {pinyin}")
            return result.split('\n') if result else []
        result = self._cli_cmd(pinyin, ['--quiet'])
        return result.split('\n') if result else []
    
    def top_k(self, pinyin, k=5):
        """Get top-k candidates for pinyin input."""
        if self.daemon_available:
            result = self._daemon_cmd(f"TOPK {k} {pinyin}")
            return result.split('\t') if result else []
        result = self._cli_cmd(pinyin, ['--quiet', '--top-k', str(k)])
        return result.split('\t') if result else []
    
    def predict(self, text):
        """Predict next word from Chinese text."""
        if self.daemon_available:
            return self._daemon_cmd(f"PREDICT {text}")
        return self._cli_cmd(text, ['--quiet', '--predict'])
    
    def learn(self, pinyin, chinese):
        """Learn a pinyin→chinese pair."""
        if self.daemon_available:
            return self._daemon_cmd(f"LEARN {pinyin}\t{chinese}")
        subprocess.run(
            [self.cli_path, '--add-user', f'{pinyin}={chinese}', '--quiet',
             self.lm_path, self.dict_path],
            capture_output=True, text=True, timeout=10, env=self._env
        )
        return "OK"
    
    def status(self):
        """Get daemon status."""
        if self.daemon_available:
            import json
            resp = self._daemon_cmd("STATUS")
            return json.loads(resp)
        return {"daemon": False, "cli": self.cli_path}

# CLI interface
if __name__ == '__main__':
    import sys
    ime = PinyinIME()
    
    if len(sys.argv) < 2:
        print("Usage: pinyin_client.py <pinyin> [--top-k N] [--predict] [--learn py=zh]")
        sys.exit(1)
    
    if sys.argv[1] == "--status":
        print(ime.status())
    elif sys.argv[1] == "--learn" and len(sys.argv) >= 4:
        py, zh = sys.argv[2], sys.argv[3]
        print(ime.learn(py, zh))
    elif sys.argv[1] == "--predict" and len(sys.argv) >= 3:
        print(ime.predict(sys.argv[2]))
    elif "--top-k" in sys.argv:
        idx = sys.argv.index("--top-k")
        k = int(sys.argv[idx + 1])
        pinyin = sys.argv[1]
        results = ime.top_k(pinyin, k)
        print('\t'.join(results))
    else:
        pinyin = sys.argv[1]
        print(ime.top1(pinyin))
