/// \file
/// \brief One-time installed-mode migration from upstream Beeftext.

#ifndef LEAN_BEEFTEXT_LEGACY_MIGRATION_MANAGER_H
#define LEAN_BEEFTEXT_LEGACY_MIGRATION_MANAGER_H


class LegacyMigrationManager {
public:
    static void runIfNeeded(); ///< Detect, import, validate, and optionally clean up legacy sources.
};


#endif // LEAN_BEEFTEXT_LEGACY_MIGRATION_MANAGER_H
