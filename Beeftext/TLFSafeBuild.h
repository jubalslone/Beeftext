#ifndef TLF_SAFE_BUILD_H
#define TLF_SAFE_BUILD_H

#include <QString>

class QJsonObject;
class QSettings;

namespace constants {

inline constexpr bool kRestrictedBuild = true;

} // namespace constants


namespace tlf {


inline constexpr char kAllowRealLineBreaksInSnippetsSettingKey[] = "AllowRealLineBreaksInSnippets";
inline constexpr bool kDefaultAllowRealLineBreaksInSnippets = false;


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
QString sanitizeText(QString const &text, bool allowRealLineBreaks = false);
RestrictedSnippet prepareSnippet(QString const &text, bool allowRealLineBreaks = false);
bool readAllowRealLineBreaksInSnippets(QSettings const &settings);
void writeAllowRealLineBreaksInSnippets(QSettings &settings, bool value);
void exportAllowRealLineBreaksInSnippets(QSettings const &settings, QJsonObject &object);
void importAllowRealLineBreaksInSnippets(QJsonObject const &object, QSettings &settings);


} // namespace tlf

#endif // TLF_SAFE_BUILD_H
