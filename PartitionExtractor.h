#pragma once

#include <QFile>
#include <QList>
#include <QString>
#include <QUuid>

enum class PartitionScheme {
    MBR,
    GPT,
    Logical,
    Unknown
};

enum class FilesystemType {
    FAT12,
    FAT16,
    FAT32,
    NTFS,
    exFAT,
    EXT,
    Unknown
};

struct DiskPartition {
    quint64 startSector;
    quint64 sectorCount;
    PartitionScheme scheme;
    FilesystemType fsType;
    quint8 mbrType;
    bool isActive;
};

QList<DiskPartition> extractDiskPartitions(QFile &file);
QString describePartitions(const QList<DiskPartition> &partitions);
QString toString(PartitionScheme scheme);
QString toString(FilesystemType fsType);
