#include "mainwindow.h"
#include <QApplication>
#include <QCommandLineParser>
#include "cli/clihandler.h"

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <signal.h>
#endif

class SavvyCANApplication : public QApplication
{
public:
    MainWindow *mainWindow;

    SavvyCANApplication(int &argc, char **argv) : QApplication(argc, argv)
    {
    }

    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::FileOpen)
        {
            QFileOpenEvent *openEvent = static_cast<QFileOpenEvent *>(event);
            mainWindow->handleDroppedFile(openEvent->file());
        }

        return QApplication::event(event);
    }
};

static CLIHandler *g_cliHandler = nullptr;

#ifdef Q_OS_WIN
static BOOL WINAPI consoleHandler(DWORD signal)
{
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT) {
        if (g_cliHandler) {
            QMetaObject::invokeMethod(g_cliHandler, "shutdown", Qt::QueuedConnection);
        }
        return TRUE;
    }
    return FALSE;
}
#else
static void unixSignalHandler(int)
{
    if (g_cliHandler) {
        QMetaObject::invokeMethod(g_cliHandler, "shutdown", Qt::QueuedConnection);
    }
}
#endif

static bool hasCliFlag(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        if (QString(argv[i]) == "--cli") return true;
    }
    return false;
}

static int runCli(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setOrganizationName("EVTV");
    app.setApplicationName("SavvyCAN");
    app.setOrganizationDomain("evtv.me");
    QSettings::setDefaultFormat(QSettings::IniFormat);

    QCommandLineParser parser;
    parser.setApplicationDescription("SavvyCAN - CAN bus tool (CLI mode)");
    parser.addHelpOption();

    // Connection options
    parser.addOption(QCommandLineOption(QStringList() << "cli", "Run in CLI mode (no GUI)"));
    parser.addOption(QCommandLineOption(QStringList() << "p" << "port", "Serial port for LAWicel device", "port"));
    parser.addOption(QCommandLineOption(QStringList() << "s" << "speed", "CAN bus speed in bps (default: 500000)", "speed", "500000"));
    parser.addOption(QCommandLineOption(QStringList() << "serial-speed", "Serial baud rate (default: 115200)", "baud", "115200"));
    parser.addOption(QCommandLineOption(QStringList() << "send", "Send a CAN frame (ID#HEXDATA)", "frame"));
    parser.addOption(QCommandLineOption(QStringList() << "o" << "output", "Save captured frames to file", "file"));
    parser.addOption(QCommandLineOption(QStringList() << "f" << "format", "Output format: csv, crtd, candump (default: csv)", "format", "csv"));
    parser.addOption(QCommandLineOption(QStringList() << "listen-only", "Enable listen-only mode"));
    parser.addOption(QCommandLineOption(QStringList() << "c" << "count", "Stop after N frames (default: unlimited)", "count"));
    // ISO-TP options
    parser.addOption(QCommandLineOption(QStringList() << "isotp-listen", "Enable ISO-TP message reassembly"));
    parser.addOption(QCommandLineOption(QStringList() << "isotp-send", "Send ISO-TP message (ID#HEXDATA)", "frame"));
    parser.addOption(QCommandLineOption(QStringList() << "isotp-extended", "Use ISO-TP extended addressing"));
    // UDS options
    parser.addOption(QCommandLineOption(QStringList() << "uds-listen", "Enable UDS message decoding"));
    parser.addOption(QCommandLineOption(QStringList() << "uds-send", "Send UDS request (ID#SERVICE.SUBFUNC[.DATA])", "frame"));
    // Playback options
    parser.addOption(QCommandLineOption(QStringList() << "playback", "Play back a CAN log file to hardware", "file"));
    parser.addOption(QCommandLineOption(QStringList() << "playback-speed", "Playback timer interval in ms (default: 5)", "ms", "5"));
    parser.addOption(QCommandLineOption(QStringList() << "playback-burst", "Frames per tick (default: 1)", "n", "1"));
    parser.addOption(QCommandLineOption(QStringList() << "playback-original", "Use original frame timing from file"));
    parser.addOption(QCommandLineOption(QStringList() << "playback-bus", "Send on bus N (-1=all, -2=from file, default: 0)", "n", "0"));
    parser.addOption(QCommandLineOption(QStringList() << "playback-loop", "Loop N times (-1=infinite, default: 1)", "n", "1"));

    parser.process(app);

    if (!parser.isSet("port") && !parser.isSet("playback")) {
        fprintf(stderr, "Error: --port or --playback is required in CLI mode\n");
        fprintf(stderr, "Usage: SavvyCAN --cli --port COM3 --speed 500000\n");
        fprintf(stderr, "       SavvyCAN --cli --port COM3 --playback capture.csv\n");
        return 1;
    }

    CLIHandler::Settings settings;
    settings.port = parser.value("port");
    settings.busSpeed = parser.value("speed").toInt();
    settings.serialSpeed = parser.value("serial-speed").toInt();
    settings.listenOnly = parser.isSet("listen-only");
    settings.outputFile = parser.value("output");
    settings.outputFormat = parser.value("format");
    settings.sendFrames = parser.values("send");

    if (parser.isSet("count")) {
        settings.frameCount = parser.value("count").toInt();
    }

    // ISO-TP settings
    settings.isotpListen = parser.isSet("isotp-listen");
    settings.isotpSendFrames = parser.values("isotp-send");
    settings.isotpExtended = parser.isSet("isotp-extended");

    // UDS settings
    settings.udsListen = parser.isSet("uds-listen");
    settings.udsSendFrames = parser.values("uds-send");

    // Playback settings
    settings.playbackFile = parser.value("playback");
    settings.playbackSpeed = parser.value("playback-speed").toInt();
    settings.playbackBurst = parser.value("playback-burst").toInt();
    settings.playbackOriginalTiming = parser.isSet("playback-original");
    settings.playbackBus = parser.value("playback-bus").toInt();
    settings.playbackLoops = parser.value("playback-loop").toInt();

    CLIHandler handler(settings);
    g_cliHandler = &handler;

#ifdef Q_OS_WIN
    SetConsoleCtrlHandler(consoleHandler, TRUE);
#else
    struct sigaction sa;
    sa.sa_handler = unixSignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
#endif

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&handler]() {
        g_cliHandler = nullptr;
    });

    if (!handler.start()) {
        return 1;
    }

    return app.exec();
}

int main(int argc, char *argv[])
{
#ifdef QT_DEBUG
    //uncomment for verbose debug data in application output
    //qputenv("QT_FATAL_WARNINGS", "1");
    //qSetMessagePattern("Type: %{type}\nProduct Name: %{appname}\nFile: %{file}\nLine: %{line}\nMethod: %{function}\nThreadID: %{threadid}\nThreadPtr: %{qthreadptr}\nMessage: %{message}");
#endif

    if (hasCliFlag(argc, argv)) {
        return runCli(argc, argv);
    }

    SavvyCANApplication a(argc, argv);

    //Add a local path for Qt extensions, to allow for per-application extensions.
    a.addLibraryPath("plugins");

    //These things are used by QSettings to set up setting storage
    a.setOrganizationName("EVTV");
    a.setApplicationName("SavvyCAN");
    a.setOrganizationDomain("evtv.me");
    QSettings::setDefaultFormat(QSettings::IniFormat);

    a.mainWindow = new MainWindow();

    QSettings settings;
    int fontSize = settings.value("Main/FontSize", 9).toUInt();
    QFont sysFont = QFont(); //get default font
    sysFont.setPointSize(fontSize);
    a.setFont(sysFont);

    a.mainWindow->show();

    int retCode = a.exec();

    delete a.mainWindow; a.mainWindow = NULL;

    return retCode;
}
