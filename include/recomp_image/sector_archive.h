/* Bounded SGCDIMG1 raw-sector image reader. The FILE handle is host state and
 * stays outside SegaCD device/simulation state. */
#ifndef RECOMP_SECTOR_ARCHIVE_H
#define RECOMP_SECTOR_ARCHIVE_H

#include <stddef.h>
#include <stdint.h>
#include "recomp_image/sector_toc.h"

#ifdef __cplusplus
extern "C" {
#endif
typedef struct RecompSectorArchive RecompSectorArchive;

/* Opens and validates the complete image, including its payload SHA-256.
 * On failure, returns NULL and writes a short error when capacity is nonzero. */
RecompSectorArchive *recomp_sector_archive_open(const char *path, char *error, size_t capacity);
/* Optional complete-image expectation from an import/cache manifest. This is
 * not a verification token: both actual full-image and payload identities are
 * checked on every open, in the same pass. Retains a protected Windows object
 * or private portable snapshot for all later reads; fails closed on ownership
 * failure. NULL expectation retains ordinary open semantics. */
RecompSectorArchive *recomp_sector_archive_open_expected(const char *path,const uint8_t expected[32],
                                       char *error,size_t capacity);
void recomp_sector_archive_close(RecompSectorArchive *image);
/* Optional host execution service. The reader remains a pure portable module
 * with no SDL/thread dependency when no service is installed. try_start must
 * never join/execute inline; busy()==0 publishes completed writes; destroy
 * joins before returning. Replacement/close quiesces the prior service. */
typedef struct {
    void *(*create)(void);
    int (*try_start)(void *,void (*)(void *),void *);
    int (*busy)(void *);
    void (*destroy)(void *);
} RecompSectorArchiveAsyncHost;
void recomp_sector_archive_set_async_host(RecompSectorArchive *,const RecompSectorArchiveAsyncHost *);
void recomp_sector_archive_prefetch_cancel(RecompSectorArchive *);
const RecompImageTOC *recomp_sector_archive_toc(const RecompSectorArchive *image);
/* Returns nonzero on success. On invalid LBA or I/O failure, raw is unchanged. */
int recomp_sector_archive_read(RecompSectorArchive *image, int32_t lba, uint8_t raw[2352]);
const uint8_t *recomp_sector_archive_sha256(const RecompSectorArchive *image);
/* Actual complete-image identity, available only after an expected open. */
const uint8_t *recomp_sector_archive_whole_sha256(const RecompSectorArchive *image);
/* Optional immutable host residency, verified/prefaulted during full validation.
 * NULL means the requested sector is not resident; ordinary reads still work.
 * Pointers remain valid until close. This never advances a drive/device clock. */
const uint8_t *recomp_sector_archive_resident_sector(const RecompSectorArchive *, int32_t lba);
size_t recomp_sector_archive_resident_bytes(const RecompSectorArchive *);
/* Host-only storage and stream observations, excluding the validation pass.
 * Adjacent raw reads preserve FILE read-ahead; resident reads do not seek. */
typedef struct {
    uint64_t raw_reads,resident_reads,host_seeks;
    size_t mapped_view_bytes,private_resident_bytes;
    uint64_t prefetch_jobs,prefetch_hits,prefetch_failures,prefetch_cancels;
    uint64_t sync_read_ns,max_sync_read_ns;
    size_t prefetch_bytes;
    size_t private_snapshot_bytes;
    int whole_identity_checked;
} RecompSectorArchiveStats;
int recomp_sector_archive_host_stats(const RecompSectorArchive *,RecompSectorArchiveStats *);

#ifdef __cplusplus
}
#endif
#endif
