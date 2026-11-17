#!/usr/bin/env bash
# Package an already-built SDK into a distro-tagged tarball with checksums.
# Run after scripts/setup.sh. The CI workflow runs this for Ubuntu/Lubuntu and
# Fedora x86_64; running it locally produces a package for the local ABI only.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$root"

version="${VERSION:-0.1.0}"
arch="$(uname -m)"
distro="unknown"
if [ -r /etc/os-release ]; then
    # shellcheck disable=SC1091
    . /etc/os-release
    distro="${ID:-unknown}"
fi

prefix="$PWD/build-sdk/prefix"
for needed in \
    "$prefix/lib/libmsr_engine.so" \
    "$prefix/bin/msr-cli" \
    "$prefix/include/msr/engine/engine.h" \
    "$prefix/lib/cmake/msr/msrConfig.cmake"; do
    if [ ! -e "$needed" ]; then
        echo "missing $needed — run scripts/setup.sh first" >&2
        exit 1
    fi
done

stage="$PWD/dist/stage"
rm -rf "$stage"
mkdir -p "$stage"/{bin,lib/cmake,include,docs,examples}
cp -a "$prefix"/lib/libmsr_engine.so* "$stage/lib/"
cp -a "$prefix"/bin/msr-cli "$stage/bin/"
cp -a "$prefix"/include/msr "$stage/include/"
cp -a "$prefix"/lib/cmake/msr "$stage/lib/cmake/"
cp -a sdk/examples/minimal.cpp sdk/examples/CMakeLists.txt "$stage/examples/"
cp -a docs/FRAMEWORK.md docs/setup-fedora.md docs/setup-lubuntu.md \
       LIMITATION.md README.md "$stage/docs/"

cat > "$stage/install.sh" <<'INSTALL'
#!/usr/bin/env sh
# Optional system install. Without it, use LD_LIBRARY_PATH=./lib ./bin/msr-cli.
set -eu
PREFIX="${1:-/usr/local}"
install -d "$PREFIX/bin" "$PREFIX/lib" "$PREFIX/include" "$PREFIX/lib/cmake/msr"
cp -a bin/msr-cli "$PREFIX/bin/"
cp -a lib/libmsr_engine.so* "$PREFIX/lib/"
cp -a include/msr "$PREFIX/include/"
cp -a lib/cmake/msr/. "$PREFIX/lib/cmake/msr/"
ldconfig "$PREFIX/lib" 2>/dev/null || true
echo "installed to $PREFIX"
INSTALL
chmod +x "$stage/install.sh"

cat > "$stage/README.txt" <<EOF
Mobility Engine SDK ${version} — ${distro}/${arch}

Run without installing:
  LD_LIBRARY_PATH=./lib ./bin/msr-cli <model.gguf> "The capital of France is" 8 1024 4 4 1

Install system-wide (root):
  ./install.sh /usr/local

Use from CMake:
  find_package(msr REQUIRED CONFIG)
  target_link_libraries(my_app PRIVATE msr::engine)

Contents: bin/msr-cli, lib/libmsr_engine.so*, include/msr/engine/engine.h,
lib/cmake/msr/, examples/, docs/ (API, setup guides, limitations).
Model weights are NOT included; supply your own GGUF.
The engine forces no whole-file mmap; budget comes from msr_engine_config.
EOF

mkdir -p "$PWD/dist"
name="msr-${distro}-${arch}-v${version}.tar.gz"
tar -czf "$PWD/dist/$name" -C "$stage" .
(cd "$PWD/dist" && sha256sum "$name" > "$name.sha256")
echo "created dist/$name"
cat "$PWD/dist/$name.sha256"
