# Sample TTD Opus music set

This directory is only a template for the Ogg Opus music playback path. It does
not contain music files.

The workflow for making a real pack is:

```sh
mkdir -p my-opus-pack
ffmpeg -i "source-track.ogg" -c:a libopus -b:a 160k my-opus-pack/track_01.opus
md5sum my-opus-pack/track_01.opus
```

Then edit `ttd_opus.obm`:

- put the `.opus` filenames into `[files]`
- mark those playlist slots as `opus` in `[formats]`
- add display names in `[names]`
- paste the matching MD5 hashes into `[md5s]`

To try the pack, place the completed folder in an OpenTTD baseset directory and
start OpenTTD with:

```sh
openttd -M sample_opus
```
