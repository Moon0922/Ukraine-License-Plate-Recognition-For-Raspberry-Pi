#ifndef QDETECTOR_H
#define QDETECTOR_H

#include "qplayer.h"
#include "../UALPREngine/UALPREngine.h"
class QDetector : public QThread
{
    Q_OBJECT
public:
    QDetector(QPlayer* player);
    ~QDetector();

private:
    QMutex mutex;
    QWaitCondition condition;
    QPlayer* qPlayer;
    bool isStarted;
    CARPLATE_DATA resultData;
signals:
     void sendLPRResult(CARPLATE_DATA result);
protected:
    void run();

public:
    void stop_detection();
};

#endif // QDETECTOR_H
