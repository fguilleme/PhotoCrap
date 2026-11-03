#!/bin/bash
# A full disposable 32 MiB filesystem; never fill a host/workspace filesystem.
set -euo pipefail
if [[ $(uname -s) != Darwin ]]; then
    echo "This harness requires macOS hdiutil." >&2
    exit 1
fi
repository=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
binary=${1:-"$repository/build/photocrap_tiled_tests"}
[[ -x "$binary" ]]
test_root=$(mktemp -d /private/tmp/photocrap-cache-full.XXXXXX)
attached=0
cleanup() {
    if [[ $attached == 1 ]]; then
        if ! hdiutil detach "$test_root/mount" -quiet; then
            echo "Could not detach the test volume; retained at $test_root" >&2
            return
        fi
    fi
    rm -rf -- "${test_root:?}"
}
trap cleanup EXIT
hdiutil create -size 32m -fs HFS+ -type SPARSE -volname PhotoCrapCacheTest "$test_root/cache" -quiet
mkdir "$test_root/mount"
hdiutil attach "$test_root/cache.sparseimage" -nobrowse -mountpoint "$test_root/mount" -quiet
attached=1
printf '%s\n' 'PhotoCrap isolated full-disk test' > "$test_root/mount/photocrap-isolated-test"
PHOTOCRAP_FULL_DISK_DIRECTORY="$test_root/mount" QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
    "$binary" realDiskFullPreservesMappedVersions
