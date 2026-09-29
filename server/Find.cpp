#include <QSet>

#include "Find.h"

/*
 =======================================================================================================================
 =======================================================================================================================
*/
static QString szMaskPart(const QString& part)
{
    QString rx;

    for (int i = 0; i < part.size(); ++i)
    {
        const QChar c = part.at(i);

        if (c == '*')
            rx += "[^.]*";
        else if (c == '?')
        {
            // Trailing "?" also match no character (DOS semantics)
            bool trailing = true;
            for (int j = i + 1; j < part.size(); ++j)
            {
                if ((part.at(j) != '?') && (part.at(j) != '*'))
                {
                    trailing = false;
                    break;
                }
            }
            rx += trailing ? "[^.]?" : "[^.]";
        }
        else
            rx += QRegularExpression::escape(QString(c));
    }

    return rx;
}

static bool bOnlyWildcards(const QString& part)
{
    for (const QChar c : part)
    {
        if ((c != '?') && (c != '*'))
            return false;
    }

    return true;
}

static QRegularExpression compileMask(const QString& dosMask)
{
    QString mask = dosMask.trimmed();

    if (mask.isEmpty() || (mask == "*") || (mask == "*.*"))
        return QRegularExpression(".*");

    // "NAME" matches files without extension, "NAME.EXT" with extension (optional if only wildcards)
    QString rx;
    int     dot = mask.lastIndexOf('.');

    if (dot < 0)
        rx = "^" + szMaskPart(mask) + "$";
    else
    {
        const QString ext = mask.mid(dot + 1);

        rx = "^" + szMaskPart(mask.left(dot));
        rx += bOnlyWildcards(ext) ? "(\\." + szMaskPart(ext) + ")?" : "\\." + szMaskPart(ext);
        rx += "$";
    }

    return QRegularExpression(rx, QRegularExpression::CaseInsensitiveOption);
}

/*
 =======================================================================================================================
    MSX-DOS 8.3 names
 =======================================================================================================================
*/
static bool bIsDosChar(const QChar c)
{
    const ushort u = c.unicode();

    if ((u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z') || (u >= '0' && u <= '9'))
        return true;

    return QString("!#$%&'()-@^_`{}~").contains(c);
}

static bool bIsValidDosName(const QString& _szName)
{
    const int       dot = _szName.indexOf('.');
    const QString   base = (dot < 0) ? _szName : _szName.left(dot);
    const QString   ext = (dot < 0) ? QString() : _szName.mid(dot + 1);

    if (base.isEmpty() || (base.size() > 8) || (ext.size() > 3) || ((dot >= 0) && ext.isEmpty()))
        return false;

    for (const QChar c : base + ext)
    {
        if (!bIsDosChar(c))
            return false;
    }

    return true;
}

// Valid characters of a name, in upper case, at most _iMax characters
static QString szDosPart(const QString& _szPart, int _iMax)
{
    QString szResult;

    for (const QChar c : _szPart)
    {
        if (bIsDosChar(c) && (c != '~') && (szResult.size() < _iMax))
            szResult += c.toUpper();
    }

    return szResult;
}

QMap<QString, QString> oGetDosNames(const QString& _szDirectory)
{
    const QStringList       aszNames = QDir(_szDirectory).entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
    QMap<QString, QString>  oNames;
    QSet<QString>           oUsed;

    // valid 8.3 names first: they are never changed
    for (const QString& szName : aszNames)
    {
        const QString szUpper = szName.toUpper();

        if (bIsValidDosName(szName) && !oUsed.contains(szUpper))
        {
            oNames[szName] = szUpper;
            oUsed.insert(szUpper);
        }
    }

    // then aliases for the other names (in name order)
    for (const QString& szName : aszNames)
    {
        if (oNames.contains(szName))
            continue;

        const int   dot = szName.lastIndexOf('.');
        QString     szBase = szDosPart((dot > 0) ? szName.left(dot) : szName, 8);
        QString     szExt = (dot > 0) ? szDosPart(szName.mid(dot + 1), 3) : QString();
        QString     szAlias;

        if (szBase.isEmpty())
            szBase = "_";

        for (int i = 1; ; i++)
        {
            const QString szTail = "~" + QString::number(i);

            szAlias = szBase.left(8 - szTail.size()) + szTail + (szExt.isEmpty() ? "" : "." + szExt);
            if (!oUsed.contains(szAlias))
                break;
        }

        oNames[szName] = szAlias;
        oUsed.insert(szAlias);
    }

    return oNames;
}

QString szGetDosName(const QString& _szPath)
{
    const QFileInfo oInfo(_szPath);
    const QString   szName = oInfo.fileName();

    if ((szName == ".") || (szName == ".."))
        return szName;

    return oGetDosNames(oInfo.path()).value(szName, szName.toUpper());
}

// Host name of the entry given by its host name (case insensitive) or its 8.3 name, empty if not found
QString szFindHostEntry(const QString& _szDirectory, const QString& _szName)
{
    const QMap<QString, QString>    oNames = oGetDosNames(_szDirectory);
    const QString                   szUpper = _szName.toUpper();

    for (auto it = oNames.cbegin(); it != oNames.cend(); ++it)
    {
        if (it.key().compare(_szName, Qt::CaseInsensitive) == 0)
            return it.key();
    }

    for (auto it = oNames.cbegin(); it != oNames.cend(); ++it)
    {
        if (it.value() == szUpper)
            return it.key();
    }

    return QString();
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
    const QMap<QString, QString> dosNames = oGetDosNames(dir.absolutePath());

    for (const QString& name : entries)
    {
        if (reg.match(dosNames.value(name, name)).hasMatch())
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

    const QMap<QString, QString> dosNames = oGetDosNames(dir.absolutePath());

    for (int i = qMax(0, start); i < all.size(); ++i)
    {
        const QString& name = all.at(i);
        if (reg.match(dosNames.value(name, name)).hasMatch())
        {
            return new QFile(dir.absoluteFilePath(name));
        }
    }

    return nullptr;
}
