#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSerialPortInfo>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QBluetoothAddress>
#include <QTextStream>
#include <csignal>
#include <cstdio>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#include <sys/socket.h>
#include <QSocketNotifier>
#endif

#include "Server.h"

#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)

/*
 =======================================================================================================================
    Log of the command line server: the text of the server (sent in parts) is written by lines, with the time, on the
    console (colours if it is a terminal) and in the log file. A line repeated is written once, with the number of
    repetitions (attempts to connect).
 =======================================================================================================================
 */
class Logger
{
public:
    bool        m_bConsole = true;
    bool        m_bColors = true;
    bool        m_bBrief = false;       // no details of the BDOS functions
    QFile       m_oFile;

    void vAdd(tdLogType _eLogType, const QString &_szText, bool _bModify)
    {
        for(const QChar &rcChar : _szText)
        {
            if(m_szPending.isEmpty())
            {
                m_eLineType = _eLogType;
                m_bLineModify = _bModify;
            }

            if(rcChar == '\n')
            {
                vLine(m_szPending);
                m_szPending.clear();
            }
            else if(rcChar != '\r')
            {
                m_szPending += rcChar;
            }
        }
    }

    void vFlush()
    {
        if(!m_szPending.isEmpty())
        {
            vLine(m_szPending);
            m_szPending.clear();
        }
        vRepeated();
    }

private:
    QString     m_szPending;
    QString     m_szLine;               // last line written
    tdLogType   m_eLineType = eLogInfo;
    bool        m_bLineModify = false;
    int         m_iRepeated = 0;

    void vLine(const QString &_szLine)
    {
        if(_szLine.trimmed().isEmpty()) return;
        if(m_bBrief && (m_eLineType == eLogBDOSDetails)) return;

        if(!_szLine.isEmpty() && (_szLine == m_szLine))
        {
            m_iRepeated++;
            return;
        }

        vRepeated();
        m_szLine = _szLine;
        vWrite(m_eLineType, m_bLineModify, _szLine);
    }

    void vRepeated()
    {
        if(m_iRepeated)
        {
            vWrite(eLogInfo, false, QString("  (repeated %1 times)").arg(m_iRepeated));
            m_iRepeated = 0;
        }
    }

    void vWrite(tdLogType _eLogType, bool _bModify, const QString &_szLine)
    {
        QDateTime oNow = QDateTime::currentDateTime();

        if(m_bConsole)
        {
            const char *szColor = "";

            if(m_bColors)
            {
                switch(_eLogType)
                {
                case eLogInfo:          szColor = ""; break;
                case eLogWarning:       szColor = "\033[33m"; break;
                case eLogError:         szColor = "\033[1;37;41m"; break;
                case eLogRead:          szColor = "\033[32m"; break;
                case eLogWrite:         szColor = "\033[38;5;208m"; break;     // orange: modifications
                case eLogBDOS:          szColor = "\033[34m"; break;
                case eLogBDOSModify:    szColor = "\033[38;5;208m"; break;
                case eLogBDOSDetails:   szColor = _bModify ? "\033[38;5;180m" : "\033[38;5;104m"; break;
                case eLogConnected:     szColor = "\033[94m"; break;
                case eLogClient:        szColor = "\033[36m"; break;
                }
            }

            fprintf(stdout, "%s %s%s%s\n", qPrintable(oNow.toString("HH:mm:ss")), szColor, qPrintable(_szLine), *szColor ? "\033[0m" : "");
            fflush(stdout);
        }

        if(m_oFile.isOpen())
        {
            m_oFile.write((oNow.toString("yyyy-MM-dd HH:mm:ss ") + _szLine + "\n").toUtf8());
            m_oFile.flush();
        }
    }
};

static Logger goLogger;

/*
 =======================================================================================================================
    Stop with Ctrl+C or SIGTERM: event loop stopped, files closed and RAM disk removed by the destructor of the server
 =======================================================================================================================
 */
#ifdef Q_OS_WIN

static BOOL WINAPI bConsoleHandler(DWORD _dwType)
{
    if((_dwType == CTRL_C_EVENT) || (_dwType == CTRL_BREAK_EVENT) || (_dwType == CTRL_CLOSE_EVENT))
    {
        QMetaObject::invokeMethod(QCoreApplication::instance(), "quit", Qt::QueuedConnection);
        return TRUE;
    }
    return FALSE;
}

static void vInstallStopHandler()
{
    SetConsoleCtrlHandler(bConsoleHandler, TRUE);
}

// UTF-8 output, colours (escape sequences) if the console supports them
static bool bInitConsole()
{
    HANDLE  hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD   dwMode = 0;

    SetConsoleOutputCP(CP_UTF8);
    return GetConsoleMode(hOutput, &dwMode) && SetConsoleMode(hOutput, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

#else

static int giSignalSockets[2];

static void vSignalHandler(int)
{
    char cByte = 1;
    if(::write(giSignalSockets[0], &cByte, 1) < 0) {}
}

static void vInstallStopHandler()
{
    if(::socketpair(AF_UNIX, SOCK_STREAM, 0, giSignalSockets) == 0)
    {
        QSocketNotifier *poNotifier = new QSocketNotifier(giSignalSockets[1], QSocketNotifier::Read, QCoreApplication::instance());

        QObject::connect(poNotifier, &QSocketNotifier::activated, []()
        {
            char cByte;
            if(::read(giSignalSockets[1], &cByte, 1) < 0) {}
            QCoreApplication::quit();
        });

        struct sigaction oAction = {};
        oAction.sa_handler = vSignalHandler;
        sigemptyset(&oAction.sa_mask);
        oAction.sa_flags = SA_RESTART;
        sigaction(SIGINT, &oAction, nullptr);
        sigaction(SIGTERM, &oAction, nullptr);
        sigaction(SIGHUP, &oAction, nullptr);
    }
}

#endif

/*
 =======================================================================================================================
    Messages of Qt on stderr, except the warning of the static build of QtSerialPort (libudev not loaded, the serial
    ports are found in /sys)
 =======================================================================================================================
 */
static void vQtMessageHandler(QtMsgType _eType, const QMessageLogContext &, const QString &_szMessage)
{
    if(_szMessage.startsWith("Failed to load the library: udev")) return;
    if(_eType == QtDebugMsg) return;
    fprintf(stderr, "%s\n", qPrintable(_szMessage));
}

/*
 =======================================================================================================================
    Serial port to use: the port given (if present), or the first USB serial adapter (FTDI first). Empty if none.
 =======================================================================================================================
 */
static QString szResolveSerialPort(const QString &_szWantedPort)
{
    const QList<QSerialPortInfo> aoPorts = QSerialPortInfo::availablePorts();

    if(!_szWantedPort.isEmpty())
    {
        for(const QSerialPortInfo &roPort : aoPorts)
        {
            if((roPort.portName() == _szWantedPort) || (roPort.systemLocation() == _szWantedPort))
                return roPort.portName();
        }

        // device not listed (link such as /dev/serial/by-id/..., pseudo terminal): opened by its path
        return QFileInfo::exists(_szWantedPort) ? _szWantedPort : QString();
    }

    for(const QSerialPortInfo &roPort : aoPorts)
    {
        if(roPort.hasVendorIdentifier() && (roPort.vendorIdentifier() == 0x0403))
            return roPort.portName();
    }

    for(const QSerialPortInfo &roPort : aoPorts)
    {
        if(roPort.hasVendorIdentifier())
            return roPort.portName();
    }

    return QString();
}

static void vListSerialPorts()
{
    const QList<QSerialPortInfo> aoPorts = QSerialPortInfo::availablePorts();
    QString szAuto = szResolveSerialPort(QString());

    QStringList aszOthers;

    printf("Serial ports:\n");

    for(const QSerialPortInfo &roPort : aoPorts)
    {
        if(!roPort.hasVendorIdentifier() && roPort.description().isEmpty())
        {
            aszOthers += roPort.portName();     // built-in ports (ttyS...)
            continue;
        }

        printf("  %-12s %s%s%s",
               qPrintable(roPort.portName()),
               qPrintable(roPort.description()),
               roPort.manufacturer().isEmpty() ? "" : qPrintable(" (" + roPort.manufacturer() + ")"),
               roPort.hasVendorIdentifier() ? qPrintable(QString::asprintf(" [%04X:%04X]", roPort.vendorIdentifier(), roPort.productIdentifier())) : "");
        printf("%s\n", roPort.portName() == szAuto ? "  <- automatic choice" : "");
    }

    if(!aszOthers.isEmpty())
        printf("  other ports: %s\n", qPrintable(aszOthers.join(' ')));
    if(szAuto.isEmpty())
        printf("No USB serial adapter (automatic choice).\n");
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
int main(int argc, char *argv[])
{
    qInstallMessageHandler(vQtMessageHandler);

    QCoreApplication oApplication(argc, argv);

    QCoreApplication::setOrganizationName("louthrax");
    QCoreApplication::setOrganizationDomain("louthrax.net");
    QCoreApplication::setApplicationName("JIOServerCLI");
    QCoreApplication::setApplicationVersion(STRINGIFY(BUILD_VERSION));

    QCommandLineParser oParser;

    oParser.setApplicationDescription(
        "JIO server, command line version: serves a disk image, or host directories as drives A: to H:, to an MSX "
        "connected by a JIO serial (USB) or Bluetooth link.\n"
        "The connection is automatic (first USB serial adapter if no port is given) and is attempted again when the "
        "device is not present or disconnected. Stop with Ctrl+C.\n\n"
        "Examples:\n"
        "  JIOServerCLI -i games.dsk\n"
        "  JIOServerCLI -d A=~/MSX/boot -d C=~/MSX/work -l jio.log\n"
        "  JIOServerCLI -i hd.img -r -p /dev/ttyUSB1");
    oParser.addHelpOption();
    oParser.addVersionOption();

    QCommandLineOption oImageOption({"i", "image"}, "Serve the disk image <file> (floppy, or hard disk with partitions).", "file");
    QCommandLineOption oDriveOption({"d", "drive"}, "Serve the directory <dir> as drive X: (A to H). Without X=, the next free drive from A:. Can be repeated.", "[X=]dir");
    QCommandLineOption oReadOnlyOption({"r", "read-only"}, "Refuse all writes (disk image, or served directories; the RAM disk H: stays writable).");
    QCommandLineOption oNoRxCRCOption("no-rx-crc", "Disk image: no CRC check of the data received by the MSX.");
    QCommandLineOption oNoTxCRCOption("no-tx-crc", "Disk image: no CRC of the data sent by the MSX.");
    QCommandLineOption oNoAutoRetryOption("no-auto-retry", "The MSX does not retry the commands automatically (disk image).");
    QCommandLineOption oTimeoutOption("timeout", "Disk image: the MSX aborts a command after a time-out.");
    QCommandLineOption oSlowTxOption("slow-tx", "Disk image: slow transmission from the MSX.");
    QCommandLineOption oPortOption({"p", "port"}, "Serial port to use (e.g. ttyUSB0, /dev/ttyUSB0, COM3). Default: first USB serial adapter (FTDI first).", "port");
    QCommandLineOption oBluetoothOption({"b", "bluetooth"}, "Connect by Bluetooth to the device <address> instead of a serial port.", "address");
    QCommandLineOption oListOption("list", "List the serial ports and exit.");
    QCommandLineOption oScanOption("scan-bluetooth", "Search the Bluetooth devices, list them and exit.");
    QCommandLineOption oLogOption({"l", "log"}, "Also write the log to <file> (appended).", "file");
    QCommandLineOption oQuietOption({"q", "quiet"}, "No log on the console.");
    QCommandLineOption oBriefOption("brief", "No details of the BDOS functions in the log.");
    QCommandLineOption oNoColorOption("no-color", "No colours on the console.");

    oParser.addOptions({ oImageOption, oDriveOption, oReadOnlyOption, oNoRxCRCOption, oNoTxCRCOption, oNoAutoRetryOption,
                         oTimeoutOption, oSlowTxOption, oPortOption, oBluetoothOption, oListOption, oScanOption,
                         oLogOption, oQuietOption, oBriefOption, oNoColorOption });
    oParser.process(oApplication);

    if(oParser.isSet(oListOption))
    {
        vListSerialPorts();
        return 0;
    }

    if(oParser.isSet(oScanOption))
    {
        QBluetoothDeviceDiscoveryAgent oAgent;

        QObject::connect(&oAgent, &QBluetoothDeviceDiscoveryAgent::deviceDiscovered, [](const QBluetoothDeviceInfo &_roInfo)
        {
            printf("  %s  %s\n", qPrintable(_roInfo.address().toString()), qPrintable(_roInfo.name()));
            fflush(stdout);
        });
        QObject::connect(&oAgent, &QBluetoothDeviceDiscoveryAgent::finished, &oApplication, &QCoreApplication::quit);
        QObject::connect(&oAgent, &QBluetoothDeviceDiscoveryAgent::errorOccurred, [&oAgent, &oApplication]()
        {
            fprintf(stderr, "Bluetooth error: %s\n", qPrintable(oAgent.errorString()));
            oApplication.exit(1);
        });
        printf("Bluetooth devices:\n");
        oAgent.start();
        return oApplication.exec();
    }

    // what is served
    QString szImage = oParser.value(oImageOption);
    QStringList aszDrives = oParser.values(oDriveOption);
    QString aszDrivePaths[8];

    if(szImage.isEmpty() == aszDrives.isEmpty())
    {
        fprintf(stderr, "%s\n", szImage.isEmpty() ? "Nothing to serve: give a disk image (-i) or directories (-d)." : "Give a disk image (-i) or directories (-d), not both.");
        fprintf(stderr, "Use --help for the options.\n");
        return 2;
    }

    for(const QString &rszDrive : aszDrives)
    {
        int     iDrive = -1;
        QString szPath = rszDrive;

        if((rszDrive.size() >= 2) && (rszDrive[1] == '=') && (rszDrive[0].toUpper() >= 'A') && (rszDrive[0].toUpper() <= 'H'))
        {
            iDrive = rszDrive[0].toUpper().unicode() - 'A';
            szPath = rszDrive.mid(2);
        }
        else
        {
            for(int i = 0; (i < 8) && (iDrive < 0); i++)
                if(aszDrivePaths[i].isEmpty()) iDrive = i;
        }

        if(szPath.startsWith("~/")) szPath = QDir::homePath() + szPath.mid(1);

        if(iDrive < 0)
        {
            fprintf(stderr, "Too many drives (A: to H:).\n");
            return 2;
        }
        if(!aszDrivePaths[iDrive].isEmpty())
        {
            fprintf(stderr, "Drive %c: given twice.\n", 'A' + iDrive);
            return 2;
        }
        if(!QFileInfo(szPath).isDir())
        {
            fprintf(stderr, "Not a directory: %s\n", qPrintable(szPath));
            return 2;
        }
        aszDrivePaths[iDrive] = QFileInfo(szPath).absoluteFilePath();
    }

    if(!szImage.isEmpty())
    {
        if(szImage.startsWith("~/")) szImage = QDir::homePath() + szImage.mid(1);
        if(!QFileInfo(szImage).isFile())
        {
            fprintf(stderr, "Disk image not found: %s\n", qPrintable(szImage));
            return 2;
        }
    }

    // log
    goLogger.m_bConsole = !oParser.isSet(oQuietOption);
    goLogger.m_bColors = !oParser.isSet(oNoColorOption) && isatty(fileno(stdout));
#ifdef Q_OS_WIN
    goLogger.m_bColors = bInitConsole() && goLogger.m_bColors;
#endif
    goLogger.m_bBrief = oParser.isSet(oBriefOption);

    if(oParser.isSet(oLogOption))
    {
        goLogger.m_oFile.setFileName(oParser.value(oLogOption));
        if(!goLogger.m_oFile.open(QIODevice::WriteOnly | QIODevice::Append))
        {
            fprintf(stderr, "Cannot open the log file %s\n", qPrintable(oParser.value(oLogOption)));
            return 2;
        }
    }

    // server
    Server  *poServer = new Server();

    QObject::connect(poServer, &Server::log, [](tdLogType _eLogType, const QString &_szMessage, bool _bModify)
    {
        goLogger.vAdd(_eLogType, _szMessage, _bModify);
    });

    goLogger.vAdd(eLogInfo, QString("JIOServerCLI %1\n").arg(QCoreApplication::applicationVersion()), false);

    poServer->m_bRxCRC = !oParser.isSet(oNoRxCRCOption);
    poServer->m_bTxCRC = !oParser.isSet(oNoTxCRCOption);
    poServer->m_bAutoRetry = !oParser.isSet(oNoAutoRetryOption);
    poServer->m_bTimeout = oParser.isSet(oTimeoutOption);
    poServer->m_bSlowTx = oParser.isSet(oSlowTxOption);
    poServer->m_bReadOnly = oParser.isSet(oReadOnlyOption);

    if(!szImage.isEmpty())
    {
        poServer->vSetServeMode(eServeDiskImage);
        if(!poServer->bInsertMedia(szImage))
        {
            goLogger.vFlush();
            delete poServer;
            return 2;
        }
    }
    else
    {
        for(int i = 0; i < 8; i++)
            poServer->vSetDrivePath(i, aszDrivePaths[i]);
        poServer->vSetServeMode(eServeDirectories);
        goLogger.vAdd(eLogInfo, "Serving directories\n" + poServer->szGetServerInfo() + "\n", false);
    }

    // connection, attempted again until the device is present
    poServer->vSetRetryAlways(true);

    if(oParser.isSet(oBluetoothOption))
    {
        QString szAddress = oParser.value(oBluetoothOption);

        if(QBluetoothAddress(szAddress).isNull())
        {
            fprintf(stderr, "Invalid Bluetooth address: %s\n", qPrintable(szAddress));
            delete poServer;
            return 2;
        }
        poServer->vSetInterface(eInterfaceBluetooth);
        goLogger.vAdd(eLogInfo, "Connecting to " + szAddress + "...\n", false);
        poServer->vConnect(szAddress);
    }
    else
    {
        QString szWantedPort = oParser.value(oPortOption);

        poServer->vSetInterface(eInterfaceSerial);
        poServer->vSetDeviceResolver([szWantedPort]()
        {
            static bool sbWaiting = false;
            QString     szPort = szResolveSerialPort(szWantedPort);

            if(szPort.isEmpty() && !sbWaiting)
                goLogger.vAdd(eLogInfo, szWantedPort.isEmpty() ? "Waiting for a USB serial adapter...\n" : "Waiting for " + szWantedPort + "...\n", false);
            sbWaiting = szPort.isEmpty();
            return szPort;
        });
        poServer->vConnect();
    }

    vInstallStopHandler();

    int iResult = oApplication.exec();

    poServer->vDisconnect();
    goLogger.vAdd(eLogInfo, QString::asprintf("Stopped: %llu bytes received, %llu bytes transmitted, %llu reception errors, %llu transmission errors\n",
                                              (unsigned long long) poServer->uiBytesReceived(), (unsigned long long) poServer->uiBytesTransmitted(),
                                              (unsigned long long) poServer->uiReceiveErrors(), (unsigned long long) poServer->uiTransmitErrors()), false);
    delete poServer;
    goLogger.vFlush();

    return iResult;
}
