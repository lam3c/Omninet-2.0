# OmniNet

**A desktop network and router-management application built with C++17 and Qt 6.**

OmniNet brings network inspection and router-management workflows into one desktop application. It is designed around a modular architecture and structured router profiles rather than hardcoding every router model into the application.

## Current project areas

- Network and gateway detection
- Gateway inspection
- Connected-device discovery
- Router profiles and router-specific configuration workflows
- Wireless intelligence and network diagnostics
- Qt Widgets desktop interface

Router-specific capabilities depend on the selected profile and the connected router. The initial profile focus is the Tenda F3; compatibility with other models should not be assumed unless a profile is available and tested.

## Technology

- C++17
- Qt 6 (Widgets, Network, SVG)
- CMake
- Windows x64 release build

## Download

Windows x64 installer builds are published under [GitHub Releases](https://github.com/lam3c/Omninet-2.0/releases).

**Security notice:** The initial 1.0.0 installer is unsigned. Windows Smart App Control or an organisation-managed application-control policy may block it. Do not disable Windows security protections to run it. A signed installer may be provided in a future release.

## Build from source

Requirements:
- C++17 compiler
- CMake 3.21 or later
- Qt 6 with the modules required by the project

Clone the repository:

```bash
git clone https://github.com/lam3c/Omninet-2.0.git
cd Omninet-2.0
cmake -S . -B build
cmake --build build
```

Open the project in Qt Creator or another compatible CMake environment if preferred.

## Licensing

Original OmniNet source code is licensed under the MIT License; see [LICENSE](LICENSE). Third-party components, including Qt and MinGW runtime libraries, are governed by their own licenses. The MIT License does not relicense those components. Review the applicable third-party terms before redistributing bundled binaries.

## Developer

Developed by **Cheru**.

Enjoying OmniNet? Voluntary support helps fund maintenance and future development.

PayPal: **thelamechrono@gmail.com**

---

**OmniNet — Simplifying Network Administration.**
