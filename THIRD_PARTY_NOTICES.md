# Lean Beeftext Third-Party Notices

This file identifies third-party work retained or shipped with Lean Beeftext. It is an attribution and packaging record, not a substitute for the complete license texts distributed with each component.

## Upstream Beeftext

Lean Beeftext is based on [Beeftext 16.0](https://github.com/xmichelo/Beeftext), originally authored by Xavier Michelon and distributed under the MIT License. The upstream copyright and MIT terms are preserved in the repository's `LICENSE` file and source headers. Other retained upstream project assets remain covered by those project terms.

## Qt 6

The Windows package is built with Qt 6.8 and ships the Qt libraries and plugins selected by `windeployqt`. The modules used directly by Lean Beeftext include Qt Core, GUI, Widgets, Network, and SVG.

Qt is Copyright © The Qt Company Ltd. and other contributors. The open-source Qt 6 libraries are available under the GNU Lesser General Public License version 3 and GNU General Public License version 3, with commercial licensing also available from The Qt Company. Lean Beeftext uses the LGPLv3 option for the dynamically linked Qt libraries. See:

- <https://doc.qt.io/qt-6/licensing.html>
- <https://www.gnu.org/licenses/lgpl-3.0.html>
- <https://www.qt.io/download-open-source>

The package includes `LICENSE.Qt-LGPL-3.0.txt` and `LICENSE.GPL-3.0.txt`. Recipients may replace the dynamically linked Qt libraries with compatible versions. Corresponding Qt source and additional Qt license materials are available from the Qt links above.

The Qt deployment also contains Qt plugins and its software OpenGL fallback, `opengl32sw.dll`. That fallback is built from Mesa 3D components under permissive licenses documented in Qt's source and installation notices. Qt image-format plugins can incorporate compatible third-party codec code; the authoritative attribution records for the exact Qt 6.8 source are distributed in Qt's `qtbase/src/3rdparty` sources and can be enumerated with Qt's attribution scanner.

## XMiLib

XMiLib is Copyright © 2017 Xavier Michelon and is distributed under the MIT License. The complete notice is retained at `Submodules/XMiLib/LICENSE.md` in the source repository.

## emojilib

emojilib is Copyright © 2014 Mu-An Chiou and is distributed under the MIT License. The complete notice is retained at `Submodules/emojilib/LICENSE` in the source repository.

## Beeftext Translations

The application retains translations contributed to upstream Beeftext. Contributor attribution is maintained by the upstream project at <https://github.com/xmichelo/Beeftext/wiki/User-provided-translations>. These files are included as part of the MIT-licensed upstream Beeftext source. Product-specific strings that have not yet been translated fall back to English.

## Microsoft Visual C++ Runtime

The portable Windows package includes Microsoft Visual C++ runtime DLLs selected from the Visual Studio 2022 build environment. Qt deployment also supplies Microsoft's `D3Dcompiler_47.dll` when required by the Windows graphics stack. Those binaries are Microsoft redistributables and remain subject to the applicable Microsoft Visual Studio and Windows SDK licensing terms: <https://visualstudio.microsoft.com/license-terms/>.

## OpenSSL Status

The current Windows portable workflow does not deliberately package OpenSSL DLLs; Qt networking uses the Windows platform TLS backend for the disabled update path. If a future deployment includes OpenSSL, its exact version and matching license must be audited and added here before distribution. OpenSSL 3.x is licensed under Apache License 2.0, while older releases use earlier OpenSSL/SSLeay terms. See <https://openssl-library.org/source/license/>.

## Packaging Audit Note

The Windows workflow generates `SHA256SUMS.txt` from the actual packaged files. Before release, review that manifest against this notice, Qt's deployment output, and the selected Microsoft runtime so newly introduced third-party binaries cannot be distributed without an explicit license review.
