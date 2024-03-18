#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "qplayer.h"
#include "qdetector.h"
#include <unistd.h>			//Used for UART
#include <fcntl.h>			//Used for UART
#include <termios.h>		//Used for UART
#include <wiringPi.h>
#define THREAD_COUNT 2

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    unsigned char calculateBCC(unsigned char* pointer, int lenght);
    void validatePlate(unsigned char newPlate[7], float conf);
    void cleanPlatesAfterSend();

private:
    Ui::MainWindow *ui;

private:
    QMutex m_Mutex;
    QPlayer *qPlayer;
    QDetector *qDetector[THREAD_COUNT];
    CARPLATE_DATA lprResult;
private slots:
    void updatePlayerUI(QImage qImg);
    void setLPRResult(CARPLATE_DATA lResult);
    void on_openButton_pressed();
    void on_closeButton_pressed();
};

#endif // MAINWINDOW_H
