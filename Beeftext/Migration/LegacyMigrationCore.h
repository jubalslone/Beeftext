/// \file
/// \brief Testable safety rules for one-time compatible Beeftext migration.

#ifndef LEAN_BEEFTEXT_LEGACY_MIGRATION_CORE_H
#define LEAN_BEEFTEXT_LEGACY_MIGRATION_CORE_H

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>


namespace migration {


enum class ESourceType {
    Installed,
    Portable,
};


enum class EPortableProduct {
    UpstreamBeeftext,
    LeanBeeftext,
};


enum class EState {
    NeverChecked = 0,
    ImportCompleted = 1,
    Complete = 2,
};


struct ValidationResult {
    bool snapshotCreated { false };
    bool parsed { false };
    bool persisted { false };
    bool reloaded { false };
    bool corresponds { false };
};


bool isStrongPortableCandidate(QString const &executableFolder, QString *outComboFilePath = nullptr,
    QString *outDedicatedRootPath = nullptr, EPortableProduct *outProduct = nullptr); ///< Require a beacon, expected data layout, one recognized executable, and readable combo data.
bool isBroadCleanupRoot(QString const &candidateRoot, QStringList const &protectedRoots); ///< Refuse shared/general-purpose roots.
bool isRecognizableInstalledCandidate(QString const &displayName, QString const &publisher,
	QString const &installLocation, QString const &executablePath); ///< Identify upstream without granting cleanup permission.
bool installedMetadataIsConsistent(QString const &displayName, QString const &publisher, QString const &installLocation,
    QString const &uninstallCommand, QString const &executablePath); ///< Validate registered upstream uninstall metadata.
QList<QList<qsizetype>> groupSourcesByContent(QList<QByteArray> const &digests); ///< Group byte-identical source libraries.
bool cleanupAllowed(ValidationResult const &result); ///< Gate all cleanup on the full import validation sequence.
bool shouldRunMigration(bool portableMode, bool leanComboFileExists, EState state); ///< Installed first-run gating.


} // namespace migration


#endif // LEAN_BEEFTEXT_LEGACY_MIGRATION_CORE_H
