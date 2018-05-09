#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QScreen>
#include <QPixmap>
#include <thread>
#include <unistd.h>
#include "cppext/cppext.h"
#include <queue>
#include <fcntl.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>


class ClientContext {
public:
    unsigned char buffy[4096];
    size_t bytesAvail;
    size_t offset;
    std::shared_ptr<System::IO::IOCallback> beethoven; //Call Beethoven bach!
    std::shared_ptr<System::Event> callbackEvt;
    ClientContext() {
        bytesAvail = 0;
        offset = 0;
    }
};

static int fd;
static unsigned char* firstpacket = 0;
static size_t firstpacket_size = 0;
void MainWindow::onPacket(AVPacket *vpacket) {
    if(!firstpacket) {
        unsigned char* packet = new unsigned char[1+8+4+vpacket->size];
        System::BStream str(packet,1+8+4+vpacket->size);
        str.Write((unsigned char)0);
        str.Write((int64_t)vpacket->pts);
        str.Write((int32_t)vpacket->size);
        str.Write(vpacket->data,vpacket->size);
        firstpacket = packet;
        firstpacket_size = 1+8+4+vpacket->size;
    }
    std::unique_lock<std::mutex> l(mtx);
    bool connected = this->clientConnected;
    l.unlock();
    if(connected) {
    unsigned char* packet = new unsigned char[1+8+4+vpacket->size];
    System::BStream str(packet,1+8+4+vpacket->size);
    str.Write((unsigned char)0);
    str.Write((int64_t)vpacket->pts);
    str.Write((int32_t)vpacket->size);
    str.Write(vpacket->data,vpacket->size);
    write(fd,packet,1+8+4+vpacket->size);
    delete[] packet;
    }
   // av_packet_free(&vpacket);
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    this->clientConnected = false;
    ui->setupUi(this);
    player = getMediaServer()->createMediaPlayer();
    encoder = getMediaServer()->createEncoder();

    connect(encoder,SIGNAL(packetAvailable(AVPacket*)),this,SLOT(onPacket(AVPacket*)));
    player->attachEncoder(encoder);

    connect(this,SIGNAL(refresh()),SLOT(doRefresh()));
    std::thread serverThread([=](){
        System::Net::IPEndpoint ep;
        ep.ip = "::";
        ep.port = 3870;
        sockaddr_in6 addr;
        int serverfd = socket(PF_INET6,SOCK_STREAM,IPPROTO_TCP);
      memset(&addr,0,sizeof(addr));
      memcpy(&addr.sin6_addr,ep.ip.raw,16);
      addr.sin6_port = htons(ep.port);
      addr.sin6_family = AF_INET6;
      bind(serverfd,(sockaddr*)&addr,sizeof(addr));
      listen(serverfd,20);
      sockaddr_in6 clientAddr;
      memset(&clientAddr,0,sizeof(clientAddr));
      clientAddr.sin6_family = AF_INET6;
      socklen_t slen = sizeof(clientAddr);
      fd = accept(serverfd,(struct sockaddr*)&clientAddr,&slen);
      write(fd,firstpacket,firstpacket_size);
      std::unique_lock<std::mutex> l(mtx);
      clientConnected = true;
    });

    serverThread.detach();
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
