/// \file
/// \author Xavier Michelon
///
/// \brief Declaration of pre-compiled headers
///  
/// Copyright (c) Xavier Michelon. All rights reserved.  
/// Licensed under the MIT License. See LICENSE file in the project root for full license information.    


#ifndef BEEFTEXT_STDAFX_H
#define BEEFTEXT_STDAFX_H

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

#include <QtWidgets>
#include <QtNetwork>
#include <QtGui>
#include <QtCore>
#include "TLFSafeBuild.h"


#endif // BEEFTEXT_STDAFX_H
