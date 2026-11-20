#!/usr/bin/env python3
"""云拼音接口的参考实现 / 联调 mock 服务。

用途：
  1. 本机联调客户端（`./cloud_pinyin_test http://127.0.0.1:8765/pinyin`）；
  2. 作为第三方自建云拼音服务的协议样板（对照 cpp/cloud_pinyin.h 的注释）。

协议：
  POST /pinyin
  Authorization: Bearer <token>（设置了 --token 时校验）
  {"pinyin":"nihao","limit":9,"client":"pinyin-ime","version":"0.9.175"}
  ->
  {"candidates":[{"chinese":"你好","pinyin":"ni hao","confidence":95}]}

只依赖标准库，不读任何语料文件。
"""
import argparse
import json
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

# 联调用的最小词表（通用词，按拼音索引）
DEMO_DICT = {
    "nihao": [("你好", "ni hao", 95), ("你", "ni", 40)],
    "zhongguo": [("中国", "zhong guo", 95), ("中", "zhong", 30)],
    "beijing": [("北京", "bei jing", 93)],
    "shurufa": [("输入法", "shu ru fa", 90)],
    "pinyin": [("拼音", "pin yin", 88)],
    # 本地词典里没有的生僻组合，用来验证「云候选出现在候选窗尾部并可点选」
    "xuanfutianqi": [("炫福天气", "xuan fu tian qi", 99)],
}

TOKEN = None


class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):  # 安静一点，只打关键行
        sys.stderr.write("[mock] " + (fmt % args) + "\n")

    def _json(self, code, obj):
        body = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path in ("/health", "/"):
            self._json(200, {"ok": True, "service": "mock-cloud-pinyin"})
        else:
            self._json(404, {"error": "not found"})

    def do_POST(self):
        if self.path != "/pinyin":
            self._json(404, {"error": "not found"})
            return
        if TOKEN:
            auth = self.headers.get("Authorization", "")
            if auth != f"Bearer {TOKEN}":
                self._json(401, {"error": "bad token"})
                return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            req = json.loads(self.rfile.read(length) or b"{}")
        except Exception:
            self._json(400, {"error": "bad json"})
            return

        pinyin = (req.get("pinyin") or "").strip().lower()
        limit = int(req.get("limit") or 9)
        rows = DEMO_DICT.get(pinyin, [])
        cands = [
            {"chinese": w, "pinyin": p, "confidence": c} for w, p, c in rows[:limit]
        ]
        self._json(200, {"candidates": cands, "query": pinyin})


def main():
    global TOKEN
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=8765)
    ap.add_argument("--bind", default="127.0.0.1")
    ap.add_argument("--token", default="")
    a = ap.parse_args()
    TOKEN = a.token or None
    srv = ThreadingHTTPServer((a.bind, a.port), Handler)
    print(f"mock cloud pinyin at http://{a.bind}:{a.port}/pinyin (token={bool(TOKEN)})")
    try:
        srv.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
