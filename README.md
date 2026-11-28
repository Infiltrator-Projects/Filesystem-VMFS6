# Filesystem-VMFS6

Independent native Common V2 offline reader development. VMFS6-specific 64-bit direct block IDs and paged directory layout. The reader bootstraps the actual FDC and FBB metadata files, reconstructs file-block allocation and reconciles descriptor identities before serving CORE stat/list/read. Only bounded single-extent direct file blocks and inline content are implemented; subblock and indirect schemes, RDM, multi-extent, locked metadata, mutation and recovery are refused. Structural fixtures only: no authentic VMFS6 medium is available, so this remains unqualified development code. No physical BOOT or kernel qualification. The current v0.0.1 release remains documentation-only pending exact-source runtime qualification.

[Manual](docs/VMFS6.pdf) · [Technical reference](docs/VMFS6-TECHNICAL-REFERENCE.md) · [Migration status](MIGRATION_STATUS.md) · [Common V2](https://github.com/Infiltrator-Projects/Libraries)
