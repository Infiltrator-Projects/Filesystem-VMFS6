# VMFS6 Common V2 migration

Native single-extent VMFS6 geometry, LVM UUID, 8192-byte FDC bitmap and descriptor validation. Genuine VMFS6 media remains unavailable; structural fixtures only.

The native C implementation uses Common V2 bounded host I/O only. It is not a kernel, FUSE or external tool adapter. Test tools are independent oracles only. Constructed metadata fixtures, malformed-media rejection, hosted and import-free builds and sanitizers are defined in CI. Exact-source results establish the qualification boundary. Native namespace, allocation ownership beyond the decoded structures, journal or distributed-lock semantics, writes, repair, formatting and defragmentation remain outstanding. Runtime release is not yet authorized by these checks.

## Native direct namespace development

CORE mount/stat/list/read and VOLUME query now implement bounded offline FDC/FBB bootstrap and allocation-checked direct reads. Populated structural fixtures exercise real descriptor and bitmap paths, including refusal of free-block payloads and mismatched bitmap counts. VMFS6 has a distinct 64-bit pointer decoder and page-map reader; genuine VMFS6 media remains a blocker. No runtime publication until independent qualification. Subblock, pointer blocks, distributed locking, mutation, recovery and physical integration are outstanding.

Native metadata identity checks now cover mapped FDC descriptor and bitmap addresses and FBB metadata positions after bootstrap. Zero-marked allocated pointers still require allocation ownership. The `check-real` target requires an external captured VMFS6 partition, its SHA-256 and independently exported payload; no valid external VMFS6 capture has yet been obtained. Runtime publication remains blocked on actual independent qualification.

## Expanded structural qualification

Added native directory dot/self identity and first-free hint checks plus multi-page namespace, fragmented direct read, sparse zero and inline payload fixtures. Real ESXi-created media remains required; no runtime publication or mutation qualification is claimed.

### Mounted volume lifetime

The provider copies the caller's size-versioned host descriptor and assigns each mount an opaque generation. An operation reserves its mount until its callback returns. A concurrent unmount or second operation on that mount returns `IFS_FS_BUSY`. Independent mounts remain usable. Unmount removes the generation before releasing native state, so a closed handle returns `IFS_FS_INVALID` even after a new mount reuses the same memory. The Common V2 regression holds a directory read inside a host I/O callback while a second thread queries and unmounts both volumes.
