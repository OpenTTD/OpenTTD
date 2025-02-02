# If you change this version, change the numbers in .github/workflows/ci-emscripten.yml (2x)
# and .github/workflows/preview-build.yml (2x) too.
FROM emscripten/emsdk:3.1.57

RUN apt-get update \
    && apt-get install -y gcc-12 g++-12 \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 100 \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

COPY ports/liblzma.py /emsdk/upstream/emscripten/tools/ports/contrib/liblzma.py
