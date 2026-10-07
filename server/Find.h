#ifndef Find_h
#define Find_h

#include <QRegularExpression>
#include <QDir>
#include <QFile>
#include <QMap>

struct MsxPathParts
{
    QString path;      // Always ends with '\'
    QString filename;  // Empty if path ends with '\'
};

// MSX-DOS 8.3 names of the entries of a host directory: host name -> 8.3 name.
// Valid 8.3 names are kept (upper case), other names get an alias as XXXXXX~N.EXT
QMap<QString, QString> oGetDosNames(const QString& _szDirectory);
QString            szGetDosName(const QString& _szPath);
QString            szFindHostEntry(const QString& _szDirectory, const QString& _szName);

QFile*             poGetFirstEntry(const QFile* _poDirectoryFile, const QString& _szMask, const QString& _szRootPath);
QFile* poGetNextEntry(const QFile* _poCurrentEntry,
                      const QString& _szMask,
                      const QString& _szRootPath,
                      bool currentWasDir);
#endif
