#include <QApplication>
#include <QWebEngineView>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QWebEngineView view;
    view.setUrl(QUrl("http://www.baidu.com"));
    view.show();
    view.resize(1024, 750);
    return app.exec();
}