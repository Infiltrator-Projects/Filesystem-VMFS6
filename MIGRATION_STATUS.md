# VMFS6 Common V2 migration status

## Source and ownership

[Exact central baseline](https://github.com/Infiltrator-Projects/Filesystems/tree/101026021d9088d80b7f0f472d9061f0e2c688a3/native/filesystems/vmfs6). Design-only central entry; no implementation to extract. External vmfs6-tools may be used as independent read-only evidence, not a target runtime. Establish a documented native implementation with verifiable source provenance; do not claim an existing engine. New project-authored code uses GPL-3.0-or-later. No source transferred by this documentation commit.

## Qualification plan

1. Enumerate supported on-disk variants, validate geometry and features; implement bounded CORE probe.
2. Provide CORE mount/stat/list/read and VOLUME query using native VMFS6-specific extent and volume layout, metadata records, namespace, allocation, locking and journal state.
3. Test independently produced VMFS6 images; reject VMFS3/5, incomplete extents and inconsistent lock metadata; compare independent payloads and unchanged-image hashes; inject malformed media and I/O faults.
4. Implement mutation, format, check/repair and DEFRAG map/plan/execute/verify/recover only with proven allocation, durability and interruption/recovery semantics. Shared-disk writes additionally require independently qualified cluster locking and ownership. Unsupported operations must refuse before writing.
5. Qualify exact packaged Common V2 ABI, hosted and import-free freestanding profiles, then actual BOOT and kernel integration independently.

All independent stages, PDF manual and packaging are pending. No runtime capability is claimed.

The [design and qualification reference](docs/VMFS6.pdf) is available. It is not an operational manual; release and installation qualification remain pending.
