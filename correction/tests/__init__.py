"""correction 测试包。

必须保留此文件：没有它时 `python -m unittest discover` 会静默跳过本目录，
输出 "Ran 0 tests / OK" 并以 0 退出码结束——测试从未执行却看起来全部通过。
正确跑法（补上本 __init__.py 后两者皆可）：
    python -m unittest discover            # 从仓库根
    python -m unittest correction.tests.test_core
"""
