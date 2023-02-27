FROM emscripten/emsdk:3.1.28

COPY emsdk-liblzma.patch /
RUN cd /emsdk/upstream/emscripten && patch -p1 < /emsdk-liblzma.patch
