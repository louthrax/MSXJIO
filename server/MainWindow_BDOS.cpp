#include <QDirIterator>

#include "MainWindow.h"
#include "Find.h"
#include "../common/drv_jio.inc"
#include "Pack.h"

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vResetNFS()
{
    for(int i = 0; i < 256; i++)
    {
        if (m_apoOpenedFiles[i])
        {
            m_apoOpenedFiles[i]->close();
            m_apoOpenedFiles[i] = NULL;
        }
    }

    for (QString &dir : m_szBDOSCurrentDir)
        dir = "";

    m_ucCurrentPhysicalDrive = 0;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
unsigned char MainWindow::ucAddFile(QFile * _poFile)
{
    if (_poFile)
    {
        for(int i = 128; i < 256; i++)
        {
            if (!m_apoOpenedFiles[i])
            {
                m_apoOpenedFiles[i] = _poFile;
                return i;
            }
        }
    }

    return 0;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
unsigned char MainWindow::BDOSToQt(QString & _roString)
{
    unsigned char ucDrive;

    ucDrive = m_ucCurrentPhysicalDrive;

    _roString = _roString.toUpper();
    _roString.replace("\\", "/");

    if ((_roString.size() >= 2) && (_roString[1]==':'))
    {
        ucDrive = _roString[0].toLatin1() - 'A';
        _roString.remove(0, 2);
    }

    if ((_roString.length() > 0) && (_roString[0] == '/'))
            _roString = m_szBDOSRootDir[ucDrive] + _roString;
        else
        {
            QString szResult;

            szResult = m_szBDOSRootDir[ucDrive] + "/";

            if (m_szBDOSCurrentDir[ucDrive].length() > 0)
                szResult += m_szBDOSCurrentDir[ucDrive] + "/";

            if(_roString.length())
                szResult += _roString;

            _roString = szResult;
        }

    return ucDrive;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::QtToBDOS(QString & _roString)
{
    if (_roString == ".")
        _roString = "";

    if (_roString.startsWith("/"))
    {
        _roString.remove(0, 1);
    }

    _roString.replace("/", "\\");
    _roString = _roString.toUpper();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vUpdateFIB(tdFileInfoBlock *_poFIB)
{
    _poFIB->m_ucFF = 0xFF;

    if (_poFIB->m_poFile)
    {
        QString szFileName = QFileInfo(*_poFIB->m_poFile).fileName().toUpper();
        strncpy(_poFIB->m_acFileName,
                szFileName.toLocal8Bit().constData(), sizeof(_poFIB->m_acFileName) - 1);

        QDateTime dt = _poFIB->m_poFile->fileTime(QFileDevice::FileModificationTime).toLocalTime(); ;

        QDate date = dt.date();
        QTime time = dt.time();

        _poFIB->m_uiLastModificationDate = ((date.year() - 1980) << 9)
                                           | (date.month() << 5)
                                           | (date.day());
        _poFIB->m_uiLastModificationTime =  (time.hour() << 11)
                                           | (time.minute() << 5)
                                           | (time.second() / 2);

        _poFIB->m_ulFileSize = _poFIB->m_poFile->size();
        _poFIB->m_cAttributes = QFileInfo(*_poFIB->m_poFile).isDir() ? ATTRIBUTE_DIRECTORY : 0;
        _poFIB->m_ucResult = 0;
        _poFIB->m_uiStartCluster = 0;
    }
    else
    {
        _poFIB->m_ucResult = DOS_ERR_NOFIL;
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vDOS_CLOSE_FILE_HANDLE(unsigned char _ucFileHandle)
{
    unsigned char ucError;

    if (m_apoOpenedFiles[_ucFileHandle])
    {
        m_apoOpenedFiles[_ucFileHandle]->close();
        delete m_apoOpenedFiles[_ucFileHandle];
        m_apoOpenedFiles[_ucFileHandle] = NULL;
    }

    ucError = 0;
    uiTransmit(&ucError, sizeof(ucError), 0, 0, false, TRANSMIT_DELAY_NORMAL);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vDOS_READ_FROM_FILE_HANDLE(unsigned char _ucFileHandle, unsigned short int _uiSize)
{
    PACK_PUSH
    struct
    {
        unsigned char ucError;
        unsigned short int uiSize;
    } s;
    PACK_POP

    QByteArray data = m_apoOpenedFiles[_ucFileHandle]->read(_uiSize);
    s.uiSize = data.size();
    s.ucError = 0;
    uiTransmit(&s, sizeof(s), 0, 0, false, TRANSMIT_DELAY_NORMAL);
    uiTransmit(data.constData(), s.uiSize, 0, 0, false, TRANSMIT_DELAY_ACKNOWLEDGE);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vDOS_WRITE_TO_FILE_HANDLE(unsigned char _ucFileHandle, unsigned short int _uiSize, char * _pcData)
{
    PACK_PUSH
    struct
    {
        unsigned char ucError;
        unsigned short int uiSize;
    } s;
    PACK_POP
    s.ucError = 0;
    s.uiSize = m_apoOpenedFiles[_ucFileHandle]->write(_pcData, _uiSize);

    uiTransmit(&s, sizeof(s), 0, 0, false, TRANSMIT_DELAY_NORMAL);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vDOS_FIND_FIRST_ENTRY(unsigned char _ucSearchAttributes, unsigned _ucPhysicalDrive, QString szPath, tdFileInfoBlock &_roFIB)
{
    if (_ucSearchAttributes & ATTRIBUTE_VOLUME_NAME)
    {
        strcpy(_roFIB.m_acFileName, "JIONFS A:");
        _roFIB.m_acFileName[7] = _ucPhysicalDrive + 'A';
        _roFIB.m_ucDrive = _ucPhysicalDrive + 1;
    }
    else
    {
        QString szDirectory;
        QString szMask;

        if (szPath.endsWith('/'))
        {
            szDirectory = szPath;
            szDirectory.chop(1);
            szMask = "*";
        }
        else
        {
            int lastSlash = szPath.lastIndexOf('/');
            if (lastSlash < 0)
            {
                szDirectory = szPath;
                szMask = "*";
            }
            else
            {
                szDirectory = szPath.left(lastSlash);
                szMask = szPath.mid(lastSlash + 1);
            }
        }

        strncpy(_roFIB.m_acRegExp, szMask.toLocal8Bit().constData(), sizeof(_roFIB.m_acFileName) - 1);

        QFile oDir(szDirectory);
        _roFIB.m_poFile = poGetFirstEntry(&oDir, _roFIB.m_acRegExp, m_szBDOSRootDir[_ucPhysicalDrive]);

        vUpdateFIB(&_roFIB);
        _roFIB.m_ucDrive = _ucPhysicalDrive + 1;
    }

    uiTransmit(&_roFIB, sizeof(_roFIB), 0, 0, false, TRANSMIT_DELAY_NORMAL);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vDOS_FIND_NEW_ENTRY(unsigned char ucCreateAttributes, unsigned _ucPhysicalDrive, QString szPath, tdFileInfoBlock &_roFIB)
{
    QString szDirectory;
    QString szMask;

    if (szPath.endsWith('/'))
    {
        szDirectory = szPath;
        szDirectory.chop(1);
        szMask = "*";
    }
    else
    {
        int lastSlash = szPath.lastIndexOf('/');
        if (lastSlash < 0)
        {
            szDirectory = szPath;
            szMask = "*";
        }
        else
        {
            szDirectory = szPath.left(lastSlash);
            szMask = szPath.mid(lastSlash + 1);
        }
    }

    if (szMask.contains("?") || szMask.contains("*"))
    {
        szMask = _roFIB.m_acFileName;
    }

    szDirectory += "/" + szMask;


    if (ucCreateAttributes & ATTRIBUTE_DIRECTORY)
    {
        if (QFile::exists(szDirectory) && QFileInfo(szDirectory).isDir())
            _roFIB.m_ucResult = 0;
        else
        {
            _roFIB.m_ucResult = QDir().mkdir(szDirectory) ? DOS_ERR_OK : 255;

            if (_roFIB.m_ucResult == DOS_ERR_OK)
            {
                QFileDevice::Permissions perms = QFile(szDirectory).permissions();
                QFile::setPermissions(szDirectory, perms);
            }
        }
    }
    else
    {
        QFile *poFile;

        QIODevice::OpenMode mode = QIODevice::Truncate | QIODevice::ReadWrite;

        poFile = new QFile(szDirectory);

        if (poFile->open(mode))
            _roFIB.m_ucResult = DOS_ERR_OK;
        else
            _roFIB.m_ucResult = DOS_ERR_FILE;

        poFile->close();

        delete poFile;
    }

    if(_roFIB.m_ucResult == DOS_ERR_OK)
        _roFIB.m_poFile = new QFile(szDirectory);

    vUpdateFIB(&_roFIB);
    _roFIB.m_ucDrive = _ucPhysicalDrive + 1;

    uiTransmit(&_roFIB, sizeof(_roFIB), 0, 0, false, TRANSMIT_DELAY_NORMAL);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
QString findFileCaseInsensitive(const QString& path)
{
    QFileInfo info(path);
    QDir dir = info.dir();
    const QString name = info.fileName();

    QStringList entries = dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString& entry : entries)
    {
        if (entry.compare(name, Qt::CaseInsensitive) == 0)
            return dir.absoluteFilePath(entry);
    }

    return QString();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vDOS_OPEN_FILE_HANDLE(unsigned char _ucOpenMode, QString _szDirectory)
{
    PACK_PUSH
    struct
    {
        unsigned char ucError;
        unsigned char ucNewFileHandle;
    } s;
    PACK_POP
    QFile * poFile;

    QIODevice::OpenMode mode = QIODevice::ReadWrite;
    if (_ucOpenMode & 0x01) mode |= QIODevice::ReadOnly;
    if (_ucOpenMode & 0x02) mode |= QIODevice::WriteOnly;

    if (_szDirectory != "/dev/null")
        _szDirectory = findFileCaseInsensitive(_szDirectory);

    poFile = new QFile(_szDirectory);

    if (poFile->exists() && poFile->open(mode))
        s.ucNewFileHandle = ucAddFile(poFile);
    else
    {
        delete poFile;
        s.ucNewFileHandle = 0;
    }

    s.ucError = s.ucNewFileHandle ? DOS_ERR_OK : DOS_ERR_FILE;
    uiTransmit(&s, sizeof(s), 0, 0, false, TRANSMIT_DELAY_NORMAL);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vDOS_CHANGE_CURRENT_DIRECTORY(unsigned _ucPhysicalDrive, QString _szDirectory)
{
    unsigned char ucResult;

    m_szBDOSCurrentDir[_ucPhysicalDrive] = QDir(m_szBDOSRootDir[_ucPhysicalDrive]).relativeFilePath(_szDirectory);

    if (m_szBDOSCurrentDir[_ucPhysicalDrive] == ".")
        m_szBDOSCurrentDir[_ucPhysicalDrive] = "";


    ucResult = DOS_ERR_OK;

    uiTransmit(&ucResult, sizeof(ucResult), 0, 0, false, TRANSMIT_DELAY_NORMAL);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vDOS_FIND_NEXT_ENTRY(tdFileInfoBlock &_roFIB)
{
    _roFIB.m_poFile = poGetNextEntry(_roFIB.m_poFile, _roFIB.m_acRegExp, m_szBDOSRootDir[_roFIB.m_ucDrive - 1], _roFIB.m_cAttributes & ATTRIBUTE_DIRECTORY);

    vUpdateFIB(&_roFIB);

    uiTransmit(&_roFIB, sizeof(_roFIB), 0, 0, false, TRANSMIT_DELAY_NORMAL);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vDOS_MOVE_FILE_HANDLE_POINTER(unsigned char _ucFileHandle, unsigned char _ucMethodCode, int _iOffset)
{
    QFile * poFile;

    PACK_PUSH
    struct
    {
        unsigned char ucResult;
        int           iNewPos;
    } s;
    PACK_POP

    if ((poFile = m_apoOpenedFiles[_ucFileHandle]))
    {
        switch(_ucMethodCode)
        {
        case 0:  s.ucResult = poFile->seek(_iOffset)                  ? DOS_ERR_OK : DOS_ERR_FILE; break;
        case 1:  s.ucResult = poFile->seek(poFile->pos() + _iOffset)  ? DOS_ERR_OK : DOS_ERR_FILE; break;
        case 2:  s.ucResult = poFile->seek(poFile->size() + _iOffset) ? DOS_ERR_OK : DOS_ERR_FILE; break;
        default: s.ucResult = DOS_ERR_INERR;
        }

        s.iNewPos = poFile->pos();
    }
    else
    {
        s.ucResult = DOS_ERR_FILE;
        s.iNewPos = 0;
    }

    uiTransmit(&s, sizeof(s), 0, 0, false, TRANSMIT_DELAY_NORMAL);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vDOS_GET_CURRENT_DIRECTORY(unsigned char _ucDriveNumber)
{
    unsigned char ucSize;

    _ucDriveNumber = _ucDriveNumber ? _ucDriveNumber - 1 : m_ucCurrentPhysicalDrive;

    QString szLocalPath = m_szBDOSCurrentDir[_ucDriveNumber];

    QtToBDOS(szLocalPath);
    vLog(eLogBDOSDetails, "Result: %s\n", szLocalPath.toLocal8Bit().constData());

    ucSize = szLocalPath.size() + 1;

    uiTransmit(&ucSize, sizeof(ucSize), 0, 0, false, TRANSMIT_DELAY_NORMAL);
    uiTransmit(szLocalPath.toLocal8Bit().constData(), ucSize, 0, 0, false, TRANSMIT_DELAY_ACKNOWLEDGE);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vDOS_CREATE_FILE_HANDLE(QString _szPath, unsigned char _ucOpenMode, unsigned char _ucAttributes)
{
    QFile * poFile;

    PACK_PUSH
    struct
    {
        unsigned char ucError;
        unsigned char ucNewFileHandle;
    } s;
    PACK_POP

    BDOSToQt(_szPath);

    if (_ucAttributes & ATTRIBUTE_DIRECTORY)
    {
        s.ucNewFileHandle = 0xFF;

        if (QFile::exists(_szPath) && QFileInfo(_szPath).isDir())
            s.ucError = 0;
        else
        {
            s.ucError = QDir().mkdir(_szPath) ? DOS_ERR_OK : 255;

            if (s.ucError == DOS_ERR_OK)
            {
                QFileDevice::Permissions perms = QFile(_szPath).permissions();

                if (_ucOpenMode & 0x01)
                    perms &= ~QFileDevice::WriteOwner;

                if (_ucOpenMode & 0x02)
                    perms &= ~QFileDevice::ReadOwner;

                QFile::setPermissions(_szPath, perms);
            }
        }
    }
    else
    {
        QIODevice::OpenMode mode = QIODevice::Truncate;
        if ((_ucOpenMode & 0x01) == 0) mode |= QIODevice::ReadOnly;
        if ((_ucOpenMode & 0x02) == 0) mode |= QIODevice::WriteOnly;

        poFile = new QFile(_szPath);

        if (poFile->open(mode))
        {
            s.ucNewFileHandle = ucAddFile(poFile);
            s.ucError = s.ucNewFileHandle ? DOS_ERR_OK : DOS_ERR_FILE;
        }
        else
        {
            s.ucNewFileHandle = 0;
            s.ucError = 255;
        }
    }

    uiTransmit(&s, sizeof(s), 0, 0, false, TRANSMIT_DELAY_NORMAL);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vDOS_GET_WHOLE_PATH_STRING(unsigned char _ucPhysicalDrive, QString szDirectory)
{
    PACK_PUSH
    struct
    {
        unsigned char ucError;
        unsigned char ucPos;
        unsigned char ucSize;
    } s;
    PACK_POP

    if (szDirectory.startsWith(m_szBDOSRootDir[_ucPhysicalDrive]))
    {
        szDirectory.remove(0, m_szBDOSRootDir[_ucPhysicalDrive].length());
        s.ucError = 0;
    }
    else
    {
        s.ucError = 0xFF;
    }

    QtToBDOS(szDirectory);

    s.ucSize = szDirectory.size() + 1;
    s.ucPos = szDirectory.lastIndexOf('\\') + 1;

    uiTransmit(&s, sizeof(s), 0, 0, false, TRANSMIT_DELAY_NORMAL);
    uiTransmit(szDirectory.toLocal8Bit().constData(), s.ucSize, 0, 0, false, TRANSMIT_DELAY_ACKNOWLEDGE);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vDOS_DELETE_FILE_OR_SUBDIRECTORY(QString _szPath)
{
    unsigned char ucError;

    if (QFileInfo(_szPath).isDir())
    {
        ucError = QDir().rmdir(_szPath) ?  DOS_ERR_OK : 255;
    }
    else
    {
        ucError = QFile::remove(_szPath) ? DOS_ERR_OK : 255;
    }

    uiTransmit(&ucError, sizeof(ucError), 0, 0, false, TRANSMIT_DELAY_NORMAL);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vDOS_GET_SET_FILE_ATTRIBUTES(QString _szPath, unsigned char _ucSetAttributes, unsigned char _ucNewAttributes)
{
    PACK_PUSH
    struct
    {
        unsigned char ucError;
        unsigned char ucCurrentAttributes;
    } s;
    PACK_POP

    s.ucError = QFileInfo::exists(_szPath) ? DOS_ERR_OK : DOS_ERR_FILE;

    if (_ucSetAttributes)
    {
        // TODO
    }

    s.ucCurrentAttributes = QFileInfo(_szPath).isDir() ? 0 : ATTRIBUTE_DIRECTORY;

    uiTransmit(&s, sizeof(s), 0, 0, false, TRANSMIT_DELAY_NORMAL);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vDOS_GET_SET_FILE_HANDLE_DATE_AND_TIME(QString _szPath, unsigned char ucSet, unsigned short int uiNewDate, unsigned short int uiNewTime)
{
    struct PACKED
    {
        unsigned char ucError;
        unsigned short int uiCurrentFileTimeValue;
        unsigned short int uiCurrentFileDateValue;
    } s;

    if (ucSet)
    {
        const int year   = 1980 + ((uiNewDate >> 9) & 0x7F);
        const int month  = (uiNewDate >> 5) & 0x0F;
        const int day    =  uiNewDate       & 0x1F;

        const int hour   = (uiNewTime >> 11) & 0x1F;
        const int minute = (uiNewTime >> 5)  & 0x3F;
        const int second = (uiNewTime & 0x1F) * 2; // 2-second resolution

        const QDate date(year, month, day);
        const QTime time(hour, minute, second);

        const QDateTime newDt(date, time, Qt::LocalTime);

        QFile f(_szPath);

        s.ucError = f.setFileTime(newDt, QFileDevice::FileModificationTime) ? DOS_ERR_OK : DOS_ERR_FILE;
    }

    s.ucError = 0;

    QDateTime dt = QFile(_szPath).fileTime(QFileDevice::FileModificationTime).toLocalTime(); ;

    QDate date = dt.date();
    QTime time = dt.time();

    s.uiCurrentFileDateValue = ((date.year() - 1980) << 9)
                                       | (date.month() << 5)
                                       | (date.day());
    s.uiCurrentFileTimeValue =  (time.hour() << 11)
                                       | (time.minute() << 5)
                                       | (time.second() / 2);

    uiTransmit(&s, sizeof(s), 0, 0, false, TRANSMIT_DELAY_NORMAL);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vDOS_SELECT_DISK(unsigned char _ucDiskToSelect)
{
    unsigned char ucNumberOfDrives;

    m_ucCurrentPhysicalDrive = _ucDiskToSelect;
    ucNumberOfDrives = 8;
    uiTransmit(&ucNumberOfDrives, sizeof(ucNumberOfDrives), 0, 0, false, TRANSMIT_DELAY_NORMAL);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vDOS_OPEN_FILE_FCB(tdFileControlBlock & _roFCB)
{
    QString szPath;
    QString szFileName;
    QFile * poFile;

    szPath = m_szBDOSRootDir[m_ucCurrentPhysicalDrive];

    if (!m_szBDOSCurrentDir[m_ucCurrentPhysicalDrive].isEmpty())
        szPath = szPath + "/" + m_szBDOSCurrentDir[m_ucCurrentPhysicalDrive];

    szFileName =QString(_roFCB.m_acFileName).first(8);

    if (strlen(_roFCB.m_acFileNameExtension) > 0)
    {
        szFileName += ".";
        szFileName += _roFCB.m_acFileNameExtension;
    }
    szFileName = szFileName.toUpper();

    if (!szFileName.isEmpty())
        szPath = szPath + "/" + szFileName;


    poFile = new QFile(szPath);

    _roFCB.m_ucResult = 0;

    if (poFile->exists() && poFile->open(QIODevice::ReadWrite))
    {
        _roFCB.ucNewFileHandle = ucAddFile(poFile);
        _roFCB.m_ulFileSize = poFile->size();
    }
    else
    {
        delete poFile;
        _roFCB.ucNewFileHandle = 0;
        _roFCB.m_ucResult = 1;
    }

    uiTransmit(&_roFCB, sizeof(_roFCB), 0, 0, false, TRANSMIT_DELAY_NORMAL);
}


void MainWindow::vDOS_CLOSE_FILE_FCB(tdFileControlBlock & _roFCB)
{
    _roFCB.m_ucResult = 0;
    uiTransmit(&_roFCB, sizeof(_roFCB), 0, 0, false, TRANSMIT_DELAY_NORMAL);
}


void MainWindow::vDOS_RANDOM_BLOCK_READ_FCB(tdFileControlBlock & _roFCB)
{
    QByteArray data = m_apoOpenedFiles[_roFCB.ucNewFileHandle]->read(_roFCB.m_uiNumberOfRecords);
    _roFCB.m_ucResult = data.size() != _roFCB.m_uiNumberOfRecords;
    _roFCB.m_uiNumberOfRecords = data.size();
    _roFCB.m_ulRandomRecordNumber += data.size();
    uiTransmit(&_roFCB, sizeof(_roFCB), 0, 0, false, TRANSMIT_DELAY_NORMAL);
    uiTransmit(data.constData(), data.size(), 0, 0, false, TRANSMIT_DELAY_ACKNOWLEDGE);
}
