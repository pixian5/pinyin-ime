#!/usr/bin/env python3
"""训练自定义领域语言模型（Route A）

用 kenlm 从中文语料训练 n-gram ARPA LM，再转 libime 二进制格式。
用法:
  python3 scripts/train_domain_lm.py --corpus file1.txt file2.txt ... \
    --output /path/to/custom.lm --order 4

前提：已构建引擎（scripts/setup_build_env.sh + build_conda_forge.sh）
"""
import argparse
import os
import subprocess
import sys
import tempfile

NB = os.environ.get("NB", "/root/workspace/native-build")
LMPLZ = f"{NB}/build/kenlm/bin/lmplz"
SLM_BUILD = f"{NB}/opt/libime/bin/libime_slm_build_binary"
DICT = f"{NB}/opt/libime/share/libime/sc.dict"

def segment_corpus(corpus_files, output_path):
    """用 jieba 分词，输出 kenlm 格式（每行空格分隔词）"""
    import jieba
    total = 0
    with open(output_path, 'w') as out:
        for path in corpus_files:
            with open(path) as f:
                for line in f:
                    line = line.strip()
                    if not line or len(line) < 2:
                        continue
                    chinese = ''.join(c for c in line if '\u4e00' <= c <= '\u9fff')
                    if not chinese or len(chinese) < 2:
                        continue
                    words = jieba.cut(chinese)
                    seg = ' '.join(words)
                    out.write(seg + '\n')
                    total += 1
    return total

def train_arpa(seg_file, arpa_file, order=4):
    """用 kenlm lmplz 训练 ARPA LM"""
    cmd = [LMPLZ, "-o", str(order), "-T", "/tmp/kenlm_tmp", "-S", "500M"]
    with open(seg_file) as stdin, open(arpa_file, 'w') as stdout:
        subprocess.run(cmd, stdin=stdin, stdout=stdout, check=True)

def convert_to_libime(arpa_file, output_lm):
    """转 ARPA → libime 二进制（trie + 量化 + 压缩指针）"""
    cmd = [SLM_BUILD, "-q", "8", "-b", "8", "-a", "255", "trie", arpa_file, output_lm]
    subprocess.run(cmd, check=True)

def main():
    parser = argparse.ArgumentParser(description="训练自定义领域语言模型")
    parser.add_argument("--corpus", nargs="+", required=True, help="语料文件列表")
    parser.add_argument("--output", required=True, help="输出 LM 路径")
    parser.add_argument("--order", type=int, default=4, help="n-gram 阶数（默认4）")
    args = parser.parse_args()

    with tempfile.TemporaryDirectory() as tmpdir:
        seg_file = os.path.join(tmpdir, "segmented.txt")
        arpa_file = os.path.join(tmpdir, "model.arpa")

        print(f"=== 分词 {len(args.corpus)} 个语料文件 ===", file=sys.stderr)
        n = segment_corpus(args.corpus, seg_file)
        print(f"  {n} 句已分词", file=sys.stderr)

        print(f"=== 训练 {args.order}-gram ARPA LM ===", file=sys.stderr)
        train_arpa(seg_file, arpa_file, args.order)
        arpa_size = os.path.getsize(arpa_file)
        print(f"  ARPA: {arpa_size / 1024 / 1024:.1f} MB", file=sys.stderr)

        print(f"=== 转换为 libime 二进制 ===", file=sys.stderr)
        convert_to_libime(arpa_file, args.output)
        lm_size = os.path.getsize(args.output)
        print(f"  LM: {lm_size / 1024 / 1024:.1f} MB → {args.output}", file=sys.stderr)

        print(f"完成！{n} 句训练，{args.order}-gram，{lm_size} 字节", file=sys.stderr)

if __name__ == "__main__":
    main()
