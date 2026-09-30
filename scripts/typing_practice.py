#!/usr/bin/env python3
"""
pinyin-ime 打字练习服务器
展示输入法实际使用效果，测量打字速度和准确率

用法: python3 scripts/typing_practice.py
访问: http://localhost:8081
"""

import http.server
import json
import os
import subprocess
import sys
import urllib.parse
import random

project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(project_root)

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

PORT = 8081

# 练习句子库
SENTENCES = [
    {"pinyin": "nihao", "text": "你好", "level": "简单"},
    {"pinyin": "zhongguo", "text": "中国", "level": "简单"},
    {"pinyin": "beijingdaxue", "text": "北京大学", "level": "简单"},
    {"pinyin": "woaixuexi", "text": "我爱学习", "level": "简单"},
    {"pinyin": "jintiantianqihenhao", "text": "今天天气很好", "level": "中等"},
    {"pinyin": "woxiangqubeijinglvyou", "text": "我想去北京旅游", "level": "中等"},
    {"pinyin": "rengongzhinenggaibianshijie", "text": "人工智能改变世界", "level": "困难"},
    {"pinyin": "shurufashirenleijiaohuandejichu", "text": "输入法是人类交互的基础", "level": "困难"},
    {"pinyin": "zhonghuawenzimingshanshanshuo", "text": "中华文化名山闪烁", "level": "困难"},
    {"pinyin": "kexuejishushidiyishengchanli", "text": "科学技术是第一生产力", "level": "困难"},
    {"pinyin": "xueshengxuyaorenzhenxuexi", "text": "学生需要认真学习", "level": "中等"},
    {"pinyin": "laoshijiaodaoxueshengxuexi", "text": "老师教导学生学习", "level": "中等"},
    {"pinyin": "zhongguorenminreaihexie", "text": "中国人民热爱和平", "level": "困难"},
    {"pinyin": "chuntianlailebaihuakai", "text": "春天来了百花开", "level": "中等"},
    {"pinyin": "pengyoumenyiqiquyouwan", "text": "朋友们一起去游玩", "level": "中等"},
]

HTML_PAGE = """<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>pinyin-ime 打字练习</title>
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body { font-family: 'PingFang SC', 'Microsoft YaHei', sans-serif; background: #f0f2f5; min-height: 100vh; }
.container { max-width: 800px; margin: 0 auto; padding: 20px; }
h1 { text-align: center; color: #333; margin: 20px 0; }
.card { background: white; border-radius: 12px; padding: 24px; box-shadow: 0 2px 8px rgba(0,0,0,0.1); margin-bottom: 20px; }
.target { font-size: 32px; text-align: center; color: #333; padding: 20px; background: #f8f9fa; border-radius: 8px; margin-bottom: 15px; }
.target .char { display: inline-block; transition: all 0.2s; }
.target .char.correct { color: #4CAF50; }
.target .char.wrong { color: #f44336; text-decoration: line-through; }
.target .char.current { background: #fff3cd; border-bottom: 2px solid #ffc107; }
.pinyin-hint { text-align: center; color: #888; font-size: 16px; margin-bottom: 15px; }
input[type="text"] { width: 100%; padding: 14px; font-size: 20px; border: 2px solid #e0e0e0; border-radius: 8px; outline: none; }
input[type="text"]:focus { border-color: #4CAF50; }
.candidates { display: flex; flex-wrap: wrap; gap: 8px; margin-top: 10px; }
.cand { padding: 6px 14px; background: #f5f5f5; border-radius: 6px; font-size: 18px; cursor: pointer; }
.cand:first-child { background: #4CAF50; color: white; }
.cand:hover { background: #e8f5e9; }
.cand:first-child:hover { background: #45a049; }
.stats { display: flex; justify-content: space-around; margin-top: 15px; }
.stat { text-align: center; }
.stat .num { font-size: 28px; font-weight: bold; color: #4CAF50; }
.stat .label { font-size: 12px; color: #888; margin-top: 4px; }
.progress { height: 6px; background: #e0e0e0; border-radius: 3px; margin-top: 15px; }
.progress-bar { height: 100%; background: #4CAF50; border-radius: 3px; transition: width 0.3s; }
.level-badge { display: inline-block; padding: 2px 10px; border-radius: 12px; font-size: 12px; margin-left: 8px; }
.level-简单 { background: #e8f5e9; color: #4CAF50; }
.level-中等 { background: #fff3cd; color: #856404; }
.level-困难 { background: #fce4ec; color: #c62828; }
.result { text-align: center; }
.result h2 { color: #4CAF50; margin: 10px 0; }
.btn { padding: 10px 30px; background: #4CAF50; color: white; border: none; border-radius: 8px; font-size: 16px; cursor: pointer; margin: 5px; }
.btn:hover { background: #45a049; }
.history { margin-top: 15px; }
.history-item { display: flex; justify-content: space-between; padding: 6px 0; border-bottom: 1px solid #f0f0f0; font-size: 14px; }
.history-item .correct { color: #4CAF50; }
.history-item .wrong { color: #f44336; }
</style>
</head>
<body>
<div class="container">
<h1>⌨️ pinyin-ime 打字练习</h1>

<div class="card" id="game-area">
<div class="target" id="target-text">点击开始练习</div>
<div class="pinyin-hint" id="pinyin-hint"></div>
<input type="text" id="input" placeholder="输入拼音..." disabled autocomplete="off">
<div class="candidates" id="candidates"></div>
<div class="progress"><div class="progress-bar" id="progress-bar" style="width:0%"></div></div>
<div class="stats">
<div class="stat"><div class="num" id="stat-wpm">0</div><div class="label">字/分钟</div></div>
<div class="stat"><div class="num" id="stat-acc">100%</div><div class="label">准确率</div></div>
<div class="stat"><div class="num" id="stat-done">0</div><div class="label">已完成</div></div>
<div class="stat"><div class="num" id="stat-total">15</div><div class="label">总句数</div></div>
</div>
</div>

<div class="card">
<button class="btn" onclick="startGame()">开始练习</button>
<button class="btn" onclick="resetGame()" style="background:#757575">重置</button>
<div class="history" id="history"></div>
</div>
</div>

<script>
let sentences = SENTENCES;
let currentIdx = 0;
let startTime = 0;
let totalChars = 0;
let correctChars = 0;
let doneCount = 0;
let history = [];

function startGame() {
    currentIdx = 0;
    totalChars = 0;
    correctChars = 0;
    doneCount = 0;
    history = [];
    startTime = Date.now();
    document.getElementById('stat-total').textContent = sentences.length;
    loadSentence();
}

function resetGame() {
    currentIdx = 0;
    totalChars = 0;
    correctChars = 0;
    doneCount = 0;
    history = [];
    document.getElementById('target-text').textContent = '点击开始练习';
    document.getElementById('pinyin-hint').textContent = '';
    document.getElementById('input').value = '';
    document.getElementById('input').disabled = true;
    document.getElementById('candidates').innerHTML = '';
    document.getElementById('progress-bar').style.width = '0%';
    document.getElementById('stat-wpm').textContent = '0';
    document.getElementById('stat-acc').textContent = '100%';
    document.getElementById('stat-done').textContent = '0';
    document.getElementById('history').innerHTML = '';
}

function loadSentence() {
    if (currentIdx >= sentences.length) {
        showResult();
        return;
    }
    const s = sentences[currentIdx];
    const html = s.text.split('').map((c, i) => 
        `<span class="char" id="char-${i}">${c}</span>`
    ).join('');
    document.getElementById('target-text').innerHTML = html + 
        `<span class="level-badge level-${s.level}">${s.level}</span>`;
    document.getElementById('pinyin-hint').textContent = '拼音: ' + s.pinyin;
    document.getElementById('input').value = '';
    document.getElementById('input').disabled = false;
    document.getElementById('input').focus();
    document.getElementById('candidates').innerHTML = '';
    document.getElementById('progress-bar').style.width = (currentIdx / sentences.length * 100) + '%';
}

async function onInput() {
    const pinyin = document.getElementById('input').value.trim();
    if (!pinyin) {
        document.getElementById('candidates').innerHTML = '';
        return;
    }
    const res = await fetch(`/api?q=${encodeURIComponent(pinyin)}`);
    const data = await res.json();
    if (data.candidates.length > 0) {
        const html = data.candidates.slice(0, 5).map((c, i) => 
            `<div class="cand" onclick="selectCand('${c}')">${i+1}. ${c}</div>`
        ).join('');
        document.getElementById('candidates').innerHTML = html;
    }
}

function selectCand(text) {
    const target = sentences[currentIdx].text;
    totalChars += target.length;
    const correct = text === target;
    if (correct) correctChars += target.length;
    
    // Color the target
    const chars = document.getElementById('target-text').querySelectorAll('.char');
    chars.forEach((c, i) => {
        c.className = 'char ' + (correct ? 'correct' : 'wrong');
    });
    
    doneCount++;
    history.push({ target, input: text, correct });
    updateHistory();
    
    const elapsed = (Date.now() - startTime) / 1000;
    const wpm = Math.round(totalChars / elapsed * 60);
    const acc = Math.round(correctChars / totalChars * 100);
    document.getElementById('stat-wpm').textContent = wpm;
    document.getElementById('stat-acc').textContent = acc + '%';
    document.getElementById('stat-done').textContent = doneCount;
    
    setTimeout(() => {
        currentIdx++;
        loadSentence();
    }, 500);
}

function updateHistory() {
    const html = history.map(h => 
        `<div class="history-item">
            <span>${h.target}</span>
            <span class="${h.correct ? 'correct' : 'wrong'}">
                ${h.correct ? '✅ ' + h.input : '❌ ' + h.input}
            </span>
        </div>`
    ).join('');
    document.getElementById('history').innerHTML = '<h3 style="margin-bottom:8px;font-size:14px;">练习记录</h3>' + html;
}

function showResult() {
    const elapsed = (Date.now() - startTime) / 1000;
    const wpm = Math.round(totalChars / elapsed * 60);
    const acc = Math.round(correctChars / totalChars * 100);
    document.getElementById('target-text').innerHTML = 
        `<div class="result"><h2>练习完成！</h2>
         <p style="font-size:18px;color:#666;margin:10px 0;">
         速度: ${wpm} 字/分钟 | 准确率: ${acc}% | 用时: ${elapsed.toFixed(1)}秒</p>
         <button class="btn" onclick="startGame()">再来一轮</button></div>`;
    document.getElementById('pinyin-hint').textContent = '';
    document.getElementById('input').disabled = true;
    document.getElementById('candidates').innerHTML = '';
    document.getElementById('progress-bar').style.width = '100%';
}

let timer;
document.getElementById('input').addEventListener('input', () => {
    clearTimeout(timer);
    timer = setTimeout(onInput, 200);
});
document.getElementById('input').addEventListener('keydown', (e) => {
    if (e.key === 'Enter') {
        const cands = document.getElementById('candidates').querySelectorAll('.cand');
        if (cands.length > 0) cands[0].click();
    }
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
            page = HTML_PAGE.replace("SENTENCES", json.dumps(SENTENCES, ensure_ascii=False))
            self.wfile.write(page.encode("utf-8"))
        
        elif parsed.path == "/api":
            params = urllib.parse.parse_qs(parsed.query)
            q = params.get("q", [""])[0]
            candidates = self.get_candidates(q)
            
            self.send_response(200)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(json.dumps({"candidates": candidates}).encode("utf-8"))
        else:
            self.send_response(404)
            self.end_headers()
    
    def get_candidates(self, q):
        if not q:
            return []
        try:
            result = subprocess.run(
                [CLI, "--quiet", LM, DICT, "5"],
                input=q + "\n", capture_output=True, text=True, timeout=5
            )
            lines = [l.strip() for l in result.stdout.strip().split("\n") if l.strip()]
            if lines:
                return lines[0].split("\t")
            return []
        except:
            return []
    
    def log_message(self, format, *args):
        pass


def main():
    print(f"╔══════════════════════════════════════════════════════╗")
    print(f"║  pinyin-ime 打字练习服务器                            ║")
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
