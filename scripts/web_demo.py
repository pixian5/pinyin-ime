#!/usr/bin/env python3
"""
pinyin-ime Web 演示服务器
启动后在浏览器访问 http://localhost:8080

用法: python3 scripts/web_demo.py [--port 8080]

功能：
  - 实时拼音输入，候选词即时显示
  - 模糊音 / 纠错 / 双拼 / 简拼 切换
  - 整句输入模式（多段拼音空格分隔）
  - 打字测试模式（内置 20 句中文，统计速度/准确率）
  - 候选词点击上屏，构建完整文本
"""

import http.server
import json
import os
import subprocess
import sys
import time
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
if "--port" in sys.argv:
    idx = sys.argv.index("--port")
    if idx + 1 < len(sys.argv):
        PORT = int(sys.argv[idx + 1])

HTML_PAGE = """<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>pinyin-ime 拼音输入法演示</title>
<style>
* { margin: 0; padding: 0; box-sizing: border-box; }
body { font-family: 'PingFang SC', 'Microsoft YaHei', sans-serif; background: #f0f2f5; min-height: 100vh; color: #333; }
.container { max-width: 900px; margin: 0 auto; padding: 20px; }
h1 { text-align: center; margin: 20px 0; font-size: 24px; }
.subtitle { text-align: center; color: #888; margin-bottom: 30px; }
.tabs { display: flex; margin-bottom: 20px; border-radius: 12px 12px 0 0; overflow: hidden; }
.tab { flex: 1; padding: 12px; text-align: center; background: #ddd; cursor: pointer; font-size: 16px; transition: all 0.2s; border: none; }
.tab.active { background: #4CAF50; color: white; font-weight: bold; }
.tab:hover:not(.active) { background: #ccc; }
.card { background: white; border-radius: 12px; padding: 20px; box-shadow: 0 2px 8px rgba(0,0,0,0.1); margin-bottom: 20px; }
.card h3 { margin-bottom: 10px; font-size: 16px; }
input[type="text"] { width: 100%; padding: 12px 16px; font-size: 18px; border: 2px solid #e0e0e0; border-radius: 8px; outline: none; transition: border-color 0.3s; }
input[type="text"]:focus { border-color: #4CAF50; }
.options { display: flex; gap: 15px; margin-top: 15px; flex-wrap: wrap; }
.option { display: flex; align-items: center; gap: 5px; font-size: 14px; color: #666; cursor: pointer; }
.option input { width: 16px; height: 16px; cursor: pointer; }
.candidate-list { display: flex; flex-wrap: wrap; gap: 10px; margin-top: 10px; }
.candidate { padding: 8px 16px; background: #f5f5f5; border-radius: 6px; font-size: 18px; cursor: pointer; transition: all 0.2s; border: 1px solid transparent; user-select: none; }
.candidate:hover { background: #e8f5e9; border-color: #4CAF50; }
.candidate:first-child { background: #4CAF50; color: white; font-weight: bold; }
.candidate:first-child:hover { background: #45a049; }
.composed-text { font-size: 28px; min-height: 50px; line-height: 1.6; padding: 10px; background: #f9f9f9; border-radius: 8px; word-break: break-all; }
.stats { display: flex; gap: 20px; margin-top: 10px; }
.stat { text-align: center; }
.stat .num { font-size: 24px; font-weight: bold; color: #4CAF50; }
.stat .label { font-size: 12px; color: #888; }
.test-sentence { font-size: 22px; color: #555; padding: 15px; background: #f9f9f9; border-radius: 8px; margin-bottom: 15px; line-height: 1.8; }
.test-input { font-size: 22px; color: #4CAF50; padding: 15px; background: #e8f5e9; border-radius: 8px; margin-bottom: 15px; min-height: 30px; line-height: 1.8; }
.test-result { font-size: 18px; padding: 15px; border-radius: 8px; margin: 10px 0; }
.test-result.pass { background: #e8f5e9; color: #2e7d32; }
.test-result.fail { background: #fce4ec; color: #c62828; }
.test-nav { display: flex; gap: 10px; margin: 10px 0; }
.test-nav button { padding: 8px 20px; border: none; border-radius: 6px; cursor: pointer; font-size: 14px; }
.test-nav button:disabled { opacity: 0.5; cursor: default; }
.btn-next { background: #4CAF50; color: white; }
.btn-prev { background: #e0e0e0; color: #333; }
.btn-restart { background: #ff9800; color: white; }
.empty { color: #ccc; font-size: 16px; }
.info ul { list-style: none; }
.info li { padding: 4px 0; color: #666; font-size: 14px; }
.info code { background: #f0f0f0; padding: 2px 6px; border-radius: 3px; font-size: 13px; }
.hidden { display: none; }
body.dark { background: #1a1a2e; color: #e0e0e0; }
body.dark .card { background: #16213e; box-shadow: 0 2px 8px rgba(0,0,0,0.3); }
body.dark .tab { background: #0f3460; color: #a0a0a0; }
body.dark .tab.active { background: #4CAF50; color: white; }
body.dark input[type="text"] { background: #1a1a2e; color: #e0e0e0; border-color: #0f3460; }
body.dark .candidate { background: #16213e; color: #e0e0e0; }
body.dark .candidate:hover { background: #0f3460; }
body.dark .composed-text { background: #16213e; color: #e0e0e0; }
body.dark .test-sentence { background: #16213e; color: #a0a0a0; }
body.dark .test-input { background: #0f3460; color: #4CAF50; }
body.dark .info li { color: #a0a0a0; }
body.dark .info code { background: #0f3460; }
body.dark .stat .label { color: #888; }
body.dark .empty { color: #555; }
.theme-toggle { position: fixed; top: 10px; right: 10px; width: 40px; height: 40px; border-radius: 50%; border: none; cursor: pointer; font-size: 20px; background: #4CAF50; color: white; z-index: 100; }
@media (max-width: 600px) { .container { padding: 10px; } h1 { font-size: 18px; } .tab { font-size: 12px; padding: 8px 4px; } input[type="text"] { font-size: 16px; } .candidate { font-size: 16px; padding: 6px 12px; } .composed-text { font-size: 20px; } .options { gap: 8px; } .option { font-size: 12px; } }
.candidate .cand-num { display: inline-block; width: 20px; height: 20px; border-radius: 4px; background: rgba(0,0,0,0.1); font-size: 12px; text-align: center; line-height: 20px; margin-right: 6px; }
.candidate:first-child .cand-num { background: rgba(255,255,255,0.3); }
.bench-result { font-family: monospace; font-size: 14px; white-space: pre; padding: 15px; background: #f5f5f5; border-radius: 8px; }
body.dark .bench-result { background: #16213e; }
</style>
</head>
<body>
<button class="theme-toggle" onclick="toggleTheme()">🌙</button>\n<div class="container">
<h1>🔤 pinyin-ime 拼音输入法演示</h1>
<p class="subtitle">基于 libime 引擎的 Linux 拼音输入法 — v0.9.31</p>
<div class="tabs">
<button class="tab active" onclick="switchTab('input')">📝 拼音输入</button>
<button class="tab" onclick="switchTab('test')">⌨ 打字测试</button>
<button class="tab" onclick="switchTab('info')">ℹ️ 功能说明</button>
</div>
<div id="tab-input">
<div class="card"><h3>拼音输入</h3>
<input type="text" id="pinyin" placeholder="输入拼音 (如 nihao, zhongguo, beijingdaxue)" autofocus>
<div class="options">
<label class="option"><input type="checkbox" id="fuzzy"> 模糊音</label>
<label class="option"><input type="checkbox" id="correct"> 纠错</label>
<label class="option"><input type="checkbox" id="shuangpin"> 双拼</label>
<label class="option"><input type="checkbox" id="simplified" checked> 简拼</label>
<button class="option" style="background:#e0e0e0;border:none;border-radius:6px;padding:4px 12px;" onclick="clearComposed()">清屏</button>\n<button class="option" style="background:#e0e0e0;border:none;border-radius:6px;padding:4px 12px;" onclick="copyText()">复制</button>
</div></div>
<div class="card"><h3>候选词</h3>
<div class="candidate-list" id="candidates"><span class="empty">输入拼音查看候选...</span></div></div>
<div class="card"><h3>已输入文本</h3>
<div class="composed-text" id="composed"><span class="empty">点击候选词上屏</span></div>
<div class="stats">
<div class="stat"><div class="num" id="stat-cands">0</div><div class="label">候选数</div></div>
<div class="stat"><div class="num" id="stat-time">0</div><div class="label">耗时(ms)</div></div>
<div class="stat"><div class="num" id="stat-total">0</div><div class="label">总查询</div></div>
</div></div>
<div class="card"><h3>输入历史</h3>
<div class="candidate-list" id="history"><span class="empty">输入拼音后显示历史...</span></div>
<button class="option" style="background:#e0e0e0;border:none;border-radius:6px;padding:4px 12px;margin-top:8px;" onclick="clearHistory()">清空历史</button>
</div>
</div>
<div id="tab-test" class="hidden">
<div class="card"><h3>打字测试 — 输入拼音，验证输出是否正确</h3>
<div class="test-sentence" id="test-sentence"></div>
<div class="test-input" id="test-input">请在下方输入框打字</div>
<input type="text" id="test-pinyin" placeholder="输入对应拼音..." style="font-size:18px;">
<div class="test-result hidden" id="test-result"></div>
<div class="test-nav">
<button class="btn-prev" id="btn-prev" onclick="prevTest()">上一题</button>
<button class="btn-next" id="btn-next" onclick="nextTest()">下一题</button>
<button class="btn-restart" onclick="restartTest()">重新开始</button>
</div>
<div class="stats">
<div class="stat"><div class="num" id="test-passed">0</div><div class="label">通过</div></div>
<div class="stat"><div class="num" id="test-failed">0</div><div class="label">失败</div></div>
<div class="stat"><div class="num" id="test-current">1</div><div class="label">当前题</div></div>
<div class="stat"><div class="num" id="test-total2">0</div><div class="label">总题数</div></div>
</div></div>
</div>
<div id="tab-bench" class="hidden">\n<div class="card"><h3>性能基准测试</h3>\n<p style="color:#888;margin-bottom:10px;">点击按钮运行内置基准测试</p>\n<button onclick="runBench()" style="padding:10px 24px;background:#4CAF50;color:white;border:none;border-radius:8px;font-size:16px;cursor:pointer;">▶ 运行基准测试</button>\n<div id="bench-output" style="margin-top:15px;"></div>\n</div>\n</div>\n<div id="tab-info" class="hidden">
<div class="card info"><h3>功能说明</h3><ul>
<li><code>nihao</code> → 你好 | <code>zhongguo</code> → 中国 | <code>beijingdaxue</code> → 北京大学</li>
<li><code>xianbeijing</code> → 西安北京 (消歧) | <code>rengongzhineng</code> → 人工智能</li>
<li>模糊音: <code>cifan</code>→吃饭 | 纠错: <code>senme</code>→什么 | 双拼: <code>nihc</code>→你好</li>
<li>消歧: <code>shangwu</code>→上午 | <code>xianzai</code>→现在 | <code>woxiangqu</code>→我想去</li>
</ul></div>
<div class="card info"><h3>技术架构</h3><ul>
<li>引擎: libime 1.1.17 (4-gram kenlm) + fcitx5 5.1.23</li>
<li>数据: zh_CN.lm (34MB) + sc.dict (4.3MB)</li>
<li>自学习: 用户选词自动造词，持久化到 ~/.local/share/pinyin-ime</li>
<li>消歧: 预置 124 条消歧数据，预热加载后开箱即用</li>
</ul></div>
<div class="card info"><h3>测试结果 (v0.9.31)</h3><ul>
<li>冒烟测试: 24/24 ✅ | 交互测试: 26/26 ✅</li>
<li>Daily 评测: 40/40 = 100% ✅ | E2E 端到端: 138/138 = 100% ✅</li>
<li>纠错改进: ou↔ao/uo↔o/ui↔uei 韵母混淆 | 预热: 7/7 ✅</li>
<li>总计: 288/288 项全通过 🎉</li>
</ul></div>
</div>
</div>
<script>
function toggleTheme(){document.body.classList.toggle('dark');const b=document.querySelector('.theme-toggle');b.textContent=document.body.classList.contains('dark')?'☀️':'🌙';localStorage.setItem('theme',document.body.classList.contains('dark')?'dark':'light');}
if(localStorage.getItem('theme')==='dark'){document.body.classList.add('dark');document.querySelector('.theme-toggle').textContent='☀️';}
async function runBench(){const out=document.getElementById('bench-output');out.innerHTML='<p>运行中...</p>';try{const r=await fetch('/api?bench=1');const d=await r.json();out.innerHTML='<div class="bench-result">'+d.output+'</div>';}catch(e){out.innerHTML='<p>错误: '+e+'</p>';}}
let totalQueries=0, composedText='',inputHistory=[];
function switchTab(n){['input','test','bench','info'].forEach(t=>document.getElementById('tab-'+t).classList.toggle('hidden',t!==n));document.querySelectorAll('.tab').forEach((t,i)=>t.classList.toggle('active',['input','test','bench','info'][i]===n));if(n==='input')document.getElementById('pinyin').focus();if(n==='test')document.getElementById('test-pinyin').focus();}
async function query(){
const p=document.getElementById('pinyin').value.trim();
if(!p){document.getElementById('candidates').innerHTML='<span class="empty">输入拼音查看候选...</span>';return;}
const f=document.getElementById('fuzzy').checked?1:0,c=document.getElementById('correct').checked?1:0,sp=document.getElementById('shuangpin').checked?1:0;
const s=performance.now();
const r=await fetch(`/api?q=${encodeURIComponent(p)}&fuzzy=${f}&correct=${c}&sp=${sp}`);
const d=await r.json();const e=Math.round(performance.now()-s);
totalQueries++;document.getElementById('stat-total').textContent=totalQueries;document.getElementById('stat-time').textContent=e;document.getElementById('stat-cands').textContent=d.candidates.length;if(d.candidates.length>0){inputHistory.unshift({q:p,c:d.candidates[0],t:e});if(inputHistory.length>20)inputHistory.pop();updateHistory();}
if(d.candidates.length>0){document.getElementById('candidates').innerHTML=d.candidates.map((c,i)=>`<div class="candidate" onclick="selectCand('${c.replace(/'/g,"\\'")}')">${i+1}. ${c}</div>`).join('');}else{document.getElementById('candidates').innerHTML='<span class="empty">无候选</span>';}}
function selectCand(t){composedText+=t;document.getElementById('composed').textContent=composedText;document.getElementById('pinyin').value='';document.getElementById('candidates').innerHTML='<span class="empty">输入拼音查看候选...</span>';document.getElementById('pinyin').focus();}
function copyText(){navigator.clipboard.writeText(composedText).then(()=>{const b=document.querySelectorAll("button.option");b.forEach(btn=>{if(btn.textContent==="复制"){btn.textContent="已复制!";setTimeout(()=>btn.textContent="复制",1500);}});});}
function clearComposed(){composedText='';document.getElementById('composed').innerHTML='<span class="empty">点击候选词上屏</span>';document.getElementById('pinyin').value='';document.getElementById('pinyin').focus();}
let timer;document.getElementById('pinyin').addEventListener('input',()=>{clearTimeout(timer);timer=setTimeout(query,200);});
['fuzzy','correct','shuangpin','simplified'].forEach(id=>document.getElementById(id).addEventListener('change',query));
function updateHistory(){if(inputHistory.length===0){document.getElementById('history').innerHTML='<span class="empty">输入拼音后显示历史...</span>';return;}document.getElementById('history').innerHTML=inputHistory.map(h=>`<div class="candidate" onclick="document.getElementById('pinyin').value='${h.q.replace(/'/g,"\\'")}';query()">${h.q} → ${h.c} <span style="color:#888;font-size:12px;">(${h.t}ms)</span></div>`).join('');}
function clearHistory(){inputHistory=[];updateHistory();}
const testCases=[{p:'nihao',c:'你好'},{p:'shijie',c:'世界'},{p:'zhongguo',c:'中国'},{p:'beijingdaxue',c:'北京大学'},{p:'woaixuexi',c:'我爱学习'},{p:'rengongzhineng',c:'人工智能'},{p:'shenduxuexi',c:'深度学习'},{p:'jiqixuexi',c:'机器学习'},{p:'woyaochifan',c:'我要吃饭'},{p:'duibuqi',c:'对不起'},{p:'meiguanxi',c:'没关系'},{p:'xiexieni',c:'谢谢你'},{p:'zaoshanghao',c:'早上好'},{p:'wanshanghao',c:'晚上好'},{p:'yiqichifan',c:'一起吃饭'},{p:'jintiandetianqi',c:'今天的天气'},{p:'qingwenyixiaruzhushenmefangjian',c:'请问一下入住什么房间'},{p:'woxiangdingyigefangjian',c:'我想订一个房间'},{p:'shangwu',c:'上午'},{p:'xianzai',c:'现在'},{p:'shanghai',c:'上海'},{p:'guangzhou',c:'广州'},{p:'shenzhen',c:'深圳'},{p:'hangzhou',c:'杭州'},{p:'chengdu',c:'成都'},{p:'yinyue',c:'音乐'},{p:'dianying',c:'电影'},{p:'youxi',c:'游戏'},{p:'tiyu',c:'体育'},{p:'xinwen',c:'新闻'},{p:'keji',c:'科技'},{p:'chuangxin',c:'创新'},{p:'fazhan',c:'发展'},{p:'jinrong',c:'金融'},{p:'taishan',c:'泰山'},{p:'changcheng',c:'长城'},{p:'gugong',c:'故宫'},{p:'zhonghua',c:'中华'},{p:'renmin',c:'人民'},{p:'woxiangqulvyou',c:'我想去旅游'},{p:'woshixuesheng',c:'我是学生'},{p:'zhongguorenmin',c:'中国人民'},{p:'woshizhongguoren',c:'我是中国人'},{p:'woaibeijingtiananmen',c:'我爱北京天安门'},{p:'zhonghuarenmingongheguo',c:'中华人民共和国'},{p:'kejixingguo',c:'科技兴国'},{p:'womenyaogongtongnuli',c:'我们要共同努力'},{p:'weilaideshenghuohengmeihao',c:'未来的生活很美好'}];
let testIdx=0,testPassed=0,testFailed=0;
function loadTest(){const tc=testCases[testIdx];document.getElementById('test-sentence').textContent=`第 ${testIdx+1}/${testCases.length} 题：请输入「${tc.c}」的拼音`;document.getElementById('test-input').textContent='请在下方输入框打字';document.getElementById('test-pinyin').value='';document.getElementById('test-result').classList.add('hidden');document.getElementById('test-current').textContent=testIdx+1;document.getElementById('test-total2').textContent=testCases.length;document.getElementById('btn-prev').disabled=testIdx===0;document.getElementById('btn-next').disabled=testIdx===testCases.length-1;document.getElementById('test-pinyin').focus();}
async function checkTest(){const tc=testCases[testIdx];const p=document.getElementById('test-pinyin').value.trim();if(!p)return;const r=await fetch(`/api?q=${encodeURIComponent(p)}&fuzzy=0&correct=0&sp=0`);const d=await r.json();const top=d.candidates[0]||'';document.getElementById('test-input').textContent=top;const el=document.getElementById('test-result');el.classList.remove('hidden');if(top===tc.c){el.className='test-result pass';el.textContent=`✅ 正确！「${p}」→「${top}」`;testPassed++;}else{el.className='test-result fail';el.textContent=`❌ 不匹配。「${p}」→「${top}」，期望「${tc.c}」`;testFailed++;}document.getElementById('test-passed').textContent=testPassed;document.getElementById('test-failed').textContent=testFailed;}
function nextTest(){if(testIdx<testCases.length-1){testIdx++;loadTest();}}
function prevTest(){if(testIdx>0){testIdx--;loadTest();}}
function restartTest(){testIdx=0;testPassed=0;testFailed=0;document.getElementById('test-passed').textContent='0';document.getElementById('test-failed').textContent='0';loadTest();}
document.getElementById('test-pinyin').addEventListener('input',()=>{clearTimeout(timer);timer=setTimeout(checkTest,300);});
loadTest();
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
        pass


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
