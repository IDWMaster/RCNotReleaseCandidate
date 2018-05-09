#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <qtmpeg.h>
#include <condition_variable>
#include <mutex>
#include <cppext/cppext.h>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
    void paintEvent(QPaintEvent *event);
private:
    bool clientConnected;
    System::PushStream* pushstr;
    std::condition_variable evt;
    std::mutex mtx;
    AbstractMediaPlayer* player;
    AbstractVideoEncoder* encoder;
    Ui::MainWindow *ui;
private slots:
    void doRefresh();
    void onPacket(AVPacket* packet);
signals:
    void refresh();
};

#endif // MAINWINDOW_H
