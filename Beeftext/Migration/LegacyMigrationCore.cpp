/// \file
/// \brief Testable safety rules for one-time upstream Beeftext migration.


#include "LegacyMigrationCore.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QProcess>


namespace migration {


namespace {


QString normalizedPath(QString const &path) {
    QFileInfo const info(path);
    QString result = info.canonicalFilePath();
    if (result.isEmpty())
        result = info.absoluteFilePath();
    return QDir::cleanPath(QDir::fromNativeSeparators(result)).toCaseFolded();
}


QString executableFromCommand(QString const &command) {
    QStringList const parts = QProcess::splitCommand(command.trimmed());
    return parts.isEmpty() ? QString() : parts.first();
}


} // namespace


bool isStrongPortableCandidate(QString const &executableFolder, QString *outComboFilePath,
    QString *outDedicatedRootPath) {
    QDir const appDir(executableFolder);
    QString const executable = appDir.absoluteFilePath("Beeftext.exe");
    QString comboPath;
    QString rootPath;
    if (QFileInfo(appDir.absoluteFilePath("Portable.bin")).isFile()) {
        comboPath = appDir.absoluteFilePath("Data/comboList.json");
        rootPath = appDir.absolutePath();
    } else if (QFileInfo(appDir.absoluteFilePath("PortableApps.bin")).isFile()) {
        comboPath = appDir.absoluteFilePath("../../Data/settings/comboList.json");
        rootPath = appDir.absoluteFilePath("../..");
    } else {
        return false;
    }

    QFile comboFile(comboPath);
    if (!QFileInfo(executable).isFile() || !QFileInfo(comboPath).isFile() ||
        !comboFile.open(QIODevice::ReadOnly) || comboFile.readAll().trimmed().isEmpty())
        return false;

    if (outComboFilePath)
        *outComboFilePath = QFileInfo(comboPath).absoluteFilePath();
    if (outDedicatedRootPath)
        *outDedicatedRootPath = QFileInfo(rootPath).absoluteFilePath();
    return true;
}


bool isBroadCleanupRoot(QString const &candidateRoot, QStringList const &protectedRoots) {
    QString const candidate = normalizedPath(candidateRoot);
    if (candidate.isEmpty())
        return true;
    QDir const candidateDir(candidateRoot);
    if (candidateDir.isRoot())
        return true;
    for (QString const &root: protectedRoots)
        if (!root.isEmpty() && candidate == normalizedPath(root))
            return true;
    return false;
}


bool installedMetadataIsConsistent(QString const &displayName, QString const &publisher,
    QString const &installLocation, QString const &uninstallCommand, QString const &executablePath) {
    QString const foldedName = displayName.toCaseFolded();
    if (!foldedName.contains("beeftext") || foldedName.contains("lean beeftext"))
        return false;
    if (!publisher.trimmed().isEmpty() && !publisher.contains("Michelon", Qt::CaseInsensitive) &&
        !publisher.contains("Beeftext", Qt::CaseInsensitive))
        return false;

    QString const root = normalizedPath(installLocation);
    QString const executable = normalizedPath(executablePath);
    QString const uninstaller = normalizedPath(executableFromCommand(uninstallCommand));
    if (root.isEmpty() || executable.isEmpty() || uninstaller.isEmpty())
        return false;
    QString const prefix = root.endsWith('/') ? root : root + '/';
    if (!executable.startsWith(prefix) || !uninstaller.startsWith(prefix))
        return false;
    QString const uninstallName = QFileInfo(uninstaller).fileName().toCaseFolded();
    return QFileInfo(executablePath).fileName().compare("Beeftext.exe", Qt::CaseInsensitive) == 0 &&
        (uninstallName.startsWith("unins") || uninstallName.contains("uninstall"));
}


QList<QList<qsizetype>> groupSourcesByContent(QList<QByteArray> const &digests) {
    QList<QList<qsizetype>> result;
    QHash<QByteArray, qsizetype> groupByDigest;
    for (qsizetype i = 0; i < digests.size(); ++i) {
        auto const existing = groupByDigest.constFind(digests[i]);
        if (existing == groupByDigest.constEnd()) {
            groupByDigest.insert(digests[i], result.size());
            result.append({ i });
        } else {
            result[*existing].append(i);
        }
    }
    return result;
}


bool cleanupAllowed(ValidationResult const &result) {
    return result.snapshotCreated && result.parsed && result.persisted && result.reloaded && result.corresponds;
}


bool shouldRunMigration(bool portableMode, bool leanComboFileExists, EState state) {
    return !portableMode && !leanComboFileExists && state == EState::NeverChecked;
}


} // namespace migration
