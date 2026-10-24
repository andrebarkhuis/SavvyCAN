#include <QCoreApplication>
#include <QTimer>
#include <cstdio>
#include "clihandler.h"
#include "connections/canconfactory.h"
#include "connections/canbus.h"
#include "framefileio.h"
#include "utility.h"

CLIHandler::CLIHandler(const Settings &settings, QObject *parent)
    : QObject(parent),
      mSettings(settings)
{
}

CLIHandler::~CLIHandler()
{
    if (mPlaybackObject) {
        mPlaybackObject->stopPlayback();
        mPlaybackObject->finalize();
        delete mPlaybackObject;
        mPlaybackObject = nullptr;
    }
    if (mISOTPHandler) {
        delete mISOTPHandler;
        mISOTPHandler = nullptr;
    }
    if (mUDSHandler) {
        delete mUDSHandler;
        mUDSHandler = nullptr;
    }
    if (mConnection) {
        CANConManager::getInstance()->remove(mConnection);
        mConnection->stop();
        delete mConnection;
        mConnection = nullptr;
    }
}

bool CLIHandler::start()
{
    // Set up playback if requested (can work without a live connection)
    if (!mSettings.playbackFile.isEmpty()) {
        setupPlayback();
    }

    // Create connection if a port is specified
    if (!mSettings.port.isEmpty()) {
        CANCon::type connType = resolveConnectionType(mSettings.connectionType);
        if (connType == CANCon::NONE) {
            fprintf(stderr, "Error: Unknown connection type '%s'\n",
                    qPrintable(mSettings.connectionType));
            fprintf(stderr, "Valid types: lawicel, serialbus, gvret, kayak, mqtt, canserver\n");
            return false;
        }

        mConnection = CanConFactory::create(connType, mSettings.port, mSettings.driver,
                                             mSettings.serialSpeed, mSettings.busSpeed);
        if (!mConnection) {
            fprintf(stderr, "Error: Failed to create %s connection on %s\n",
                    qPrintable(mSettings.connectionType), qPrintable(mSettings.port));
            return false;
        }

        CANBus bus;
        bus.setSpeed(mSettings.busSpeed);
        bus.setActive(true);
        bus.setListenOnly(mSettings.listenOnly);

        CANConManager *manager = CANConManager::getInstance();

        connect(manager, &CANConManager::framesReceived,
                this, &CLIHandler::framesReceived);
        connect(manager, &CANConManager::connectionStatusUpdated,
                this, &CLIHandler::connectionStatusUpdated);
        connect(mConnection, &CANConnection::status,
                this, &CLIHandler::connStatus);
        connect(mConnection, &CANConnection::debugOutput,
                this, &CLIHandler::connDebug);

        manager->add(mConnection);
        mConnection->start();
        mConnection->setBusSettings(0, bus);

        fprintf(stdout, "Connecting via %s on %s at %d bps...\n",
                qPrintable(mSettings.connectionType), qPrintable(mSettings.port), mSettings.busSpeed);
        if (!mSettings.driver.isEmpty()) {
            fprintf(stdout, "Driver: %s\n", qPrintable(mSettings.driver));
        }
        fflush(stdout);

        if (mSettings.listenOnly) {
            fprintf(stdout, "Listen-only mode enabled\n");
            fflush(stdout);
        }
    } else if (mSettings.playbackFile.isEmpty()) {
        fprintf(stderr, "Error: --port or --playback is required in CLI mode\n");
        return false;
    }

    // Set up ISO-TP if requested
    if (mSettings.isotpListen || !mSettings.isotpSendFrames.isEmpty()) {
        setupISOTP();
    }

    // Set up UDS if requested (implies ISO-TP)
    if (mSettings.udsListen || !mSettings.udsSendFrames.isEmpty()) {
        setupUDS();
    }

    if (!mSettings.outputFile.isEmpty()) {
        fprintf(stdout, "Output file: %s (format: %s)\n",
                qPrintable(mSettings.outputFile), qPrintable(mSettings.outputFormat));
        fflush(stdout);
    }

    return true;
}

void CLIHandler::setupISOTP()
{
    mISOTPHandler = new ISOTP_HANDLER();
    mISOTPHandler->setReception(true);
    mISOTPHandler->setProcessAll(true);
    mISOTPHandler->setFlowCtrl(true);
    mISOTPHandler->setExtendedAddressing(mSettings.isotpExtended);

    connect(mISOTPHandler, &ISOTP_HANDLER::newISOMessage,
            this, &CLIHandler::newISOTPMessage);

    fprintf(stdout, "ISO-TP listener enabled%s\n",
            mSettings.isotpExtended ? " (extended addressing)" : "");
    fflush(stdout);
}

void CLIHandler::setupUDS()
{
    mUDSHandler = new UDS_HANDLER();
    mUDSHandler->setReception(true);
    mUDSHandler->setProcessAllIDs(true);
    mUDSHandler->setFlowCtrl(true);

    connect(mUDSHandler, &UDS_HANDLER::newUDSMessage,
            this, &CLIHandler::newUDSMessage);

    fprintf(stdout, "UDS listener enabled\n");
    fflush(stdout);
}

void CLIHandler::setupPlayback()
{
    fprintf(stdout, "Loading %s...\n", qPrintable(mSettings.playbackFile));
    fflush(stdout);

    if (!FrameFileIO::autoDetectLoadFile(mSettings.playbackFile, &mPlaybackSequence.data)) {
        fprintf(stderr, "Error: Failed to load file %s\n", qPrintable(mSettings.playbackFile));
        return;
    }

    mPlaybackTotalFrames = mPlaybackSequence.data.count();
    fprintf(stdout, "Loaded %d frames from %s\n",
            mPlaybackTotalFrames, qPrintable(mSettings.playbackFile));
    fflush(stdout);

    // Enable all frame IDs in the filter
    for (const CANFrame &frame : mPlaybackSequence.data) {
        mPlaybackSequence.idFilters[frame.frameId()] = true;
    }

    mPlaybackSequence.filename = mSettings.playbackFile;
    mPlaybackSequence.maxLoops = mSettings.playbackLoops;
    mPlaybackSequence.currentLoopCount = 0;

    mPlaybackObject = new FramePlaybackObject();
    mPlaybackObject->initialize();
    mPlaybackObject->setSequenceObject(&mPlaybackSequence);
    mPlaybackObject->setPlaybackInterval(mSettings.playbackSpeed);
    mPlaybackObject->setPlaybackBurst(mSettings.playbackBurst);
    mPlaybackObject->setUseOriginalTiming(mSettings.playbackOriginalTiming);
    mPlaybackObject->setSendingBus(mSettings.playbackBus);
    mPlaybackObject->setNumBuses(CANConManager::getInstance()->getNumBuses());

    connect(mPlaybackObject, &FramePlaybackObject::EndOfFrameCache,
            this, &CLIHandler::playbackFinished);
    connect(mPlaybackObject, &FramePlaybackObject::statusUpdate,
            this, &CLIHandler::playbackStatus);
}

void CLIHandler::shutdown()
{
    fprintf(stdout, "\nShutting down...\n");
    fflush(stdout);

    if (mPlaybackObject) {
        mPlaybackObject->stopPlayback();
    }

    if (!mSettings.outputFile.isEmpty() && !mCapturedFrames.isEmpty()) {
        saveToFile();
    }

    fprintf(stdout, "Captured %d frames total.\n", mReceivedCount);
    fflush(stdout);

    if (mPlaybackObject) {
        mPlaybackObject->finalize();
        delete mPlaybackObject;
        mPlaybackObject = nullptr;
    }
    if (mISOTPHandler) {
        delete mISOTPHandler;
        mISOTPHandler = nullptr;
    }
    if (mUDSHandler) {
        delete mUDSHandler;
        mUDSHandler = nullptr;
    }
    if (mConnection) {
        CANConManager::getInstance()->remove(mConnection);
        mConnection->stop();
        delete mConnection;
        mConnection = nullptr;
    }

    QCoreApplication::quit();
}

void CLIHandler::framesReceived(CANConnection* conn, QVector<CANFrame>& frames)
{
    Q_UNUSED(conn);

    for (const CANFrame &frame : frames) {
        printFrame(frame);

        if (!mSettings.outputFile.isEmpty()) {
            mCapturedFrames.append(frame);
        }

        mReceivedCount++;

        if (mSettings.frameCount > 0 && mReceivedCount >= mSettings.frameCount) {
            fprintf(stdout, "Reached frame count limit (%d)\n", mSettings.frameCount);
            fflush(stdout);
            QTimer::singleShot(0, this, &CLIHandler::shutdown);
            return;
        }
    }
}

void CLIHandler::connectionStatusUpdated(int numBuses)
{
    if (numBuses > 0 && !mConnected) {
        mConnected = true;
        fprintf(stdout, "Connected! Listening for CAN frames...\n");
        fflush(stdout);

        if (!mSettings.sendFrames.isEmpty() && !mFramesSent) {
            sendQueuedFrames();
        }

        // Send ISO-TP frames after connection
        if (!mSettings.isotpSendFrames.isEmpty()) {
            sendISOTPFrames();
        }

        // Send UDS frames after connection
        if (!mSettings.udsSendFrames.isEmpty()) {
            sendUDSFrames();
        }

        // Start playback after connection if we have a playback file
        if (mPlaybackObject && mPlaybackTotalFrames > 0) {
            if (mPlaybackObject) {
                mPlaybackObject->setNumBuses(numBuses);
            }
            fprintf(stdout, "Starting playback (%d frames, %d loop(s))...\n",
                    mPlaybackTotalFrames, mSettings.playbackLoops);
            fflush(stdout);
            mPlaybackObject->startPlaybackForward();
        }
    } else if (numBuses == 0 && mConnected) {
        mConnected = false;
        fprintf(stdout, "Disconnected from device.\n");
        fflush(stdout);
    }
}

void CLIHandler::connStatus(CANConStatus pStatus)
{
    if (pStatus.conStatus == CANCon::CONNECTED) {
        fprintf(stdout, "Device connected (%d bus(es))\n", pStatus.numHardwareBuses);
        fflush(stdout);
    } else {
        fprintf(stdout, "Device not connected\n");
        fflush(stdout);
    }
}

void CLIHandler::connDebug(QString debugText)
{
    fprintf(stderr, "[debug] %s\n", qPrintable(debugText));
    fflush(stderr);
}

void CLIHandler::newISOTPMessage(ISOTP_MESSAGE msg)
{
    uint64_t timestamp = msg.timeStamp().seconds() * 1000000 + msg.timeStamp().microSeconds();
    double tsSeconds = timestamp / 1000000.0;

    fprintf(stdout, "[ISOTP] (%013.6f) can%d  %s  [%d bytes] ",
            tsSeconds, msg.bus,
            qPrintable(Utility::formatCANID(msg.frameId(), msg.hasExtendedFrameFormat())),
            msg.payload().length());

    for (int i = 0; i < msg.payload().length(); i++) {
        if (i > 0) fprintf(stdout, " ");
        fprintf(stdout, "%s",
                qPrintable(Utility::formatByteAsHex(static_cast<uint8_t>(msg.payload()[i]))));
    }

    fprintf(stdout, "\n");
    fflush(stdout);
}

void CLIHandler::newUDSMessage(UDS_MESSAGE msg)
{
    uint64_t timestamp = msg.timeStamp().seconds() * 1000000 + msg.timeStamp().microSeconds();
    double tsSeconds = timestamp / 1000000.0;

    QString serviceName;
    if (mUDSHandler) {
        serviceName = mUDSHandler->getServiceShortDesc(msg.service);
    }
    if (serviceName.isEmpty()) {
        serviceName = QString("0x%1").arg(msg.service, 2, 16, QChar('0')).toUpper();
    }

    if (msg.isErrorReply) {
        QString errorName;
        if (mUDSHandler) {
            errorName = mUDSHandler->getNegativeResponseShort(msg.subFunc);
        }
        if (errorName.isEmpty()) {
            errorName = QString("0x%1").arg(msg.subFunc, 2, 16, QChar('0')).toUpper();
        }
        fprintf(stdout, "[UDS] (%013.6f) can%d  %s  NEG_RESPONSE  Service:%s  Error:%s\n",
                tsSeconds, msg.bus,
                qPrintable(Utility::formatCANID(msg.frameId(), msg.hasExtendedFrameFormat())),
                qPrintable(serviceName),
                qPrintable(errorName));
    } else {
        fprintf(stdout, "[UDS] (%013.6f) can%d  %s  %s  SubFunc:%02X  [%d bytes] ",
                tsSeconds, msg.bus,
                qPrintable(Utility::formatCANID(msg.frameId(), msg.hasExtendedFrameFormat())),
                qPrintable(serviceName),
                msg.subFunc,
                msg.payload().length());

        for (int i = 0; i < msg.payload().length(); i++) {
            if (i > 0) fprintf(stdout, " ");
            fprintf(stdout, "%s",
                    qPrintable(Utility::formatByteAsHex(static_cast<uint8_t>(msg.payload()[i]))));
        }
        fprintf(stdout, "\n");
    }
    fflush(stdout);
}

void CLIHandler::playbackFinished()
{
    fprintf(stdout, "Playback complete.\n");
    fflush(stdout);

    // If we're only doing playback (no live capture), shutdown
    if (mSettings.port.isEmpty() || mSettings.frameCount == 0) {
        QTimer::singleShot(500, this, &CLIHandler::shutdown);
    }
}

void CLIHandler::playbackStatus(int frameNum)
{
    fprintf(stdout, "\r[%d/%d] Playing back...", frameNum, mPlaybackTotalFrames);
    fflush(stdout);
}

void CLIHandler::printFrame(const CANFrame &frame)
{
    uint64_t timestamp = frame.timeStamp().seconds() * 1000000 + frame.timeStamp().microSeconds();
    double tsSeconds = timestamp / 1000000.0;

    fprintf(stdout, "(%013.6f) can%d  %s  [%d] ",
            tsSeconds, frame.bus,
            qPrintable(Utility::formatCANID(frame.frameId(), frame.hasExtendedFrameFormat())),
            frame.payload().length());

    for (int i = 0; i < frame.payload().length(); i++) {
        if (i > 0) fprintf(stdout, " ");
        fprintf(stdout, "%s",
                qPrintable(Utility::formatByteAsHex(static_cast<uint8_t>(frame.payload()[i]))));
    }

    if (!frame.isReceived) {
        fprintf(stdout, "  TX");
    }

    fprintf(stdout, "\n");
    fflush(stdout);
}

void CLIHandler::sendQueuedFrames()
{
    mFramesSent = true;
    CANConManager *manager = CANConManager::getInstance();

    for (const QString &frameStr : mSettings.sendFrames) {
        CANFrame frame = parseFrameString(frameStr);
        if (frame.frameId() == 0 && frame.payload().isEmpty()) {
            fprintf(stderr, "Warning: Could not parse frame: %s\n", qPrintable(frameStr));
            continue;
        }

        fprintf(stdout, "Sending: %s\n", qPrintable(frameStr));
        fflush(stdout);
        if (!manager->sendFrame(frame)) {
            fprintf(stderr, "Error: Failed to send frame: %s\n", qPrintable(frameStr));
        }
    }
}

void CLIHandler::sendISOTPFrames()
{
    if (!mISOTPHandler) return;

    for (const QString &frameStr : mSettings.isotpSendFrames) {
        // Format: ID#HEXDATA
        QStringList parts = frameStr.split('#');
        if (parts.size() != 2) {
            fprintf(stderr, "Warning: Invalid ISO-TP frame format: %s (use ID#HEXDATA)\n",
                    qPrintable(frameStr));
            continue;
        }

        bool ok = false;
        uint32_t id = parts[0].toUInt(&ok, 16);
        if (!ok) {
            fprintf(stderr, "Warning: Invalid ID in ISO-TP frame: %s\n", qPrintable(frameStr));
            continue;
        }

        QByteArray data;
        QString dataStr = parts[1];
        for (int i = 0; i < dataStr.length(); i += 2) {
            bool byteOk = false;
            uint8_t byte = dataStr.mid(i, 2).toUInt(&byteOk, 16);
            if (!byteOk) {
                fprintf(stderr, "Warning: Invalid hex data in ISO-TP frame: %s\n",
                        qPrintable(frameStr));
                data.clear();
                break;
            }
            data.append(static_cast<char>(byte));
        }

        if (!data.isEmpty()) {
            fprintf(stdout, "Sending ISO-TP: ID=%s Data=[%d bytes]\n",
                    qPrintable(parts[0]), data.length());
            fflush(stdout);
            mISOTPHandler->sendISOTPFrame(0, id, data);
        }
    }
}

void CLIHandler::sendUDSFrames()
{
    if (!mUDSHandler) return;

    for (const QString &frameStr : mSettings.udsSendFrames) {
        // Format: ID#SERVICE.SUBFUNC.DATA  e.g. 7E0#10.01 or 7E0#22.F184
        QStringList parts = frameStr.split('#');
        if (parts.size() != 2) {
            fprintf(stderr, "Warning: Invalid UDS frame format: %s (use ID#SERVICE.SUBFUNC[.DATA])\n",
                    qPrintable(frameStr));
            continue;
        }

        bool ok = false;
        uint32_t id = parts[0].toUInt(&ok, 16);
        if (!ok) {
            fprintf(stderr, "Warning: Invalid ID in UDS frame: %s\n", qPrintable(frameStr));
            continue;
        }

        QStringList fields = parts[1].split('.');
        if (fields.size() < 2) {
            fprintf(stderr, "Warning: UDS frame needs at least SERVICE.SUBFUNC: %s\n",
                    qPrintable(frameStr));
            continue;
        }

        UDS_MESSAGE msg;
        msg.setFrameId(id);
        msg.bus = 0;
        msg.isReceived = false;
        msg.setFrameType(QCanBusFrame::DataFrame);
        if (id > 0x7FF) msg.setExtendedFrameFormat(true);

        msg.service = fields[0].toUInt(&ok, 16);
        if (!ok) {
            fprintf(stderr, "Warning: Invalid service byte: %s\n", qPrintable(fields[0]));
            continue;
        }

        // SubFunc can be multi-byte (e.g., F184 for DID)
        QByteArray subFuncAndData;
        for (int i = 1; i < fields.size(); i++) {
            QString field = fields[i];
            for (int j = 0; j < field.length(); j += 2) {
                bool byteOk = false;
                uint8_t byte = field.mid(j, 2).toUInt(&byteOk, 16);
                if (byteOk) {
                    subFuncAndData.append(static_cast<char>(byte));
                }
            }
        }

        if (!subFuncAndData.isEmpty()) {
            msg.subFunc = static_cast<uint8_t>(subFuncAndData[0]);
            msg.subFuncLen = qMin(subFuncAndData.length(), 3);
        }
        msg.setPayload(subFuncAndData);

        QString serviceName = mUDSHandler->getServiceShortDesc(msg.service);
        fprintf(stdout, "Sending UDS: ID=%s Service=%s(0x%02X) Data=[%d bytes]\n",
                qPrintable(parts[0]),
                serviceName.isEmpty() ? "UNKNOWN" : qPrintable(serviceName),
                msg.service, subFuncAndData.length());
        fflush(stdout);

        mUDSHandler->sendUDSFrame(msg);
    }
}

bool CLIHandler::saveToFile()
{
    bool result = false;
    QString format = mSettings.outputFormat.toLower();

    fprintf(stdout, "Saving %d frames to %s...\n",
            mCapturedFrames.size(), qPrintable(mSettings.outputFile));
    fflush(stdout);

    if (format == "crtd") {
        result = FrameFileIO::saveCRTDFile(mSettings.outputFile, &mCapturedFrames);
    } else if (format == "candump") {
        result = FrameFileIO::saveCanDumpFile(mSettings.outputFile, &mCapturedFrames);
    } else {
        result = FrameFileIO::saveNativeCSVFile(mSettings.outputFile, &mCapturedFrames);
    }

    if (result) {
        fprintf(stdout, "File saved successfully.\n");
    } else {
        fprintf(stderr, "Error: Failed to save file.\n");
    }
    fflush(stdout);

    return result;
}

CANCon::type CLIHandler::resolveConnectionType(const QString &typeStr)
{
    if (typeStr == "lawicel")    return CANCon::LAWICEL;
    if (typeStr == "serialbus")  return CANCon::SERIALBUS;
    if (typeStr == "gvret")      return CANCon::GVRET_SERIAL;
    if (typeStr == "kayak")      return CANCon::KAYAK;
    if (typeStr == "mqtt")       return CANCon::MQTT;
    if (typeStr == "canserver")  return CANCon::CANSERVER;
    return CANCon::NONE;
}

CANFrame CLIHandler::parseFrameString(const QString &frameStr)
{
    CANFrame frame;

    QStringList parts = frameStr.split('#');
    if (parts.size() != 2) {
        return frame;
    }

    bool ok = false;
    uint32_t id = parts[0].toUInt(&ok, 16);
    if (!ok) {
        return frame;
    }

    frame.setFrameId(id);
    frame.bus = 0;
    frame.isReceived = false;
    frame.setFrameType(QCanBusFrame::DataFrame);

    if (id > 0x7FF) {
        frame.setExtendedFrameFormat(true);
    }

    QString dataStr = parts[1];
    if (dataStr.length() % 2 != 0) {
        return CANFrame();
    }

    QByteArray data;
    for (int i = 0; i < dataStr.length(); i += 2) {
        bool byteOk = false;
        uint8_t byte = dataStr.mid(i, 2).toUInt(&byteOk, 16);
        if (!byteOk) {
            return CANFrame();
        }
        data.append(static_cast<char>(byte));
    }

    frame.setPayload(data);
    return frame;
}
