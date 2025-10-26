#include <QDirIterator>

#include "MainWindow.h"
#include "Find.h"
#include "../common/drv_jio.inc"

#define PACKED __attribute__((packed))

/*
 =======================================================================================================================
 =======================================================================================================================
 */

/*
0 - Standard input (CON)
1 - Standard output (CON)
2 - Standard error input/output (CON)
3 - Standard auxiliary input/output (AUX)
4 - Standard printer output (PRN)
5 - Standard NUL handle (NUL)
*/

unsigned char MainWindow::ucAddFile(QFile * _poFile)
{
    if (_poFile)
    {
        for(int i = 5; i < 256; i++)
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
void MainWindow::BDOSToQt(QString & _roString)
{
    _roString = _roString.toUpper();

    _roString.replace("A:", "");

    _roString.replace("\\", "/");

    if ((_roString.length() > 0) && (_roString[0] == '/'))
        _roString = m_szBDOSRootDir + _roString;

    if (_roString == "NUL")
        _roString = "/dev/null";

    if (_roString == "")
        _roString = ".";
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::QtToBDOS(QString & _roString)
{
    if (_roString == ".")
        _roString = "";

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
        _poFIB->m_ucDrive = 1;
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
    struct PACKED
    {
        unsigned char ucError;
        unsigned short int uiSize;
    } s;

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
    struct PACKED
    {
        unsigned char ucError;
        unsigned short int uiSize;
    } s;

    s.ucError = 0;
    s.uiSize = m_apoOpenedFiles[_ucFileHandle]->write(_pcData, _uiSize);

    uiTransmit(&s, sizeof(s), 0, 0, false, TRANSMIT_DELAY_NORMAL);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vDOS_FIND_FIRST_ENTRY(unsigned char ucSearchAttributes, QString szDirectory, tdFileInfoBlock &_roFIB, QString _szWildcard)
{
    if (ucSearchAttributes & ATTRIBUTE_VOLUME_NAME)
    {
        strcpy(_roFIB.m_acFileName, "JIONFS");
        _roFIB.m_ucDrive = 1;
    }
    else
    {
        if (_roFIB.m_ucFF == 0xFF)
        {
            szDirectory = QFileInfo(*_roFIB.m_poFile).absoluteFilePath() + "/" + _szWildcard;
            QtToBDOS(szDirectory);
        }

        struct MsxPathParts result;
        result = splitMsxPath(szDirectory);

        if ((result.filename == "") || (result.filename == "*.*"))
            result.filename = "*";

        BDOSToQt(result.path);

        strncpy(_roFIB.m_acRegExp, result.filename.toLocal8Bit().constData(), sizeof(_roFIB.m_acFileName) - 1);

        QFile oDir(result.path);
        _roFIB.m_poFile = poGetFirstEntry(&oDir, _roFIB.m_acRegExp, m_szBDOSRootDir);

        vUpdateFIB(&_roFIB);
    }

    uiTransmit(&_roFIB, sizeof(_roFIB), 0, 0, false, TRANSMIT_DELAY_NORMAL);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vDOS_FIND_NEW_ENTRY(unsigned char ucCreateAttributes, QString _szPath, tdFileInfoBlock &_roFIB, QString _szWildcard)
{
    if (_roFIB.m_ucFF == 0xFF)
    {
        _szPath = QFileInfo(*_roFIB.m_poFile).absoluteFilePath() + "/" + _szWildcard;
    }
    else
        BDOSToQt(_szPath);

    if (ucCreateAttributes & ATTRIBUTE_DIRECTORY)
    {
        if (QFile::exists(_szPath) && QFileInfo(_szPath).isDir())
            _roFIB.m_ucResult = 0;
        else
        {
            _roFIB.m_ucResult = QDir().mkdir(_szPath) ? DOS_ERR_OK : 255;

            if (_roFIB.m_ucResult == DOS_ERR_OK)
            {
                QFileDevice::Permissions perms = QFile(_szPath).permissions();
                QFile::setPermissions(_szPath, perms);
            }
        }
    }
    else
    {
        QFile *poFile;

        QIODevice::OpenMode mode = QIODevice::Truncate | QIODevice::ReadWrite;

        poFile = new QFile(_szPath);

        qWarning() << QFileInfo(*poFile).absoluteFilePath();
        if (poFile->open(mode))
            _roFIB.m_ucResult = DOS_ERR_OK;
        else
            _roFIB.m_ucResult = DOS_ERR_FILE;

        poFile->close();

        delete poFile;
    }

    if(_roFIB.m_ucResult == DOS_ERR_OK)
        _roFIB.m_poFile = new QFile(_szPath);

    vUpdateFIB(&_roFIB);

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
void MainWindow::vDOS_OPEN_FILE_HANDLE(unsigned char _ucOpenMode, QString _szDirectory, tdFileInfoBlock &_roFIB)
{
    struct PACKED
    {
        unsigned char ucError;
        unsigned char ucNewFileHandle;
    } s;
    QFile * poFile;

    QIODevice::OpenMode mode = QIODevice::ReadWrite;
    if (_ucOpenMode & 0x01) mode |= QIODevice::ReadOnly;
    if (_ucOpenMode & 0x02) mode |= QIODevice::WriteOnly;

    if (_roFIB.m_ucFF == 0xFF)
        _szDirectory = QFileInfo(*_roFIB.m_poFile).absoluteFilePath();
    else
    {
        BDOSToQt(_szDirectory);
        if (_szDirectory != "/dev/null")
            _szDirectory = findFileCaseInsensitive(_szDirectory);
    }

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
void MainWindow::vDOS_CHANGE_CURRENT_DIRECTORY(QString _szDirectory)
{
    unsigned char ucResult;

    BDOSToQt(_szDirectory);

    ucResult = QDir::setCurrent(_szDirectory) ? DOS_ERR_OK : DOS_ERR_NODIR;

    uiTransmit(&ucResult, sizeof(ucResult), 0, 0, false, TRANSMIT_DELAY_NORMAL);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vDOS_FIND_NEXT_ENTRY(tdFileInfoBlock &_roFIB)
{
    _roFIB.m_poFile = poGetNextEntry(_roFIB.m_poFile, _roFIB.m_acRegExp, m_szBDOSRootDir, _roFIB.m_cAttributes & ATTRIBUTE_DIRECTORY);

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
    struct PACKED
    {
        unsigned char ucResult;
        int           iNewPos;
    } s;

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

    QString szLocalPath = QDir(m_szBDOSRootDir).relativeFilePath(QDir::currentPath());

    QtToBDOS(szLocalPath);

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

    struct PACKED
    {
        unsigned char ucError;
        unsigned char ucNewFileHandle;
    } s;

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
void MainWindow::vDOS_GET_WHOLE_PATH_STRING(QString szDirectory, tdFileInfoBlock &_roFIB)
{
    struct PACKED
    {
        unsigned char ucError;
        unsigned char ucPos;
        unsigned char ucSize;
    } s;

    if (_roFIB.m_ucFF == 0xFF)
        szDirectory = QFileInfo(*_roFIB.m_poFile).absoluteFilePath();
    else
        BDOSToQt(szDirectory);

    QString szLocalPath = QDir(m_szBDOSRootDir).relativeFilePath(szDirectory);

    QtToBDOS(szLocalPath);

    s.ucError = 0;
    s.ucSize = szLocalPath.size() + 1;
    s.ucPos = szLocalPath.lastIndexOf('\\') + 1;

    vLog(eLogBDOSDetails, "szLocalPath: %s\n", szLocalPath.toLocal8Bit().constData());

    uiTransmit(&s, sizeof(s), 0, 0, false, TRANSMIT_DELAY_NORMAL);
    uiTransmit(szLocalPath.toLocal8Bit().constData(), s.ucSize, 0, 0, false, TRANSMIT_DELAY_ACKNOWLEDGE);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vDOS_DELETE_FILE_OR_SUBDIRECTORY(QString _szPath, tdFileInfoBlock &_roFIB)
{
    unsigned char ucError;

    if (_roFIB.m_ucFF == 0xFF)
        _szPath = QFileInfo(*_roFIB.m_poFile).absoluteFilePath();
    else
        BDOSToQt(_szPath);

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
void MainWindow::vDOS_GET_SET_FILE_ATTRIBUTES(QString _szPath, tdFileInfoBlock &_roFIB, unsigned char _ucSetAttributes, unsigned char _ucNewAttributes)
{
    struct PACKED
    {
        unsigned char ucError;
        unsigned char ucCurrentAttributes;
    } s;

    if (_roFIB.m_ucFF == 0xFF)
        _szPath = QFileInfo(*_roFIB.m_poFile).absoluteFilePath();
    else
        BDOSToQt(_szPath);

    s.ucError = QFileInfo::exists(_szPath) ? DOS_ERR_OK : DOS_ERR_FILE;

    if (_ucSetAttributes)
    {

    }

    s.ucCurrentAttributes = QFileInfo(_szPath).isDir() ? 0 : ATTRIBUTE_DIRECTORY;

    uiTransmit(&s, sizeof(s), 0, 0, false, TRANSMIT_DELAY_NORMAL);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void MainWindow::vDOS_GET_SET_FILE_HANDLE_DATE_AND_TIME(QString _szPath, tdFileInfoBlock &_roFIB, unsigned char ucSet, unsigned short int uiNewDate, unsigned short int uiNewTime)
{
    struct PACKED
    {
        unsigned char ucError;
        unsigned short int uiCurrentFileTimeValue;
        unsigned short int uiCurrentFileDateValue;
    } s;

    if (_roFIB.m_ucFF == 0xFF)
        _szPath = QFileInfo(*_roFIB.m_poFile).absoluteFilePath();
    else
        BDOSToQt(_szPath);

    if (ucSet)
    {
        // Decode MSX-DOS packed date/time
        const int year   = 1980 + ((uiNewDate >> 9) & 0x7F);
        const int month  = (uiNewDate >> 5) & 0x0F;
        const int day    =  uiNewDate       & 0x1F;

        const int hour   = (uiNewTime >> 11) & 0x1F;
        const int minute = (uiNewTime >> 5)  & 0x3F;
        const int second = (uiNewTime & 0x1F) * 2; // 2-second resolution

        const QDate date(year, month, day);
        const QTime time(hour, minute, second);

        const QDateTime newDt(date, time, Qt::LocalTime);

        bool ok = false;
        QFile f(_szPath);
        ok = f.setFileTime(newDt, QFileDevice::FileModificationTime);

        s.ucError = ok ? DOS_ERR_OK : DOS_ERR_FILE;
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
