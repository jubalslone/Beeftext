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


// XMiLib's two-part version type is retained at the upstream base version for
// compatibility with the disabled upstream update-comparison machinery. All
// public product surfaces use kProductVersion instead.
xmilib::VersionNumber const kVersionNumber(16, 0);
QString const kProductVersion = "1.0.0";
QString const kUpstreamVersion = "16.0";
QString const kSingleInstanceIdentifier = "LeanBeeftextSingleInstanceIdentifier";
QString const kApplicationName = "Lean Beeftext";
// These are permanent Lean compatibility identifiers. The migration subsystem
// reads the legacy beeftext.org/Beeftext identity only as an upstream source.
QString const kSettingsApplicationName = "Lean Beeftext";
QString const kOrganizationName = "Jubal Slone";
QString const kBeeftextWikiHomeUrl = "https://github.com/jubalslone/lean-beeftext";
QString const kBeeftextWikiVariablesUrl = "https://github.com/jubalslone/lean-beeftext#variables";
QString const kBeeftextReleasesPagesUrl = "https://github.com/jubalslone/lean-beeftext/releases";
QString const kBeeftextIssueTrackerUrl = "https://github.com/jubalslone/lean-beeftext/issues";
QString const kKeyVariableRegExpStr(R"(#{key:(\w+)(?>:(\d+))?})");
QString const kShortcutVariableRegExpStr(R"(#{shortcut:(.+)})");
QString const kDelayVariableRegExpStr(R"(#{delay:(\d+)})");
QRegularExpression const kVariableRegExp(R"(#\{((.*?)(?<!\\))\})");


} // namespace constants
