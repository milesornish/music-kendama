#!/usr/bin/env bash
# selftest.sh — prove the pre-commit gate blocks secrets/artifacts and allows templates/source.
# Runs in a throwaway temp repo; never touches the real one. Exit non-zero if any case misbehaves.
set -uo pipefail
HOOK_DIR="$(cd "$(dirname "$0")" && pwd)"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
cd "$TMP"
git init -q
git config user.email t@example.com
git config user.name test
git config core.hooksPath "$HOOK_DIR"

pass=0; fail=0
# case <name> <file> <content> <expect: block|allow>
run() {
  local name=$1 file=$2 content=$3 expect=$4 got
  mkdir -p "$(dirname "$file")" 2>/dev/null || true
  printf '%s\n' "$content" > "$file"
  git add -f "$file" >/dev/null 2>&1
  if "$HOOK_DIR/pre-commit" >/dev/null 2>&1; then got=allow; else got=block; fi
  git reset -q >/dev/null 2>&1; rm -f "$file"
  if [ "$got" = "$expect" ]; then printf '  PASS  [%s]  %s\n' "$got" "$name"; pass=$((pass+1))
  else printf '  FAIL  expected %s got %s  %s\n' "$expect" "$got" "$name"; fail=$((fail+1)); fi
}

echo "git-hooks selftest:"
# should BLOCK
run "secrets.h filename"        "secrets.h"          "static const char k[]=\"x\";" block
run ".env.local filename"       ".env.local"         "TOKEN=abc"                    block
run "build artifact (.bin)"     "build/firmware.bin" "binary-ish"                   block
run "AWS key in content"        "notes.txt"          "key = AKIA0000000000000000"   block
run "GitHub token in content"   "notes.txt"          "ghp_000000000000000000000000000000000000" block
run "private key block"         "notes.txt"          "-----BEGIN RSA PRIVATE KEY-----" block
run "hardcoded password"        "config.py"          "password = 'hunter2hunter2'"  block
# should ALLOW
run "secrets.h.template"        "secrets.h.template" "static const char k[]={0x00};" allow
run ".env.local.example"        ".env.local.example" "# TOKEN=your-token-here"       allow
run "ordinary source"           "src/main.cpp"       "int main(){return 0;}"         allow

echo "  ---"
echo "  $pass passed, $fail failed"
[ "$fail" -eq 0 ]
