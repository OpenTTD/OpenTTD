# TTD Opus test music set

This is local test content for the Ogg Opus music playback path. The source
files in `ogg/` were Ogg Vorbis, so they were re-encoded to Ogg Opus before
being listed in `ttd_opus.obm`.

To try it from a local dev build, copy this whole directory into the build
baseset directory:

```sh
cp -R test_content/ttd-opus build-devbuild/baseset/
toolbox run -c devbuild ./build-devbuild/openttd -M ttd_opus
```

The workflow for making a similar pack is:

```sh
mkdir -p build-devbuild/baseset/my-opus-pack
n=1
for file in ogg/*.ogg; do
  ffmpeg -y -i "$file" -c:a libopus -b:a 160k \
    "$(printf 'build-devbuild/baseset/my-opus-pack/track_%02d.opus' "$n")"
  n=$((n + 1))
done
md5sum build-devbuild/baseset/my-opus-pack/*.opus
```

Then create an `.obm` next to the `.opus` files with `[files]`, `[formats]`,
`[names]`, and `[md5s]`. Use `opus` in `[formats]` for every sampled track.
