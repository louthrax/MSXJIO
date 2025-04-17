#include <QtEndian>
#include <QDebug>

#include "PartitionExtractor.h"

/*
 =======================================================================================================================
 =======================================================================================================================
 */

static QByteArray readSector(QFile &file, quint64 sector)
{
	file.seek(sector * 512);
	return file.read(512);
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static QList<DiskPartition> parsePartitionTable(const QByteArray &sector, quint64 baseLBA = 0)
{
	/*~~~~~~~~~~~~~~~~~~~~~~~~~*/
	QList<DiskPartition>	list;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~*/

	for(int i = 0; i < 4; ++i)
	{
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
		const int	offset = 446 + i * 16;
		const uchar *entry = reinterpret_cast < const uchar * > (sector.constData() + offset);
		quint8		type = entry[4];
		quint32		lbaStart = qFromLittleEndian<quint32> (entry + 8);
		quint32		sectors = qFromLittleEndian<quint32> (entry + 12);
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

		if(type != 0 && sectors > 0)
		{
			list.append(
		{
			baseLBA + lbaStart, sectors, PartitionScheme::MBR, FilesystemType::Unknown, type
		});
		}
	}

	return list;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
static FilesystemType detectFilesystem(QFile &file, quint64 sectorStart)
{
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	QByteArray	bootSector = readSector(file, sectorStart);
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	if(bootSector.size() < 512) return FilesystemType::Unknown;

	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	auto	at = [&] (int offset, int length)
	{ return QString::fromLatin1(bootSector.mid(offset, length)).trimmed().toUpper(); };
	QString oemName = at(3, 8);
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	if(oemName == "NTFS") return FilesystemType::NTFS;
	if(oemName == "EXFAT") return FilesystemType::exFAT;

	/*~~~~~~~~~~~~~~~~~~~~~*/
	QString fat1 = at(54, 8);
	QString fat2 = at(82, 8);
	/*~~~~~~~~~~~~~~~~~~~~~*/

	if(fat1 == "FAT12") return FilesystemType::FAT12;
	if(fat1 == "FAT16") return FilesystemType::FAT16;
	if(fat1 == "FAT32") return FilesystemType::FAT32;
	if(fat2 == "FAT32") return FilesystemType::FAT32;

	file.seek((sectorStart * 512) + 1024 + 56);

	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	QByteArray	magic = file.read(2);
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	if(magic.size() == 2 && quint8(magic[0]) == 0x53 && quint8(magic[1]) == 0xEF)
	{
		return FilesystemType::EXT;
	}

	return FilesystemType::Unknown;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
QString describePartitions(const QList<DiskPartition> &partitions)
{
	/*~~~~~~~~~~~~~~*/
	QStringList lines;
	/*~~~~~~~~~~~~~~*/

	for(int i = 0; i < partitions.size(); ++i)
	{
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
		const DiskPartition &p = partitions[i];
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

		lines << QString("#%1 - Start: %2, Size: %3 sectors, Scheme: %4, FS: %5, MBR Type: 0x%6").arg(i + 1).arg(p.startSector).arg(p.sectorCount).arg(toString(p.scheme)).arg(toString(p.fsType)).arg
			(
				p.mbrType,
				2,
				16,
				QLatin1Char('0')
			).toUpper();
	}

	return lines.join('\n');
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
QList<DiskPartition> extractDiskPartitions(QFile &file)
{
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	QList<DiskPartition>	partitions;
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	if(!file.isOpen() && !file.open(QIODevice::ReadOnly))
	{
		qWarning() << "Cannot open file";
		return partitions;
	}

	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	QByteArray	mbr = readSector(file, 0);
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	if(mbr.size() < 512) return partitions;

	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
	quint8	partType = static_cast<quint8>(mbr[450]);
	/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

	if(partType == 0xEE)
	{
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
		QByteArray	header = readSector(file, 1);
		quint32		entryCount = qFromLittleEndian<quint32>
			(reinterpret_cast < const uchar * > (header.constData() + 80));
		quint32		entrySize = qFromLittleEndian<quint32>
			(reinterpret_cast < const uchar * > (header.constData() + 84));
		quint64		entriesLBA = qFromLittleEndian<quint64>
			(reinterpret_cast < const uchar * > (header.constData() + 72));
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

		for(quint32 i = 0; i < entryCount; ++i)
		{
			/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
			quint64 offset = entriesLBA * 512 + i * entrySize;
			/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

			file.seek(offset);

			/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
			QByteArray	entry = file.read(entrySize);
			QUuid		typeGuid = QUuid::fromRfc4122(entry.mid(0, 16));
			/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

			if(typeGuid.isNull()) continue;

			/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
			quint64 firstLBA = qFromLittleEndian<quint64> (reinterpret_cast < const uchar * > (entry.constData() + 32));
			quint64 lastLBA = qFromLittleEndian<quint64> (reinterpret_cast < const uchar * > (entry.constData() + 40));
			/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

			if(firstLBA > 0 && lastLBA >= firstLBA)
			{
				/*~~~~~~~~~~~~~~~~~~*/
				DiskPartition	part =
				{
					firstLBA, lastLBA - firstLBA + 1, PartitionScheme::GPT,
					FilesystemType::Unknown, 0
				};
				/*~~~~~~~~~~~~~~~~~~*/

				part.fsType = detectFilesystem(file, part.startSector);
                if ((part.fsType == FilesystemType::FAT12) || (part.fsType == FilesystemType::FAT16))
                    partitions.append(part);
			}
		}
	}
	else
	{
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
		QList<DiskPartition>	entries = parsePartitionTable(mbr);
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

		for(const DiskPartition & entry : entries)
		{
			if(entry.mbrType == 0x05 || entry.mbrType == 0x0F)
			{
				/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
				quint64 ebrBase = entry.startSector;
				quint64 nextEbr = ebrBase;
				/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

				while(nextEbr != 0)
				{
					/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
					QByteArray				ebr = readSector(file, nextEbr);
					QList<DiskPartition>	ebrParts = parsePartitionTable(ebr, 0);
					/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

					if(!ebrParts.isEmpty())
					{
						/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
						DiskPartition	logical = ebrParts[0];
						/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

						logical.startSector += nextEbr;
						logical.scheme = PartitionScheme::Logical;
						logical.fsType = detectFilesystem(file, logical.startSector);
                        if ((logical.fsType == FilesystemType::FAT12) || (logical.fsType == FilesystemType::FAT16))
                            partitions.append(logical);
					}

					if(ebrParts.size() > 1 && ebrParts[1].sectorCount > 0)
					{
						nextEbr = ebrBase + ebrParts[1].startSector;
					}
					else
					{
						break;
					}
				}
			}
			else
			{
				/*~~~~~~~~~~~~~~~~~~~~~~~~~*/
				DiskPartition	part = entry;
				/*~~~~~~~~~~~~~~~~~~~~~~~~~*/

				part.scheme = PartitionScheme::MBR;
				part.fsType = detectFilesystem(file, part.startSector);
                if ((part.fsType == FilesystemType::FAT12) || (part.fsType == FilesystemType::FAT16))
                    partitions.append(part);
			}
		}
	}

	return partitions;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
QString toString(PartitionScheme scheme)
{
	switch(scheme)
	{
	case PartitionScheme::MBR:		return "MBR";
	case PartitionScheme::GPT:		return "GPT";
	case PartitionScheme::Logical:	return "Logical";
	default:						return "Unknown";
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
QString toString(FilesystemType fsType)
{
	switch(fsType)
	{
	case FilesystemType::FAT12: return "FAT12";
	case FilesystemType::FAT16: return "FAT16";
	case FilesystemType::FAT32: return "FAT32";
	case FilesystemType::NTFS:	return "NTFS";
	case FilesystemType::exFAT: return "exFAT";
	case FilesystemType::EXT:	return "ext2/3/4";
	default:					return "Unknown";
	}
}
