

#include "Drive.h"
Drive::Drive()
{
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
Drive::~Drive()
{
	vEjectMedia();
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
bool Drive::bInsertMedia(QString _szMediaPath)
{
	vEjectMedia();

	m_poMediaFile = new QFile(_szMediaPath);

	if(m_poMediaFile)
	{
		if(!QFile::exists(m_poMediaFile->fileName()))
		{
			m_poMediaFile = nullptr;
		}
		else
		{
			if(m_poMediaFile->open(QIODevice::ReadWrite))
			{
				m_bMediaWriteProtected = false;
			}
			else if(m_poMediaFile->open(QIODevice::ReadOnly))
			{
				m_bMediaWriteProtected = true;
			}
			else
			{
				m_poMediaFile = nullptr;
			}
		}
	}

	if(m_poMediaFile)
	{
		m_oPartitions = extractDiskPartitions(*m_poMediaFile);
        m_oLastMediaInserted = _szMediaPath;
        m_oLastPathBrowsed = _szMediaPath;
    }

	return m_poMediaFile != nullptr;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
QString Drive::oMediaPath()
{
	if(m_poMediaFile)
	{
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
		QFileInfo	oInfo(*m_poMediaFile);
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

		return oInfo.absoluteFilePath();
	}
	else
		return "";
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
qint64 Drive::iMediaSize()
{
	if(m_poMediaFile)
	{
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
		QFileInfo	oInfo(*m_poMediaFile);
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

		return oInfo.size();
	}
	else
		return 0;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
QString Drive::oMediaLastModified()
{
	if(m_poMediaFile)
	{
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
		QFileInfo	oInfo(*m_poMediaFile);
		/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

		return oInfo.lastModified().toString(Qt::ISODate);
	}
	else
		return "";
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
tdDriveError Drive::eReadSectors
(
	unsigned char	_uiPartition,
	unsigned int	_uiSector,
	unsigned int	_uiSectorsCount,
	QByteArray		&_roResult
)
{
	if(m_poMediaFile)
	{
		if(m_oPartitions.size() > _uiPartition)
		{
			_uiSector += m_oPartitions[_uiPartition].startSector;
		}

		if(m_poMediaFile->seek((_uiSector) * 512))
		{
			_roResult = m_poMediaFile->read(_uiSectorsCount * 512);
			return(_roResult.size() == _uiSectorsCount * 512) ? eDriveErrorOK : eDriveErrorReadError;
		}
		else
			return eDriveErrorReadError;
	}
	else
	{
		return eDriveErrorNoMedia;
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
tdDriveError Drive::eWriteSectors
(
	unsigned char	_uiPartition,
	unsigned int	_uiSector,
	unsigned int	_uiSectorsCount,
	char			*_pcData
)
{
	if(!m_poMediaFile)
	{
		return eDriveErrorNoMedia;
	}
	else
	{
		if(m_bMediaWriteProtected || m_bDriveWriteProtected)
		{
			return eDriveErrorWriteProtected;
		}
		else
		{
			/*~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
			unsigned int	uiBytesWritten;
			/*~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

			if(m_oPartitions.size() > _uiPartition)
			{
				_uiSector += m_oPartitions[_uiPartition].startSector;
			}

			if(m_poMediaFile->seek((_uiSector) * 512))
			{
				uiBytesWritten = m_poMediaFile->write(_pcData, _uiSectorsCount * 512);
				m_poMediaFile->flush();

				if(uiBytesWritten == _uiSectorsCount * 512)
					return eDriveErrorOK;
				else
					return eDriveErrorWriteError;
			}
			else
			{
				return eDriveErrorWriteError;
			}
		}
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void Drive::vEjectMedia()
{
	if(m_poMediaFile)
	{
		m_poMediaFile->close();
		delete m_poMediaFile;
		m_poMediaFile = nullptr;
        m_oLastMediaInserted = "";
		m_oPartitions.empty();
	}
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
bool Drive::bIsMediaWriteProtected()
{
	return m_bMediaWriteProtected;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
tdMediaType Drive::eMediaType()
{
	if(m_poMediaFile)
	{
		switch(m_oPartitions.size())
		{
		case 0:
			return eMediaFloppy;

		case 1:
			return
				(
					(m_oPartitions[0].sectorCount == 1440)
				||	(m_oPartitions[0].sectorCount == 720)
				) ? eMediaFloppy : eMediaHardDisk;

		default:
			return eMediaHardDisk;
		}
	}
	else
		return eMediaEmpty;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
unsigned int Drive::uiPartitionCount()
{
	if(m_poMediaFile)
	{
		return(m_oPartitions.size() > 0) ? m_oPartitions.size() : 1;
	}
	else
		return 0;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
unsigned int Drive::uiFirstActivePartition()
{
	/*~~~~~~~~~~~~~~~~~~~~~*/
	unsigned int	uiResult;
	/*~~~~~~~~~~~~~~~~~~~~~*/

	uiResult = 0;

	for(unsigned int i = 0; i < m_oPartitions.size(); ++i)
	{
		if(m_oPartitions[i].isActive)
		{
			return i;
		}
	}

	return uiResult;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
QString Drive::szDescription()
{
	return describePartitions(m_oPartitions);
}
