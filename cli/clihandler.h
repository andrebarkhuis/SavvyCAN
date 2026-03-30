#ifndef CLIHANDLER_H
#define CLIHANDLER_H

#include <QObject>
#include <QVector>
#include "can_structs.h"
#include "connections/canconmanager.h"
#include "connections/canconnection.h"
#include "bus_protocols/isotp_handler.h"
#include "bus_protocols/uds_handler.h"
#include "frameplaybackobject.h"

class CLIHandler : public QObject
{
    Q_OBJECT

public:
    struct Settings {
        QString port;
        int busSpeed = 500000;
        int serialSpeed = 115200;
        bool listenOnly = false;
        QString outputFile;
        QString outputFormat = "csv";
        int frameCount = -1;
        QStringList sendFrames;
        // ISO-TP
        bool isotpListen = false;
        QStringList isotpSendFrames;
        bool isotpExtended = false;
        // UDS
        bool udsListen = false;
        QStringList udsSendFrames;
        // Playback
        QString playbackFile;
        int playbackSpeed = 5;
        int playbackBurst = 1;
        bool playbackOriginalTiming = false;
        int playbackBus = 0;
        int playbackLoops = 1;
    };

    explicit CLIHandler(const Settings &settings, QObject *parent = nullptr);
    ~CLIHandler();

    bool start();

public slots:
    void shutdown();

private slots:
    void framesReceived(CANConnection* conn, QVector<CANFrame>& frames);
    void connectionStatusUpdated(int numBuses);
    void connStatus(CANConStatus pStatus);
    void connDebug(QString debugText);
    void newISOTPMessage(ISOTP_MESSAGE msg);
    void newUDSMessage(UDS_MESSAGE msg);
    void playbackFinished();
    void playbackStatus(int frameNum);

private:
    void printFrame(const CANFrame &frame);
    void sendQueuedFrames();
    void setupISOTP();
    void setupUDS();
    void setupPlayback();
    void sendISOTPFrames();
    void sendUDSFrames();
    bool saveToFile();
    CANFrame parseFrameString(const QString &frameStr);

    Settings mSettings;
    CANConnection *mConnection = nullptr;
    QVector<CANFrame> mCapturedFrames;
    int mReceivedCount = 0;
    bool mConnected = false;
    bool mFramesSent = false;

    // ISO-TP / UDS
    ISOTP_HANDLER *mISOTPHandler = nullptr;
    UDS_HANDLER *mUDSHandler = nullptr;

    // Playback
    FramePlaybackObject *mPlaybackObject = nullptr;
    SequenceItem mPlaybackSequence;
    int mPlaybackTotalFrames = 0;
};

#endif // CLIHANDLER_H
