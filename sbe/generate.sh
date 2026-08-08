#!/usr/bin/env bash
# Regenerate SBE C headers from swissquote/sbe/bbo.xml
#
# Requires: Java 17+, sbe-all jar (default: /tmp/sbe-all.jar)
#
# Usage (from repo root or sbe/):
#   ./sbe/generate.sh
#   SBE_JAR=/path/to/sbe-all.jar ./sbe/generate.sh

set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
SCHEMA="${SBE_SCHEMA:-$DIR/bbo.xml}"
OUT="${SBE_OUT:-$DIR/generated}"
JAR="${SBE_JAR:-/tmp/sbe-all.jar}"

if [[ ! -f "$SCHEMA" ]]; then
  echo "schema not found: $SCHEMA" >&2
  exit 1
fi

if [[ ! -f "$JAR" ]]; then
  echo "downloading sbe-all 1.32.0..."
  curl -fsSL -o "$JAR" \
    "https://repo1.maven.org/maven2/uk/co/real-logic/sbe-all/1.32.0/sbe-all-1.32.0.jar"
fi

mkdir -p "$OUT"
java -Dsbe.output.dir="$OUT" -Dsbe.target.language=C -jar "$JAR" "$SCHEMA"
echo "generated SBE headers in $OUT"
