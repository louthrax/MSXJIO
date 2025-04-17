#ifndef Drive_h
#define Drive_h

#include <QString>
#include <QFile>
#include <QFileInfo>
#include "PartitionExtractor.h"

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
    unsigned int    uiPartitionCount();
    unsigned int    uiFirstActivePartition();
    QString         szDescription();
    tdDriveError    eReadSectors(unsigned char _uiPartition, unsigned int _uiSector, unsigned int _uiSectorsCount, QByteArray& _roResult);
    tdDriveError    eWriteSectors(unsigned char _uiPartition, unsigned int _uiSector, unsigned int _uiSectorsCount, char * _pcData);
    void            vEjectMedia();
    bool            bIsMediaWriteProtected();

private:
    QFile       *m_poMediaFile = nullptr;
    bool        m_bMediaWriteProtected = false;
    bool        m_bDriveWriteProtected = false;
    QList<DiskPartition> m_oPartitions;
};

#endif
