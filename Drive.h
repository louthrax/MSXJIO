#ifndef Drive_h
#define Drive_h

#include <QString>
#include <QFile>
#include <QFileInfo>

typedef enum {
    eMediaEmpty,
    eMediaFloppy,
    eMediaHardDisk
} tdMediaType;

typedef enum {
    eDriveErrorOK,
    eDriveErrorNoMedia,
    eDriveErrorWriteError,
    eDriveErrorReadError,
    eDriveErrorWriteProtected
} tdDriveError;


class Drive
{

public:
    Drive();
    ~Drive();

    bool            bInsertMedia(QString _szMediaPath);

    QString         oMediaPath();
    qint64          iMediaSize();
    QString         oMediaLastModified();

    tdMediaType     eMediaType();
    tdDriveError    eReadSectors(unsigned int _uiSector, unsigned int _uiSectorsCount, QByteArray& _roResult);
    tdDriveError    eWriteSectors(unsigned int _uiSector, unsigned int _uiSectorsCount, char * _pcData);
    void            vEjectMedia();
    bool            bIsMediaWriteProtected();

private:
    tdMediaType m_eMediaType = eMediaEmpty;
    QFile       *m_poMediaFile = nullptr;
    bool        m_bMediaWriteProtected = false;
    bool        m_bDriveWriteProtected = false;
};

#endif
