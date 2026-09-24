#!/usr/bin/env bash
# One-time (or refresh-after-DCMTK-upgrade) native build of the DCMTK Windows
# static libraries consumed by dcmtk_flutter. Run from the dcmtk repo root.
set -euo pipefail
usage() {
  cat <<'EOF'
Usage: build_mobile/scripts/sync_build_windows.sh [windows_ip] [windows_user]
Syncs the DCMTK C++ source (not build/, not other platforms' prebuilt libs) to
C:/Projects/dcmtk and runs build_mobile/scripts/build_windows.ps1 there,
producing flutter_plugin/windows/{Headers,Libs}. Uses exported WINDOWS_IP and
WINDOWS_USER by default.
EOF
}
[[ "${1:-}" == "-h" || "${1:-}" == "--help" ]] && { usage; exit 0; }
win_ip="${1:-${WINDOWS_IP:-}}"
win_user="${2:-${WINDOWS_USER:-}}"
[[ -n "$win_ip" && -n "$win_user" ]] || { echo 'Export WINDOWS_IP and WINDOWS_USER first (source ~/.zshrc).' >&2; exit 2; }
[[ "$win_ip" =~ ^[a-zA-Z0-9._:-]+$ && "$win_user" =~ ^[a-zA-Z0-9._@-]+$ ]] || { echo 'Invalid SSH host/user.' >&2; exit 2; }
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$root"
target="$win_user@$win_ip"
remote='C:/Projects/dcmtk'
ssh_opts=(-o BatchMode=yes -o ConnectTimeout=15)
modules=(ofstd oflog oficonv dcmdata dcmimgle dcmimage dcmjpeg dcmjpls dcmtls
  dcmnet dcmsr dcmsign dcmwlm dcmqrdb dcmpstat dcmrt dcmiod dcmfg dcmseg
  dcmtract dcmpmap dcmect dcmapps doxygen config CMake build_mobile)
echo "Syncing DCMTK source to $target:$remote"
ssh "${ssh_opts[@]}" "$target" "powershell -NoProfile -Command \"New-Item -ItemType Directory -Force $remote | Out-Null\""
COPYFILE_DISABLE=1 tar czf - "${modules[@]}" CMakeLists.txt |
  ssh "${ssh_opts[@]}" "$target" "tar xzf - -C $remote"
# Pin to the x64 Build Tools instance so CMake's VS generator can't pick an ARM64 install.
# The SSH remote shell is cmd.exe, which does not treat single quotes as grouping —
# use double quotes so paths with spaces survive as one argument.
vs_instance='C:\Program Files\Microsoft Visual Studio\2022\BuildTools-x64'
cmake_path="$vs_instance\\Common7\\IDE\\CommonExtensions\\Microsoft\\CMake\\CMake\\bin\\cmake.exe"
echo "Building DCMTK Windows static libraries (Release+Debug)..."
ssh "${ssh_opts[@]}" "$target" "powershell -NoProfile -ExecutionPolicy Bypass -File $remote/build_mobile/scripts/build_windows.ps1 -GeneratorInstance \"$vs_instance\" -CMakePath \"$cmake_path\""
