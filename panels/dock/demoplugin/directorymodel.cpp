// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "directorymodel.h"

#include <QDir>
#include <QMimeDatabase>
#include <QMimeType>
#include <QIcon>
#include <QPixmap>
#include <QBuffer>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(demoDirModelLog, "org.deepin.dde.shell.dock.demoplugin.directorymodel")

namespace dock {

DirectoryModel::DirectoryModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int DirectoryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_entries.size();
}

QVariant DirectoryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const auto &entry = m_entries.at(index.row());
    switch (role) {
    case NameRole:   return entry.name;
    case PathRole:   return entry.path;
    case IconUrlRole: return entry.iconUrl;
    case IconNameRole: return entry.iconName;
    case IsDirRole:  return entry.isDir;
    }
    return {};
}

QHash<int, QByteArray> DirectoryModel::roleNames() const
{
    return {
        {NameRole,    "fileName"},
        {PathRole,    "filePath"},
        {IconUrlRole, "iconUrl"},
        {IconNameRole, "iconName"},
        {IsDirRole,   "isDir"},
    };
}

QString DirectoryModel::path() const
{
    return m_path;
}

void DirectoryModel::setPath(const QString &path)
{
    if (m_path == path)
        return;
    m_path = path;
    Q_EMIT pathChanged();
    loadDirectory();
}

int DirectoryModel::folderCount() const
{
    return m_folderCount;
}

int DirectoryModel::totalCount() const
{
    return m_entries.size();
}

void DirectoryModel::refresh()
{
    loadDirectory();
}

bool DirectoryModel::canGoBack() const
{
    return m_historyIndex > 0;
}

bool DirectoryModel::canGoForward() const
{
    return m_historyIndex < m_history.size() - 1;
}

void DirectoryModel::navigateTo(const QString &path)
{
    if (path.isEmpty())
        return;

    // Truncate forward history
    m_history = m_history.mid(0, m_historyIndex + 1);
    m_history.append(path);
    m_historyIndex = m_history.size() - 1;

    setPath(path);
    Q_EMIT navigationChanged();
}

void DirectoryModel::goBack()
{
    if (!canGoBack())
        return;
    m_historyIndex--;
    setPath(m_history.at(m_historyIndex));
    Q_EMIT navigationChanged();
}

void DirectoryModel::goForward()
{
    if (!canGoForward())
        return;
    m_historyIndex++;
    setPath(m_history.at(m_historyIndex));
    Q_EMIT navigationChanged();
}

QVariantMap DirectoryModel::get(int index) const
{
    if (index < 0 || index >= m_entries.size())
        return {};
    const auto &entry = m_entries.at(index);
    return {
        {QStringLiteral("fileName"),  entry.name},
        {QStringLiteral("filePath"),  entry.path},
        {QStringLiteral("iconUrl"),   entry.iconUrl},
        {QStringLiteral("iconName"),  entry.iconName},
        {QStringLiteral("isDir"),     entry.isDir},
    };
}

void DirectoryModel::loadDirectory()
{
    beginResetModel();
    m_entries.clear();
    m_folderCount = 0;

    QDir dir(m_path);
    if (!dir.exists()) {
        endResetModel();
        Q_EMIT countChanged();
        return;
    }

    QMimeDatabase mimeDb;

    const auto entries = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot,
        QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);

    for (const auto &info : entries) {
        Entry entry;
        entry.name = info.fileName();
        entry.path = info.absoluteFilePath();
        entry.isDir = info.isDir();

        if (entry.isDir) {
            entry.iconName = QStringLiteral("folder");
            m_folderCount++;
        } else {
            auto mime = mimeDb.mimeTypeForFile(info);
            entry.iconName = mime.iconName();
            if (entry.iconName.isEmpty())
                entry.iconName = mime.genericIconName();
            if (entry.iconName.isEmpty())
                entry.iconName = QStringLiteral("text-x-generic");
        }

        entry.iconUrl = iconToDataUrl(entry.iconName, 64);
        m_entries.append(entry);
    }

    endResetModel();
    Q_EMIT countChanged();
}

QString DirectoryModel::iconToDataUrl(const QString &iconName, int size)
{
    QIcon icon = QIcon::fromTheme(iconName);
    if (icon.isNull())
        icon = QIcon::fromTheme(QStringLiteral("text-x-generic"));
    if (icon.isNull())
        return QString();

    QPixmap pm = icon.pixmap(size, size);
    if (pm.isNull())
        return QString();

    QByteArray ba;
    QBuffer buf(&ba);
    buf.open(QIODevice::WriteOnly);
    pm.save(&buf, "PNG");
    return QStringLiteral("data:image/png;base64,") + ba.toBase64();
}

}
