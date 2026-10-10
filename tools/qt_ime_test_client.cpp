/*
 * qt_ime_test_client.cpp —— Qt5 端到端输入验证（QT_IM_MODULE=fcitx 链路）
 *
 * 创建一个 QLineEdit，把每个 commit 的文本打到 stdout（COMMIT:<text>），
 * 供 scripts/xvfb_desktop_ime_test.sh 类脚本断言。依赖 fcitx5-qt 的
 * platforminputcontext 插件（QT_QPA_PLATFORMINPUTCONTEXT_PATH）。
 *
 * 编译（conda Qt5，见 scripts/build_fcitx5_qt.sh 同款环境）：
 *   $ENV/bin/aarch64-conda-linux-gnu-c++ -O2 -o tools/qt_ime_test_client \
 *       tools/qt_ime_test_client.cpp $(pkg-config --cflags --libs Qt5Widgets Qt5Gui Qt5Core) \
 *       -fPIC -std=c++17
 */
#include <QApplication>
#include <QLineEdit>
#include <QKeyEvent>
#include <QTimer>
#include <cstdio>

class CommitWatcher : public QObject {
public:
    using QObject::QObject;
protected:
    bool eventFilter(QObject *obj, QEvent *event) override {
        if (event->type() == QEvent::KeyPress) {
            auto *ke = static_cast<QKeyEvent *>(event);
            // QLineEdit 自身不打印 commit；QInputMethod::commitString 经
            // QKeyEvent 附带的 commit text 传递（Qt5 中 commit 通过
            // QInputMethodEvent 到达 widget，这里拦截并打印）。
            if (!ke->text().isEmpty())
                fprintf(stdout, "KEY:%s\n", qPrintable(ke->text()));
        }
        if (event->type() == QEvent::InputMethod) {
            auto *ime = static_cast<QInputMethodEvent *>(event);
            if (!ime->commitString().isEmpty())
                fprintf(stdout, "COMMIT:%s\n", qPrintable(ime->commitString()));
            if (!ime->preeditString().isEmpty())
                fprintf(stdout, "PREEDIT:%s\n", qPrintable(ime->preeditString()));
            fflush(stdout);
        }
        return QObject::eventFilter(obj, event);
    }
};

int main(int argc, char **argv) {
    // 让 Qt 用 X11 platform plugin（无显示服务器时由 Xvfb 提供）
    qputenv("QT_QPA_PLATFORM", "xcb");
    QApplication app(argc, argv);
    QLineEdit edit;
    CommitWatcher watcher;
    edit.installEventFilter(&watcher);
    edit.show();
    // 打印窗口 id，供 keysend_target 定向注入按键
    fprintf(stdout, "WINDOW_ID=0x%lx\n", (unsigned long)edit.winId());
    fflush(stdout);
    QTimer::singleShot(120000, &app, &QCoreApplication::quit);
    return app.exec();
}
