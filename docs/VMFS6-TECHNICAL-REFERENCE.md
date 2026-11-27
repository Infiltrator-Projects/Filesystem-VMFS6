# VMFS6: technical reference

## Scope and version-specific evidence

VMFS6 was introduced with the VMware vSphere 6.5 generation. It extends the clustered datastore design with changes including allocation geometry, addressing and metadata layout. The evidence for this chapter is Weafon Tsao's vmfs6-tool fork, commit 84e094a16f92b1883f16c347ede4c0edadaf5290. Its source explicitly describes limited VMFS6 support. It is a reverse-engineered reader with inherited experimental mutation paths, not a complete vendor specification. Do not apply the older VMFS3/5 descriptor and directory layout to VMFS6 or infer complete compatibility from this source.

VMFS6 was introduced with VMware vSphere 6.5. Earlier VMFS readers are not automatically compatible.

## Physical volume and filesystem information

The observed partition-relative volume information begins at 0x100000, with little-endian magic 0xC001D00D at offset 0 and 32-bit volume version at 4. LUN byte is at 14, 28-byte name at 18, 32-bit physical-size field at 95 and 16-byte volume UUID at 130. The reference describes the physical-size field as scaled by 256; validate its unit against known media. The filesystem address space starts 0x1000000 bytes after the volume-information base. Within the assembled logical volume, filesystem information begins at 0x200000 and has magic 0x2FABF15E. For a single partition and first extent beginning at segment zero, that corresponds to partition byte 0x1300000. For a whole-disk image, add the actual partition start. Validate partition boundaries rather than assuming a legacy MBR.

## Filesystem information field inventory

Relative to the filesystem information record: magic 0, volume version 4, one-byte filesystem version 8, 16-byte UUID 9, mode 25, 128-byte label 29, device-block-size field 157, 64-bit filesystem block size 161, 32-bit creation-time field 169, unknown four bytes 173, LVM UUID 177, unknown sixteen bytes 193, FDC header size 209, FDC bitmap count 213 and subblock size 217. Decode fixed-width little-endian numbers without assuming host structure alignment. Verify sizes, member identity, version and metadata geometry before using block addressing. Preserve the unknown fields; the timestamp interpretations are reference observations rather than a complete vendor specification.

## Logical-volume extent assembly

The LVM information contains total logical size and block-count fields at relative offsets 0 and 8, textual UUID at 20 (35 bytes), binary UUID at 84, creation-time field 104, number of segments 116, first segment 120, last segment 128, modification-time field 136 and extent count 144. The reference maps extents in 256 MiB segments, with inclusive first and last segment numbers. For logical byte p, choose the member covering floor(p/268435456); its data-relative offset is p minus first_segment times 268435456. Add the member's actual volume base and data prefix. Split cross-member reads. Validate UUIDs, complete nonoverlapping coverage, member sizes and arithmetic; a missing member is not a sparse file hole.

## VMFS6 extent header and root discovery

In the pinned fork the LVM record starts at relative offset 0x1000, not the older 0x200. The reference reads an 8192-byte volume-information window and accepts volume version 6. The ordinary filesystem magic is 0x2FABF15E; the fork also defines 0x2FABF15F as VMFSL, which must not be treated as proof of ordinary VMFS6 compatibility. The heartbeat area starts at logical-volume byte 0x300000 with 1024 records of 4096 bytes. Initial FDC bootstrap block is max(1,floor((0x300000+1024*4096)/filesystem_block_size)). Open descriptor entry 0, item 0 as root and locate all real metadata catalogues. The extra .pb2.sf catalogue has magic 0x10C00006.

## Metadata ownership header

The decoded common prefix holds magic at 0 (32 bits), logical object position at 4 (64 bits), heartbeat position at 12, heartbeat sequence at 20, object sequence at 28, lock indicator at 36 (32 bits), 16-byte locking-host UUID at 40 and modification-time field at 56 (64 bits). Remaining header bytes are not fully interpreted by the reference. Validate object identity, expected magic and address space; keep position, lock state and timestamps separate. Heartbeat records use observed active/inactive magic values 0xABCDEF02 and 0xABCDEF01. Never clear locks, declare another host dead, or treat a storage reservation as sufficient proof of exclusive ownership merely from these fields.

## File descriptor fields

Offsets relative to the end of the metadata header are: ID 0, second ID 4, link count 8, file type 12, flags 16, 64-bit logical byte length 20, allocation block size 28, block count 36, then 32-bit mtime 44, ctime 48, atime 52, UID 56, GID 60, mode 64, allocation scheme (ZLA) 68, to-be-zeroed field 72, copy-on-write field 76 and unknown bytes 80..511. The descriptor magic is 0x10C00001. File type and allocation scheme determine whether the subsequent union contains inline bytes, encoded allocation references or a raw-device mapping identifier. Raw-device mappings are not ordinary resident payload. Validate size, pointer capacity and mode rather than trusting a plausible inode signature.

## VMFS6 descriptor geometry

The metadata header is 4096 bytes and the file-descriptor allocation stride is 8192 bytes in the fork, unlike 512 and 2048 in VMFS3/5. The descriptor's known fields begin at byte 4096; the inline/content union begins at 4608. A further 1024-byte unknown prefix precedes the 320-entry, 64-bit block-ID array at byte 5632. Its array occupies 2560 bytes through byte 8191. Do not truncate these block references to 32 bits. The inline union is a distinct interpretation of those same bytes, not an additional copy of file data. Preserve the unknown prefix and use the allocation scheme to select the mapping.

## VMFS6 encoded block identifiers

The fork extracts low three bits as type: 0 none, 1 file, 2 subblock, 3 pointer, 4 descriptor, 5 PB2, 6 unknown, 7 large file block. It defines a 512 MiB large-block size. File-block item is split across masks 0x0FF8000000000000 and 0x00000000FFFF8000, reconstructed into item low 9 and next 17 bits. Flags mask is 0x38; a separate zero mask is 0x80. Subblock item uses low eight bits from ID mask 0xFF00000000000000 and next four from 0x000000000003C000; entry mask is 0x0000000000003FC0. These masks are reverse-engineered observations; unknown combinations and large-file-block semantics require fixtures. Do not apply VMFS3/5 contiguous file-block extraction.

## VMFS6 pointer and descriptor masks

The fork's ordinary PB entry mask is 0x0FFFFFC0 and item mask 0xF0000000; PB2 entry mask is 0x07FFFFC0 and item mask 0xF8000000. File-descriptor entry mask is 0x003FFFC0 and item mask 0xFFC00000; flags mask 0x38. These refer to positions within a 64-bit stored identifier even when a particular mask only covers its low word. Convert a masked field to its numerical value by shifting right by its mask's trailing zero count. Keep all high bits until the type is established. Check every decoded entry and item against the relevant catalogue's geometry.

## Allocation catalogue principles

The filesystem uses metadata files for file blocks (.fbb.sf), subblocks (.sbc.sf), pointer blocks (.pbc.sf) and file descriptors (.fdc.sf). Their magic values are 0x10C00002 through 0x10C00005 respectively. A bitmap file's header describes items per entry, bitmap entries per area, header size, item-data size, area size, total items and area count. Allocation entries contain a common ownership header followed by entry ID, total item count, free count, first-free hint and 496 bitmap bytes in the observed format. The pinned VMFS3/5 implementation uses set bits for free items and clear bits for allocated items; verify that convention for each admitted revision. Counts and first-free hints must agree with validated bits. Check unused tail bits and metadata ownership before allocation.

## Bitmap item address calculation

Let I be items per bitmap entry, E entries per area, H header size, A area size, D data bytes per item and S persistent bitmap-entry stride. A requested entry e and item i map to n=e*I+i. Area is floor(n/(I*E)), and the item position within the bitmap file is H+area*A+E*S+(n modulo (I*E))*D. These are file-relative addresses, not raw disk locations. Resolve the metadata file's own mapping before reading. In the observed bitmap, the local item index selects byte floor(index/8) and bit index modulo 8. Reject zero sizes, overflows, item indexes beyond the last entry and self-referential metadata chains.

## VMFS6 bitmap and indirect addressing

In this fork a bitmap entry has 8192-byte stride with a 4096-byte ownership header, then 16 bytes of ID/count fields and 496 interpreted bitmap bytes; the remaining record is not fully decoded. The fork uses 64-bit allocation pointers, including a separate PB2 catalogue. For a PB2 item of D bytes, D/8 references fit. For logical block k, select inode slot floor(k/(D/8)), then pointer index k modulo (D/8). The double-indirect path uses subblock catalogue data size to derive both fanouts, then selects a first-level pointer and a second-level pointer. For example, with a 65536-byte pointer item, fanout is 8192; logical block 8195 in a single indirect map uses inode slot 1 and pointer slot 3. Validate the actual scheme, item size and index rather than hardcoding this example. The fork's ordinary PB path also uses SBC in some cases; that relationship requires format-specific corroboration.

## VMFS6 directory page layout

The fork uses directory records of 288 bytes (0x120), with type at 0, 32-bit descriptor ID at 4, record ID at 8, unknown word 12, 64-bit file-offset field 16 and 128-byte name at 24. Unlike VMFS3/5, record slots 0 and 1 occur at directory offsets 0x3B8 and 0x4D8. Subsequent slots use pages of 4096 bytes: for slot n>=2, p=floor((n-2)/14), i=(n-2) modulo 14, and offset=0x11040+p*4096+i*288. Thus slot 16 begins at 0x12040. The fork reads page-state nibbles from directory offset 0x10040; bit 3 terminates and bit 0 indicates a present page in its observed traversal. It skips zero descriptor IDs. Exact page-map meaning and bounds must be verified on real images, not extrapolated from the older flat directory format.

## Integrity and shared-storage semantics

VMFS is designed for simultaneous access by hypervisor hosts. Logical ownership, distributed metadata locks, heartbeats, fencing and recovery ordering are critical; a static image reader is not a cluster participant. A stable offline image can be checked by comparing descriptor identities, directory links, allocation bitmaps, pointer blocks, actual payload coverage and all member boundaries. Record contradictions without modifying the source. VMDK descriptor and snapshot/delta-chain formats sit above VMFS and are distinct from filesystem allocation metadata. Do not infer VMFS journal replay, lock expiry, atomic update or reclamation rules from a VMDK parser.

## Writer and defragmentation boundary

An independent writer requires exact native allocation transitions, metadata update ordering, durable flush and recovery semantics, and proven exclusion of every other host. Moving a file block changes its ownership and every reference; subblock and pointer-block relocation also changes catalogue metadata. An apparent free bitmap bit alone does not authorise reuse while another host may hold a valid reference. Formatting must establish coherent extent metadata, filesystem information, root descriptor, allocator catalogues, heartbeats and recovery state. The available reverse-engineering evidence does not establish complete native formatting, journal replay, distributed write or repair algorithms. Treat those operations as unqualified until they are independently demonstrated.

## Implementation and qualification boundary

The original central VMFS6 record was design-only; the pinned vmfs6-tool reader now supplies version-specific structure evidence. Reject VMFS3/5, incomplete extent sets and unknown ownership state; distributed locking and fencing require separate qualification.

Common V2 is the independent provider interface: https://github.com/Infiltrator-Projects/Libraries/blob/main/docs/FILESYSTEM_COMMON_ABI_V2.md . Negotiate real operation masks and media profiles. Use bounded host I/O, exclusive unmounted ownership and durable flush for mutation; distinguish unsupported, corrupt, I/O and stale-handle results. Test Media and Defragmenter use the same engine. Hosted, freestanding, BOOT, kernel, recovery and package qualification are independent. Current capability and release status belongs to the repository README and exact-source qualification records; historical evidence is not a current status claim.

Historical source or test reference: https://github.com/Infiltrator-Projects/Filesystems/tree/101026021d9088d80b7f0f472d9061f0e2c688a3/native/filesystems/vmfs6 .

## References and qualification

Pinned evidence: https://github.com/weafon/vmfs6-tool/tree/84e094a16f92b1883f16c347ede4c0edadaf5290/libvmfs . Review vmfs_volume.c, vmfs_fs.c, vmfs_inode.c, vmfs_block.h, vmfs_bitmap.h, vmfs_dirent.c and vmfs_heartbeat.h. Use genuine VMFS6 images with different file sizes, directory pages, pointer types, sparse allocations, large file blocks, and multiple members. Compare extracted names and file hashes against a matching ESXi environment and independent read-only tools. Reject VMFS3/5 rather than falling back to those layouts. Complete large-block mapping, page-state semantics, native allocation/recovery and cluster write protocol remain unresolved.
