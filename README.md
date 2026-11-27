# recomp-image-core

Current licensing and the grant for existing owner contributions are documented in [LICENSE-SCOPE.md](LICENSE-SCOPE.md).

Reusable host-native, read-only disc-image library. No console runtime or game inputs are required to build it. Rich native profiles retain their source geometry, identities, cached views and filesystem metadata.

## Build

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure --timeout 0
```

The pinned libchdr source archive builds offline. `RECOMP_IMAGE_CHDR_SOURCE_DIR` can select an engine's existing qualified decoder source pin; the provider requires decoded-block CRC checking, raw sector data, subcode and the in-tree dr_flac backend. An existing `chdr-static` target must satisfy the same contract. Link `recomp-image-core::core` for the original reader, `::chd` for verified 2448-byte CHD frames, `::opera` for Opera volumes, `::cue` for cached CUE/ISO views or `::xdvdfs` for bounded byte/XDVDFS sources. Source consumers use `add_subdirectory`; no installed binary ABI is promised.

## Interface

C++: `recomp_image/image_reader.hpp` provides instance-owned `ImageReader` with the extracted mount/sector/track and ISO9660 methods. `recomp_image/cue_sheet.hpp` exposes the reusable CUE parser. C: `recomp_image/image.h` provides opaque lifecycle, track, sector-view and identity operations with exception containment. One owner/thread per reader; different instances are independent. No global mount or writable disk handle exists.

Supported: cooked 2048-byte ISO; raw 2352-byte BIN/IMG; single-/multiple-BINARY-file CUE, INDEX00/01, virtual PREGAP/POSTGAP and CDDA; libchdr-supported CD CHD track modes; generated subchannel Q and SBI replacements inherited from the donor. Raw Mode1 user data starts at 16, Mode2 at 24. Additional views expose Mode2 body (2336) and Form2 payload (2324) when raw geometry permits. Raw audio follows the donor little-endian PCM contract; CHD CDDA is byte-swapped accordingly.

The original reader's ISO9660 volume/root lookup, root files and one subdirectory level retain donor behavior. Richer profiles are separate interfaces below. Rock Ridge, Joliet, console authentication and disc-controller timing are not supplied. MODE2/2336 backing files and WAVE/MP3 CUE payloads are explicitly unsupported by the original reader. Parent CHDs need unavailable parent data and are rejected.

`chd_cd.hpp` supplies the verified single plain MODE1_RAW CHD-v5 profile: 2448-byte frames, bounded hunk cache, raw/combined SHA-1 metadata, parent rejection, track/padding validation and complete raw/subcode bytes. A borrowed hunk view lasts until another hunk is decoded or the source closes. `opera_volume.hpp` supplies recursive bounded read-only Opera volume indexing, stable file IDs, identity and byte/block reads; Portfolio items, scheduling and boot selection stay with the caller.

`cached_cue.hpp` supplies a single session of one BINARY file per track with embedded pregaps, complete-track caching, raw and MODE1 reads, and paged ISO9660 directories retaining attributes, XA unit/gap/number and self/parent records. A raw view is null before caching and immutable until that mounted owner closes; later reads and cache calls do not invalidate it. Saturn boot fields, first-read selection and FAD conversion stay in the caller.

`byte_source.hpp` and `xdvdfs.hpp` supply bounded physical byte reads and root-file lookup across compact/full XDVDFS partition locations. Physical source size is separate from the returned logical file length. `python/recomp_image/xdvdfs.py` supplies the tools-side read-only volume/index API with safe names, nested traversal, extent-overlap/cycle limits and source-change detection. The caller selects the executable and owns title admission, import-cache publication and progress/UI.

Images can mount without ISO9660, which permits sector access to other console formats. Access outside the logical disc or an undersized explicit-view buffer fails. Directory reads reject invalid record names and geometry before allocation (64 MiB directory resource guard). CHD hunk storage is capped at 16 MiB. Partial trailing sectors remain unaddressable as in the donor. No inference from file length overrides declared CUE track geometry.

## Identity-v1

`Identity`/`recomp_image_identity` explicitly reads the complete mounted image; it is never an automatic mount cost. It computes a SHA-256 chain with domain `recomp-image-identity-v1`. Starting digest is SHA256(domain). Each append is SHA256(previous_digest || SHA256(bytes) || uint32_le(length)). First append 4-byte LE values individually: sector count, track count, then number/start/pregap/audio for each track. For every LBA append the raw 2352-byte view when available, otherwise its cooked 2048 bytes, then the 12-byte Q packet plus one validity byte. No path or modification-time trust cache contributes. Output is published only on complete success. This is a versioned mounted-media identity, not a standard whole-file SHA256 or a promise that cooked and raw containers have equal identity.

## Validation and limits

Synthetic source-only tests cover original reader geometry, mixed tracks, stored/virtual gaps, SBI Q, ISO9660 lookup, raw Mode1, Mode2 Form2, identity repeatability/change, unchanged source bytes, C interoperability, malformed directories, invalid indices and an uncompressed CHD-v5 fixture. These are component results, not console boot/gameplay or real-compressed-CHD acceptance. Preserve donor licenses and see `PROVENANCE.md`.
