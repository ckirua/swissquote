#!/usr/bin/env bash
# Sync repo Grafana provisioning into system paths and restart grafana-server.
# Run as root on the monitoring host.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
GRAFANA_SRC="${REPO_ROOT}/grafana"
ENV_FILE="${ENV_FILE:-${HOME}/.env}"

if [[ "$(id -u)" -ne 0 ]]; then
  echo "Run as root (needed for /etc/grafana and systemd)." >&2
  exit 1
fi

if [[ ! -f "${ENV_FILE}" ]]; then
  echo "Missing ${ENV_FILE} (CLICKHOUSE_*). Set ENV_FILE=..." >&2
  exit 1
fi

# Plugin (idempotent)
cd /usr/share/grafana
grafana cli plugins install grafana-clickhouse-datasource >/dev/null || true

# Datasource + dashboard providers (readable by grafana user under /etc)
install -d -o root -g grafana -m 0755 /etc/grafana/provisioning/datasources
install -d -o root -g grafana -m 0755 /etc/grafana/provisioning/dashboards
install -o root -g grafana -m 0640 \
  "${GRAFANA_SRC}/provisioning/datasources/clickhouse.yaml" \
  /etc/grafana/provisioning/datasources/swissquote-clickhouse.yaml
install -o root -g grafana -m 0640 \
  "${GRAFANA_SRC}/provisioning/dashboards/dashboards.yaml" \
  /etc/grafana/provisioning/dashboards/swissquote.yaml

# Dashboard JSON (path referenced by dashboards.yaml)
install -d -o grafana -g grafana -m 0755 /var/lib/grafana/dashboards/swissquote
install -o grafana -g grafana -m 0644 \
  "${GRAFANA_SRC}/dashboards/"*.json \
  /var/lib/grafana/dashboards/swissquote/

# Full range = min(ts) → now.
# Default view = min(full history, last 6 months) → now
#   i.e. from = max(min(ts), now-6M)
patch_time_ranges() {
  set -a
  # shellcheck disable=SC1090
  source "${ENV_FILE}"
  set +a
  local host="${CLICKHOUSE_HOST:-127.0.0.1}"
  local port="${CLICKHOUSE_PORT:-9000}"
  local user="${CLICKHOUSE_USER:-default}"
  local pass="${CLICKHOUSE_PASSWORD:-}"
  local row
  row="$(
    clickhouse-client \
      --host "${host}" \
      --port "${port}" \
      --user "${user}" \
      --password "${pass}" \
      --query "
SELECT
  formatDateTime(data_min, '%Y-%m-%dT%H:%i:%S.000Z'),
  formatDateTime(greatest(data_min, now64(3) - INTERVAL 6 MONTH), '%Y-%m-%dT%H:%i:%S.000Z')
FROM (
  SELECT coalesce(
    (SELECT min(day) FROM (
      SELECT toStartOfDay(ts) AS day, count() AS n
      FROM swissquote.bbo
      GROUP BY day
      HAVING n >= 1000
    )),
    (SELECT min(ts) FROM swissquote.bbo)
  ) AS data_min
)
FORMAT TSV" \
      2>/dev/null || true
  )"
  if [[ -z "${row}" ]]; then
    echo "WARN: could not read min(ts) for time ranges; leaving dashboard JSON as-is" >&2
    return 0
  fi
  local min_ts default_from
  min_ts="$(printf '%s\n' "${row}" | cut -f1)"
  default_from="$(printf '%s\n' "${row}" | cut -f2)"
  if [[ -z "${min_ts}" || "${min_ts}" == "1970-01-01T00:00:00.000Z" ]]; then
    echo "WARN: empty/invalid min(ts); leaving dashboard JSON as-is" >&2
    return 0
  fi
  python3 - "${min_ts}" "${default_from}" <<'PY'
import json
import sys
from pathlib import Path

min_ts, default_from = sys.argv[1], sys.argv[2]
dash = Path("/var/lib/grafana/dashboards/swissquote/swissquote-bbo.json")
d = json.loads(dash.read_text())
ranges = d.setdefault("timepicker", {}).setdefault("quick_ranges", [])
found = False
for q in ranges:
    if q.get("display") == "Full range":
        q["from"] = min_ts
        q["to"] = "now"
        found = True
        break
if not found:
    ranges.append({"display": "Full range", "from": min_ts, "to": "now"})
# Default open range: min(full history, 6 months)
d["time"] = {"from": default_from, "to": "now"}
dash.write_text(json.dumps(d, indent=2) + "\n")
print(f"Full range → {min_ts} .. now")
print(f"Default    → {default_from} .. now  (min of full history and 6 months)")
PY
  chown grafana:grafana /var/lib/grafana/dashboards/swissquote/swissquote-bbo.json
}
patch_time_ranges

# Pass ClickHouse creds from ~/.env into grafana-server (systemd reads as root)
install -d -m 0755 /etc/systemd/system/grafana-server.service.d
cat >/etc/systemd/system/grafana-server.service.d/swissquote-clickhouse.conf <<EOF
[Service]
# Loaded by systemd as root; values become process env for \${CLICKHOUSE_*} expansion
# in /etc/grafana/provisioning/datasources/swissquote-clickhouse.yaml
EnvironmentFile=-${ENV_FILE}
EOF

# Prefer localhost bind (SSH tunnel for remote access)
if grep -qE '^;?http_addr\s*=' /etc/grafana/grafana.ini; then
  sed -i 's/^;*\s*http_addr\s*=.*/http_addr = 127.0.0.1/' /etc/grafana/grafana.ini
else
  # Ensure [server] section has http_addr
  if grep -q '^\[server\]' /etc/grafana/grafana.ini; then
    sed -i '/^\[server\]/a http_addr = 127.0.0.1' /etc/grafana/grafana.ini
  fi
fi

systemctl daemon-reload
systemctl enable --now grafana-server
systemctl restart grafana-server

echo "Applied Swissquote Grafana provisioning."
echo "  URL:  http://127.0.0.1:3000/"
echo "  Env:  ${ENV_FILE}"
echo "  DS:   /etc/grafana/provisioning/datasources/swissquote-clickhouse.yaml"
echo "  Dash: /var/lib/grafana/dashboards/swissquote/"
