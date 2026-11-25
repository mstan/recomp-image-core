# Provenance

The image reader and CUE parser are extracted from RetroPortingToolKit/psxrecomp commit `cea1b825e48480530a38815499b8112992a1b2ff`:

- `runtime/include/iso_reader.h`, `runtime/src/iso_reader.cpp`
- `runtime/include/cue_sheet.h`, `runtime/src/cue_sheet.cpp`
- `recompiler/include/host_path.h` (portable path helper)
- `runtime/tests/test_iso_reader_{geometry,cdda,pregap,sbi}.cpp`

They retain the donor PolyForm Noncommercial license and permissions. Changes normalize namespaces and includes, add an independent build/C API, remove the consumer path helper dependency, add bounded directory/image geometry validation and sector views, correct raw Mode1 user offsets, and expose an explicit mounted-image identity. Existing consumer code is untouched.

The SHA-256 block implementation (`src/image_sha256.c`) is the public-domain reference from mstan/vbrecomp commit `f7dd008e09f849ceb9707aac881955b78a3e8c5d`, `runtime/src/vb_sha256.c` and its header, renamed without algorithm changes. Its original public-domain notice is retained.

libchdr source archive is the exact pinned psxrecomp third-party input at upstream commit `6cde5348eb118da3baf94f75a69577a005a484fd`. SHA-256: `a222b1f1c842f988c2aad51e5f56ba94a423ed87ccab71f1abdb757caa7444f5`. The source archive contains its upstream license/notices and dependency licenses. It is source-only, not a generated game artifact.

CD-i, Sega CD, Saturn and 3DO source readers were inspected for overlap and format boundaries. No implementation from those repositories was copied; console filesystems, drive commands and guest scheduling remain outside this core.

New C bindings, contract tests and extraction glue are author-owned under the root license. No game image, firmware, user save or generated guest code is included.
