#!/usr/bin/env bash
# Enable swissquote systemd services (dev layout, after deploy.sh).
set -eu
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
sudo install -m 644 "${REPO_ROOT}/systemd/dev/swissquote-feed.service" /etc/systemd/system/
sudo install -m 644 "${REPO_ROOT}/systemd/dev/swissquote-writer.service" /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now swissquote-feed.service
sudo systemctl enable --now swissquote-writer.service
