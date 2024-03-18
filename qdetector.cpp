#include "qdetector.h"
#include <QDebug>
QDetector::QDetector(QPlayer* player)
{
    qPlayer = player;
    isStarted = true;
    start(LowPriority);
}

QDetector::~QDetector()
{
    wait();
}


void QDetector::run()
{
    void* m_hEngineHandle = UALPR_EngineHandleCreate();
    while(isStarted){
        Mat mImg;
        qPlayer->read_mat(mImg);
        if(mImg.empty())
           continue;
        Mat gray;
        cvtColor(mImg, gray, CV_BGR2GRAY);
        InitSet iniSet;
        memset(&iniSet, 0, sizeof(InitSet));
        iniSet.skewAng = 0;
        UALPR_EngineProcess(m_hEngineHandle, gray.data, gray.cols, gray.rows, &iniSet, &resultData);
        emit sendLPRResult(resultData);
    }
    UALPR_EngineHandleDestroy(m_hEngineHandle);
}

void QDetector::stop_detection()
{
    isStarted = false;
}
