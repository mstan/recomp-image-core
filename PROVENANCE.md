# Provenance

The image reader and CUE parser are extracted from RetroPortingToolKit/psxrecomp commit `cea1b825e48480530a38815499b8112992a1b2ff`:

- `runtime/include/iso_reader.h`, `runtime/src/iso_reader.cpp`
- `runtime/include/cue_sheet.h`, `runtime/src/cue_sheet.cpp`
- `recompiler/include/host_path.h` (portable path helper)
- `runtime/tests/test_iso_reader_{geometry,cdda,pregap,sbi}.cpp`

They retain the donor PolyForm Noncommercial license and permissions. Changes normalize namespaces and includes, add an independent build/C API, remove the consumer path helper dependency, add bounded directory/image geometry validation and sector views, correct raw Mode1 user offsets, and expose an explicit mounted-image identity. Existing consumer code is untouched.

The SHA-256 block implementation (`src/image_sha256.c`) is the public-domain reference from mstan/vbrecomp commit `f7dd008e09f849ceb9707aac881955b78a3e8c5d`, `runtime/src/vb_sha256.c` and its header, renamed without algorithm changes. Its original public-domain notice is retained.

libchdr source archive is the exact pinned psxrecomp third-party input at upstream commit `6cde5348eb118da3baf94f75a69577a005a484fd`. SHA-256: `a222b1f1c842f988c2aad51e5f56ba94a423ed87ccab71f1abdb757caa7444f5`. The source archive contains its upstream license/notices and dependency licenses. It is source-only, not a generated game artifact.

The accepted `781b4e4407745bfdbbf908164d45e5622ac75ed0` Sega sector-archive/residency implementation is retained unchanged, including its existing attribution and SHA helper. Console drive commands and guest scheduling remain outside the core.

The rich native profiles are extracted from owner-authored sources, with their complete Git history identifying Matthew Stanley as their author. These files use his MIT grant in LICENSES/MATTHEW-STANLEY-MIT.txt; this does not relicense unrelated donor material:

- mstan/3dollr `e5a76a55748f82e4d4893c49fd4ba891b96e827d`: `include/3dollr/{disc,opera_fs}.hpp`, `src/{disc,opera_fs}.cpp`. These become `chd_cd` and `opera_volume`; the donor's PROVENANCE states that its Opera parser was independently authored from documented on-disc layouts, without importing 3dokit implementation. The engine retains its qualified libchdr pin `47d794151b2b64f6aba60304e6480f4f5a2bc24b`; its BSD-3-Clause and permissive bundled decoder notices remain with that source. No Portfolio implementation is imported.
- mstan/saturnllr `a790a06c73321590fa75fc65abcaf17794e8dd80`: generic CUE/cache/raw/MODE1/ISO portions of `engine/disc.cpp` and `include/saturnllr/disc.h` become `cached_cue`; Saturn system-ID/header interpretation, first-read policy and C drive callbacks stay in the engine.
- mstan/xboxllr `3dd76039c7f60a5d3f9695f319c655153c23c523`: owner-authored `pilots/native-launcher/disc_image.h` and generic read-only portions of `tools/native_disc.py` become `byte_source`, `xdvdfs.hpp` and `python/recomp_image/xdvdfs.py`. The donor GPL grant remains available; this extraction selects the owner's additional MIT grant. Xbox executable admission, import-cache publication, native writable disks/FATX and GPL renderer/system code are not imported. Format references describe XDVDFS layouts; no extract-xiso implementation is copied.

New C bindings, contract tests and extraction glue are author-owned under the root license. No game image, firmware, user save or generated guest code is included.
