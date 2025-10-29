#ifndef Find_h
#define Find_h

#include <QRegularExpression>
#include <QDir>
#include <QFile>

struct MsxPathParts
{
    QString path;      // Always ends with '\'
    QString filename;  // Empty if path ends with '\'
};

QFile*             poGetFirstEntry(const QFile* _poDirectoryFile, const QString& _szMask, const QString& _szRootPath);
QFile* poGetNextEntry(const QFile* _poCurrentEntry,
                      const QString& _szMask,
                      const QString& _szRootPath,
                      bool currentWasDir);
#endif
