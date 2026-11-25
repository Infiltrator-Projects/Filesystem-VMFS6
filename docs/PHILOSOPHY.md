# Engineering Philosophy

Filesystem-VMFS6 adopts the [Infiltrator Engineering Philosophy](https://github.com/Infiltrator-Projects/Libraries/blob/main/docs/PHILOSOPHY.md). Engineering excellence, reliability, disciplined simplicity and postgraduate-level reasoning govern source and documentation.

## Ownership and ABI

One format, one native C/C++ engine, one distributable VMFS6.so. Common owns the [V2 ABI](https://github.com/Infiltrator-Projects/Libraries/blob/main/docs/FILESYSTEM_COMMON_ABI_V2.md). BOOT, Kernel, Files, Disks, Setup, Test Media and Defragmenter consume it; no legacy ABI, FUSE, duplicate parser or separate format driver. Hosted and freestanding profiles require independent validation.

## Integrity and failure

Validate geometry, features, metadata, arithmetic and bounds. Specify locking, transaction ordering, durability and recovery. Unsupported operations fail closed without media mutation. Preserve inherited source and attribution.

## Evidence and craftsmanship

Use independent media, malformed fixtures, fault injection and packaged-binary tests. Compilation and ABI recognition do not prove integrity or bootability. Single main branch; generic amd64 .deb, source-downloading CPU-native .run and GitHub-generated source ZIP only. No Python in new implementation or installer. Maintain concise Markdown and an accurate illustrated PDF manual as capabilities are qualified.
