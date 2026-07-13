FROM debian:12-slim

ENV DEBIAN_FRONTEND=noninteractive

# Docs-only image: Doxygen parses the public headers (text-based parsing,
# so unlike lint.Dockerfile it needs no Qt dev headers/compiler at all) and
# emits XML; Sphinx+Breathe render that XML into HTML. Kept separate from
# build.Dockerfile/lint.Dockerfile for the same reason those are split from
# each other — no image carries tooling it doesn't need.
RUN apt-get update && apt-get install -y --no-install-recommends \
    doxygen \
    graphviz \
    python3-sphinx \
    python3-breathe \
    python3-pip \
    && rm -rf /var/lib/apt/lists/*

# Debian 12's apt-packaged python3-breathe (4.34.0) can't render Doxygen's
# "property" member kind (from Q_PROPERTY) natively — fixed on the
# Doxygen side instead, via filter-qt-macros.sh, so apt's breathe is fine
# as-is. sphinx-breeze-theme isn't packaged for Debian, so it's the one
# thing pulled via pip; no need to move the rest of the stack off apt.
RUN pip3 install --break-system-packages --no-cache-dir sphinx-breeze-theme

WORKDIR /home/kiosk/kiosk
