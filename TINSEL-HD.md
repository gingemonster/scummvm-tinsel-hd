# ScummVM Tinsel HD proof of concept

This branch adds an optional 2× graphics layer to the PC Discworld II Tinsel
engine. Original game files, logic, audio, music, and BMV cutscenes remain the
base. A separate `hd/` directory beside the game's `INDEX` file supplies PNG
replacements by eight-digit IMAGE handle. Missing or invalid replacements
fall back to original artwork. Movies are shown at the larger output size
without replacement.

The native game state remains 640×432; output is 1280×864 (or letterboxed in
1280×960), and mouse coordinates are scaled back to native space. The add-on
is enabled when `hd/` exists in a detected Discworld II game directory and
ScummVM was built with PNG support. Files may live in `hd/backgrounds/`,
`hd/sprites/`, `hd/ui/`, `hd/fonts/`, or directly in `hd/`. A matching PNG
must be exactly twice the source IMAGE width and height. PNG alpha is
composited over preceding layers; glyph masks in `hd/fonts/` inherit the
game's current text color. Restart after changing assets because lookup
results are cached.

To build on macOS from the repository root:

```sh
./configure --disable-all-engines --enable-engine=tinsel \
  --disable-debug --enable-release
make -j8 scummvm
./scummvm --auto-detect --no-fullscreen --path='/path/to/dw2'
```

Use this build's `./scummvm`, not an installed release. The separate asset
project holds original extracted images, an `hd/` folder for replacement
PNGs, and the `tinsel_utils` extraction CLI. Copy that project's `hd/`
directory into the game folder to test the remaster. This remains a proof of
concept; all scenes and drawing modes have not been verified.
