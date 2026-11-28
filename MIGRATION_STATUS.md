# VMFS6 Common V2 migration

Native single-extent VMFS6 geometry, LVM UUID, 8192-byte FDC bitmap and descriptor validation. Genuine VMFS6 media remains unavailable; structural fixtures only.

The native C implementation uses Common V2 bounded host I/O only. It is not a kernel, FUSE or external tool adapter. Test tools are independent oracles only. Constructed metadata fixtures, malformed-media rejection, hosted and import-free builds and sanitizers are defined in CI. Exact-source results establish the qualification boundary. Native namespace, allocation ownership beyond the decoded structures, journal or distributed-lock semantics, writes, repair, formatting and defragmentation remain outstanding. Runtime release is not yet authorized by these checks.
