## How to build with Emscripten

Please use docker with the supplied `Dockerfile` to build for emscripten.
It takes care of a few things:
- Use a version of emscripten we know works
- Patch in LibLZMA support (as this is not supported by upstream)

First, build the docker image by navigating in the folder this `README.md` is in, and executing:
```
  docker build -t emsdk-openttd .
```

Next, navigate back to the root folder of this project.

Now we build the host tools first:
```
  mkdir build-host
  docker run -it --rm -v $(pwd):$(pwd) -u $(id -u):$(id -g) --workdir $(pwd)/build-host emsdk-openttd cmake .. -DOPTION_TOOLS_ONLY=ON
  docker run -it --rm -v $(pwd):$(pwd) -u $(id -u):$(id -g) --workdir $(pwd)/build-host emsdk-openttd make -j$(nproc) tools
```

Finally, we build the actual game:
```
  mkdir build
  docker run -it --rm -v $(pwd):$(pwd) -u $(id -u):$(id -g) --workdir $(pwd)/build emsdk-openttd emcmake cmake .. -DHOST_BINARY_DIR=../build-host -DCMAKE_BUILD_TYPE=Release -DOPTION_USE_ASSERTS=OFF
  docker run -it --rm -v $(pwd):$(pwd) -u $(id -u):$(id -g) --workdir $(pwd)/build emsdk-openttd emmake make -j$(nproc)
```

In the `build` folder you will now see `openttd.html`.

To run it locally, you would have to start a local webserver; something like:

```
  cd build
  python3 -m http.server
````

You can now play the game via http://127.0.0.1:8000/openttd.html .
