#ifndef TLF_SAFE_BUILD_H
#define TLF_SAFE_BUILD_H

#include <QString>

namespace constants {

inline constexpr bool kRestrictedBuild = true;

} // namespace constants


namespace tlf {


enum class ERestrictedVariable {
    Blocked,
    Cursor,
    Date,
    Time,
    DateTime,
    CustomDateTime,
    Combo,
    Upper,
    Lower,
    Trim,
    Input
};


struct RestrictedSnippet {
    QString text;
    qint32 cursorLeftCount { -1 };
    bool cursorSyntaxRejected { false };
};


ERestrictedVariable classifyVariable(QString const &variable);
QString sanitizeText(QString const &text);
RestrictedSnippet prepareSnippet(QString const &text);


} // namespace tlf

#endif // TLF_SAFE_BUILD_H
