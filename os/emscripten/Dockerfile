FROM emscripten/emsdk:2.0.31

COPY emsdk-liblzma.patch /
RUN cd /emsdk/upstream/emscripten && patch -p1 < /emsdk-liblzma.patch
