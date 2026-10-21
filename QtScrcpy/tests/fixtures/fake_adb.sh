#!/usr/bin/env bash
set -euo pipefail

dir="${FAKE_ADB_DIR:?FAKE_ADB_DIR is not set}"
key=$(printf '%s' "$*" | sed 's/[ /:]/_/g')

if [[ -f "${dir}/${key}.sleep" ]]; then
  sleep "$(tr -d '[:space:]' < "${dir}/${key}.sleep")"
fi

if [[ -f "${dir}/${key}.txt" ]]; then
  cat "${dir}/${key}.txt"
fi

code=0
if [[ -f "${dir}/${key}.code" ]]; then
  code=$(tr -d '[:space:]' < "${dir}/${key}.code")
fi
exit "${code}"
