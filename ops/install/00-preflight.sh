#!/usr/bin/env bash
set -eu
# shellcheck source=../common.sh
source "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/common.sh"

if [[ "${EUID}" -ne 0 && "${DRY_RUN}" != "1" ]]; then
  echo "note: mutating epochs require root; epoch 0 is read-only"
fi

if ! command -v id >/dev/null; then
  die "id command missing"
fi

if [[ ! -f /etc/os-release ]]; then
  die "/etc/os-release missing (unsupported OS)"
fi

# shellcheck disable=SC1091
source /etc/os-release
case "${ID:-}" in
  ubuntu | debian) log "OS: ${PRETTY_NAME:-unknown}" ;;
  *) die "unsupported OS: ${ID:-unknown} (need ubuntu/debian)" ;;
esac

df -h / /opt 2>/dev/null || df -h /


for unit in swissquote-feed.service swissquote-writer.service; do
  if systemctl is-active --quiet "${unit}" 2>/dev/null; then
    if [[ "${STAGING}" != "1" && "${FORCE}" != "1" && "${DRY_RUN}" != "1" ]]; then
      die "production unit ${unit} is active — use --staging or --force"
    fi
    log "WARN ${unit} is active"
  fi
done

if ! curl -fsSI --max-time 10 https://github.com >/dev/null 2>&1; then
  die "outbound HTTPS failed (cmake FetchContent needs github.com)"
fi

log "preflight checks passed"
