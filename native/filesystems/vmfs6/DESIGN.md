# VMFS6 — native filesystem design

## Format scope

VMware VMFS6, separate from VMFS3/VMFS5. This is a design contract, not a claim of implemented capabilities.

## On-disk structures and ownership

Establish VMFS6-specific volume/extent geometry, metadata records, namespace and file mapping, allocation, ownership/locking and journal semantics from validated sources. The single native C/C++ engine owns these format semantics; consumers provide bounded Common V2 host services, never competing parsers.

## Integrity and mutation invariants

Never apply earlier VMFS assumptions without version-specific evidence. Refuse incomplete extents and unknown ownership or recovery state; shared-disk mutation requires qualified distributed locking, fencing and crash recovery. All arithmetic, offsets and lengths are checked before I/O. Writable operations require exclusive access, explicit ordering, flush and interruption/recovery evidence. Read-only operations leave the medium unchanged.

## Common V2 boundary

Only `infiltrator_filesystem_api_v2` is public. Target the genuine PROBE/MOUNT/UNMOUNT/STAT/LIST/READ operations first; negotiate actual supported operation masks. VOLUME, METADATA and DEFRAG operations (including MAP, PLAN, EXECUTE, VERIFY, RECOVER) are individually qualified or refused. Test Media uses the same provider's qualified format and namespace operations. No FUSE, legacy ABI, private driver or kernel proxy. Hosted and import-free freestanding profiles share the format engine, with generic environment-specific host services.

## Qualification

Use independently produced VMFS6 images and compatible external read-only evidence, including vmfs6-tools where applicable. Test corrupt metadata, missing extents, dirty ownership and VMFS3/5 rejection. Compare file bytes against an independent oracle, hash unchanged read-only images, inject short I/O, corruption and interruption; validate exact published module separately in BOOT and kernel. Use disposable media only. No new independent binary or test pass is claimed.

## Source and migration

[Central source and historical design, pinned revision](https://github.com/Infiltrator-Projects/Filesystems/tree/101026021d9088d80b7f0f472d9061f0e2c688a3/native/filesystems/vmfs6). The central entry is design-only; acquire and validate appropriately licensed source or implement against documented on-disk evidence. Do not claim that an existing engine has been extracted. New project-authored code uses GPL-3.0-or-later. See [migration status](../../../MIGRATION_STATUS.md), [philosophy](../../../docs/PHILOSOPHY.md) and [Common V2 contract](https://github.com/Infiltrator-Projects/Libraries/blob/main/docs/FILESYSTEM_COMMON_ABI_V2.md). Source migration is a separate step.
