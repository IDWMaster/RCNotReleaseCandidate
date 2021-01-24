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
#include <linux/uinput.h>
#include <netinet/tcp.h>


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
static int inputfd;
static int fd;
static unsigned char* firstpacket = 0;
static size_t firstpacket_size = 0;
void MainWindow::onPacket(AVPacket *vpacket) {
    if(!firstpacket) {
        unsigned char* packet = new unsigned char[1+8+4+vpacket->size];
        System::BStream str(packet,1+8+4+vpacket->size);
        str.Write((unsigned char)0);
        str.Write((int64_t)0);
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
    l.lock();
    if(write(fd,packet,1+8+4+vpacket->size)<=0) {
        clientConnected = false;
    }
    int oval = 1;
    int rval = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &oval, sizeof(oval));
    oval = 0;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &oval, sizeof(oval));
    l.unlock();
    delete[] packet;
    }
   // av_packet_free(&vpacket);
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    inputfd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

    this->clientConnected = false;
    ui->setupUi(this);
    if(inputfd<=0) {
        inputfd = -1;
        this->setWindowTitle("Remote Control Server (view only)");
    }else {
        this->setWindowTitle("Remote Control Server");
        ioctl(inputfd,UI_SET_EVBIT,EV_ABS);
        ioctl(inputfd,UI_SET_ABSBIT,ABS_X);
        ioctl(inputfd,UI_SET_ABSBIT,ABS_Y); //There's also an ABS_MT_SLOT not sure what that is for.
        ioctl(inputfd,UI_SET_ABSBIT,ABS_MT_POSITION_X);
        ioctl(inputfd,UI_SET_ABSBIT,ABS_MT_POSITION_Y);
        ioctl(inputfd,UI_SET_ABSBIT,ABS_MT_TRACKING_ID);

        ioctl(inputfd,UI_SET_ABSBIT,ABS_MT_SLOT);

        ioctl(inputfd,UI_SET_EVBIT,EV_KEY);
        ioctl(inputfd,UI_SET_KEYBIT,BTN_TOUCH);
        struct uinput_user_dev userdev = {};
        userdev.absmin[ABS_X] = 0;
        userdev.absmin[ABS_Y] = 0;
        userdev.absmax[ABS_X] = 1920;
        userdev.absmax[ABS_Y] = 1080; //TODO: Allow variance in resolution here.
        userdev.absmin[ABS_MT_POSITION_X] = 0;
        userdev.absmin[ABS_MT_POSITION_Y] = 0;
        userdev.absmin[ABS_MT_TRACKING_ID] = 0;
        userdev.absmax[ABS_MT_POSITION_X] = 1920;
        userdev.absmax[ABS_MT_POSITION_Y] = 1080;
        userdev.absmax[ABS_MT_TRACKING_ID] = 65535;
        userdev.absmax[ABS_MT_SLOT] = 10;
        strcpy(userdev.name,"RCNotReleaseCandidate");
        int rval = write(inputfd,&userdev,sizeof(userdev)); //NOTE: Use evtest program to debug
        ioctl(inputfd,UI_DEV_CREATE);
        if(rval<=0) {
            this->setWindowTitle("Remote Control Server (unsupported kernel)");
        }
    }
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
        int lingerval[2];
        lingerval[0] = 0;
        lingerval[1] = 0;
        setsockopt(serverfd,SOL_SOCKET,SO_LINGER,&lingerval,8);

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
      int oval = 1;
      int rval = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &oval, sizeof(oval));
      write(fd,firstpacket,firstpacket_size);
      std::unique_lock<std::mutex> l(mtx);
      clientConnected = true;
      l.unlock();
      auto readAll = [=](void* output, size_t sz) {
          unsigned char* ptr = (unsigned char*)output;
          while(sz) {
              int bytesread = read(fd,ptr,sz);
              if(bytesread<=0) {
                  return false;
              }
              sz-=bytesread;
          }
          return true;
      };

      while(clientConnected) {
          unsigned char opcode = 0;
          if(read(fd,&opcode,1)<=0) {
              break;
          }
          switch(opcode) {
          case 1:
          {
              struct {
                  int x;
                  int y;
                  int id;
              } point;
              if(!readAll(&point,sizeof(point))) {
                  return;
              }
              struct input_event evt = {};
              evt.code = ABS_MT_SLOT;
              evt.type = EV_ABS;
              evt.value = point.id+1;
              write(inputfd,&evt,sizeof(evt));
              evt.code = ABS_MT_TRACKING_ID;
              evt.type = EV_ABS;
              evt.value = point.id+1;
              write(inputfd,&evt,sizeof(evt));
              evt.code = ABS_X;
              evt.type = EV_ABS;
              evt.value = point.x;
              write(inputfd,&evt,sizeof(evt));
              evt.code = ABS_Y;
              evt.type = EV_ABS;
              evt.value = point.y;
              write(inputfd,&evt,sizeof(evt));
              evt.code = ABS_MT_POSITION_X;
              evt.type = EV_ABS;
              evt.value = point.x;
              write(inputfd,&evt,sizeof(evt));
              evt.code = ABS_MT_POSITION_Y;
              evt.type = EV_ABS;
              evt.value = point.y;
              write(inputfd,&evt,sizeof(evt));

              evt.code = BTN_TOUCH;
              evt.type = EV_KEY;
              evt.value = 1; //Pressure applied
              write(inputfd,&evt,sizeof(evt));
              evt.code = SYN_REPORT;
              evt.type = EV_SYN;
              evt.value = 0;
              write(inputfd,&evt,sizeof(evt));
          }
              break;
          case 2:
          {

              struct {
                  int x;
                  int y;
                  int id;
              } point;
              if(!readAll(&point,sizeof(point))) {
                  return;
              }

              struct input_event evt = {};
              evt.code = ABS_MT_SLOT;
              evt.type = EV_ABS;
              evt.value = point.id+1;
              write(inputfd,&evt,sizeof(evt));
              evt.code = ABS_MT_TRACKING_ID;
              evt.type = EV_ABS;
              evt.value = -1;
              write(inputfd,&evt,sizeof(evt));
              evt.code = ABS_X;
              evt.type = EV_ABS;
              evt.value = point.x;
              write(inputfd,&evt,sizeof(evt));
              evt.code = ABS_Y;
              evt.type = EV_ABS;
              evt.value = point.y;
              write(inputfd,&evt,sizeof(evt));
              evt.code = ABS_MT_POSITION_X;
              evt.type = EV_ABS;
              evt.value = point.x;
              write(inputfd,&evt,sizeof(evt));
              evt.code = ABS_MT_POSITION_Y;
              evt.type = EV_ABS;
              evt.value = point.y;
              write(inputfd,&evt,sizeof(evt));
              evt.code = BTN_TOUCH;
              evt.type = EV_KEY;
              evt.value = 0; //Pressure applied
              write(inputfd,&evt,sizeof(evt));
              evt.code = SYN_REPORT;
              evt.type = EV_SYN;
              evt.value = 0;
              write(inputfd,&evt,sizeof(evt));
          }
              break;
          case 3:
          {
              struct {
                  int x;
                  int y;
                  int id;
              } point;
              if(!readAll(&point,sizeof(point))) {
                  return;
              }
              struct input_event evt = {};
              evt.code = ABS_MT_SLOT;
              evt.type = EV_ABS;
              evt.value = point.id+1;
              write(inputfd,&evt,sizeof(evt));
              evt.code = ABS_MT_TRACKING_ID;
              evt.type = EV_ABS;
              evt.value = point.id+1;
              write(inputfd,&evt,sizeof(evt));
              evt.code = ABS_X;
              evt.type = EV_ABS;
              evt.value = point.x;
              write(inputfd,&evt,sizeof(evt));
              evt.code = ABS_Y;
              evt.type = EV_ABS;
              evt.value = point.y;
              write(inputfd,&evt,sizeof(evt));
              evt.code = ABS_MT_POSITION_X;
              evt.type = EV_ABS;
              evt.value = point.x;
              write(inputfd,&evt,sizeof(evt));
              evt.code = ABS_MT_POSITION_Y;
              evt.type = EV_ABS;
              evt.value = point.y;
              write(inputfd,&evt,sizeof(evt));
              evt.code = SYN_REPORT;
              evt.type = EV_SYN;
              evt.value = 0;
              write(inputfd,&evt,sizeof(evt));
          }
              break;
          }
      }
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
