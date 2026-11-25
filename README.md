# Filesystem-VMFS6

## 1. Purpose and format scope

VMware VMFS6, distinct from VMFS3 and VMFS5. This repository is the designated independent implementation home for InfiltratorOS. **Documentation and migration scaffold only: no independent runtime provider or release is present.**

## 2. On-disk architecture and invariants

VMFS6-specific extent and volume geometry, metadata records, namespace, allocation, locking and journal state require a dedicated decoder. Do not reuse earlier VMFS assumptions without validated evidence. These are design requirements for the native implementation, not claims about code already present in this repository.

## 3. Common V2 implementation and capabilities

No local Common V2 implementation or advertised operation set has yet been qualified. The target entry point is `infiltrator_filesystem_api_v2`, using bounded, partition-relative host I/O. Implement native probe first, then mount, stat, enumeration, read and volume query where applicable. Consumers must negotiate supported groups and profiles; an unsupported operation must not report success or delegate to a private driver.

## 4. Integrity, mutation, allocation and recovery

Mutation, formatting, check/repair and DEFRAG map/plan/execute/verify/recover require format-specific allocation and ordering invariants, exclusive writable host access and demonstrated interruption recovery. Until qualified, reject them before writing. Shared-disk mutation additionally requires independently qualified distributed locking and ownership. Preserve native disk semantics and do not add a kernel or FUSE fallback as an alternative filesystem authority.

## 5. Build, installation and release

The target installation is `/System/Filesystems/VMFS6.so` and `/System/Resources/Documentation/Filesystem-VMFS6.pdf`. Neither file is currently distributed by this repository. Once the implementation qualifies, the intended release is a portable amd64 DEB and CPU-native source-first RUN, with GitHub's automatic source archive. No install or build command is advertised before an executable build exists.

## 6. Qualification and limitations

Independent VMFS6 images; reject VMFS3/5, incomplete extents and inconsistent lock state. The central entry is design-only, not an implementation. External vmfs6-tools may serve as independent read-only evidence. Compare independent payloads and unchanged-image hashes; inject truncation, corruption and I/O faults. Establish exact-revision ABI, hosted and import-free freestanding qualification, then independently test BOOT and kernel integration. No local build, runtime or packaging qualification is claimed.

## 7. Technical references and provenance

- [Pinned central source baseline](https://github.com/Infiltrator-Projects/Filesystems/tree/101026021d9088d80b7f0f472d9061f0e2c688a3/native/filesystems/vmfs6)
- [Native design](native/filesystems/vmfs6/DESIGN.md)
- [Migration and qualification plan](MIGRATION_STATUS.md)
- [Project philosophy](docs/PHILOSOPHY.md)
- [Common ABI](https://github.com/Infiltrator-Projects/Libraries)

Preserve inherited notices and licences on source extraction; new project-authored code uses GPL-3.0-or-later. The historical original-format reference and installed PDF manual remain pending. Central code or test results are not evidence of an independent provider.
