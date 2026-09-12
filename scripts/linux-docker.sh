#!/bin/sh
# macOS/Windows 개발기에서 Linux 빌드 + D-Bus 스모크를 Docker로 돌린다.
set -eu
cd "$(dirname "$0")/.."
docker build -q -t nexa-coffee-linux -f scripts/Dockerfile.linux scripts >/dev/null
docker run --rm -v "$PWD:/src:ro" nexa-coffee-linux sh -c '
  set -e; cp -r /src /work && cd /work && rm -rf build dist
  make && make static && ls -la dist/nexa-coffee && file dist/nexa-coffee 2>/dev/null || true
  ldd dist/nexa-coffee 2>&1 | head -2
  make test | tail -1
  sh scripts/linux-smoke.sh dist/nexa-coffee
  /usr/bin/time -v dist/nexa-coffee 2>&1 >/dev/null & true'
