#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QStorageInfo>
#include <QDirIterator>

#include "Server.h"
#include "Find.h"
#include "../common/drv_jio.inc"
#include "Pack.h"

#define FIRST_FILE_HANDLE   128
#define MAX_FIND_ENTRIES    64

/*
 =======================================================================================================================
    MSX-DOS date and time
 =======================================================================================================================
 */
static void vToDosDateTime(const QDateTime &_roDateTime, unsigned short int &_ruiTime, unsigned short int &_ruiDate)
{
    QDateTime   oDateTime = _roDateTime.toLocalTime();
    QDate       oDate = oDateTime.date();
    QTime       oTime = oDateTime.time();

    _ruiDate = ((qMax(oDate.year(), 1980) - 1980) << 9) | (oDate.month() << 5) | oDate.day();
    _ruiTime = (oTime.hour() << 11) | (oTime.minute() << 5) | (oTime.second() / 2);
}

static QDateTime oFromDosDateTime(unsigned short int _uiTime, unsigned short int _uiDate)
{
    QDate oDate(1980 + ((_uiDate >> 9) & 0x7F), (_uiDate >> 5) & 0x0F, _uiDate & 0x1F);
    QTime oTime((_uiTime >> 11) & 0x1F, (_uiTime >> 5) & 0x3F, (_uiTime & 0x1F) * 2);

    return QDateTime(oDate, oTime);
}

/*
 =======================================================================================================================
    Find an existing entry in a directory by its host name (ignoring case) or its MSX-DOS 8.3 name (alias of a long
    name, see Find.cpp). Returns the name in upper case if not found (new entry: long names are kept).
 =======================================================================================================================
 */
static QString szFindEntry(const QString &_szDirectory, const QString &_szName)
{
    const QString szEntry = szFindHostEntry(_szDirectory, _szName);

    return szEntry.isEmpty() ? _szName.toUpper() : szEntry;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void Server::vResetNFS()
{
    for (int i = 0; i < 256; i++)
    {
        if (m_apoOpenedFiles[i])
        {
            m_apoOpenedFiles[i]->close();
            delete m_apoOpenedFiles[i];
            m_apoOpenedFiles[i] = nullptr;
        }
    }

    for (QString &dir : m_szBDOSCurrentDir)
        dir = "";

    m_oFindEntries.clear();
    m_szWholePath.clear();
    m_ucCurrentPhysicalDrive = 0;

    // the RAM disk does not survive a reset of the MSX
    vDestroyRamDisk();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
unsigned char Server::ucAddFile(QFile * _poFile)
{
    if (_poFile)
    {
        for(int i = FIRST_FILE_HANDLE; i < 256; i++)
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
    Root directory of a drive. H: is the RAM disk when it exists (_RAMD): a temporary directory, removed when the RAM
    disk is destroyed, at each reset of the MSX (RESET_NFS) and when the server quits.
 =======================================================================================================================
 */
bool Server::bIsRamDrive(unsigned char _ucDrive)
{
    return (_ucDrive == 7) && m_ucRamDiskSegments && m_poRamDisk;
}

QString Server::szRootDir(unsigned char _ucDrive)
{
    if (_ucDrive >= 8)
        return QString();

    if (bIsRamDrive(_ucDrive))
        return m_poRamDisk->path();

    return m_szBDOSRootDir[_ucDrive];
}

void Server::vDestroyRamDisk()
{
    if (m_poRamDisk)
    {
        const QString szRoot = m_poRamDisk->path() + "/";

        for (int i = 0; i < 256; i++)
        {
            if (m_apoOpenedFiles[i] && QFileInfo(m_apoOpenedFiles[i]->fileName()).absoluteFilePath().startsWith(szRoot))
            {
                m_apoOpenedFiles[i]->close();
                delete m_apoOpenedFiles[i];
                m_apoOpenedFiles[i] = nullptr;
            }
        }

        delete m_poRamDisk;
        m_poRamDisk = nullptr;
        vLog(eLogInfo, "RAM disk destroyed\n");
    }

    m_ucRamDiskSegments = 0;
    m_szBDOSCurrentDir[7] = "";

    if (m_ucCurrentPhysicalDrive == 7)
        m_ucCurrentPhysicalDrive = 0;
}

// Free bytes on the RAM disk: size of the RAM disk minus the files (512 bytes sectors)
qint64 Server::iRamDiskFree()
{
    qint64          iUsed = 0;
    QDirIterator    oIt(m_poRamDisk->path(), QDir::Files | QDir::Hidden | QDir::System, QDirIterator::Subdirectories);

    while (oIt.hasNext())
    {
        oIt.next();
        iUsed += (oIt.fileInfo().size() + 511) & ~511LL;
    }

    return qMax<qint64>((qint64) m_ucRamDiskSegments * 16384 - iUsed, 0);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
bool Server::bIsDriveServed(unsigned char _ucDrive)
{
    // no drive is served when a disk image is served
    return (m_eServeMode == eServeDirectories) && (_ucDrive < 8) && !szRootDir(_ucDrive).isEmpty() && QFileInfo(szRootDir(_ucDrive)).isDir();
}

/*
 =======================================================================================================================
    "Read only" button: the served directories cannot be modified (error .WPROT, as a write protected disk).
    Checked before each modification of the host file system. The RAM disk H: (temporary directory) stays writable.
 =======================================================================================================================
 */
bool Server::bIsWriteProtected(const QString &_szHostPath)
{
    if (!m_bReadOnly)
        return false;

    if (m_poRamDisk && QFileInfo(_szHostPath).absoluteFilePath().startsWith(m_poRamDisk->path() + "/"))
        return false;

    return true;
}

/*
 =======================================================================================================================
    MSX path to host path. Returns the drive (0 = A:), or 0xFF if the drive is invalid.
 =======================================================================================================================
 */
unsigned char Server::ucResolvePath(QString _szMSXPath, QString &_rszHostPath, unsigned char _ucDefaultDrive)
{
    unsigned char   ucDrive;
    QStringList     aszItems;
    QString         szPath;

    ucDrive = (_ucDefaultDrive < 8) ? _ucDefaultDrive : m_ucCurrentPhysicalDrive;

    _szMSXPath.replace('\\', '/');

    if ((_szMSXPath.size() >= 2) && (_szMSXPath[1] == ':'))
    {
        ucDrive = _szMSXPath[0].toUpper().toLatin1() - 'A';
        _szMSXPath.remove(0, 2);
    }

    if (!bIsDriveServed(ucDrive))
        return 0xFF;

    if (!_szMSXPath.startsWith('/'))
        aszItems = m_szBDOSCurrentDir[ucDrive].split('/', Qt::SkipEmptyParts);

    for (const QString &szItem : _szMSXPath.split('/', Qt::SkipEmptyParts))
    {
        if (szItem == ".")
            continue;

        if (szItem == "..")
        {
            if (!aszItems.isEmpty())
                aszItems.removeLast();
            continue;
        }

        aszItems.append(szItem);
    }

    szPath = szRootDir(ucDrive);

    for (const QString &szItem : aszItems)
        szPath += "/" + szFindEntry(szPath, szItem);

    _rszHostPath = szPath;

    return ucDrive;
}

/*
 =======================================================================================================================
    Host path to MSX path relative to the drive root: no drive, no leading backslash, MSX-DOS 8.3 names
 =======================================================================================================================
 */
QString Server::szRelativePath(unsigned char _ucDrive, const QString &_szHostPath)
{
    QString szHostPath = szRootDir(_ucDrive);
    QString szPath;
    QString szRelative = QDir(szRootDir(_ucDrive)).relativeFilePath(_szHostPath);

    if (szRelative == ".")
        return "";

    for (const QString &szItem : szRelative.split('/', Qt::SkipEmptyParts))
    {
        szHostPath += "/" + szItem;

        if (!szPath.isEmpty())
            szPath += "\\";

        szPath += szGetDosName(szHostPath);
    }

    return szPath;
}

/*
 =======================================================================================================================
    Get the host path of an ASCIIZ path or FIB
 =======================================================================================================================
 */
unsigned char Server::ucGetTarget(const tdFileInfoBlock &_roFIB, const QString &_szMSXPath, unsigned char &_rucDrive, QString &_rszHostPath)
{
    if (_roFIB.m_ucFF == 0xFF)
    {
        auto it = m_oFindEntries.find(_roFIB.m_uiFindId);

        if (it == m_oFindEntries.end())
            return DOS_ERR_NOFIL;

        _rucDrive = _roFIB.m_ucDrive ? _roFIB.m_ucDrive - 1 : m_ucCurrentPhysicalDrive;
        _rszHostPath = it.value();
    }
    else
    {
        _rucDrive = ucResolvePath(_szMSXPath, _rszHostPath);

        if (_rucDrive == 0xFF)
            return DOS_ERR_IDRV;
    }

    return DOS_ERR_OK;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
unsigned char Server::ucGetAttributes(const QString &_szHostPath)
{
    QFileInfo       oInfo(_szHostPath);
    unsigned char   ucAttributes;

    if (oInfo.isDir())
        ucAttributes = ATTRIBUTE_DIRECTORY;
    else
        ucAttributes = m_oArchiveCleared.contains(oInfo.absoluteFilePath()) ? 0 : ATTRIBUTE_ARCHIVE_BIT;

    if (!oInfo.isWritable())
        ucAttributes |= ATTRIBUTE_READ_ONLY;

    if (oInfo.isHidden() && (oInfo.fileName() != ".") && (oInfo.fileName() != ".."))
        ucAttributes |= ATTRIBUTE_HIDDEN_FILE;

    return ucAttributes;
}

/*
 =======================================================================================================================
    Archive attribute of a host file (no such attribute on the host): set again when the file is written, created
    or deleted, reset by _ATTR / _HATTR (used by programs to mark the files done, e.g. SofaCopy)
 =======================================================================================================================
 */
void Server::vSetArchive(const QString &_szHostPath, bool _bSet)
{
    if (_bSet)
        m_oArchiveCleared.remove(QFileInfo(_szHostPath).absoluteFilePath());
    else
        m_oArchiveCleared.insert(QFileInfo(_szHostPath).absoluteFilePath());
}

/*
 =======================================================================================================================
    Rename / move: the archive attribute follows the file (and the files of a directory)
 =======================================================================================================================
 */
void Server::vRenameArchive(const QString &_szHostPath, const QString &_szNewHostPath)
{
    QString szOld = QFileInfo(_szHostPath).absoluteFilePath();
    QString szNew = QFileInfo(_szNewHostPath).absoluteFilePath();

    for (const QString &szPath : m_oArchiveCleared.values())
    {
        if (szPath == szOld)
        {
            m_oArchiveCleared.remove(szPath);
            m_oArchiveCleared.insert(szNew);
        }
        else if (szPath.startsWith(szOld + "/"))
        {
            m_oArchiveCleared.remove(szPath);
            m_oArchiveCleared.insert(szNew + szPath.mid(szOld.length()));
        }
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void Server::vFillFIB(tdFileInfoBlock &_roFIB, const QString &_szHostPath, unsigned char _ucDrive)
{
    QFileInfo   oInfo(_szHostPath);
    QByteArray  acName = szGetDosName(_szHostPath).toLatin1().left(sizeof(_roFIB.m_acFileName) - 1);

    _roFIB.m_ucFF = 0xFF;
    memset(_roFIB.m_acFileName, 0, sizeof(_roFIB.m_acFileName));
    memcpy(_roFIB.m_acFileName, acName.constData(), acName.size());
    _roFIB.m_cAttributes = ucGetAttributes(_szHostPath);
    vToDosDateTime(oInfo.lastModified(), _roFIB.m_uiLastModificationTime, _roFIB.m_uiLastModificationDate);
    _roFIB.m_uiStartCluster = 0;
    _roFIB.m_ulFileSize = oInfo.isDir() ? 0 : (unsigned int) qMin<qint64>(oInfo.size(), 0xFFFFFFFF);
    _roFIB.m_ucDrive = _ucDrive + 1;
    memset(_roFIB.m_aucClient, 0, sizeof(_roFIB.m_aucClient));
    _roFIB.m_ucResult = DOS_ERR_OK;
}

/*
 =======================================================================================================================
    Remember the entry found, and its whole path (for _WPATH)
 =======================================================================================================================
 */
void Server::vSetFindEntry(tdFileInfoBlock &_roFIB, const QString &_szHostPath, unsigned char _ucDrive)
{
    if (!_roFIB.m_uiFindId || !m_oFindEntries.contains(_roFIB.m_uiFindId))
    {
        while (m_oFindEntries.size() >= MAX_FIND_ENTRIES)
            m_oFindEntries.erase(m_oFindEntries.begin());

        if (!++m_uiNextFindId)
            m_uiNextFindId = 1;

        _roFIB.m_uiFindId = m_uiNextFindId;
    }

    m_oFindEntries[_roFIB.m_uiFindId] = _szHostPath;

    // Whole path keeps the entry name ("." and ".." are not resolved)
    QString szDirectory = szRelativePath(_ucDrive, QFileInfo(_szHostPath).path());
    QString szName = szGetDosName(_szHostPath);
    m_szWholePath = szDirectory.isEmpty() ? szName : szDirectory + "\\" + szName;

    vFillFIB(_roFIB, _szHostPath, _ucDrive);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static bool bAttributesMatch(const QString &_szHostPath, unsigned char _ucSearchAttributes)
{
    QFileInfo oInfo(_szHostPath);

    if (oInfo.isDir() && !(_ucSearchAttributes & ATTRIBUTE_DIRECTORY))
        return false;

    if (oInfo.isHidden() && !oInfo.isDir() && !(_ucSearchAttributes & ATTRIBUTE_HIDDEN_FILE))
        return false;

    return true;
}

/*
 =======================================================================================================================
    Answer helpers: nothing is sent for empty data (the client does not wait for it)
 =======================================================================================================================
 */
void Server::vBDOSAnswer(const void *_pvData, unsigned int _uiSize)
{
    if (_uiSize)
        uiTransmit(_pvData, _uiSize, 0, 0, false, TRANSMIT_DELAY_NORMAL);
}

void Server::vBDOSData(const void *_pvData, unsigned int _uiSize)
{
    if (_uiSize)
        uiTransmit(_pvData, _uiSize, 0, 0, false, TRANSMIT_DELAY_ACKNOWLEDGE);
}

void Server::vBDOSError(unsigned char _ucError)
{
    vLog(eLogBDOSDetails, "Result: %02Xh\n", _ucError);
    vBDOSAnswer(&_ucError, sizeof(_ucError));
}

/*
 =======================================================================================================================
    Function $0E _SELDSK
 =======================================================================================================================
 */
void Server::vDOS_SELECT_DISK(unsigned char _ucDiskToSelect)
{
    unsigned char ucNumberOfDrives = 8;

    if (_ucDiskToSelect < 8)
        m_ucCurrentPhysicalDrive = _ucDiskToSelect;

    vBDOSAnswer(&ucNumberOfDrives, sizeof(ucNumberOfDrives));
}

/*
 =======================================================================================================================
    Function $18 _LOGIN: drives served (bit 0 = A:)
 =======================================================================================================================
 */
unsigned char Server::ucLoginVector()
{
    unsigned char ucLogin = 0;

    for (int i = 0; i < 8; i++)
    {
        if (bIsDriveServed(i))
            ucLogin |= 1 << i;
    }

    return ucLogin;
}

void Server::vDOS_GET_LOGIN_VECTOR()
{
    unsigned char ucLogin = ucLoginVector();

    vLog(eLogBDOSDetails, "Drives: %02Xh\n", ucLogin);
    vBDOSAnswer(&ucLogin, sizeof(ucLogin));
}

/*
 =======================================================================================================================
    Function $68 _RAMD: RAM disk H: (0 = destroy, 1-FEh = create with this number of 16 KB segments, FFh = get size).
    Answer: error, RAM disk size (segments), drives served (the JIO only ROM updates its drive list)
 =======================================================================================================================
 */
void Server::vDOS_CREATE_OR_DESTROY_RAMDISK(unsigned char _ucSegments)
{
    PACK_PUSH
    struct
    {
        unsigned char       ucError;
        unsigned char       ucSegments;
        unsigned char       ucLogin;
    } s;
    PACK_POP

    s.ucError = DOS_ERR_OK;

    if (_ucSegments == 0)
        vDestroyRamDisk();
    else if (_ucSegments != 0xFF)
    {
        if (m_eServeMode != eServeDirectories)
            s.ucError = DOS_ERR_NORAM;  // a disk image is served
        else if (m_ucRamDiskSegments || bIsDriveServed(7))
            s.ucError = DOS_ERR_RAMDX;  // RAM disk exists, or H: is a served directory
        else
        {
            m_poRamDisk = new QTemporaryDir(QDir::tempPath() + "/JIOServer_RAM_XXXXXX");

            if (m_poRamDisk->isValid())
            {
                m_ucRamDiskSegments = _ucSegments;
                m_szBDOSCurrentDir[7] = "";
                vLog(eLogInfo, "RAM disk H: %d KB, %s\n", _ucSegments * 16, qPrintable(m_poRamDisk->path()));
            }
            else
            {
                delete m_poRamDisk;
                m_poRamDisk = nullptr;
                s.ucError = DOS_ERR_NORAM;
            }
        }
    }

    s.ucSegments = m_ucRamDiskSegments;
    s.ucLogin = ucLoginVector();

    vLog(eLogBDOSDetails, "Result: %02Xh, %d segments, drives %02Xh\n", s.ucError, s.ucSegments, s.ucLogin);
    vBDOSAnswer(&s, sizeof(s));
}

/*
 =======================================================================================================================
    Function $1B _ALLOC: sectors per cluster, total clusters, free clusters (512 bytes sectors)
    Cluster counts are kept below 8000H (DSKF in BASIC is a signed integer)
 =======================================================================================================================
 */
void Server::vDOS_GET_ALLOCATION_INFORMATION(unsigned char _ucDrive)
{
    PACK_PUSH
    struct
    {
        unsigned char       ucSectorsPerCluster;
        unsigned short int  uiTotalClusters;
        unsigned short int  uiFreeClusters;
    } s;
    PACK_POP

    unsigned char ucDrive = _ucDrive ? _ucDrive - 1 : m_ucCurrentPhysicalDrive;

    memset(&s, 0, sizeof(s));

    if (bIsDriveServed(ucDrive))
    {
        QStorageInfo    oStorage(szRootDir(ucDrive));
        qint64          iSectors = oStorage.bytesTotal() / 512;
        qint64          iFreeSectors = oStorage.bytesAvailable() / 512;

        if (bIsRamDrive(ucDrive))
        {
            iSectors = (qint64) m_ucRamDiskSegments * 16384 / 512;
            iFreeSectors = qMin(iRamDiskFree() / 512, iFreeSectors);
        }

        // at most 2 sectors per cluster: COMMAND2 computes the free space in K on 16 bits (clusters x sectors per
        // cluster up to 65535 sectors), a bigger disk is shown as 32767K free (largest value shown right)
        s.ucSectorsPerCluster = 1;
        while ((iSectors / s.ucSectorsPerCluster > 0x7FFF) && (s.ucSectorsPerCluster < 2))
            s.ucSectorsPerCluster <<= 1;

        s.uiTotalClusters = qMin<qint64>(iSectors / s.ucSectorsPerCluster, 0x7FFF);
        s.uiFreeClusters = qMin<qint64>(iFreeSectors / s.ucSectorsPerCluster, 0x7FFF);
    }

    vLog(eLogBDOSDetails, "Drive %c: | Result: %d sectors per cluster, %d clusters, %d free\n", 'A' + ucDrive,
         s.ucSectorsPerCluster, s.uiTotalClusters, s.uiFreeClusters);
    vBDOSAnswer(&s, sizeof(s));
}

/*
 =======================================================================================================================
    Split an ASCIIZ path in directory and last item
 =======================================================================================================================
 */
static void vSplitPath(const QString &_szMSXPath, QString &_rszDirectory, QString &_rszItem)
{
    QString szPath = _szMSXPath;
    int     iPos;

    szPath.replace('\\', '/');
    iPos = qMax(szPath.lastIndexOf('/'), szPath.lastIndexOf(':'));
    _rszItem = szPath.mid(iPos + 1);
    _rszDirectory = szPath.left(iPos + 1);
}

/*
 =======================================================================================================================
    Directory and last item of path or FIB + file name
 =======================================================================================================================
 */
unsigned char Server::ucGetFindTarget(const tdFileInfoBlock &_roFIB, const QString &_szMSXPath, const QString &_szFileName, unsigned char &_rucDrive, QString &_rszDirectory, QString &_rszItem)
{
    if (_roFIB.m_ucFF == 0xFF)
    {
        // File name in the directory of the FIB
        _rszItem = _szFileName;
        return ucGetTarget(_roFIB, "", _rucDrive, _rszDirectory);
    }

    QString szPath;

    vSplitPath(_szMSXPath, szPath, _rszItem);
    _rucDrive = ucResolvePath(szPath, _rszDirectory);

    return (_rucDrive == 0xFF) ? DOS_ERR_IDRV : DOS_ERR_OK;
}

/*
 =======================================================================================================================
    Function $40 _FFIRST
 =======================================================================================================================
 */
void Server::vDOS_FIND_FIRST_ENTRY(tdFileInfoBlock &_roFIB, const QString &_szMSXPath, const QString &_szFileName, unsigned char _ucSearchAttributes)
{
    unsigned char   ucDrive;
    QString         szDirectory;
    QString         szMask;
    unsigned char   ucError;

    ucError = ucGetFindTarget(_roFIB, _szMSXPath, _szFileName, ucDrive, szDirectory, szMask);

    if (szMask.isEmpty())
        szMask = "*";

    memset(&_roFIB, 0, sizeof(_roFIB));

    if (ucError == DOS_ERR_OK)
    {
        if (_ucSearchAttributes & ATTRIBUTE_VOLUME_NAME)
        {
            _roFIB.m_ucFF = 0xFF;
            if (bIsRamDrive(ucDrive))
                snprintf(_roFIB.m_acFileName, sizeof(_roFIB.m_acFileName), "RAM DISK");
            else
                snprintf(_roFIB.m_acFileName, sizeof(_roFIB.m_acFileName), "JIO DRIVE %c", 'A' + ucDrive);
            _roFIB.m_cAttributes = ATTRIBUTE_VOLUME_NAME;
            _roFIB.m_ucDrive = ucDrive + 1;
        }
        else
        {
            QFile   oDirectory(szDirectory);
            QFile   *poEntry;

            strncpy(_roFIB.m_acRegExp, szMask.toUpper().toLatin1().constData(), sizeof(_roFIB.m_acRegExp) - 1);

            poEntry = poGetFirstEntry(&oDirectory, _roFIB.m_acRegExp, szRootDir(ucDrive));

            while (poEntry && !bAttributesMatch(poEntry->fileName(), _ucSearchAttributes))
            {
                QFile *poNext = poGetNextEntry(poEntry, _roFIB.m_acRegExp, szRootDir(ucDrive), QFileInfo(*poEntry).isDir());
                delete poEntry;
                poEntry = poNext;
            }

            if (poEntry)
            {
                vSetFindEntry(_roFIB, poEntry->fileName(), ucDrive);
                _roFIB.m_aucClient[0] = _ucSearchAttributes;    // sent back by the client with _FNEXT
                delete poEntry;
            }
            else
                ucError = DOS_ERR_NOFIL;
        }
    }

    _roFIB.m_ucResult = ucError;
    vLog(eLogBDOSDetails, "Result: %s %02Xh\n", _roFIB.m_acFileName, ucError);
    vBDOSAnswer(&_roFIB, sizeof(_roFIB));
}

/*
 =======================================================================================================================
    Function $41 _FNEXT
 =======================================================================================================================
 */
void Server::vDOS_FIND_NEXT_ENTRY(tdFileInfoBlock &_roFIB)
{
    unsigned char   ucDrive = _roFIB.m_ucDrive ? _roFIB.m_ucDrive - 1 : m_ucCurrentPhysicalDrive;
    unsigned char   ucSearchAttributes = _roFIB.m_aucClient[0];
    auto            it = m_oFindEntries.find(_roFIB.m_uiFindId);
    QFile           *poEntry = nullptr;

    if ((it != m_oFindEntries.end()) && bIsDriveServed(ucDrive))
    {
        QFile oCurrent(it.value());

        poEntry = poGetNextEntry(&oCurrent, _roFIB.m_acRegExp, szRootDir(ucDrive), QFileInfo(oCurrent).isDir());

        while (poEntry && !bAttributesMatch(poEntry->fileName(), ucSearchAttributes))
        {
            QFile *poNext = poGetNextEntry(poEntry, _roFIB.m_acRegExp, szRootDir(ucDrive), QFileInfo(*poEntry).isDir());
            delete poEntry;
            poEntry = poNext;
        }
    }

    if (poEntry)
    {
        vSetFindEntry(_roFIB, poEntry->fileName(), ucDrive);
        _roFIB.m_aucClient[0] = ucSearchAttributes;
        delete poEntry;
    }
    else
    {
        // No more entry: the FIB still refers to the last entry found, it can
        // still be used (e.g. COMMAND2 2.31 opens AUTOEXEC.BAT with its FIB
        // after checking that there is no other match)
        _roFIB.m_ucResult = DOS_ERR_NOFIL;
    }

    vLog(eLogBDOSDetails, "Result: %s %02Xh\n", _roFIB.m_acFileName, _roFIB.m_ucResult);
    vBDOSAnswer(&_roFIB, sizeof(_roFIB));
}

/*
 =======================================================================================================================
    Function $42 _FNEW
    Attributes: b7 = create new (error if exists), b4 = sub-directory
 =======================================================================================================================
 */
void Server::vDOS_FIND_NEW_ENTRY(tdFileInfoBlock &_roFIB, const QString &_szMSXPath, const QString &_szFileName, unsigned char _ucAttributes, const char *_acTemplate)
{
    unsigned char   ucDrive;
    QString         szDirectory;
    QString         szName;
    unsigned char   ucError;

    ucError = ucGetFindTarget(_roFIB, _szMSXPath, _szFileName, ucDrive, szDirectory, szName);

    if (szName.contains('?') || szName.contains('*') || szName.isEmpty())
        szName = QString::fromLatin1(_acTemplate);

    memset(&_roFIB, 0, sizeof(_roFIB));

    if ((ucError == DOS_ERR_OK) && !QFileInfo(szDirectory).isDir())
        ucError = DOS_ERR_NODIR;

    if ((ucError == DOS_ERR_OK) && szName.isEmpty())
        ucError = DOS_ERR_IFNM;

    if (ucError == DOS_ERR_OK)
    {
        QString szPath = szDirectory + "/" + szFindEntry(szDirectory, szName);

        if (QFileInfo::exists(szPath))
        {
            if (_ucAttributes & 0x80)
                ucError = DOS_ERR_FILEX;
            else if (QFileInfo(szPath).isDir())
                ucError = (_ucAttributes & ATTRIBUTE_DIRECTORY) ? DOS_ERR_OK : DOS_ERR_DIRX;
            else if (_ucAttributes & ATTRIBUTE_DIRECTORY)
                ucError = DOS_ERR_FILEX;
        }

        if ((ucError == DOS_ERR_OK) && !((_ucAttributes & ATTRIBUTE_DIRECTORY) && QFileInfo(szPath).isDir()) && bIsWriteProtected(szPath))
            ucError = DOS_ERR_WPROT;

        if (ucError == DOS_ERR_OK)
        {
            if (_ucAttributes & ATTRIBUTE_DIRECTORY)
            {
                if (!QFileInfo(szPath).isDir() && !QDir().mkdir(szPath))
                    ucError = DOS_ERR_DKFUL;
            }
            else
            {
                QFile oFile(szPath);

                if (oFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
                    oFile.close();
                else
                    ucError = DOS_ERR_DKFUL;
            }
        }

        // An existing entry (error .FILEX or .DIRX) is also returned in the FIB, as MSX-DOS 2 does:
        // e.g. COMMAND2 2.31 copies into a directory with the FIB of the failed _FNEW
        if ((ucError == DOS_ERR_OK) || (ucError == DOS_ERR_FILEX) || (ucError == DOS_ERR_DIRX))
            vSetFindEntry(_roFIB, szPath, ucDrive);
    }

    _roFIB.m_ucResult = ucError;
    vLog(eLogBDOSDetails, "Result: %s %02Xh\n", _roFIB.m_acFileName, ucError);
    vBDOSAnswer(&_roFIB, sizeof(_roFIB));
}

/*
 =======================================================================================================================
    Open mode: b0 = no write, b1 = no read
 =======================================================================================================================
 */
static QIODevice::OpenMode eOpenMode(unsigned char _ucOpenMode)
{
    if (_ucOpenMode & 0x01)
        return QIODevice::ReadOnly;

    if (_ucOpenMode & 0x02)
        return QIODevice::WriteOnly;

    return QIODevice::ReadWrite;
}

/*
 =======================================================================================================================
    Answer error and new file handle (0FFH if no file)
 =======================================================================================================================
 */
void Server::vAnswerHandle(unsigned char _ucError, QFile *_poFile)
{
    PACK_PUSH
    struct
    {
        unsigned char ucError;
        unsigned char ucNewFileHandle;
    } s;
    PACK_POP

    s.ucError = _ucError;
    s.ucNewFileHandle = 0xFF;

    if ((_ucError == DOS_ERR_OK) && _poFile)
    {
        s.ucNewFileHandle = ucAddFile(_poFile);

        if (!s.ucNewFileHandle)
        {
            delete _poFile;
            s.ucNewFileHandle = 0xFF;
            s.ucError = DOS_ERR_NHAND;
        }
    }
    else
        delete _poFile;

    vLog(eLogBDOSDetails, "Result: %02Xh, handle %02Xh\n", s.ucError, s.ucNewFileHandle);
    vBDOSAnswer(&s, sizeof(s));
}

/*
 =======================================================================================================================
    Function $43 _OPEN
 =======================================================================================================================
 */
void Server::vDOS_OPEN_FILE_HANDLE(const tdFileInfoBlock &_roFIB, const QString &_szMSXPath, unsigned char _ucOpenMode)
{
    unsigned char   ucDrive;
    QString         szPath;
    unsigned char   ucError;
    QFile           *poFile = nullptr;

    ucError = ucGetTarget(_roFIB, _szMSXPath, ucDrive, szPath);

    if (ucError == DOS_ERR_OK)
    {
        vLog(eLogBDOSDetails, "Path: %s\n", qPrintable(szPath));

        if (!QFileInfo::exists(szPath))
            ucError = DOS_ERR_NOFIL;
        else if (QFileInfo(szPath).isDir())
            ucError = DOS_ERR_DIRX;
        else
        {
            poFile = new QFile(szPath);

            // Write protected: opened read-only on the host, _WRITE answers .WPROT
            if (bIsWriteProtected(szPath))
            {
                if (!poFile->open(QIODevice::ReadOnly))
                    ucError = DOS_ERR_FILRO;
            }
            // A read-only file opened for read and write is opened read-only
            else if (!poFile->open(eOpenMode(_ucOpenMode)) && ((_ucOpenMode & 0x02) || !poFile->open(QIODevice::ReadOnly)))
                ucError = DOS_ERR_FILRO;
        }
    }

    vAnswerHandle(ucError, poFile);
}

/*
 =======================================================================================================================
    Function $44 _CREATE
    Attributes: b7 = create new (error if exists), b4 = sub-directory (no file handle)
 =======================================================================================================================
 */
void Server::vDOS_CREATE_FILE_HANDLE(const tdFileInfoBlock &_roFIB, const QString &_szMSXPath, unsigned char _ucOpenMode, unsigned char _ucAttributes)
{
    unsigned char   ucDrive;
    QString         szPath;
    unsigned char   ucError;
    QFile           *poFile = nullptr;

    Q_UNUSED(_ucOpenMode);

    ucError = ucGetTarget(_roFIB, _szMSXPath, ucDrive, szPath);

    if (ucError == DOS_ERR_OK)
    {
        QFileInfo oInfo(szPath);

        vLog(eLogBDOSDetails, "Path: %s\n", qPrintable(szPath));

        if (!QFileInfo(oInfo.absolutePath()).isDir())
            ucError = DOS_ERR_NODIR;
        else if (oInfo.exists() && (_ucAttributes & 0x80))
            ucError = DOS_ERR_FILEX;
        else if (_ucAttributes & ATTRIBUTE_DIRECTORY)
        {
            if (oInfo.exists())
                ucError = oInfo.isDir() ? DOS_ERR_DIRX : DOS_ERR_FILEX;
            else if (bIsWriteProtected(szPath))
                ucError = DOS_ERR_WPROT;
            else if (!QDir().mkdir(szPath))
                ucError = DOS_ERR_DKFUL;
        }
        else if (oInfo.isDir())
            ucError = DOS_ERR_DIRX;
        else if (bIsWriteProtected(szPath))
            ucError = DOS_ERR_WPROT;
        else
        {
            poFile = new QFile(szPath);

            if (!poFile->open(QIODevice::ReadWrite | QIODevice::Truncate))
                ucError = DOS_ERR_FILRO;
            else
                vSetArchive(szPath, true);          // new (or truncated) file
        }
    }

    vAnswerHandle(ucError, poFile);
}

/*
 =======================================================================================================================
    Function $45 _CLOSE
 =======================================================================================================================
 */
void Server::vDOS_CLOSE_FILE_HANDLE(unsigned char _ucFileHandle)
{
    unsigned char ucError = DOS_ERR_OK;

    if (m_apoOpenedFiles[_ucFileHandle])
    {
        m_apoOpenedFiles[_ucFileHandle]->close();
        delete m_apoOpenedFiles[_ucFileHandle];
        m_apoOpenedFiles[_ucFileHandle] = nullptr;
    }
    else
        ucError = DOS_ERR_IHAND;

    vBDOSError(ucError);
}

/*
 =======================================================================================================================
    Function $48 _READ
 =======================================================================================================================
 */
void Server::vDOS_READ_FROM_FILE_HANDLE(unsigned char _ucFileHandle, unsigned short int _uiSize)
{
    PACK_PUSH
    struct
    {
        unsigned char       ucError;
        unsigned short int  uiSize;
    } s;
    PACK_POP

    QByteArray  acData;
    QFile       *poFile = m_apoOpenedFiles[_ucFileHandle];

    s.ucError = DOS_ERR_OK;

    if (!poFile)
        s.ucError = DOS_ERR_IHAND;
    else if (!poFile->isOpen())
        s.ucError = DOS_ERR_HDEAD;
    else
        acData = poFile->read(_uiSize);

    s.uiSize = acData.size();
    vLog(eLogBDOSDetails, "Result: %02Xh, %d bytes\n", s.ucError, s.uiSize);
    vBDOSAnswer(&s, sizeof(s));
    vBDOSData(acData.constData(), s.uiSize);
}

/*
 =======================================================================================================================
    Function $49 _WRITE
 =======================================================================================================================
 */
void Server::vDOS_WRITE_TO_FILE_HANDLE(unsigned char _ucFileHandle, const QByteArray &_racData)
{
    PACK_PUSH
    struct
    {
        unsigned char       ucError;
        unsigned short int  uiSize;
    } s;
    PACK_POP

    QFile *poFile = m_apoOpenedFiles[_ucFileHandle];

    s.ucError = DOS_ERR_OK;
    s.uiSize = 0;

    if (!poFile)
        s.ucError = DOS_ERR_IHAND;
    else if (!poFile->isOpen())
        s.ucError = DOS_ERR_HDEAD;
    else if (bIsWriteProtected(poFile->fileName()))
        s.ucError = DOS_ERR_WPROT;
    else if (!poFile->isWritable())
        s.ucError = DOS_ERR_ACCV;
    else
    {
        QByteArray  acData = _racData;
        qint64      iWritten;

        if (m_poRamDisk && QFileInfo(poFile->fileName()).absoluteFilePath().startsWith(m_poRamDisk->path() + "/"))
        {
            // RAM disk: the file can grow up to the free space of the RAM disk
            qint64 iOldSectors = (poFile->size() + 511) / 512;
            qint64 iNewSize = qMax(poFile->size(), poFile->pos() + (qint64) acData.size());
            qint64 iExtra = ((iNewSize + 511) / 512 - iOldSectors) * 512;
            qint64 iFree = iRamDiskFree();

            if (iExtra > iFree)
                acData.truncate(qMax<qint64>(0, qMin<qint64>(acData.size(), (iOldSectors * 512 + iFree) - poFile->pos())));
        }

        iWritten = poFile->write(acData);
        vSetArchive(poFile->fileName(), true);      // file modified

        if (iWritten < _racData.size())
            s.ucError = DOS_ERR_DKFUL;

        s.uiSize = qMax<qint64>(iWritten, 0);
        poFile->flush();
    }

    vLog(eLogBDOSDetails, "Result: %02Xh, %d bytes\n", s.ucError, s.uiSize);
    vBDOSAnswer(&s, sizeof(s));
}

/*
 =======================================================================================================================
    Function $4A _SEEK
 =======================================================================================================================
 */
void Server::vDOS_MOVE_FILE_HANDLE_POINTER(unsigned char _ucFileHandle, unsigned char _ucMethodCode, qint32 _iOffset)
{
    PACK_PUSH
    struct
    {
        unsigned char   ucResult;
        quint32         uiNewPos;
    } s;
    PACK_POP

    QFile   *poFile = m_apoOpenedFiles[_ucFileHandle];
    qint64  iPos = 0;

    s.ucResult = DOS_ERR_OK;

    if (poFile && poFile->isOpen())
    {
        switch(_ucMethodCode)
        {
        case 0:  iPos = _iOffset; break;
        case 1:  iPos = poFile->pos() + _iOffset; break;
        case 2:  iPos = poFile->size() + _iOffset; break;
        default: s.ucResult = DOS_ERR_ISBFN;
        }

        if (s.ucResult == DOS_ERR_OK)
        {
            iPos = qBound<qint64>(0, iPos, 0xFFFFFFFF);
            if (!poFile->seek(iPos))
                s.ucResult = DOS_ERR_SEEK;
        }

        iPos = poFile->pos();
    }
    else
        s.ucResult = poFile ? DOS_ERR_HDEAD : DOS_ERR_IHAND;

    s.uiNewPos = iPos;
    vLog(eLogBDOSDetails, "Result: %02Xh, position %u\n", s.ucResult, s.uiNewPos);
    vBDOSAnswer(&s, sizeof(s));
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
unsigned char Server::ucDelete(unsigned char _ucDrive, const QString &_szPath)
{
    QFileInfo oInfo(_szPath);

    if (!oInfo.exists())
        return DOS_ERR_NOFIL;

    if (szRelativePath(_ucDrive, _szPath).isEmpty())
        return DOS_ERR_IPATH;

    if (bIsWriteProtected(_szPath))
        return DOS_ERR_WPROT;

    if (oInfo.isDir())
    {
        if (!QDir(_szPath).isEmpty(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot))
            return DOS_ERR_DIRNE;

        return QDir().rmdir(_szPath) ? DOS_ERR_OK : DOS_ERR_ACCV;
    }

    // Read-only attribute (the host would delete a file without write permission)
    if (!oInfo.isWritable())
        return DOS_ERR_FILRO;

    vSetArchive(_szPath, true);
    return QFile::remove(_szPath) ? DOS_ERR_OK : DOS_ERR_FILRO;
}

/*
 =======================================================================================================================
    Function $4D _DELETE
 =======================================================================================================================
 */
void Server::vDOS_DELETE_FILE_OR_SUBDIRECTORY(const tdFileInfoBlock &_roFIB, const QString &_szMSXPath)
{
    unsigned char   ucDrive;
    QString         szPath;
    unsigned char   ucError;

    ucError = ucGetTarget(_roFIB, _szMSXPath, ucDrive, szPath);

    if (ucError == DOS_ERR_OK)
    {
        vLog(eLogBDOSDetails, "Path: %s\n", qPrintable(szPath));
        ucError = ucDelete(ucDrive, szPath);
    }

    vBDOSError(ucError);
}

/*
 =======================================================================================================================
    Rename / move an entry to its new host path
 =======================================================================================================================
 */
static unsigned char ucRename(const QString &_szPath, const QString &_szNewPath)
{
    if (!QFileInfo::exists(_szPath))
        return DOS_ERR_NOFIL;

    if (QFileInfo::exists(_szNewPath) && (QFileInfo(_szPath).canonicalFilePath() != QFileInfo(_szNewPath).canonicalFilePath()))
        return DOS_ERR_DUPF;

    return QDir().rename(_szPath, _szNewPath) ? DOS_ERR_OK : DOS_ERR_ACCV;
}

/*
 =======================================================================================================================
    New host path for _RENAME (new name in same directory) or _MOVE (new directory, same drive)
 =======================================================================================================================
 */
unsigned char Server::ucNewPath(unsigned char _ucDrive, const QString &_szPath, const QString &_szNew, bool _bMove, QString &_rszNewPath)
{
    QFileInfo oInfo(_szPath);

    if (_bMove)
    {
        QString szDirectory;

        if (_szNew.contains(':'))
            return DOS_ERR_IDRV;

        if (ucResolvePath(_szNew, szDirectory, _ucDrive) == 0xFF)
            return DOS_ERR_IDRV;

        if (!QFileInfo(szDirectory).isDir())
            return DOS_ERR_NODIR;

        if (oInfo.isDir() && (QFileInfo(szDirectory).canonicalFilePath() + "/").startsWith(oInfo.canonicalFilePath() + "/"))
            return DOS_ERR_DIRE;

        _rszNewPath = szDirectory + "/" + oInfo.fileName();
    }
    else
    {
        if (_szNew.isEmpty() || _szNew.contains('\\') || _szNew.contains('/') || _szNew.contains(':') || _szNew.contains('*') || _szNew.contains('?'))
            return DOS_ERR_IFNM;

        _rszNewPath = oInfo.absolutePath() + "/" + szFindEntry(oInfo.absolutePath(), _szNew);
    }

    return DOS_ERR_OK;
}

/*
 =======================================================================================================================
    Function $4E _RENAME
    Function $4F _MOVE
 =======================================================================================================================
 */
void Server::vDOS_RENAME_OR_MOVE(const tdFileInfoBlock &_roFIB, const QString &_szMSXPath, const QString &_szNew, bool _bMove)
{
    unsigned char   ucDrive;
    QString         szPath;
    QString         szNewPath;
    unsigned char   ucError;

    ucError = ucGetTarget(_roFIB, _szMSXPath, ucDrive, szPath);

    if (ucError == DOS_ERR_OK)
        ucError = ucNewPath(ucDrive, szPath, _szNew, _bMove, szNewPath);

    if ((ucError == DOS_ERR_OK) && bIsWriteProtected(szPath))
        ucError = DOS_ERR_WPROT;

    if (ucError == DOS_ERR_OK)
    {
        vLog(eLogBDOSDetails, "%s -> %s\n", qPrintable(szPath), qPrintable(szNewPath));
        ucError = ucRename(szPath, szNewPath);
        if (ucError == DOS_ERR_OK)
            vRenameArchive(szPath, szNewPath);
    }

    vBDOSError(ucError);
}

/*
 =======================================================================================================================
    Get or set attributes (read-only: permissions of the host file, archive: kept by the server)
 =======================================================================================================================
 */
void Server::vAttributes(unsigned char _ucError, const QString &_szPath, unsigned char _ucSet, unsigned char _ucNewAttributes)
{
    PACK_PUSH
    struct
    {
        unsigned char ucError;
        unsigned char ucAttributes;
    } s;
    PACK_POP

    s.ucError = _ucError;
    s.ucAttributes = 0;

    if ((s.ucError == DOS_ERR_OK) && !QFileInfo::exists(_szPath))
        s.ucError = DOS_ERR_NOFIL;

    if ((s.ucError == DOS_ERR_OK) && _ucSet && bIsWriteProtected(_szPath))
        s.ucError = DOS_ERR_WPROT;

    if (s.ucError == DOS_ERR_OK)
    {
        if (_ucSet && !QFileInfo(_szPath).isDir())
        {
            QFileDevice::Permissions ePermissions = QFile::permissions(_szPath);

            if (_ucNewAttributes & ATTRIBUTE_READ_ONLY)
                ePermissions &= ~(QFileDevice::WriteOwner | QFileDevice::WriteUser | QFileDevice::WriteGroup | QFileDevice::WriteOther);
            else
                ePermissions |= QFileDevice::WriteOwner | QFileDevice::WriteUser;

            QFile::setPermissions(_szPath, ePermissions);
            vSetArchive(_szPath, _ucNewAttributes & ATTRIBUTE_ARCHIVE_BIT);
        }

        s.ucAttributes = ucGetAttributes(_szPath);
    }

    vLog(eLogBDOSDetails, "Result: %02Xh, attributes %02Xh\n", s.ucError, s.ucAttributes);
    vBDOSAnswer(&s, sizeof(s));
}

/*
 =======================================================================================================================
    Get or set date and time
 =======================================================================================================================
 */
void Server::vDateTime(unsigned char _ucError, const QString &_szPath, QFile *_poFile, unsigned char _ucSet, unsigned short int _uiNewTime, unsigned short int _uiNewDate)
{
    PACK_PUSH
    struct
    {
        unsigned char       ucError;
        unsigned short int  uiTime;
        unsigned short int  uiDate;
    } s;
    PACK_POP

    s.ucError = _ucError;
    s.uiTime = 0;
    s.uiDate = 0;

    if ((s.ucError == DOS_ERR_OK) && !QFileInfo::exists(_szPath))
        s.ucError = DOS_ERR_NOFIL;

    if ((s.ucError == DOS_ERR_OK) && _ucSet && bIsWriteProtected(_szPath))
        s.ucError = DOS_ERR_WPROT;

    if (s.ucError == DOS_ERR_OK)
    {
        if (_ucSet && !QFileInfo(_szPath).isDir())
        {
            QDateTime oDateTime = oFromDosDateTime(_uiNewTime, _uiNewDate);

            if (_poFile && _poFile->isOpen())
            {
                _poFile->flush();
                _poFile->setFileTime(oDateTime, QFileDevice::FileModificationTime);
            }
            else
            {
                QFile oFile(_szPath);

                if (oFile.open(QIODevice::ReadOnly) || oFile.open(QIODevice::ReadWrite))
                    oFile.setFileTime(oDateTime, QFileDevice::FileModificationTime);
            }
        }

        vToDosDateTime(QFileInfo(_szPath).lastModified(), s.uiTime, s.uiDate);
    }

    vBDOSAnswer(&s, sizeof(s));
}

/*
 =======================================================================================================================
    Function $50 _ATTR
 =======================================================================================================================
 */
void Server::vDOS_GET_SET_FILE_ATTRIBUTES(const tdFileInfoBlock &_roFIB, const QString &_szMSXPath, unsigned char _ucSet, unsigned char _ucNewAttributes)
{
    unsigned char   ucDrive;
    QString         szPath;
    unsigned char   ucError = ucGetTarget(_roFIB, _szMSXPath, ucDrive, szPath);

    vAttributes(ucError, szPath, _ucSet, _ucNewAttributes);
}

/*
 =======================================================================================================================
    Function $51 _FTIME
 =======================================================================================================================
 */
void Server::vDOS_GET_SET_FILE_DATE_AND_TIME(const tdFileInfoBlock &_roFIB, const QString &_szMSXPath, unsigned char _ucSet, unsigned short int _uiNewTime, unsigned short int _uiNewDate)
{
    unsigned char   ucDrive;
    QString         szPath;
    unsigned char   ucError = ucGetTarget(_roFIB, _szMSXPath, ucDrive, szPath);

    vDateTime(ucError, szPath, nullptr, _ucSet, _uiNewTime, _uiNewDate);
}

/*
 =======================================================================================================================
    Get host path of file handle
 =======================================================================================================================
 */
unsigned char Server::ucGetHandlePath(unsigned char _ucFileHandle, QString &_rszPath)
{
    QFile *poFile = m_apoOpenedFiles[_ucFileHandle];

    if (!poFile)
        return DOS_ERR_IHAND;

    if (!poFile->isOpen())
        return DOS_ERR_HDEAD;

    _rszPath = poFile->fileName();

    return DOS_ERR_OK;
}

/*
 =======================================================================================================================
    Function $52 _HDELETE
    The file handle stays allocated (but dead) until it is closed.
 =======================================================================================================================
 */
void Server::vDOS_DELETE_FILE_HANDLE(unsigned char _ucFileHandle)
{
    QString         szPath;
    unsigned char   ucError = ucGetHandlePath(_ucFileHandle, szPath);

    if ((ucError == DOS_ERR_OK) && bIsWriteProtected(szPath))
        ucError = DOS_ERR_WPROT;

    if ((ucError == DOS_ERR_OK) && !QFileInfo(szPath).isWritable())
        ucError = DOS_ERR_FILRO;    // read-only attribute

    if (ucError == DOS_ERR_OK)
    {
        m_apoOpenedFiles[_ucFileHandle]->close();
        vSetArchive(szPath, true);
        ucError = QFile::remove(szPath) ? DOS_ERR_OK : DOS_ERR_FILRO;
    }

    vBDOSError(ucError);
}

/*
 =======================================================================================================================
    Function $53 _HRENAME
    Function $54 _HMOVE
 =======================================================================================================================
 */
void Server::vDOS_RENAME_OR_MOVE_FILE_HANDLE(unsigned char _ucFileHandle, const QString &_szNew, bool _bMove)
{
    QString         szPath;
    QString         szNewPath;
    unsigned char   ucError = ucGetHandlePath(_ucFileHandle, szPath);
    unsigned char   ucDrive = m_ucCurrentPhysicalDrive;

    for (int i = 0; i < 8; i++)
    {
        if (bIsDriveServed(i) && szPath.startsWith(szRootDir(i) + "/"))
            ucDrive = i;
    }

    if (ucError == DOS_ERR_OK)
        ucError = ucNewPath(ucDrive, szPath, _szNew, _bMove, szNewPath);

    if ((ucError == DOS_ERR_OK) && bIsWriteProtected(szPath))
        ucError = DOS_ERR_WPROT;

    if (ucError == DOS_ERR_OK)
    {
        QFile               *poFile = m_apoOpenedFiles[_ucFileHandle];
        qint64              iPos = poFile->pos();
        QIODevice::OpenMode eMode = poFile->openMode() & ~QIODevice::Truncate;

        vLog(eLogBDOSDetails, "%s -> %s\n", qPrintable(szPath), qPrintable(szNewPath));

        poFile->close();
        ucError = ucRename(szPath, szNewPath);
        if (ucError == DOS_ERR_OK)
        {
            vRenameArchive(szPath, szNewPath);
            poFile->setFileName(szNewPath);
        }
        poFile->open(eMode);
        poFile->seek(iPos);
    }

    vBDOSError(ucError);
}

/*
 =======================================================================================================================
    Function $55 _HATTR
 =======================================================================================================================
 */
void Server::vDOS_GET_SET_FILE_HANDLE_ATTRIBUTES(unsigned char _ucFileHandle, unsigned char _ucSet, unsigned char _ucNewAttributes)
{
    QString         szPath;
    unsigned char   ucError = ucGetHandlePath(_ucFileHandle, szPath);

    vAttributes(ucError, szPath, _ucSet, _ucNewAttributes);
}

/*
 =======================================================================================================================
    Function $56 _HFTIME
 =======================================================================================================================
 */
void Server::vDOS_GET_SET_FILE_HANDLE_DATE_AND_TIME(unsigned char _ucFileHandle, unsigned char _ucSet, unsigned short int _uiNewTime, unsigned short int _uiNewDate)
{
    QString         szPath;
    unsigned char   ucError = ucGetHandlePath(_ucFileHandle, szPath);

    vDateTime(ucError, szPath, m_apoOpenedFiles[_ucFileHandle], _ucSet, _uiNewTime, _uiNewDate);
}

/*
 =======================================================================================================================
    Function $59 _GETCD
 =======================================================================================================================
 */
void Server::vDOS_GET_CURRENT_DIRECTORY(unsigned char _ucDriveNumber)
{
    unsigned char   ucDrive = _ucDriveNumber ? _ucDriveNumber - 1 : m_ucCurrentPhysicalDrive;
    QByteArray      acPath;
    unsigned char   ucSize;

    if (ucDrive < 8)
    {
        QString szPath = m_szBDOSCurrentDir[ucDrive].isEmpty() ? "" : szRelativePath(ucDrive, szRootDir(ucDrive) + "/" + m_szBDOSCurrentDir[ucDrive]);

        acPath = szPath.toLatin1().left(63);
    }

    acPath.append('\0');
    ucSize = acPath.size();

    vLog(eLogBDOSDetails, "Result: %s\n", acPath.constData());
    vBDOSAnswer(&ucSize, sizeof(ucSize));
    vBDOSData(acPath.constData(), ucSize);
}

/*
 =======================================================================================================================
    Function $5A _CHDIR
 =======================================================================================================================
 */
void Server::vDOS_CHANGE_CURRENT_DIRECTORY(const tdFileInfoBlock &_roFIB, const QString &_szMSXPath)
{
    unsigned char   ucDrive;
    QString         szPath;
    unsigned char   ucError = ucGetTarget(_roFIB, _szMSXPath, ucDrive, szPath);

    if (ucError == DOS_ERR_OK)
    {
        if (!QFileInfo(szPath).isDir())
            ucError = DOS_ERR_NODIR;
        else
        {
            m_szBDOSCurrentDir[ucDrive] = QDir(szRootDir(ucDrive)).relativeFilePath(szPath);

            if (m_szBDOSCurrentDir[ucDrive] == ".")
                m_szBDOSCurrentDir[ucDrive] = "";
        }
    }

    vBDOSError(ucError);
}

/*
 =======================================================================================================================
    Function $5E _WPATH: whole path of the last entry found
 =======================================================================================================================
 */
void Server::vDOS_GET_WHOLE_PATH_STRING()
{
    PACK_PUSH
    struct
    {
        unsigned char ucError;
        unsigned char ucPos;
        unsigned char ucSize;
    } s;
    PACK_POP

    QByteArray acPath = m_szWholePath.toLatin1().left(63);

    acPath.append('\0');

    s.ucError = DOS_ERR_OK;
    s.ucSize = acPath.size();
    s.ucPos = acPath.lastIndexOf('\\') + 1;

    vLog(eLogBDOSDetails, "Result: %s\n", acPath.constData());
    vBDOSAnswer(&s, sizeof(s));
    vBDOSData(acPath.constData(), s.ucSize);
}
