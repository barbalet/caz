# Caz Release Procedure

This checklist describes the release flow for Caz Core, the portable C source code for the Cat Operating System VM, body model, loader, OpenCat bridge, bundled `.caz` programs, and command-line simulator. Caz releases ship one source archive only. CazEnv app binaries, DMGs, notarization, and architecture-specific packages are outside this release procedure.

Before starting, decide the new release number and use it as the `VERSION` input throughout this checklist. Set `VERSION` without a leading `v`; the Git tag adds the leading `v` separately. The first Caz Core version is `0.001`, represented in source as `CAZ_CORE_VERSION`.

## 1. Prepare the Version

Update the Caz Core version in `src/caz_core.h`:

```c
#define CAZ_CORE_VERSION "0.001"
#define CAZ_CORE_VERSION_NUMBER 1u
```

`CAZ_CORE_VERSION` is the release-facing string. `CAZ_CORE_VERSION_NUMBER` is a monotonically increasing integer for code that wants a compact comparison value. Keep the string in three-decimal form for the `0.xxx` release series.

Build and confirm the queryable version:

```bash
make
./build/caz --core-version
```

The output must exactly match `VERSION`.

## 2. Confirm CazEnv Recognition

CazEnv links the same C core and exposes the embedded version through `caz_env_core_version()`. When CazEnv is built with a Caz release, its displayed core version must match `CAZ_CORE_VERSION`.

```bash
xcodebuild \
  -project cazenv/cazenv.xcodeproj \
  -scheme cazenv \
  -configuration Debug \
  -derivedDataPath cazenv/build \
  build
```

This is a recognition check, not a release artifact. Do not attach the CazEnv app bundle to a Caz Core source release.

## 3. Write the Release Synopsis

Create an approximately 200-word synopsis for this version before packaging the release. Summarize user-facing Caz Core changes first, then call out VM, bytecode language, source-compatibility, CazEnv integration, program archive, or hardware-bridge changes that matter to downstream users. Use this synopsis as the GitHub release description.

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

After the version number is decided and the final release commit is ready, tag the source code with the matching version number. The tag must point at the exact commit used to create the source archive.

```bash
VERSION="<VERSION>"
git tag -a "v${VERSION}" -m "Caz Core ${VERSION}"
git push origin "v${VERSION}"
```

If the release version changes, update `VERSION`, update `src/caz_core.h`, and recreate the tag before publishing it.

## 6. Create the Source Package

Stage the source into a versioned folder so the archive has a stable top-level directory. Exclude VCS folders, build outputs, release artifacts, local Xcode user state, and Finder metadata.

```bash
VERSION="$(./build/caz --core-version)"
mkdir -p dist
SRC_ROOT="caz-${VERSION}"
SRC_STAGE="$(mktemp -d)/${SRC_ROOT}"
rsync -a ./ "$SRC_STAGE"/ \
  --exclude .git \
  --exclude build \
  --exclude .build \
  --exclude dist \
  --exclude "cazenv/build" \
  --exclude "*.xcuserstate" \
  --exclude "xcuserdata" \
  --exclude ".DS_Store"
ditto -c -k --keepParent "$SRC_STAGE" "dist/caz-src-${VERSION}.zip"
```

## 7. Verify Release Artifact

There is one release artifact:

```text
dist/caz-src-<VERSION>.zip
```

Verify its size and checksum:

```bash
VERSION="$(./build/caz --core-version)"
ls -lh "dist/caz-src-${VERSION}.zip"
shasum -a 256 "dist/caz-src-${VERSION}.zip"
```

Attach only this file to the GitHub release:

```text
dist/caz-src-<VERSION>.zip
```
