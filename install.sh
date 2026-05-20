#!/usr/bin/env sh
set -eu

REPO="Felpor-cmd/cMonkey"
BINARY_NAME="cmonkey"
INSTALL_DIR="${INSTALL_DIR:-/usr/local/bin}"
VERSION="${CMONKEY_VERSION:-latest}"

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Error: required command not found: $1" >&2
        exit 1
    fi
}

for cmd in curl uname mktemp chmod install mkdir; do
    require_cmd "$cmd"
done

os="$(uname -s)"
arch="$(uname -m)"

case "$os" in
    Linux) os="linux" ;;
    Darwin) os="macos" ;;
    *)
        echo "Error: unsupported operating system: $os" >&2
        exit 1
        ;;
esac

case "$arch" in
    x86_64|amd64) arch="x86_64" ;;
    arm64|aarch64) arch="arm64" ;;
    *)
        echo "Error: unsupported architecture: $arch" >&2
        exit 1
        ;;
esac

asset="${BINARY_NAME}-${os}-${arch}"
if [ "$VERSION" = "latest" ]; then
    download_url="https://github.com/${REPO}/releases/latest/download/${asset}"
else
    download_url="https://github.com/${REPO}/releases/download/${VERSION}/${asset}"
fi

tmp_bin="$(mktemp)"
cleanup() {
    rm -f "$tmp_bin"
}
trap cleanup EXIT INT TERM

echo "Downloading ${asset}..."
if ! curl -fsSL "$download_url" -o "$tmp_bin"; then
    echo "Error: no prebuilt release for ${os}/${arch} (${VERSION})." >&2
    echo "Build from source instead: make && sudo make install" >&2
    exit 1
fi

chmod +x "$tmp_bin"

sudo_cmd=""
if [ "$(id -u)" -ne 0 ] && [ ! -w "$INSTALL_DIR" ]; then
    if command -v sudo >/dev/null 2>&1; then
        sudo_cmd="sudo"
    else
        echo "Error: write permission denied for ${INSTALL_DIR} and sudo is not installed." >&2
        exit 1
    fi
fi

if [ -n "$sudo_cmd" ]; then
    $sudo_cmd mkdir -p "$INSTALL_DIR"
    $sudo_cmd install -m 0755 "$tmp_bin" "${INSTALL_DIR}/${BINARY_NAME}"
else
    mkdir -p "$INSTALL_DIR"
    install -m 0755 "$tmp_bin" "${INSTALL_DIR}/${BINARY_NAME}"
fi

echo "Installed ${BINARY_NAME} to ${INSTALL_DIR}/${BINARY_NAME}"
