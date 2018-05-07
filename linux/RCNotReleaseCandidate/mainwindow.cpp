#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QScreen>
#include <QPixmap>
#include <thread>
#include <unistd.h>
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    player = getMediaServer()->createMediaPlayer();
    encoder = getMediaServer()->createEncoder();
    player->attachEncoder(encoder);
    connect(this,SIGNAL(refresh()),SLOT(doRefresh()));
    std::thread mthread([=](){
        while(true) {
            std::unique_lock<std::mutex> l(mtx);
            evt.wait(l);
            QScreen* screen = QGuiApplication::primaryScreen();

            QPixmap map = screen->grabWindow(0);
        auto bot = map.toImage();
        QVideoFrame frame(bot);
        encoder->present(frame);
        emit refresh();
        usleep(1000*15);

        }
    });
    mthread.detach();

    ui->gridLayout_2->addWidget(player);

}
void MainWindow::doRefresh() {
    repaint(0,0,1,1);
}

void MainWindow::paintEvent(QPaintEvent *event) {
    evt.notify_one();
}

MainWindow::~MainWindow()
{
    delete ui;
}
