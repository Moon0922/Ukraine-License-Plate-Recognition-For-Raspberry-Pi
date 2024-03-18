#include "qplayer.h"
#include <QMessageBox>
#include <time.h>
#include <QDebug>
QPlayer::QPlayer()
{
    m_nPlaySpeed = 1;
    m_bOpen = false;
    fscale = 1.0f;
}

QPlayer::~QPlayer()
{
    mutex.lock();
    m_bOpen = false;
    condition.wakeOne();
    mutex.unlock();
    wait();
}

bool QPlayer::loadCamera(int nIndex)
{
    if(m_bOpen){
        QMessageBox messageBox;
        messageBox.setText("The camera already is opened.");
        messageBox.exec();
        return false;
    }
    capture.open(nIndex);
    m_bOpen = false;
    if (capture.isOpened()){
        frameRate = (int) capture.get(CV_CAP_PROP_FPS);
        qDebug() << frameRate;
        start(HighPriority);
        m_bOpen = true;
        return true;
    }
    else{
        QMessageBox msg;
        msg.setText("The camera is not connected. Please connect with camera and open.");
        msg.exec();
        return false;
    }
}

bool QPlayer::isCameraOpened()
{
    return m_bOpen;
}

void QPlayer::push_mat(Mat mImg)
{
    if(bufferFrames.size() >= 10)
        pop_mat();
    mutex.lock();
    bufferFrames.push_back(mImg);
    mutex.unlock();
}

void QPlayer::pop_mat()
{
    mutex.lock();
    if(bufferFrames.size() > 0)
        bufferFrames.erase(bufferFrames.begin());
    mutex.unlock();
}

void QPlayer::read_mat(Mat &mImg)
{
    mutex.lock();
    if(bufferFrames.size() > 0)
        bufferFrames.at(0).copyTo(mImg);
    mutex.unlock();
    pop_mat();
}

void mmsleep(int ms)
{
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000 * 1000 };
    nanosleep(&ts, NULL);
}

void QPlayer::run()
{
    int delay = (1000 / frameRate);
    int64 time;
    int64 freq = getTickFrequency();
    int timeiInMs;
    while(m_bOpen){
        time = getTickCount();
        for(int nSkipCnt = 0; nSkipCnt < m_nPlaySpeed; nSkipCnt++){
            capture.read(frame);
        }
        if(frame.empty())
            continue;
        push_mat(frame);
        if(frame.channels() == 3)
        {
            cvtColor(frame, RGBframe, CV_BGR2RGB);
            img = QImage((const unsigned char*)(RGBframe.data), RGBframe.cols, RGBframe.rows, QImage::Format_RGB888);
        }else{
            img = QImage((const unsigned char*)(frame.data), frame.cols, frame.rows, QImage::Format_Indexed8);
        }
        emit processedImage(img);
        time = getTickCount() - time;
        timeiInMs = (time * 1000) / freq;
        if(timeiInMs < delay){
           mmsleep(delay - timeiInMs);
        }
    }
}

