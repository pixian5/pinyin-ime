#!/usr/bin/env python3
"""
pinyin-ime daemon: Unix socket server with persistent CLI process.
Keeps the model loaded in memory, eliminating cold start (~33ms).

Protocol (line-based, newline-terminated):
  INPUT <pinyin>     → type pinyin, get candidates (multi-line)
  TOP1 <pinyin>      → type pinyin, return top-1 candidate only
  TOPK <n> <pinyin>  → type pinyin, return top-n candidates (tab-separated)
  BATCH <file>       → batch process file, return top-1 results
  PREDICT <text>     → predict next word from Chinese text
  LEARN <pinyin>\t<chinese>  → learn a pinyin→chinese pair
  PING               → return PONG
  STATUS             → return server status (JSON)
  QUIT               → shutdown server

Usage:
  python3 scripts/pinyin_daemon.py start [--socket /tmp/pinyin-ime.sock]
  python3 scripts/pinyin_daemon.py stop  [--socket /tmp/pinyin-ime.sock]
  python3 scripts/pinyin_daemon.py status [--socket /tmp/pinyin-ime.sock]
"""
import sys, os, subprocess, socket, signal, json, time, argparse, atexit, select, threading

DEFAULT_SOCKET = "/tmp/pinyin-ime.sock"
PID_FILE = "/tmp/pinyin-ime-daemon.pid"

def find_cli():
    candidates = [
        os.path.join(os.path.dirname(__file__), '..', 'cpp', 'build', 'pinyin_ime_cli'),
        '/usr/local/bin/pinyin_ime_cli',
        '/usr/bin/pinyin_ime_cli',
    ]
    for c in candidates:
        c = os.path.abspath(c)
        if os.path.isfile(c) and os.access(c, os.X_OK):
            return c
    return None

def find_model_files():
    nb = "/root/workspace/native-build/opt"
    candidates = [
        (f"{nb}/libime/lib/libime/zh_CN.lm", f"{nb}/libime/share/libime/sc.dict"),
        ("/usr/share/libime/zh_CN.lm", "/usr/share/libime/sc.dict"),
    ]
    for lm, dp in candidates:
        if os.path.isfile(lm) and os.path.isfile(dp):
            return lm, dp
    return None, None

def get_env():
    nb = "/root/workspace/native-build"
    env = os.environ.copy()
    env['PATH'] = f"{nb}/mamba-env/bin:{env.get('PATH', '')}"
    env['LD_LIBRARY_PATH'] = f"{nb}/mamba-env/lib:{nb}/opt/fcitx5/lib:{nb}/opt/libime/lib:{env.get('LD_LIBRARY_PATH', '')}"
    return env

class PinyinIMEDaemon:
    def __init__(self, socket_path=DEFAULT_SOCKET, cli_path=None, lm_path=None, dict_path=None):
        self.socket_path = socket_path
        self.cli_path = cli_path or find_cli()
        self.lm_path, self.dict_path = lm_path or None, dict_path or None
        if not self.lm_path or not self.dict_path:
            self.lm_path, self.dict_path = find_model_files()
        
        if not self.cli_path:
            raise RuntimeError("Cannot find pinyin_ime_cli")
        if not self.lm_path or not self.dict_path:
            raise RuntimeError("Cannot find model files")
        
        self.running = False
        self.request_count = 0
        self.freq_tracker = UserFreqTracker() if UserFreqTracker else None
        self.corrector = PostCorrector() if PostCorrector else None
        self.start_time = 0
        self.env = get_env()
    
    def _run_cli(self, pinyin, extra_args, timeout=10):
        """Run CLI with given pinyin input and return output."""
        try:
            result = subprocess.run(
                [self.cli_path] + extra_args + [self.lm_path, self.dict_path],
                input=pinyin, capture_output=True, text=True, timeout=timeout, env=self.env
            )
            return result.stdout.strip()
        except subprocess.TimeoutExpired:
            return "TIMEOUT"
        except Exception as e:
            return f"ERROR: {e}"
    
    def _run_batch(self, filepath, timeout=120):
        """Run CLI in batch mode."""
        try:
            result = subprocess.run(
                [self.cli_path, '--quiet', '--batch', filepath, '--top1',
                 self.lm_path, self.dict_path],
                capture_output=True, text=True, timeout=timeout, env=self.env
            )
            return result.stdout.strip()
        except subprocess.TimeoutExpired:
            return "TIMEOUT"
        except Exception as e:
            return f"ERROR: {e}"
    
    def start(self):
        """Start the daemon server."""
        # Check if already running
        if os.path.exists(self.socket_path):
            try:
                s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
                s.connect(self.socket_path)
                s.sendall(b"PING\n")
                resp = s.recv(1024).decode().strip()
                s.close()
                if resp == "PONG":
                    print(f"Daemon already running on {self.socket_path}")
                    return
            except:
                pass
            os.unlink(self.socket_path)
        
        self.running = True
        self.start_time = time.time()
        
        # Warm up: preload model
        warmup = self._run_cli("nihao", ['--quiet', '--top1'])
        
        # Create Unix socket server
        server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        server.bind(self.socket_path)
        server.listen(10)
        os.chmod(self.socket_path, 0o666)
        
        # Write PID file
        with open(PID_FILE, 'w') as f:
            f.write(str(os.getpid()))
        
        atexit.register(self.cleanup)
        
        print(f"pinyin-ime daemon v0.9.65 started on {self.socket_path}")
        print(f"  CLI:   {self.cli_path}")
        print(f"  LM:    {self.lm_path}")
        print(f"  Dict:  {self.dict_path}")
        print(f"  Warmup: {warmup}")
        
        signal.signal(signal.SIGTERM, lambda s, f: self._shutdown())
        try:
            while self.running:
                server.settimeout(1.0)
                try:
                    conn, _ = server.accept()
                except socket.timeout:
                    continue
                except OSError:
                    break
                
                try:
                    self._handle_client(conn)
                except Exception as e:
                    pass
                finally:
                    try:
                        conn.close()
                    except:
                        pass
        except KeyboardInterrupt:
            pass
        
        self.cleanup()
        print("Daemon stopped")
    
    def _handle_client(self, conn):
        """Handle a client connection."""
        data = conn.recv(8192).decode().strip()
        if not data:
            return
        
        self.request_count += 1
        parts = data.split(None, 1)
        cmd = parts[0].upper()
        arg = parts[1] if len(parts) > 1 else ""
        
        if cmd == "PING":
            conn.sendall(b"PONG\n")
        elif cmd == "STATUS":
            uptime = time.time() - self.start_time
            status = json.dumps({
                "version": "0.9.65",
                "uptime": round(uptime, 1),
                "requests": self.request_count,
                "socket": self.socket_path,
                "cli": self.cli_path,
                "lm_size_mb": round(os.path.getsize(self.lm_path) / 1048576, 1),
                "dict_size_mb": round(os.path.getsize(self.dict_path) / 1048576, 1),
            }, ensure_ascii=False)
            conn.sendall(f"{status}\n".encode())
        elif cmd == "QUIT":
            conn.sendall(b"OK\n")
            self.running = False
        elif cmd == "TOP1":
            result = self._run_cli(arg, ['--quiet', '--top1'])
            conn.sendall(f"{result}\n".encode())
        elif cmd == "INPUT":
            result = self._run_cli(arg, ['--quiet'])
            conn.sendall(f"{result}\n".encode())
        elif cmd == "TOPK":
            sub = arg.split(None, 1)
            if len(sub) >= 2:
                n, py = sub[0], sub[1]
                result = self._run_cli(py, ['--quiet', '--top-k', n])
            else:
                result = ""
            conn.sendall(f"{result}\n".encode())
        elif cmd == "BATCH":
            # BATCH <file> — process file, return all top-1 results
            if os.path.isfile(arg):
                result = self._run_batch(arg)
            else:
                result = f"ERROR: File not found: {arg}"
            conn.sendall(f"{result}\n".encode())
        elif cmd == "PREDICT":
            result = self._run_cli(arg, ['--quiet', '--predict'])
            conn.sendall(f"{result}\n".encode())
        elif cmd == "LEARN":
            if '\t' in arg:
                py, ch = arg.split('\t', 1)
                try:
                    subprocess.run(
                        [self.cli_path, '--add-user', f'{py}={ch}', '--quiet',
                         self.lm_path, self.dict_path],
                        capture_output=True, text=True, timeout=10, env=self.env
                    )
                    conn.sendall(b"OK\n")
                except Exception as e:
                    conn.sendall(f"ERROR: {e}\n".encode())
            else:
                conn.sendall(b"ERROR: LEARN requires pinyin<TAB>chinese\n")
        elif cmd == "CORRECT":
            # CORRECT <text> — apply post-correction to text
            if self.corrector and arg:
                corrected = self.corrector.correct(arg)
                conn.sendall(f"{corrected}\n".encode())
            else:
                conn.sendall(b"ERROR: No corrector\n")
        elif cmd == "FREQ":
            # FREQ <word> — record word usage
            if self.freq_tracker and arg:
                self.freq_tracker.record(arg)
                conn.sendall(b"OK\n")
            else:
                conn.sendall(b"ERROR: No freq tracker\n")
        elif cmd == "FREQ_TOP":
            # FREQ_TOP [N] — top N frequent words
            if self.freq_tracker:
                n = int(arg) if arg else 20
                top = self.freq_tracker.top_words(n)
                lines = [f"{w}\t{c}" for w, c in top]
                conn.sendall(("\n".join(lines) + "\n").encode())
            else:
                conn.sendall(b"ERROR: No freq tracker\n")
        elif cmd == "FREQ_STATS":
            if self.freq_tracker:
                import json
                stats = self.freq_tracker.stats()
                conn.sendall((json.dumps(stats, ensure_ascii=False) + "\n").encode())
            else:
                conn.sendall(b"ERROR: No freq tracker\n")
        else:
            conn.sendall(f"ERROR: Unknown command {cmd}\n".encode())
    
    def cleanup(self):
        if self.freq_tracker:
            self.freq_tracker.save()
        for p in [self.socket_path, PID_FILE]:
            if os.path.exists(p):
                try:
                    os.unlink(p)
                except:
                    pass
    
    def _shutdown(self):
        self.running = False

def send_command(socket_path, command):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(socket_path)
    s.sendall(f"{command}\n".encode())
    resp = s.recv(65536).decode().strip()
    s.close()
    return resp

def cmd_stop(socket_path):
    try:
        resp = send_command(socket_path, "QUIT")
        print(f"Daemon response: {resp}")
    except:
        if os.path.exists(PID_FILE):
            with open(PID_FILE) as f:
                pid = int(f.read().strip())
            os.kill(pid, signal.SIGTERM)
            print(f"Sent SIGTERM to PID {pid}")
        else:
            print("Daemon not running")

def cmd_status(socket_path):
    try:
        resp = send_command(socket_path, "STATUS")
        data = json.loads(resp)
        for k, v in data.items():
            print(f"  {k}: {v}")
    except:
        print("Daemon not running")

def main():
    parser = argparse.ArgumentParser(description="pinyin-ime daemon")
    parser.add_argument("action", choices=["start", "stop", "status"])
    parser.add_argument("--socket", default=DEFAULT_SOCKET)
    parser.add_argument("--cli", help="Path to pinyin_ime_cli")
    parser.add_argument("--lm", help="Path to language model")
    parser.add_argument("--dict", help="Path to dictionary")
    args = parser.parse_args()
    
    if args.action == "start":
        daemon = PinyinIMEDaemon(
            socket_path=args.socket,
            cli_path=args.cli,
            lm_path=args.lm,
            dict_path=args.dict,
        )
        daemon.start()
    elif args.action == "stop":
        cmd_stop(args.socket)
    elif args.action == "status":
        cmd_status(args.socket)

if __name__ == '__main__':
    main()
