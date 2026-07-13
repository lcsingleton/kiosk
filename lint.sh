#!/usr/bin/env bash
# Static analysis pass over the C++ sources: clang-format --dry-run,
# clang-tidy (incl. -Wdocumentation, since docs/ now has real Doxygen
# comments to validate), clazy (Qt-specific checks), and cppcheck. Runs in
# its own container (docker/lint.Dockerfile) so the compile image (docker/
# build.Dockerfile) doesn't carry tooling it never needs to actually build.
#
# Configures+builds into ./build-lint rather than reusing ./build: clang-tidy
# and clazy need Qt's AUTOMOC-generated moc_*.cpp files to exist, and a
# compile_commands.json (CMAKE_EXPORT_COMPILE_COMMANDS) to know how each
# file was compiled — neither of which build.sh's ./build tree guarantees.
#
# Every check runs even if an earlier one finds violations — each is
# independent, so stopping at the first would hide the rest. Exit status
# reflects whether ANY check failed.
set -uo pipefail

IMAGE=ha-tab-kiosk-lint
FAILED=()

run_check() {
  local name="$1"
  shift
  echo "== $name =="
  if ! docker run --rm -v "$(pwd):/home/kiosk/kiosk" "$IMAGE" bash -c "$1"; then
    FAILED+=("$name")
  fi
}

echo "== building image =="
docker build -f docker/lint.Dockerfile -t "$IMAGE" . || exit 1

echo "== configuring + building (compile_commands.json + generated moc files) =="
docker run --rm \
  -v "$(pwd):/home/kiosk/kiosk" \
  "$IMAGE" \
  bash -c "cmake -B build-lint -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && cmake --build build-lint -j\$(nproc)" || exit 1

FIND_SOURCES='find . -path "./build-lint" -prune -o -path "./build" -prune -o \( -name "*.cpp" -o -name "*.h" \) -print'
FIND_CPP='find . -path "./build-lint" -prune -o -path "./build" -prune -o -name "*.cpp" -print'

run_check "clang-format --dry-run" \
  "$FIND_SOURCES | xargs clang-format --dry-run --Werror"

run_check "clang-tidy" \
  "$FIND_CPP | xargs -P\$(nproc) -I{} clang-tidy -p build-lint --extra-arg=-Wdocumentation --extra-arg=-Wdocumentation-pedantic {}"

run_check "clazy" \
  "$FIND_CPP | xargs -P\$(nproc) -I{} clazy-standalone -p build-lint {}"

run_check "cppcheck" \
  "cppcheck --enable=warning,performance,portability --inline-suppr --project=build-lint/compile_commands.json"

echo
if [ "${#FAILED[@]}" -eq 0 ]; then
  echo "== all checks passed =="
else
  echo "== failed: ${FAILED[*]} =="
  exit 1
fi
