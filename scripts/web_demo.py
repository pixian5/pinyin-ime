#!/usr/bin/env python3
"""
pinyin-ime Web 演示服务器
启动后在浏览器访问 http://localhost:8080

用法: python3 scripts/web_demo.py
"""

import http.server
import json
import os
import subprocess
import sys
import urllib.parse

# Setup environment
project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(project_root)

# Source setup_env.sh to get paths
result = subprocess.run(
    ["bash", "-c", "source ./setup_env.sh 2>/dev/null && echo \"$PINYIN_IME_CLI|$PINYIN_IME_LM|$PINYIN_IME_DICT\""],
    capture_output=True, text=True
)
parts = result.stdout.strip().split("|")
CLI = parts[0] if len(parts) > 0 else ""
LM = parts[1] if len(parts) > 1 else ""
DICT = parts[2] if len(parts) > 2 else ""

if not CLI or not os.path.exists(CLI):
    print("错误: CLI 未构建，请先运行 bash scripts/build_cli.sh")
    sys.exit(1)

PORT = 8080

HTML_PAGE = """<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>pinyin-ime 拼音输入法演示</title>
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body { font-family: 'PingFang SC', 'Microsoft YaHei', sans-serif; background: #f0f2f5; min-height: 100vh; }
.container { max-width: 800px; margin: 0 auto; padding: 20px; }
h1 { text-align: center; color: #333; margin: 20px 0; font-size: 24px; }
.subtitle { text-align: center; color: #888; margin-bottom: 30px; }
.input-area { background: white; border-radius: 12px; padding: 20px; box-shadow: 0 2px 8px rgba(0,0,0,0.1); margin-bottom: 20px; }
input[type="text"] { width: 100%; padding: 12px 16px; font-size: 18px; border: 2px solid #e0e0e0; border-radius: 8px; outline: none; transition: border-color 0.3s; }
input[type="text"]:focus { border-color: #4CAF50; }
.options { display: flex; gap: 15px; margin-top: 15px; flex-wrap: wrap; }
.option { display: flex; align-items: center; gap: 5px; font-size: 14px; color: #666; }
.option input { width: 16px; height: 16px; }
.candidates { background: white; border-radius: 12px; padding: 20px; box-shadow: 0 2px 8px rgba(0,0,0,0.1); margin-bottom: 20px; }
.candidate-list { display: flex; flex-wrap: wrap; gap: 10px; margin-top: 10px; }
.candidate { padding: 8px 16px; background: #f5f5f5; border-radius: 6px; font-size: 18px; cursor: pointer; transition: all 0.2s; border: 1px solid transparent; }
.candidate:hover { background: #e8f5e9; border-color: #4CAF50; }
.candidate:first-child { background: #4CAF50; color: white; font-weight: bold; }
.candidate:first-child:hover { background: #45a049; }
.output-area { background: white; border-radius: 12px; padding: 20px; box-shadow: 0 2px 8px rgba(0,0,0,0.1); margin-bottom: 20px; }
.output-text { font-size: 24px; color: #333; min-height: 40px; line-height: 1.6; }
.info { background: white; border-radius: 12px; padding: 20px; box-shadow: 0 2px 8px rgba(0,0,0,0.1); }
.info h3 { color: #333; margin-bottom: 10px; font-size: 16px; }
.info ul { list-style: none; }
.info li { padding: 4px 0; color: #666; font-size: 14px; }
.info code { background: #f0f0f0; padding: 2px 6px; border-radius: 3px; font-size: 13px; }
.stats { display: flex; gap: 20px; margin-top: 10px; }
.stat { text-align: center; }
.stat .num { font-size: 24px; font-weight: bold; color: #4CAF50; }
.stat .label { font-size: 12px; color: #888; }
.empty { color: #ccc; font-size: 16px; }
</style>
</head>
<body>
<div class="container">
<h1>🔤 pinyin-ime 拼音输入法演示</h1>
<p class="subtitle">基于 libime 引擎的 Linux 拼音输入法</p>

<div class="input-area">
<input type="text" id="pinyin" placeholder="输入拼音 (如 nihao, zhongguo, beijingdaxue)" autofocus>
<div class="options">
<label class="option"><input type="checkbox" id="fuzzy"> 模糊音</label>
<label class="option"><input type="checkbox" id="correct"> 纠错</label>
<label class="option"><input type="checkbox" id="shuangpin"> 双拼</label>
<label class="option"><input type="checkbox" id="simplified" checked> 简拼</label>
</div>
</div>

<div class="candidates">
<h3 style="color:#333;font-size:16px;margin-bottom:10px;">候选词</h3>
<div class="candidate-list" id="candidates"><span class="empty">输入拼音查看候选...</span></div>
</div>

<div class="output-area">
<h3 style="color:#333;font-size:16px;margin-bottom:10px;">首选输出</h3>
<div class="output-text" id="output"><span class="empty">—</span></div>
</div>

<div class="info">
<h3>功能说明</h3>
<ul>
<li><code>nihao</code> → 你好 | <code>zhongguo</code> → 中国 | <code>beijingdaxue</code> → 北京大学</li>
<li><code>xianbeijing</code> → 西安北京 (消歧) | <code>rengongzhineng</code> → 人工智能</li>
<li>模糊音: <code>cifan</code>→吃饭 | 纠错: <code>senme</code>→什么 | 双拼: <code>nihc</code>→你好</li>
</ul>
<div class="stats">
<div class="stat"><div class="num" id="stat-cands">0</div><div class="label">候选数</div></div>
<div class="stat"><div class="num" id="stat-time">0</div><div class="label">耗时(ms)</div></div>
<div class="stat"><div class="num" id="stat-total">0</div><div class="label">总查询</div></div>
</div>
</div>
</div>

<script>
let totalQueries = 0;

async function query() {
    const pinyin = document.getElementById('pinyin').value.trim();
    if (!pinyin) {
        document.getElementById('candidates').innerHTML = '<span class="empty">输入拼音查看候选...</span>';
        document.getElementById('output').innerHTML = '<span class="empty">—</span>';
        return;
    }
    
    const fuzzy = document.getElementById('fuzzy').checked ? 1 : 0;
    const correct = document.getElementById('correct').checked ? 1 : 0;
    const shuangpin = document.getElementById('shuangpin').checked ? 1 : 0;
    
    const start = performance.now();
    const res = await fetch(`/api?q=${encodeURIComponent(pinyin)}&fuzzy=${fuzzy}&correct=${correct}&sp=${shuangpin}`);
    const data = await res.json();
    const elapsed = Math.round(performance.now() - start);
    
    totalQueries++;
    document.getElementById('stat-total').textContent = totalQueries;
    document.getElementById('stat-time').textContent = elapsed;
    document.getElementById('stat-cands').textContent = data.candidates.length;
    
    if (data.candidates.length > 0) {
        document.getElementById('output').textContent = data.candidates[0];
        const html = data.candidates.map((c, i) => 
            `<div class="candidate" onclick="selectCand('${c}')">${i+1}. ${c}</div>`
        ).join('');
        document.getElementById('candidates').innerHTML = html;
    } else {
        document.getElementById('candidates').innerHTML = '<span class="empty">无候选</span>';
        document.getElementById('output').innerHTML = '<span class="empty">—</span>';
    }
}

function selectCand(text) {
    document.getElementById('output').textContent = text;
}

let timer;
document.getElementById('pinyin').addEventListener('input', () => {
    clearTimeout(timer);
    timer = setTimeout(query, 200);
});
['fuzzy', 'correct', 'shuangpin', 'simplified'].forEach(id => {
    document.getElementById(id).addEventListener('change', query);
});
</script>
</body>
</html>"""


class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        
        if parsed.path == "/":
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            self.wfile.write(HTML_PAGE.encode("utf-8"))
        
        elif parsed.path == "/api":
            params = urllib.parse.parse_qs(parsed.query)
            q = params.get("q", [""])[0]
            fuzzy = params.get("fuzzy", ["0"])[0] == "1"
            correct = params.get("correct", ["0"])[0] == "1"
            sp = params.get("sp", ["0"])[0] == "1"
            
            candidates = self.get_candidates(q, fuzzy, correct, sp)
            
            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(json.dumps({"candidates": candidates}).encode("utf-8"))
        else:
            self.send_response(404)
            self.end_headers()
    
    def get_candidates(self, q, fuzzy, correct, sp):
        if not q:
            return []
        
        cmd = [CLI, "--quiet", LM, DICT, "10"]
        if fuzzy:
            cmd.insert(1, "--fuzzy")
        if correct:
            cmd.insert(1, "--correct")
        if sp:
            cmd.insert(1, "--shuangpin")
            cmd.insert(2, "--sp-scheme")
            cmd.insert(3, "xiaohe")
        
        try:
            result = subprocess.run(
                cmd, input=q + "\n", capture_output=True, text=True, timeout=5
            )
            lines = [l.strip() for l in result.stdout.strip().split("\n") if l.strip()]
            if lines:
                return lines[0].split("\t")
            return []
        except Exception as e:
            return [f"错误: {e}"]
    
    def log_message(self, format, *args):
        pass  # Suppress logs


def main():
    print(f"╔══════════════════════════════════════════════════════╗")
    print(f"║  pinyin-ime Web 演示服务器                            ║")
    print(f"║  访问: http://localhost:{PORT}                          ║")
    print(f"║  按 Ctrl+C 退出                                        ║")
    print(f"╚══════════════════════════════════════════════════════╝")
    
    server = http.server.HTTPServer(("0.0.0.0", PORT), Handler)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n服务器已停止")
        server.server_close()


if __name__ == "__main__":
    main()
