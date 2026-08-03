// version.h
// RuneVault — project version metadata.
//
// This header establishes the public include root (include/runevault) that the
// RuneVault sources and headers added by later tasks will live under. It is
// intentionally minimal for the project-scaffolding task (task 1.1); subsequent
// tasks add the Geometry, Board, Model, View, and Controller headers alongside
// it.

#ifndef RUNEVAULT_VERSION_H
#define RUNEVAULT_VERSION_H

namespace runevault {

constexpr int kVersionMajor = 0;
constexpr int kVersionMinor = 1;
constexpr int kVersionPatch = 0;

constexpr const char *kVersionString = "0.1.0";
constexpr const char *kProjectName = "RuneVault";

} // namespace runevault

#endif // RUNEVAULT_VERSION_H
