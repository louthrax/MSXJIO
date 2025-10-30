#include "Find.h"

/*
 =======================================================================================================================
 =======================================================================================================================
*/
static QRegularExpression compileMask(const QString& dosMask)
{
    QString mask = dosMask.trimmed();

    if (mask == "")
        mask = "*";

    mask.replace("*.*", "*");

    QString rx = QRegularExpression::wildcardToRegularExpression(mask);
    return QRegularExpression(rx, QRegularExpression::CaseInsensitiveOption);
}

/*
 =======================================================================================================================
 =======================================================================================================================
*/
static QStringList sortedEntriesDirsFirst(const QDir& dir, const QString& rootPath)
{
    QString normalizedRoot = QDir(rootPath).absolutePath();
    QString currentPath = dir.absolutePath();

    QStringList dirs = dir.entryList(QDir::Dirs, QDir::Name | QDir::IgnoreCase);
    QStringList files = dir.entryList(QDir::Files, QDir::Name | QDir::IgnoreCase);

    if (!normalizedRoot.isEmpty() && currentPath == normalizedRoot)
    {
        dirs.removeAll("..");
        dirs.removeAll(".");
    }

    dirs.append(files);
    return dirs;
}

/*
 =======================================================================================================================
 =======================================================================================================================
*/
QFile* poGetFirstEntry(const QFile* _poDirectoryFile, const QString& _szMask, const QString& _szRootPath)
{
    if (!_poDirectoryFile)
        return nullptr;

    QFileInfo dirInfo(*_poDirectoryFile);
    if (!dirInfo.exists() || !dirInfo.isDir())
        return nullptr;

    QDir dir(dirInfo.absoluteFilePath());
    const QRegularExpression reg = compileMask(_szMask);
    const QStringList entries = sortedEntriesDirsFirst(dir, _szRootPath);

    for (const QString& name : entries)
    {
        if (reg.match(name).hasMatch())
            return new QFile(dir.absoluteFilePath(name));
    }

    return nullptr;
}

/*
 =======================================================================================================================
 =======================================================================================================================
*/
QFile* poGetNextEntry(const QFile* _poCurrentEntry,
                      const QString& _szMask,
                      const QString& _szRootPath,
                      bool currentWasDir)
{
    if (!_poCurrentEntry)
        return nullptr;

    QFileInfo fi(*_poCurrentEntry);     // keeps absolute path/dir even if deleted
    QDir dir = fi.dir();

    const QRegularExpression reg = compileMask(_szMask);
    const QStringList all = sortedEntriesDirsFirst(dir, _szRootPath);
    const QString currentName = fi.fileName();

    // Find the boundary: count leading directories in 'all'
    int dirBoundary = 0;
    for (; dirBoundary < all.size(); ++dirBoundary)
    {
        if (!QFileInfo(dir.absoluteFilePath(all.at(dirBoundary))).isDir())
            break;
    }

    // Try exact position first
    int idx = all.indexOf(currentName, 0, Qt::CaseInsensitive);
    int start = -1;

    if (idx >= 0)
    {
        start = idx + 1;
    }
    else
    {
        // Binary-search-like lower_bound within the right bucket
        auto lowerBound = [&](int begin, int end, const QString& key)
        {
            int lo = begin, hi = end;
            while (lo < hi)
            {
                int mid = (lo + hi) / 2;
                const QString& val = all.at(mid);
                if (QString::compare(val, key, Qt::CaseInsensitive) <= 0)
                    lo = mid + 1;              // strictly after equal names
                else
                    hi = mid;
            }
            return lo; // first element > key in [begin, end)
        };

        if (currentWasDir)
        {
            start = lowerBound(0, dirBoundary, currentName);
            if (start > dirBoundary) start = dirBoundary;
        }
        else
        {
            start = lowerBound(dirBoundary, all.size(), currentName);
        }
    }

    for (int i = qMax(0, start); i < all.size(); ++i)
    {
        const QString& name = all.at(i);
        if (reg.match(name).hasMatch())
        {
            return new QFile(dir.absoluteFilePath(name));
        }
    }

    return nullptr;
}
