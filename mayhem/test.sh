#!/usr/bin/env bash
#
# mayhem/test.sh — RUN this repo's OWN functional test suite (already built by mayhem/build.sh).
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


passed=0 failed=0
ok()   { passed=$((passed + 1)); echo "PASS $1"; }
bad()  { failed=$((failed + 1)); echo "FAIL $1"; }

# expect <name> <expected> <actual>: exact string comparison
expect() {
  if [ "$2" = "$3" ]; then ok "$1"; else bad "$1"; echo "  expected: $2"; echo "  actual:   $3"; fi
}

# expect_prefix <name> <expected prefix> <actual>
expect_prefix() { expect "$1" "$2" "${3:0:${#2}}"; }

selftest=mayhem/obj-test/selftest
[ -x "$selftest" ] || { echo "missing $selftest (mayhem/build.sh builds it)" >&2; exit 1; }
[ -x ocat ] || { echo "missing ./ocat (mayhem/build.sh builds it)" >&2; exit 1; }
[ -f etc/demo-iphone.rec ] || { echo "missing etc/demo-iphone.rec" >&2; exit 1; }

# --- selftest: every check must PASS, and exactly the expected number must have run -------------
out=$("$selftest" 2>&1)
printf '%s\n' "$out"
st_pass=$(printf '%s\n' "$out" | grep -c '^PASS ' || true)
st_fail=$(printf '%s\n' "$out" | grep -c '^FAIL ' || true)
expect "selftest ran 18 checks" "SUMMARY: 18 tests run; 0 failed" "$(printf '%s\n' "$out" | grep '^SUMMARY:' || true)"
passed=$((passed + st_pass)); failed=$((failed + st_fail))

# --- ocat over upstream's demo recording (25 lines: 23 locations, 2 other records) ---------------
rec=etc/demo-iphone.rec
range=(-F 2015-08 -T 2015-09)

csv=$(./ocat -f csv "${range[@]}" $rec)
expect "ocat csv: 24 lines (header + 23 locations)" "24" "$(printf '%s\n' "$csv" | wc -l | tr -d ' ')"
expect "ocat csv: header" "tst,acc,_type,alt,lon,vac,vel,lat,cog,tid,batt,ghash,isorcv,isotst,disptst" "$(printf '%s\n' "$csv" | sed -n 1p)"
expect "ocat csv: first record" \
  "1440395361,3000.000000,location,51,10.027857,29.000000,-1,52.378886,-1,NE,96,u1r1upq,2015-08-24T05:55:07Z,2015-08-24T05:49:21Z,2015-08-24 05:49:21" \
  "$(printf '%s\n' "$csv" | sed -n 2p)"

expect "ocat csv --fields in a one-hour window" \
  "tst,lat,lon,tid
1440396651,52.309950,10.755302,NE
1440396961,52.288331,10.862660,NE
1440397999,52.181069,11.411779,NE
1440398690,52.137452,11.537964,NE
1440399078,52.049098,11.622193,NE
1440399566,51.906730,11.687446,NE" \
  "$(./ocat -f csv --fields tst,lat,lon,tid -F 2015-08-24T06 -T 2015-08-24T07 $rec)"

json=$(./ocat -f json "${range[@]}" $rec)
expect_prefix "ocat json: count and first record" '{"count":23,"locations":[{"tst":1440395361,"acc":3000,"_type":"location","alt":51,"lon":10.02785726730668,' "$json"

expect_prefix "ocat geojson: first feature" \
  '{"type":"FeatureCollection","features":[{"type":"Feature","geometry":{"type":"Point","coordinates":[10.02785726730668,52.37888580984668]},"properties":{"name":"NE","vel":-1,"tst":1440395361,"acc":3000,"alt":51,"address":"","isotst":"2015-08-24T05:49:21Z"}},' \
  "$(./ocat -f geojson "${range[@]}" $rec)"

expect_prefix "ocat linestring: starts at the first two points" \
  '{"type":"Feature","properties":{},"geometry":{"coordinates":[[10.02785726730668,52.37888580984668],[10.12589338283843,52.35875498932977],' \
  "$(./ocat -f linestring "${range[@]}" $rec)"

gpx=$(./ocat -f gpx "${range[@]}" $rec)
expect "ocat gpx: 23 track points" "23" "$(printf '%s\n' "$gpx" | grep -c '<trkpt ')"
expect "ocat gpx: first track point" "<trkpt lat='52.378886' lon='10.027857'>" "$(printf '%s\n' "$gpx" | grep '<trkpt ' | sed -n 1p | sed 's/^ *//')"

xml=$(./ocat -f xml "${range[@]}" $rec)
expect "ocat xml: 23 points" "23" "$(printf '%s\n' "$xml" | grep -o '<point>' | wc -l | tr -d ' ')"

expect "ocat: a window with no records yields an empty location list" '{"locations":[]}' \
  "$(./ocat -f json -F 2016-01 -T 2016-02 $rec)"
expect "ocat: bad time is refused" "./ocat: bad time(s) specified" "$(./ocat -f json -F 2020 $rec 2>&1 || true)"

emit_ctrf "recorder-selftest-ocat" "$passed" "$failed" 0
