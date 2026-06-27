# If you change this version, change the numbers in .github/workflows/ci-emscripten.yml (2x)
# and .github/workflows/preview-build.yml (2x) too.
FROM emscripten/emsdk:6.0.1

RUN apt-get update \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

COPY ports/liblzma.py /emsdk/upstream/emscripten/tools/ports/contrib/liblzma.py
