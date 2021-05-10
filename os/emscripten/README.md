## How to build with Emscripten

Building with Emscripten works with emsdk 2.0.10 and above.

Currently there is no LibLZMA support upstream; for this we suggest to apply
the provided patch in this folder to your emsdk installation.

For convenience, a Dockerfile is supplied that does this patches for you
against upstream emsdk docker. Best way to use it:

Build the docker image:
```
  docker build -t emsdk-lzma .
```

Build the host tools first:
```
  mkdir build-host
  docker run -it --rm -v $(pwd):$(pwd) -u $(id -u):$(id -g) --workdir $(pwd)/build-host emsdk-lzma cmake .. -DOPTION_TOOLS_ONLY=ON
  docker run -it --rm -v $(pwd):$(pwd) -u $(id -u):$(id -g) --workdir $(pwd)/build-host emsdk-lzma make -j5 tools
```

Next, build the game with emscripten:

```
  mkdir build
  docker run -it --rm -v $(pwd):$(pwd) -u $(id -u):$(id -g) --workdir $(pwd)/build emsdk-lzma emcmake cmake .. -DHOST_BINARY_DIR=$(pwd)/build-host -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOPTION_USE_ASSERTS=OFF
  docker run -it --rm -v $(pwd):$(pwd) -u $(id -u):$(id -g) --workdir $(pwd)/build emsdk-lzma emmake make -j5
```

And now you have in your build folder files like "openttd.html".

To run it locally, you would have to start a local webserver, like:

```
  cd build
  python3 -m http.server
````

Now you can play the game via http://127.0.0.1:8000/openttd.html .
