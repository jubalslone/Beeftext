/// \file
/// \author Xavier Michelon
///
/// \brief Implementation of application constants
///  
/// Copyright (c) Xavier Michelon. All rights reserved.  
/// Licensed under the MIT License. See LICENSE file in the project root for full license information.  


#include "stdafx.h"
#include "BeeftextConstants.h"


namespace constants {


xmilib::VersionNumber const kVersionNumber(16, 0);
QString const kApplicationName = "Beeftext";
QString const kOrganizationName = "beeftext.org";
QString const kBeeftextWikiHomeUrl = "https://github.com/jubalslone/Beeftext";
QString const kBeeftextWikiVariablesUrl = "https://github.com/jubalslone/Beeftext";
QString const kBeeftextReleasesPagesUrl = "https://github.com/jubalslone/Beeftext/releases";
QString const kBeeftextIssueTrackerUrl = "https://github.com/jubalslone/Beeftext/issues";
QString const kKeyVariableRegExpStr(R"(#{key:(\w+)(?>:(\d+))?})");
QString const kShortcutVariableRegExpStr(R"(#{shortcut:(.+)})");
QString const kDelayVariableRegExpStr(R"(#{delay:(\d+)})");
QRegularExpression const kVariableRegExp(R"(#\{((.*?)(?<!\\))\})");


} // namespace constants
