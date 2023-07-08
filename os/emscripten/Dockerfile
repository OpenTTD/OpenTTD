FROM emscripten/emsdk:3.1.42

COPY emsdk-liblzma.patch /
RUN cd /emsdk/upstream/emscripten && patch -p1 < /emsdk-liblzma.patch

COPY emsdk-nlohmann-json.patch /
RUN cd /emsdk/upstream/emscripten && patch -p1 < /emsdk-nlohmann-json.patch
