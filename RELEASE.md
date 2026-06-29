# Caz Release Procedure

This checklist describes the release flow for Caz Core and CazEnv. It follows the same release discipline as ApeSDK: decide one version, update source, tag the exact commit, build reproducible artifacts, verify checksums, then attach the outputs to the GitHub release.

Caz Core is generic C source code and ships as one source archive per release:

```text
dist/caz-src-<VERSION>.zip
```

CazEnv is the SwiftUI/Metal simulator that embeds the same committed Caz Core. When a CazEnv release is published, it uses the Caz Core version as its app marketing version and builds these package artifacts:

```text
dist/cazenv-maccatalyst-<VERSION>.zip
dist/cazenv-ios-<VERSION>.zip
```

Before starting, decide the new release number and use it as the `VERSION` input throughout this checklist. Set `VERSION` without a leading `v`; the Git tag adds the leading `v` separately. The first Caz Core version is `0.001`, represented in source as `CAZ_CORE_VERSION`.

## 1. Prepare the Version

Update the Caz Core version in `src/caz_core.h`:

```c
#define CAZ_CORE_VERSION "0.001"
#define CAZ_CORE_VERSION_NUMBER 1u
```

`CAZ_CORE_VERSION` is the release-facing string. `CAZ_CORE_VERSION_NUMBER` is a monotonically increasing integer for compact comparisons. Keep the string in three-decimal form for the `0.xxx` release series.

Build and confirm the queryable version:

```bash
make
./build/caz --core-version
./build/caz --version
```

The `--core-version` output must exactly match `VERSION`.

## 2. Sync the CazEnv Version

CazEnv links the same C core and exposes the embedded version through `caz_env_core_version()`. Its app marketing version is generated from the committed Caz Core header through `cazenv/Config/CazVersion.xcconfig`.

Regenerate the Xcode version config after changing `src/caz_core.h`:

```bash
scripts/caz_version_config.sh
git diff -- cazenv/Config/CazVersion.xcconfig
```

Commit `src/caz_core.h` and `cazenv/Config/CazVersion.xcconfig` together. CazEnv must display the same version in its side panel:

```text
CazEnv <VERSION>
Core <VERSION>
```

## 3. Write the Release Synopsis

Create an approximately 200-word synopsis for this version before packaging the release. Summarize user-facing Caz Core and CazEnv changes first, then call out VM, bytecode language, source-compatibility, program archive, hardware-bridge, Catalyst/iOS, or package changes that matter to downstream users. Use this synopsis as the GitHub release description.

## 4. Run Release Checks

From the repository root:

```bash
make clean
make
make cazenv-regression
make cazenv-stress
make cazenv-movement
```

Run longer CazEnv checks before claiming long-run survival viability:

```bash
make cazenv-survival-long
```

## 5. Tag the Source

After the version number is decided and the final release commit is ready, tag the source code with the matching version number. The tag must point at the exact commit used to build the source archive and CazEnv packages.

```bash
VERSION="<VERSION>"
git tag -a "v${VERSION}" -m "Caz ${VERSION}"
git push origin "v${VERSION}"
```

If the release version changes, update `VERSION`, update `src/caz_core.h`, regenerate `cazenv/Config/CazVersion.xcconfig`, and recreate the tag before publishing it.

## 6. Build the Caz Core Source Package

Stage the source into a versioned folder so the archive has a stable top-level directory. Exclude VCS folders, build outputs, release artifacts, local Xcode user state, and Finder metadata.

```bash
make release-src
```

The output is:

```text
dist/caz-src-<VERSION>.zip
```

## 7. Build CazEnv for Mac Catalyst

CazEnv has a Catalyst-oriented Xcode project at:

```text
cazenv/CazEnvCatalyst.xcodeproj
```

Build and package the Mac Catalyst app:

```bash
make cazenv-maccatalyst-package
```

The unsigned package output is:

```text
dist/cazenv-maccatalyst-<VERSION>.zip
```

The underlying build command uses the generic Mac Catalyst destination and disabled signing so GitHub Actions can produce a reviewable package without Developer ID credentials:

```bash
xcodebuild \
  -project cazenv/CazEnvCatalyst.xcodeproj \
  -scheme CazEnvCatalyst \
  -configuration Release \
  -destination "generic/platform=macOS,variant=Mac Catalyst" \
  CODE_SIGNING_ALLOWED=NO \
  build
```

Sign, notarize, and staple this app separately before treating it as a public end-user macOS distribution package.

## 8. Build CazEnv for iOS

Build and package the iOS app from the same Catalyst project:

```bash
make cazenv-ios-package
```

The unsigned package output is:

```text
dist/cazenv-ios-<VERSION>.zip
```

The underlying build command uses the generic iOS destination and disabled signing:

```bash
xcodebuild \
  -project cazenv/CazEnvCatalyst.xcodeproj \
  -scheme CazEnvCatalyst \
  -configuration Release \
  -destination "generic/platform=iOS" \
  CODE_SIGNING_ALLOWED=NO \
  build
```

Archive, sign, and distribute through Apple tooling separately when preparing an installable iOS release.

## 9. Verify Release Artifacts

Build all local release packages:

```bash
make release-packages
```

Verify size and checksum:

```bash
VERSION="$(./build/caz --core-version)"
ls -lh \
  "dist/caz-src-${VERSION}.zip" \
  "dist/cazenv-maccatalyst-${VERSION}.zip" \
  "dist/cazenv-ios-${VERSION}.zip"
shasum -a 256 \
  "dist/caz-src-${VERSION}.zip" \
  "dist/cazenv-maccatalyst-${VERSION}.zip" \
  "dist/cazenv-ios-${VERSION}.zip"
```

Attach these files to the GitHub release:

```text
dist/caz-src-<VERSION>.zip
dist/cazenv-maccatalyst-<VERSION>.zip
dist/cazenv-ios-<VERSION>.zip
```

## 10. GitHub Release Packaging

The GitHub Actions workflow in `.github/workflows/release.yml` runs on `v*` tags and manual dispatch. It regenerates `cazenv/Config/CazVersion.xcconfig`, fails if that generated file differs from committed source, builds Caz Core, runs the CazEnv regression gate, packages the source/Catalyst/iOS zip files, uploads them as workflow artifacts, and attaches them to the matching GitHub release on tag builds.
