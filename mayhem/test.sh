#!/usr/bin/env bash
#
# mayhem/test.sh — runs the upstream Catch2 suite (flax_upstream_tests) and the known-answer flax_selftest, both built by mayhem/build.sh.
# exit 0 = pass. EDIT per repo. PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

# flax_upstream_tests is Source/Engine/Tests/{TestGuid,TestString,TestStringUtils,TestCollections,TestMath,
# TestTime}.cpp (upstream Catch2, unmodified) linked against the engine sources. Its XML report gives the
# test-case counts. All 15 upstream test cases must run and pass; a binary that prints nothing, exits 0
# early or runs fewer cases fails here, and a missing binary is a failure, never a skip.
UP="$SRC/flax_upstream_tests"
up_passed=0; up_failed=0
if [ ! -x "$UP" ]; then
  echo "FAIL: $UP missing - mayhem/build.sh did not produce it" >&2
  up_failed=1
else
  up_xml=$("$UP" -r xml 2>&1); up_rc=$?
  cases=$(printf '%s\n' "$up_xml" | grep '<OverallResultsCases ' | tail -1)
  up_passed=$(printf '%s' "$cases" | sed -n 's/.*successes="\([0-9]*\)".*/\1/p')
  up_failed=$(printf '%s' "$cases" | sed -n 's/.*failures="\([0-9]*\)".*/\1/p')
  up_passed=${up_passed:-0}; up_failed=${up_failed:-0}
  [ "$up_rc" -eq 0 ] || { up_failed=$((up_failed + 1)); echo "FAIL flax_upstream_tests: rc=$up_rc" >&2; }
  up_cases=$((up_passed + up_failed))
  if [ "$up_cases" -lt 15 ]; then
    up_failed=$((up_failed + 1)); echo "FAIL flax_upstream_tests: expected >=15 test cases, saw $up_cases" >&2
  fi
fi

# flax_selftest links the engine's real Base64/Encryption, Guid/Version/StringUtils, JsonTools,
# TextProcessing, Variant stream reader and Visject graph loader (normal -O2 flags, no sanitizers) and
# checks each against answers computed independently of the engine (RFC 4648 vectors, hand-built
# binary graphs and streams, literal JSON). It prints "PASS <name>" / "FAIL <name>" per check. Every
# check name below must appear as a PASS line: a binary that merely exits 0, prints nothing, or drops
# checks fails here, and a missing binary is a failure, never a skip.
BIN="$SRC/flax_selftest"
passed=0; failed=0

if [ ! -x "$BIN" ]; then
  echo "FAIL: $BIN missing - mayhem/build.sh did not produce it" >&2
  emit_ctrf "flax_upstream_tests+flax_selftest" "$up_passed" $((up_failed + 1)) 0
  exit $?
fi

out=$("$BIN" 2>&1); rc=$?
[ "$rc" -eq 0 ] && passed=$((passed + 1)) || { failed=$((failed + 1)); echo "FAIL flax_selftest: rc=$rc" >&2; }

# expect <check name> <minimum number of PASS lines carrying that name>
expect() {
  local n
  n=$(printf '%s\n' "$out" | grep -Fxc -- "PASS $1")
  if [ "$n" -ge "$2" ]; then
    passed=$((passed + 1))
  else
    failed=$((failed + 1)); echo "FAIL $1: expected >=$2 PASS line(s), saw $n" >&2
  fi
}

expect base64-encode 4
expect base64-decode 4
expect base64-length 1
expect encrypt-kat 1
expect decrypt-roundtrip 1
expect guid-parse-n 1
expect guid-parse-bad 1
expect guid-tostring 1
expect version-parse 1
expect version-parse-short 1
expect parse-int 1
expect parse-hex 1
expect parse-hex-bad 1
expect parse-float 1
expect json-parse 1
expect json-guid 1
expect json-changeids 1
expect json-changeids-untouched 1
expect json-merge 1
expect text-tokens 1
expect text-lines 1
expect variant-int 1
expect variant-float 1
expect variant-string 1
expect variant-bool 1
expect graph-load 1
expect graph-nodes 1
expect graph-types 1
expect graph-values 1
expect graph-boxes 1
expect graph-connection 1
expect graph-bad-magic 1
expect graph-bad-terminator 1

if printf '%s\n' "$out" | grep -q '^FAIL '; then
  failed=$((failed + 1)); printf '%s\n' "$out" | grep '^FAIL ' >&2
fi
expect_summary=$(printf '%s\n' "$out" | grep -Fx 'SUMMARY passed=39 failed=0')
if [ -n "$expect_summary" ]; then passed=$((passed + 1)); else failed=$((failed + 1)); echo "FAIL summary: expected 'SUMMARY passed=39 failed=0'" >&2; fi

emit_ctrf "flax_upstream_tests+flax_selftest" $((passed + up_passed)) $((failed + up_failed)) 0
