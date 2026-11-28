/* SPDX-License-Identifier: GPL-3.0-or-later
 * Native bounded recognition using the documented on-disk structures and
 * the pinned version-specific vmfs-tools format reference. No kernel, libc or tool backend.
 * Recognition is not evidence of a safe mounted namespace or mutation.
 */
#include <infiltratr/filesystem_module_v2.h>
static inline uint32_t le16(const uint8_t *p){return p[0]|((uint32_t)p[1]<<8);}
static inline uint32_t le32(const uint8_t *p){return le16(p)|(le16(p+2)<<16);}
static inline uint64_t le64(const uint8_t *p){return le32(p)|((uint64_t)le32(p+4)<<32);}
static inline int equal(const uint8_t *a,const uint8_t *b,uint32_t n){
 for(uint32_t i=0;i<n;i++)if(a[i]!=b[i])return 0;
 return 1;
}
static inline uint32_t crc32(uint32_t c,const uint8_t *p,uint32_t n){
 while(n--){c^=*p++;for(unsigned i=0;i<8u;i++)c=(c>>1)^((0u-(c&1u))&0xedb88320u);}return c;
}
static int read_bytes(const IfsFsHostRuntimeV2 *h,uint64_t off,void *p,uint32_t n){
 if(off>h->volume_bytes||n>h->volume_bytes-off)return IFS_FS_NOT_FOUND;
 return ifs_fs_host_read_bytes_v2(h,off,p,n);
}


#define VOL UINT64_C(0x100000)
#define DATA UINT64_C(0x1100000)
#define SEG UINT64_C(0x10000000)
#define LVM_OFFSET 0x1000u
#define MDH 4096u
#define STRIDE 8192u
#define DESCRIPTOR 8192u
#define HB_END 0x700000u
static int nonzero(const uint8_t*p,unsigned n){while(n--)if(*p++)return 1;return 0;}
static int power2(uint64_t n){return n&&!(n&(n-1u));}
/* A single-partition, single-extent reader: the partition base is supplied
 * by Common. Volume IDs and filesystem IDs are distinct; the LVM UUID
 * is the join key. The older filesystem version fields are NOT identical
 * to volume version (the upstream VMFS3 fixture has FS volver 4). */
static int identify(const IfsFsHostRuntimeV2*h,uint32_t*variant)
{
 uint8_t vol[512],lvm[512],fs[512],header[512],entry[512],inode[512];
 int r=read_bytes(h,VOL,vol,sizeof(vol));if(r)return r;
 if(le32(vol)!=0xc001d00du)return IFS_FS_NOT_FOUND;
 uint32_t v=le32(vol+4);
 if(!(v==6u))return IFS_FS_UNSUPPORTED;
 r=read_bytes(h,VOL+LVM_OFFSET,lvm,sizeof(lvm));if(r)return r;
 uint64_t size=le64(lvm),segments=le32(lvm+116),first=le32(lvm+120),last=le32(lvm+128),count=le32(lvm+144);
 if(!count||!segments||last<first||last-first+1u!=segments||!nonzero(lvm+84,16)||!nonzero(vol+130,16))
  return IFS_FS_CORRUPT;
 if(count!=1u||first)return IFS_FS_UNSUPPORTED;
 if(size!=segments*SEG||size>h->volume_bytes||DATA>h->volume_bytes-size||!le64(lvm+8))return IFS_FS_CORRUPT;
 /* The physical-size field is stored in 256-byte units. Do not use
  * the apparent LUN length to expand the declared logical address space. */
 uint64_t physical=(uint64_t)le32(vol+95)*256u;
 if(physical<DATA+size||physical>h->volume_bytes)return IFS_FS_CORRUPT;
 r=read_bytes(h,DATA+0x200000u,fs,sizeof(fs));if(r)return r;
 if(le32(fs)==0x2fabf15fu)return IFS_FS_UNSUPPORTED;
 if(le32(fs)!=0x2fabf15eu||!equal(fs+177,lvm+84,16)||!nonzero(fs+9,16))return IFS_FS_CORRUPT;
 uint64_t block=le64(fs+161);uint32_t sub=le32(fs+217);
 if(!power2(block)||block<0x100000u||block>0x800000u||!power2(sub)||sub>block)return IFS_FS_UNSUPPORTED;
 uint64_t number=HB_END/block;if(!number)number=1;
 uint64_t fdc=number*block;
 if(fdc>=size||block>size-fdc)return IFS_FS_CORRUPT;
 r=read_bytes(h,DATA+fdc,header,sizeof(header));if(r)return r;
 uint64_t items=le32(header),entries=le32(header+4),head=le32(header+8),data=le32(header+12),area=le32(header+16),total=le32(header+20),areas=le32(header+24);
 if(!items||items>3968u||!entries||entries>65536u||head<512u||head%512u||data!=DESCRIPTOR||
    !total||!areas||items*entries*areas<total||entries*STRIDE>area||
    items*entries*data>area-entries*STRIDE||head>=block||
    head+entries*STRIDE+DESCRIPTOR>block)return IFS_FS_CORRUPT;
 /* Resolve FDC entry zero and its first allocated descriptor from the
  * actual catalogue geometry, not a fixed inode byte offset. */
 uint64_t e=DATA+fdc+head;
 r=read_bytes(h,e,entry,64);if(r)return r;
 if(le32(entry)!=0x10c00005u||le64(entry+4)!=fdc+head)return IFS_FS_CORRUPT;
 r=read_bytes(h,e+MDH,entry,sizeof(entry));if(r)return r;
 uint32_t bits=le32(entry+4),free=le32(entry+8),hint=le32(entry+12);
 if(le32(entry)!=0u||!bits||bits>items||free>bits||(free&&hint>=bits)||(entry[16]&1u))return IFS_FS_CORRUPT;
 unsigned observed=0,firstfree=bits;
 for(unsigned i=0;i<bits;i++)if(entry[16+i/8u]&(1u<<(i%8u))){observed++;if(firstfree==bits)firstfree=i;}
 if(observed!=free||(free&&hint!=firstfree))return IFS_FS_CORRUPT;
 uint64_t root=e+entries*STRIDE;
 r=read_bytes(h,root,inode,64);if(r)return r;
 if(le32(inode)!=0x10c00001u||le64(inode+4)!=root-DATA)return IFS_FS_CORRUPT;
 r=read_bytes(h,root+MDH,inode,sizeof(inode));if(r)return r;
 if(le32(inode)!=4u||!le32(inode+8)||le32(inode+12)!=2u||!le64(inode+20)||
    le64(inode+20)>size||!power2(le64(inode+28))||le64(inode+28)>block||
    le64(inode+36)>size/le64(inode+28))return IFS_FS_CORRUPT;
 *variant=v;return IFS_FS_OK;
}
static int probe(const IfsFsHostRuntimeV2 *h,uint32_t *variant){
 if(!variant||!ifs_fs_host_v2_valid(h))return IFS_FS_INVALID;
 *variant=0;
 if(h->volume_bytes%h->logical_sector_bytes)return IFS_FS_INVALID;
 return identify(h,variant);
}
static const IfsFsCoreOpsV2 core={
 .header={IFS_FS_GROUP_ABI_V2,sizeof(IfsFsCoreOpsV2),IFS_FS_GROUP_CORE,0,IFS_FS_CORE_PROBE},.probe=probe
};
static const IfsFsGroupHeaderV2 *query(uint32_t id,uint32_t min,uint32_t max){
 return id==IFS_FS_GROUP_CORE&&min<=2u&&max>=2u?&core.header:NULL;
}
static const IfsFsModuleApiV2 api={
 .abi_version=2,.structure_bytes=sizeof(IfsFsModuleApiV2),.format_id="vmfs6",
#ifdef IFS_FREESTANDING
 .runtime_profiles=IFS_FS_PROFILE_FREESTANDING,
#else
 .runtime_profiles=IFS_FS_PROFILE_HOSTED,
#endif
 .group_mask=IFS_FS_GROUP_BIT(IFS_FS_GROUP_CORE),.query_group=query
};
__attribute__((visibility("default"))) const IfsFsModuleApiV2 *infiltrator_filesystem_api_v2(void){return &api;}
