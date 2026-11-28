# Filesystem-VMFS6

Independent native Common V2 offline reader development. VMFS6-specific 64-bit direct block IDs and paged directory layout. The reader bootstraps the actual FDC and FBB metadata files, reconstructs file-block allocation and reconciles descriptor identities before serving CORE stat/list/read. Only bounded single-extent direct file blocks and inline content are implemented; subblock and indirect schemes, RDM, multi-extent, locked metadata, mutation and recovery are refused. Structural fixtures only: no authentic VMFS6 medium is available, so this remains unqualified development code. No physical BOOT or kernel qualification. The current v0.0.1 release remains documentation-only pending exact-source runtime qualification.

[Manual](docs/VMFS6.pdf) · [Technical reference](docs/VMFS6-TECHNICAL-REFERENCE.md) · [Migration status](MIGRATION_STATUS.md) · [Common V2](https://github.com/Infiltrator-Projects/Libraries)

## Independent-media qualification

`make check-real IMAGE=/path/to/partition.img IMAGE_SHA256=<capture-hash> PAYLOAD_PATH=/directory/file EXPECTED=/path/to/original-copy` checks the exact externally captured partition hash, both native builds, mount and byte-for-byte payload reads. It does not modify the image. Record ESXi format version, capture provenance and the independently exported file before running it. The inherited upstream `test.img` was introduced in the original VMFS-tools lineage (commit `6883fc4f520e836dd11cdf4a6ccc0abc6fb82ab2`), prior to VMFS6; it is not independent VMFS6 evidence. Structural fixtures now additionally test physical MDH self-addresses after catalogue bootstrap and stale zero-marked free-block pointers. This target has not yet been executed against genuine VMFS6 media.

Additional structural regressions exercise a second directory page, a fragmented two-block file, sparse direct mappings and inline payload. Directory self identity, dedicated dot slots and allocation first-free hints are validated. These are not substitutes for the required independent VMFS6 image and exported payload.
