#!/usr/bin/env bash
# scan-lib.sh — shared secret/artifact scanner. SOURCED by the git hooks (not run directly).
# Populates the FINDINGS array; callers run report_findings to print + decide pass/fail.
# Written for bash 3.2 (stock macOS): no mapfile, no ${var,,}, no associative arrays.

FINDINGS=()

_is_template() { case "$1" in *.template|*.example) return 0 ;; *) return 1 ;; esac ; }

# (a) secret-like filenames + (b) build artifacts. Args: file paths.
scan_filenames() {
  local p base
  shopt -s nocasematch
  for p in "$@"; do
    [ -n "$p" ] || continue
    if ! _is_template "$p"; then
      base=${p##*/}
      case "$base" in
        secrets.h|credentials.h|.env|.env.*|*.pem|*.key|*.p12|*.pfx|*.keystore|*.jks|id_rsa*|id_dsa*|id_ecdsa*|id_ed25519*|*.ppk)
          FINDINGS+=("$p"$'\t'"secret-like filename — should never be committed") ;;
      esac
    fi
    case "$p" in
      *.pio/*|*__pycache__/*|*node_modules/*|*.elf|*.bin|*.hex|*.o|*.pyc|bridge/music-kendama-bridge|*.DS_Store)
        FINDINGS+=("$p"$'\t'"build artifact / generated file — keep it out of git") ;;
    esac
  done
  shopt -u nocasematch
}

# High-signal secret patterns (parallel arrays). The literal pattern text is self-safe:
# e.g. "AKIA[0-9A-Z]{16}" has "[" right after AKIA, so it does not match its own regex.
_CONTENT_PATS=(
  '-----BEGIN [A-Z ]*PRIVATE KEY-----'
  'AKIA[0-9A-Z]{16}'
  'ghp_[A-Za-z0-9]{36}'
  'github_pat_[A-Za-z0-9_]{20,}'
  'gh[ours]_[A-Za-z0-9]{30,}'
  'AIza[0-9A-Za-z_-]{35}'
  'xox[baprs]-[A-Za-z0-9-]{10,}'
  "(api[_-]?key|password|passwd|secret|token)[[:space:]\"':=]+[A-Za-z0-9_./+=-]{12,}"
)
_CONTENT_DESC=(
  'private key block'
  'AWS access key id'
  'GitHub personal access token'
  'GitHub fine-grained PAT'
  'GitHub OAuth/app/refresh token'
  'Google API key'
  'Slack token'
  'hardcoded credential (key/password/token = ...)'
)

# From a unified diff on stdin, emit ADDED lines from files that are NOT templates and NOT the
# hook scripts (which legitimately contain the patterns above).
_added_scanlines() {
  awk '
    /^\+\+\+ /  { f=$2; sub(/^b\//,"",f);
                  skip = (f ~ /\.(template|example)$/) || (f ~ /scripts\/git-hooks\//); next }
    /^\+/ && !/^\+\+\+/ { if (!skip) print substr($0,2) }
  '
}

# (c) content scan. Reads a unified diff on stdin (use a here-string, not a pipe, so FINDINGS
# survives — a piped function runs in a subshell).
scan_content() {
  local lines i
  lines=$(_added_scanlines)
  [ -n "$lines" ] || return 0
  for i in "${!_CONTENT_PATS[@]}"; do
    if printf '%s\n' "$lines" | grep -qiE -e "${_CONTENT_PATS[$i]}"; then
      FINDINGS+=("(added content)"$'\t'"possible ${_CONTENT_DESC[$i]}")
    fi
  done
}

# Print findings (if any) + escape-hatch note to stderr. Return 1 if any, 0 if clean.
report_findings() {
  [ ${#FINDINGS[@]} -eq 0 ] && return 0
  {
    echo "✗ blocked by git-hooks/scan — possible secret or artifact:"
    local f
    for f in "${FINDINGS[@]}"; do printf '    %-34s  %s\n' "${f%%$'\t'*}" "${f#*$'\t'}"; done
    echo "  → remove/unstage it, or use a *.template/*.example placeholder."
    echo "  → genuine false positive? bypass with:  git commit --no-verify   (or  git push --no-verify)"
  } >&2
  return 1
}
