# Lost Odyssey RGH save converter

This is a standalone, browser-only converter for importing a supported Xbox 360/RGH Lost Odyssey save into Lost Odyssey Recomp. It accepts an original STFS `CON` file (often named `user00`) or a ZIP containing one, then downloads a ZIP containing `save/userNN/save.bin`, `.lo-content`, an optional thumbnail, `conversion.json`, and `README.txt`.

The page processes the selected bytes locally. It makes no upload or network request and has no third-party dependency. The converter preserves the 206,000-byte `save.bin` payload byte-for-byte. It changes only the local slot-listing metadata prefix when the source display name contains a two-digit slot number. The output is for RGH → PC import only; it does not rebuild or export an Xbox 360 container.

The supported input is the verified Lost Odyssey saved-game STFS layout: title ID `4D5307FA`, content type 1, one root `save.bin`, and the known 206,000-byte `LOSV` version-3 payload. The converter checks the STFS header, active hash tree and referenced block SHA-1 values, follows allocation chains, validates ZIP stored/deflate entries and CRCs, rejects traversal or ambiguous ZIP inputs, and limits input to 16 MiB. Unknown, corrupt, multi-file, or otherwise unsupported layouts stop with an error and are not partially converted. Select an empty destination slot from `user00` through `user29`; the tool never overwrites an existing local folder because it only creates a download.

## Use the page

Open the published Pages URL:

`https://freefrank.github.io/LostOdysseyRecomp/`

Choose the RGH file or its ZIP, select an empty slot, click **Convert save**, and download the result. Close the game and back up `save/`. On Windows portable installs, extract the ZIP beside the game executable. On Linux, copy its `userNN` folder into the game's `save/` folder; see the [installation guide](https://github.com/freefrank/LostOdysseyRecomp/blob/main/docs/INSTALLING.md#file-locations) for its location.

The Pages publication is separate from the game release version. Publish the contents of `web/save-converter/` from source branch `trail/issue-88-rgh-save-import` into the root of `trail/save-converter-pages`; do not publish the repository root. The page can also be served locally from the repository:

```powershell
python -m http.server --directory web/save-converter
```

Open the displayed localhost URL. Serving the directory supplies the correct JavaScript module MIME type; opening `index.html` directly may be blocked by browser module security.

## Development checks

The converter is plain browser JavaScript. Node.js 22 or newer can run its bounded test suite:

```powershell
node --test tools/tests/save_converter_test.mjs
```

The real RGH sample is private and must not be committed. To run sample-backed checks locally, set `RGH_SAMPLE_PATH` to its path. The test suite also covers synthetic active-copy data, a fragmented chain crossing 170 blocks, ZIP compression and CRC handling, traversal/ambiguity rejection, and the verified Issue #88 sample when available.

For the format boundary and the earlier isolated runtime load evidence, see [Issue #88 research](../../docs/notes/issue-88-rgh-save-import.md) and [GitHub Issue #88](https://github.com/freefrank/LostOdysseyRecomp/issues/88). The isolated `v0.7.15` load of the supplied sample established payload compatibility for that sample; it did not establish full gameplay, cross-region compatibility, console round-trip support, or general online-conversion acceptance.
