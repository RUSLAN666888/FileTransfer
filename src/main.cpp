#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    qDebug() << "Hello from Qt!";
    return 0;
}
