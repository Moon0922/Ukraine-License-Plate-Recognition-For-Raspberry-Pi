#ifndef QPLAYER_H
#define QPLAYER_H

#include <QThread>
#include <QImage>
#include <QWaitCondition>
#include <vector>
#include <qmutex.h>
#include "opencv2/opencv.hpp"
using namespace cv;
using namespace std;

class QPlayer : public QThread
{
    Q_OBJECT
public:
    QPlayer();
    ~QPlayer();
private:
   QMutex mutex;
   QWaitCondition condition;
   Mat frame, RGBframe;
   int frameRate;
   float fscale;
   VideoCapture capture;
   QImage img;
   bool m_bOpen;
   vector<Mat> bufferFrames;
   int m_nPlaySpeed;
protected:
    void run();

signals:
     void processedImage(QImage qImg);

public:
   bool loadCamera(int nIndex);
   bool isCameraOpened();
   void push_mat(Mat mImg);
   void pop_mat();
   void read_mat(Mat& mImg);
};

#endif // QPLAYER_H
