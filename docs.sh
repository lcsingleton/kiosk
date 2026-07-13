#!/usr/bin/env bash
# Build the API docs: Doxygen parses the public headers (one project per
# component, docs/Doxyfile.<component>, sharing docs/Doxyfile.common via
# @INCLUDE) into XML, then Sphinx+Breathe (docs/conf.py) render that into
# HTML. Runs in its own container (docker/docs.Dockerfile) — see that file
# for why it's split from build.Dockerfile/lint.Dockerfile.
#
# Doxygen's WARN_AS_ERROR is on, so this fails the same way lint.sh does
# if any public class/method in a component's INPUT is undocumented.
set -euo pipefail

IMAGE=ha-tab-kiosk-docs

echo "== building image =="
docker build -f docker/docs.Dockerfile -t "$IMAGE" .

echo "== doxygen (headers -> XML, one project per component) =="
docker run --rm \
  -v "$(pwd):/home/kiosk/kiosk" \
  "$IMAGE" \
  bash -c 'mkdir -p docs/_doxygen && for f in docs/Doxyfile.*; do [ "$f" = docs/Doxyfile.common ] && continue; echo "-- $f --"; doxygen "$f"; done'

echo "== sphinx (XML -> HTML) =="
docker run --rm \
  -v "$(pwd):/home/kiosk/kiosk" \
  "$IMAGE" \
  sphinx-build -W -b html docs docs/_build/html

echo "== docs at docs/_build/html/index.html =="
