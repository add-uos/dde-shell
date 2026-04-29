// SPDX-FileCopyrightText: 2026 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "demoplugin.h"
#include "pluginfactory.h"

#include <QLoggingCategory>
#include <QDesktopServices>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>

Q_LOGGING_CATEGORY(demoPluginLog, "org.deepin.dde.shell.dock.demoplugin")

namespace dock {

static const QStringList s_colorThemeNames = {
    QStringLiteral("Ocean"),
    QStringLiteral("Sunset"),
    QStringLiteral("Aurora"),
    QStringLiteral("Neon"),
};

DemoPlugin::DemoPlugin(QObject *parent)
    : DApplet(parent)
    , m_directoryModel(new DirectoryModel(this))
{
}

bool DemoPlugin::init()
{
    DApplet::init();
    connect(m_directoryModel, &DirectoryModel::pathChanged, this, &DemoPlugin::directoryPathChanged);
    connect(m_directoryModel, &DirectoryModel::navigationChanged, this, &DemoPlugin::navigationChanged);
    connect(m_directoryModel, &DirectoryModel::countChanged, this, &DemoPlugin::folderCountChanged);

    // Auto-save path on navigation change
    connect(m_directoryModel, &DirectoryModel::pathChanged, this, [this]() {
        QSettings settings(QStringLiteral("deepin/dde-shell"), QStringLiteral("dock-demoplugin"));
        settings.setValue(QStringLiteral("lastPath"), m_directoryModel->path());
    });

    // Restore last path or default to home
    QSettings settings(QStringLiteral("deepin/dde-shell"), QStringLiteral("dock-demoplugin"));
    QString lastPath = settings.value(QStringLiteral("lastPath")).toString();
    if (lastPath.isEmpty() || !QDir(lastPath).exists())
        lastPath = QDir::homePath();
    m_directoryModel->navigateTo(lastPath);
    return true;
}

int DemoPlugin::gridCount() const
{
    return m_gridCount;
}

void DemoPlugin::setGridCount(int count)
{
    if (m_gridCount != count && count >= 1 && count <= 4) {
        m_gridCount = count;
        Q_EMIT gridCountChanged();
    }
}

int DemoPlugin::colorTheme() const
{
    return m_colorTheme;
}

void DemoPlugin::setColorTheme(int theme)
{
    if (m_colorTheme != theme && theme >= 0 && theme < s_colorThemeNames.size()) {
        m_colorTheme = theme;
        Q_EMIT colorThemeChanged();
    }
}

DirectoryModel *DemoPlugin::directoryModel() const
{
    return m_directoryModel;
}

int DemoPlugin::folderCount() const
{
    return m_directoryModel->folderCount();
}

int DemoPlugin::displayFolderCount() const
{
    return qMin(m_directoryModel->totalCount(), 4);
}

QString DemoPlugin::directoryPath() const
{
    return m_directoryModel->path();
}

int DemoPlugin::iconViewMode() const
{
    return m_iconViewMode;
}

void DemoPlugin::setIconViewMode(int mode)
{
    if (m_iconViewMode != mode && mode >= 0 && mode <= 1) {
        m_iconViewMode = mode;
        Q_EMIT iconViewModeChanged();
    }
}

QStringList DemoPlugin::availableColorThemes() const
{
    return s_colorThemeNames;
}

void DemoPlugin::openFile(const QString &filePath)
{
    if (filePath.endsWith(QLatin1String(".desktop"))) {
        QSettings desktopFile(filePath, QSettings::IniFormat);
        desktopFile.beginGroup(QStringLiteral("Desktop Entry"));
        QString execLine = desktopFile.value(QStringLiteral("Exec")).toString();
        desktopFile.endGroup();

        if (!execLine.isEmpty()) {
            // Remove freedesktop field codes (%f, %F, %u, %U, etc.)
            execLine.remove(QRegularExpression(QStringLiteral(R"(%[fFuUdDnNickvm)")));

            QStringList parts = execLine.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (!parts.isEmpty()) {
                QString program = parts.takeFirst();

                // Resolve to absolute path if needed
                if (!program.startsWith(QLatin1Char('/'))) {
                    QString resolved = QStandardPaths::findExecutable(program);
                    if (!resolved.isEmpty())
                        program = resolved;
                }

                QString runId = QFileInfo(filePath).completeBaseName();

                QDBusMessage msg = QDBusMessage::createMethodCall(
                    QStringLiteral("org.desktopspec.ApplicationManager1"),
                    QStringLiteral("/org/desktopspec/ApplicationManager1"),
                    QStringLiteral("org.desktopspec.ApplicationManager1"),
                    QStringLiteral("executeCommand"));
                msg << program << parts
                    << QStringLiteral("portablebinary") << runId
                    << QVariant::fromValue(QMap<QString, QString>()) << QString();

                QDBusReply<QDBusObjectPath> reply = QDBusConnection::sessionBus().call(msg);
                if (reply.isValid())
                    return;

                qCWarning(demoPluginLog) << "AM1 executeCommand failed:" << reply.error().message()
                                         << "falling back to openUrl";
            }
        }
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
}

void DemoPlugin::refreshDirectory()
{
    m_directoryModel->refresh();
}

void DemoPlugin::navigateTo(const QString &path)
{
    m_directoryModel->navigateTo(path);
}

void DemoPlugin::goBack()
{
    m_directoryModel->goBack();
}

void DemoPlugin::goForward()
{
    m_directoryModel->goForward();
}

bool DemoPlugin::canGoBack() const
{
    return m_directoryModel->canGoBack();
}

bool DemoPlugin::canGoForward() const
{
    return m_directoryModel->canGoForward();
}

bool DemoPlugin::isDirectory(const QString &path) const
{
    return QFileInfo(path).isDir();
}

bool DemoPlugin::isFile(const QString &path) const
{
    return QFileInfo(path).isFile();
}

D_APPLET_CLASS(DemoPlugin)
}

#include "demoplugin.moc"
