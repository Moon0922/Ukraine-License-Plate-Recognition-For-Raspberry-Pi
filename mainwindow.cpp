#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QPainter>
#include <QPen>
#include <QFont>

//-------------------------------UART COMMUNICATION--------------------------------------------------
#include <unistd.h>			//Used for UART
#include <fcntl.h>			//Used for UART
#include <termios.h>		//Used for UART
#include <wiringPi.h>		//Used for GPIO
#include <sys/time.h>

int version = 3;
int uart0_filestream = -1;
unsigned char accumulatedData[256];
int commandLenght = 0;

// Variable that stores the next plate to send
unsigned char plateToSend[]={'?','?','?','?','?','?','?','?','?','?'};
// Variable that stores the last valid plate
unsigned char lastValidPlate[]={'?','?','?','?','?','?','?','?','?','?'};
// Variable that stores the last plate sent in UART
unsigned char lastSentPlate[]={'?','?','?','?','?','?','?','?','?','?'};
int plateCooldown = 30;
int validationFactor = 1;
int validationCounter = 0;
unsigned char address = 0;

long int msLastSecond = 0;
long int previousPlatesTimes[30];
unsigned char previousPlates[30][10];
int previousPlateCounter = 0;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    //-------------------------------UART COMMUNICATION--------------------------------------------------
        uart0_filestream = open("/dev/ttyS0", O_RDWR | O_NOCTTY | O_NDELAY);		//Open in non blocking read/write mode
        if (uart0_filestream == -1)
        {
            //ERROR - CAN'T OPEN SERIAL PORT
            printf("Error - Unable to open UART.  Ensure it is not in use by another application\n");
        }
        struct termios options;
        tcgetattr(uart0_filestream, &options);
        options.c_cflag = B19200 | CS8 | CLOCAL | CREAD;		//<Set baud rate
        options.c_iflag = IGNPAR;
        options.c_oflag = 0;
        options.c_lflag = 0;
        tcflush(uart0_filestream, TCIFLUSH);
        tcsetattr(uart0_filestream, TCSANOW, &options);

        plateToSend[0] = 0;

        wiringPiSetupPhys();
        pinMode(31, INPUT);
        pinMode(33, INPUT);
        pinMode(35, INPUT);
        pinMode(37, INPUT);

        struct timeval tp;
        gettimeofday(&tp, NULL);
        msLastSecond = tp.tv_sec * 1000 + tp.tv_usec / 1000;

    //---------------------------------------------------------------------------

    ui->setupUi(this);
    qPlayer = new QPlayer();
    QObject::connect( qPlayer,
                      SIGNAL(processedImage(QImage)),
                      this,
                      SLOT(updatePlayerUI(QImage)),
                      Qt::DirectConnection);
    qPlayer->loadCamera(0);
    memset(&lprResult, 0, sizeof(CARPLATE_DATA));
    if(qPlayer->isCameraOpened())
    {
        for(int i = 0; i < THREAD_COUNT; i++){
            qDetector[i] = new QDetector(qPlayer);
            QObject::connect( qDetector[i],
                              SIGNAL(sendLPRResult(CARPLATE_DATA)),
                              this,
                              SLOT(setLPRResult(CARPLATE_DATA)),
                              Qt::DirectConnection);
        }
    }
}

MainWindow::~MainWindow()
{
    if(qPlayer)
        delete qPlayer;
    for(int i = 0; i < THREAD_COUNT; i++){
        if(qDetector[i])
        {
            qDetector[i]->stop_detection();
            delete qDetector[i];
        }
    }
    delete ui;
}

void MainWindow::updatePlayerUI(QImage qImg)
{
    //-------------------------------UART COMMUNICATION----------------------------------------------------

        address = digitalRead(31);
        address += digitalRead(33)*2;
        address += digitalRead(35)*4;
        address += digitalRead(37)*8;
        //printf("ADDRESS: %i\n", address);


        if (uart0_filestream != -1)
        {
            // Read up to 255 characters from the port if they are there
            unsigned char rx_buffer[256];
            int rx_length = read(uart0_filestream, (void*)rx_buffer, 255);		//Filestream, buffer to store in, number of bytes to read (max)
            if (rx_length < 0)
            {
                //An error occured (will occur if there are no bytes)
            }
            else if (rx_length == 0)
            {
                //No data waiting
            }
            else
            {
                //Bytes received
                rx_buffer[rx_length] = '\0';
                //printf("%i bytes read : %s\n", rx_length, rx_buffer);


                for(int i=0;i<rx_length;i++){
                    accumulatedData[commandLenght] = rx_buffer[i];
                    commandLenght += 1;
                    if(commandLenght > 256){
                        commandLenght = 256;
                    }
                    if(rx_buffer[i]==0x7D){
                        //printf("\nFINAL ENCONTRADO\n");
                        unsigned char command[commandLenght];
                        for(int i=0;i<commandLenght;i++){
                            command[i] = accumulatedData[i];
                        }

                        if(command[1]==0x5B && command[2] == address && command[commandLenght-2] == calculateBCC(&command[0], commandLenght-2)){
                            unsigned char tx_buffer[20];
                            unsigned char *p_tx_buffer;
                            p_tx_buffer = &tx_buffer[0];
                            if(command[0] == 0x03){
                                *p_tx_buffer++ = 0x04;
                                *p_tx_buffer++ = 0x5D;
                                *p_tx_buffer++ = 0x5B;
                                *p_tx_buffer++ = address;
                                *p_tx_buffer++ = calculateBCC(&tx_buffer[0], 4);
                                *p_tx_buffer++ = 0x7D;

                            }else if(command[0] == 0x05){
                                if(plateToSend[0] != 63){
                                    printf("PLATE SENT");
                                    printf("\n");
                                    char length = 0;
                                    for(int i=0;i<10;i++){
                                        if(plateToSend[i] != 63){
                                            length++;
                                        }else{
                                            break;
                                        }
                                    }
                                    printf("%i largo\n", length);
                                    // Construct message to respond with plate number
                                    *p_tx_buffer++ = 0x06;
                                    *p_tx_buffer++ = 0x5D;
                                    *p_tx_buffer++ = 0x5B;
                                    *p_tx_buffer++ = address;
                                    *p_tx_buffer++ = length;
                                    for(int i = 0; i<length;i++){
                                        *p_tx_buffer++ = plateToSend[i];
                                    }
                                    *p_tx_buffer++ = calculateBCC(&tx_buffer[0], length+5);
                                    *p_tx_buffer++ = 0x7D;

                                    cleanPlatesAfterSend();
                                }else{
                                    *p_tx_buffer++ = 0x0B;
                                    *p_tx_buffer++ = 0x5D;
                                    *p_tx_buffer++ = 0x5B;
                                    *p_tx_buffer++ = address;
                                    *p_tx_buffer++ = version;
                                    *p_tx_buffer++ = calculateBCC(&tx_buffer[0], 5);
                                    *p_tx_buffer++ = 0x7D;
                                }
                            }else if(command[0] == 0x1A){
                                plateCooldown = command[3];
                                previousPlateCounter = 0;
                                printf("PLATE COOLDOWN CHANGED: %i\n", plateCooldown);
                                *p_tx_buffer++ = 0x0E;
                                *p_tx_buffer++ = 0x5D;
                                *p_tx_buffer++ = 0x00;
                                *p_tx_buffer++ = 0x58;
                                *p_tx_buffer++ = calculateBCC(&tx_buffer[0], 4);
                                *p_tx_buffer++ = 0x7D;
                            }else if(command[0] == 0x09){
                                validationFactor = command[3];
                                printf("VALIDATION FACTOR CHANGED: %i\n", validationFactor);
                                *p_tx_buffer++ = 0x0E;
                                *p_tx_buffer++ = 0x5D;
                                *p_tx_buffer++ = 0x00;
                                *p_tx_buffer++ = 0x58;
                                *p_tx_buffer++ = calculateBCC(&tx_buffer[0], 4);
                                *p_tx_buffer++ = 0x7D;
                            }
                            if (uart0_filestream != -1)
                            {
                                int count = write(uart0_filestream, &tx_buffer[0], (p_tx_buffer - &tx_buffer[0]));		//Filestream, bytes to write, number of bytes to write
                                if (count < 0)
                                {
                                    printf("UART TX error\n");
                                }
                                printf("LENGTH SENT: %i\n", tx_buffer[5]);
                            }
                            //printf("BCC: %i\n", calculateBCC(&tx_buffer[0], 4));
                            //printf("ADDRESS: %i\n", address);
                            //printf("COMMAND: %i\n", command[2]);
                        }


                        //printf("%i bytes read : %s\n", commandLenght, command);
                        commandLenght = 0;
                    }
                }
            }
        }

    //------------------------------------------------------------------------------------------

    m_Mutex.lock();
    QFont m_serifFont("MS Serif", 25, QFont::Bold);
    QPainter qPainter(&qImg);
    qPainter.setFont(m_serifFont);

    for(int i = 0; i < lprResult.nPlate; i++){
#if 1

        QPen m_penPlate(Qt::red);
        m_penPlate.setWidth(3);
        QPen m_penText(Qt::blue);
        qPainter.setPen(m_penPlate);
        qPainter.drawRect(lprResult.pPlate[i].rtPlate.left, lprResult.pPlate[i].rtPlate.top, 
                lprResult.pPlate[i].rtPlate.right - lprResult.pPlate[i].rtPlate.left, lprResult.pPlate[i].rtPlate.bottom - lprResult.pPlate[i].rtPlate.top);
        qPainter.setPen(m_penText);
        QString strCarNumber(pPlate[i].szLicense);

        //-------------------------------UART COMMUNICATION--------------------------------------------------

        unsigned char tempPlate[]={'?','?','?','?','?','?','?','?','?','?'};
        memcpy(tempPlate, strCarNumber.toStdString().c_str(), strCarNumber.size());
        validatePlate(&tempPlate[0], lprResult.pPlate[i].nTrust);
        //----------------------------------------------------------------------------------------------------

        QString strConf;
        strConf.sprintf("-[Conf:%.2f]", lprResult.pPlate[i].nTrust);
        strCarNumber += strConf + QString("%");
        QRectF rt(lprResult.pPlate[i].rtPlate.left, lprResult.pPlate[i].rtPlate.bottom, 500, 35);
        qPainter.fillRect(rt, Qt::yellow);
        qPainter.drawText(lprResult.pPlate[i].rtPlate.left, lprResult.pPlate[i].rtPlate.bottom + 30, strCarNumber);
#endif
    }
    ui->videoView->setPixmap(QPixmap::fromImage(qImg).scaled(ui->videoView->size(),
                                           Qt::KeepAspectRatio, Qt::FastTransformation));
    m_Mutex.unlock();
}

void MainWindow::setLPRResult(CARPLATE_DATA lResult)
{
    memcpy(&lprResult, &lResult, sizeof(CARPLATE_DATA));
}

void MainWindow::on_openButton_pressed()
{
    bool suc = qPlayer->loadCamera(0);
    if(qPlayer->isCameraOpened() && suc)
    {
        for(int i = 0; i < THREAD_COUNT; i++){
            qDetector[i] = new QDetector(qPlayer);
            QObject::connect( qDetector[i],
                              SIGNAL(sendLPRResult(CARPLATE_DATA)),
                              this,
                              SLOT(setLPRResult(CARPLATE_DATA)),
                              Qt::DirectConnection);
        }
    }
}

void MainWindow::on_closeButton_pressed()
{
//    if(qPlayer)
//        delete qPlayer;
//    for(int i = 0; i < THREAD_COUNT; i++){
//        if(qDetector[i])
//            delete qDetector[i];
//    }
    QApplication::quit();
}

//-------------------------------UART COMMUNICATION--------------------------------------------------

unsigned char MainWindow::calculateBCC(unsigned char* pointer, int lenght){
    unsigned char bcc = 0;
    for(int i=0;i<lenght;i++){
        if(i==0){
            bcc = *pointer;
        }else{
            bcc = bcc^*pointer;
        }
        pointer++;
    }
    return bcc;
}

void MainWindow::validatePlate(unsigned char* newPlatePointer, float conf){

    if(conf<90){
        //printf("Plate ignored %s, confidence: %f\n", newPlatePointer, conf);
        return;
    }else{
        //printf("\n");
        //printf("PLACA QUE SUPUESTAMENTE ESTA BIEN: %s\n", newPlatePointer);
        //printf("\n");
    }

    unsigned char newPlate[10];
    for(int i=0; i<10; i++){
        newPlate[i] = *newPlatePointer;
        newPlatePointer++;
    }

    if(validationCounter >= validationFactor-1){
        bool sendPlate = true;
        struct timeval tp;
        gettimeofday(&tp, NULL);
        long int msNow = tp.tv_sec * 1000 + tp.tv_usec / 1000;

        for(int i=0; i<30; i++){
            bool plateFound = true;
            for(int j=0; j<10; j++){
                if(previousPlates[i][j] != newPlate[j]){
                    plateFound = false;
                }
            }
            if(plateFound && previousPlatesTimes[i] > msNow - (plateCooldown*1000)){
                sendPlate = false;
                //printf("COOLDOWN NOT COMPLETED\n");
                break;
            }
        }
        if(sendPlate){
            previousPlateCounter++;
            if(previousPlateCounter >= 30){
                previousPlateCounter = 0;
            }

            for(int k=0; k<10; k++){
                plateToSend[k] = newPlate[k];
                previousPlates[previousPlateCounter][k] = newPlate[k];
            }
            previousPlatesTimes[previousPlateCounter] = msNow;

            validationCounter = 0;
            lastValidPlate[0] = 0;

            //printf("--------SENDING PLATE----------\n");
            //printf("PLATE: %s\n", plateToSend);
            //printf("-------------------------------\n");
        }
    }else{
        if(lastValidPlate[0] == 63){
            for(int k=0; k<10; k++){
                lastValidPlate[k] = newPlate[k];
            }
        }else{
            bool samePlate = true;
            for(int k=0; k<10; k++){
                if(lastValidPlate[k] != newPlate[k]){
                    samePlate = false;
                    break;
                }
            }
            if(samePlate){
                //AQUIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII
                validationCounter++;
            }else{
                lastValidPlate[0] = 63;
                validationCounter = 0;
            }
        }
    }
}

void MainWindow::cleanPlatesAfterSend(){
    for(int i=0; i<10; i++){
        lastSentPlate[i] = plateToSend[i];
        plateToSend[i] = '?';
    }
}
//---------------------------------------------------------------------------------------
