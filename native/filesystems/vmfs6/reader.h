/* SPDX-License-Identifier: GPL-3.0-or-later
 * Native offline datastore reader for the bounded single-extent,
 * direct file-block profile. The catalogue bootstrap follows the
 * version-specific reverse-engineered reference; it never mounts a
 * kernel or FUSE filesystem or executes a third-party reader.
 */
#define PTR_BYTES 8u
#define PTR_OFFSET 5632u
#define PTR_COUNT 320u
#define INLINE_OFFSET (MDH+512u)
#define DIR_RECORD 288u
#define DIR_NAME 24u
#define MAX_SCAN (UINT64_C(16)*1024u*1024u)
typedef struct {uint32_t items,entries,head,data,area,total,areas;} Catalogue;
typedef struct {
 const IfsFsHostRuntimeV2*h;uint64_t size,block,base,free;
 uint32_t version;
 Catalogue fdc,fbb;
 uint8_t root[DESCRIPTOR],fdc_inode[DESCRIPTOR],fbb_inode[DESCRIPTOR];
 uint8_t *used;
 int verified;
} Volume;
static void zero(void*p,size_t n){uint8_t*b=p;while(n--)*b++=0;}
static int logical_read(Volume*v,uint64_t off,void*out,uint32_t n){
 if(off>v->size||n>v->size-off)return IFS_FS_CORRUPT;
 return ifs_fs_host_read_bytes_v2(v->h,DATA+off,out,n);
}
static int block_pointer(const uint8_t*i,uint64_t index,uint64_t*item,int*hole){
 if(index>=PTR_COUNT)return IFS_FS_UNSUPPORTED;
 uint64_t id=le64(i+PTR_OFFSET+index*PTR_BYTES);
 *hole=0;*item=UINT64_MAX;
 if(!id){*hole=1;return 0;}
 if((id&7u)!=1u)return IFS_FS_UNSUPPORTED;
 if(id&~UINT64_C(0x0ff80000ffff80a7))return IFS_FS_UNSUPPORTED;
 *item=((id&UINT64_C(0x0ff8000000000000))>>51)|((id&UINT64_C(0xffff8000))>>6);
 if(id&0xa0u)*hole=1;
 return 0;
}
/* Resolve direct file blocks; during bootstrap all reads are bounded
 * but ownership is not yet known. After reconciling the FBB, repeat
 * the metadata traversal and require every referenced block allocated.
 * Metadata holes and unrecorded blocks are always rejected. */
static int file_read(Volume*v,const uint8_t*i,uint64_t off,void*out,uint32_t n,int holes){
 const uint8_t*d=i+MDH;uint64_t size=le64(d+20);uint32_t zla=le32(d+68);uint8_t*p=out;
 if((!out&&n)||off>size||n>size-off)return IFS_FS_INVALID;
 if(zla==4305u){
  if(size>DESCRIPTOR-INLINE_OFFSET)return IFS_FS_CORRUPT;
  for(unsigned j=0;j<n;j++)p[j]=i[INLINE_OFFSET+off+j];
  return 0;
 }
 if((zla!=1u&&zla!=4302u)||le64(d+28)!=v->block)return IFS_FS_UNSUPPORTED;
 while(n){
  uint64_t item;int hole;int rc=block_pointer(i,off/v->block,&item,&hole);if(rc)return rc;
  unsigned skip=(unsigned)(off%v->block),take=(unsigned)v->block-skip;if(take>n)take=n;
  /* A zero-marked allocated pointer is not the same as an absent
   * mapping. Validate ownership before returning logical zeros; stale
   * pointers into free space must not be presented as valid files. */
  if(item!=UINT64_MAX&&(item>=v->size/v->block||(v->verified&&(item>=v->fbb.total||!(v->used[item/8u]&(1u<<(item%8u)))))))return IFS_FS_CORRUPT;
  if(hole){if(!holes)return IFS_FS_CORRUPT;zero(p,take);}
  else {
   rc=logical_read(v,item*v->block+skip,p,take);if(rc)return rc;
  }
  off+=take;p+=take;n-=take;
 }
 return 0;
}

/* Resolve a complete contiguous metadata record in a catalogue file
 * to its native logical LUN position. The MDH self address must match
 * this translation even after leaving the initial bootstrap block.
 * Do not accept a valid-looking descriptor copied to a different
 * allocation slot. Crossing file-block boundaries is not qualified. */
static int metadata_position(Volume*v,const uint8_t*i,uint64_t off,uint32_t n,uint64_t*position)
{
 const uint8_t*d=i+MDH;uint32_t zla=le32(d+68);uint64_t size=le64(d+20);
 if((zla!=1u&&zla!=4302u)||le64(d+28)!=v->block)return IFS_FS_UNSUPPORTED;
 if(off>size||n>size-off||n>v->block-off%v->block)return IFS_FS_CORRUPT;
 uint64_t item;int hole;int rc=block_pointer(i,off/v->block,&item,&hole);if(rc)return rc;
 if(hole||item>=v->size/v->block||
    (v->verified&&(item>=v->fbb.total||!(v->used[item/8u]&(1u<<(item%8u))))))return IFS_FS_CORRUPT;
 *position=item*v->block+off%v->block;
 return 0;
}
static int header(const uint8_t*p,Catalogue*c,uint64_t size,uint32_t data){
 c->items=le32(p);c->entries=le32(p+4);c->head=le32(p+8);c->data=le32(p+12);
 c->area=le32(p+16);c->total=le32(p+20);c->areas=le32(p+24);
 uint64_t per=(uint64_t)c->items*c->entries,over=(uint64_t)c->entries*STRIDE;
 if(!c->items||c->items>3968u||!c->entries||c->entries>65536u||
    c->head<512u||c->head>size||!c->area||over>c->area||c->data!=data||
    !c->total||!c->areas||per*c->areas<c->total||
    (data&&per*data>c->area-over))return IFS_FS_CORRUPT;
 /* An area describes maximum capacity, not necessarily fully backed
  * bytes in the last area. Validate the final represented item and
  * its bitmap record instead of requiring unused trailing capacity. */
 uint64_t last=(uint64_t)c->total-1u,area=last/per,within=last%per;
 uint64_t start=c->head+area*c->area;
 uint64_t entry_end=(within/c->items+1u)*STRIDE;
 uint64_t data_end=over+(within+1u)*data;
 if(start>size||entry_end>size-start||(data&&data_end>size-start))return IFS_FS_CORRUPT;
 return 0;
}
static int entry_offset(const Catalogue*c,uint32_t entry,uint32_t item,uint64_t*e,uint64_t*pos){
 uint64_t n=(uint64_t)entry*c->items+item;
 if(item>=c->items||n>=c->total)return IFS_FS_CORRUPT;
 uint64_t area=entry/c->entries;
 if(area>=c->areas)return IFS_FS_CORRUPT;
 uint64_t base=c->head+area*c->area;
 *e=base+(entry%c->entries)*(uint64_t)STRIDE;
 *pos=base+(uint64_t)c->entries*STRIDE+(n%((uint64_t)c->entries*c->items))*c->data;
 return 0;
}
static int check_entry(const uint8_t*p,const Catalogue*c,uint32_t entry,uint32_t magic,uint32_t*bits,uint32_t*free){
 const uint8_t*d=p+MDH;*bits=le32(d+4);*free=le32(d+8);
 if(le32(p)!=magic||le32(p+36)||le32(d)!=entry||!*bits||*bits>c->items||*free>*bits)return IFS_FS_CORRUPT;
 uint64_t first=(uint64_t)entry*c->items;
 if(first>=c->total||*bits!=(c->total-first<c->items?c->total-first:c->items))return IFS_FS_CORRUPT;
 unsigned count=0,first_free=*bits;
 for(unsigned j=0;j<*bits;j++)if(d[16+j/8u]&(1u<<(j%8u))){if(first_free==*bits)first_free=j;count++;}
 /* The first-free hint is interpreted only when a free item exists. */
 if(count!=*free||(*free&&le32(d+12)!=first_free))return IFS_FS_CORRUPT;
 return 0;
}
static int valid_inode(Volume*v,const uint8_t*p,uint32_t id){
 const uint8_t*d=p+MDH;uint32_t type=le32(d+12),zla=le32(d+68);
 if(le32(p)!=0x10c00001u||le32(p+36)||le32(d)!=id||!le32(d+8)||type<2u||type>5u||
    le64(d+20)>v->size)return IFS_FS_CORRUPT;
 if(zla==4305u)return le64(d+20)<=DESCRIPTOR-INLINE_OFFSET?0:IFS_FS_CORRUPT;
 if(!power2(le64(d+28))||le64(d+28)>v->block)return IFS_FS_CORRUPT;
 return 0;
}
/* The temporary FDC map is a single logical file block. Later the
 * actual .fdc.sf descriptor defines its own bounded mapping. */
static int descriptor(Volume*v,uint32_t id,uint8_t*p,int boot){
 if((id&63u)!=4u)return IFS_FS_UNSUPPORTED;
 uint32_t entry=(id>>6)&65535u,item=id>>22;uint64_t e,pos;
 int rc=entry_offset(&v->fdc,entry,item,&e,&pos);if(rc)return rc;
 uint8_t buf[MDH+512u];
 if(boot){
  if(e>v->block||sizeof(buf)>v->block-e||pos>v->block||DESCRIPTOR>v->block-pos)return IFS_FS_UNSUPPORTED;
  rc=logical_read(v,v->base+e,buf,sizeof(buf));
 }else rc=file_read(v,v->fdc_inode,e,buf,sizeof(buf),0);
 if(rc)return rc;
 uint64_t physical_entry,physical_descriptor;
 if(boot){physical_entry=v->base+e;physical_descriptor=v->base+pos;}
 else {
  rc=metadata_position(v,v->fdc_inode,e,sizeof(buf),&physical_entry);if(rc)return rc;
  rc=metadata_position(v,v->fdc_inode,pos,DESCRIPTOR,&physical_descriptor);if(rc)return rc;
 }
 if(le64(buf+4)!=physical_entry)return IFS_FS_CORRUPT;
 uint32_t bits,free;rc=check_entry(buf,&v->fdc,entry,0x10c00005u,&bits,&free);if(rc)return rc;
 if(item>=bits||(buf[MDH+16+item/8u]&(1u<<(item%8u))))return IFS_FS_CORRUPT;
 if(boot)rc=logical_read(v,v->base+pos,p,DESCRIPTOR);
 else rc=file_read(v,v->fdc_inode,pos,p,DESCRIPTOR,0);
 if(rc)return rc;
 if(le64(p+4)!=physical_descriptor)return IFS_FS_CORRUPT;
 return valid_inode(v,p,id);
}
static int name_match(const uint8_t*p,const char*q,unsigned n){for(unsigned i=0;i<n;i++)if(p[i]!=(uint8_t)q[i])return 0;return 1;}
/* Only exact native names and complete records are used. VMFS6 uses
 * its own page-state nibble and slot offsets, not the older flat
 * 140-byte records. Directory IDs and descriptor generation are checked
 * at the point of resolution; symlinks are not followed automatically. */
typedef struct {uint32_t id,record,type;unsigned length;char name[129];} Dirent;
static int scan(Volume*v,const uint8_t*i,const char*name,unsigned n,uint32_t index,Dirent*out){
 uint64_t size=le64(i+MDH+20);unsigned matched=0,dots=0;uint32_t ordinal=0;
 if(le32(i+MDH+12)!=2u||!size||size>MAX_SCAN)return IFS_FS_UNSUPPORTED;
 if(size<0x11040u)return IFS_FS_UNSUPPORTED;
 for(uint64_t slot=0;slot<MAX_SCAN/DIR_RECORD;slot++){
  uint64_t off;
  uint64_t page=slot<2u?0u:(slot-2u)/14u, mapoff=0x10040u+(page+1u)/2u;
  off=slot<2u?0x3b8u+slot*DIR_RECORD:0x11040u+page*4096u+((slot-2u)%14u)*DIR_RECORD;
  if(off>=size)break;
  if(mapoff>=0x11000u)return IFS_FS_UNSUPPORTED;
  uint8_t state;int r=file_read(v,i,mapoff,&state,1,0);if(r)return r;
  unsigned nibble=(state>>((page+1u)%2u?0u:4u))&15u;
  if(nibble&8u)break;
  if(!(nibble&1u))continue;
  if(DIR_RECORD>size-off)return IFS_FS_CORRUPT;
  uint8_t p[DIR_RECORD];int rc=file_read(v,i,off,p,sizeof(p),0);if(rc)return rc;
  uint32_t type=le32(p),id=le32(p+4),record=le32(p+8);
  if(!id){if(slot<2u)return IFS_FS_CORRUPT;continue;}
  unsigned len=0;while(len<128u&&p[DIR_NAME+len]){if(p[DIR_NAME+len]=='/')return IFS_FS_CORRUPT;len++;}
  if(!len||type<2u||type>6u||(id&7u)!=4u)return IFS_FS_CORRUPT;
  unsigned dot=(len==1u&&p[DIR_NAME]=='.')?1u:(len==2u&&p[DIR_NAME]=='.'&&p[DIR_NAME+1]=='.')?2u:0u;
  if(dot){
   /* The dedicated dot slots are part of the native directory header.
    * Verify the self identity, record identity and type before hiding
    * them from Common's enumeration. */
   if(slot!=dot-1u||(dots&dot)||type!=2u)return IFS_FS_CORRUPT;
   if(dot==1u&&(id!=le32(i+MDH)||record!=le32(i+MDH+4)))return IFS_FS_CORRUPT;
   dots|=dot;continue;
  }
  if(slot<2u)return IFS_FS_CORRUPT;
  if((name&&len==n&&name_match(p+DIR_NAME,name,n))||(!name&&ordinal==index)){
   if(++matched!=1u)return IFS_FS_CORRUPT;
   out->id=id;out->record=record;out->type=type;out->length=len;
   for(unsigned j=0;j<len;j++)out->name[j]=(char)p[DIR_NAME+j];
   out->name[len]=0;
  }
  ordinal++;
 }
 if(dots!=3u)return IFS_FS_CORRUPT;
 return matched?0:name?IFS_FS_NOT_FOUND:IFS_FS_END;
}
static int lookup(Volume*v,const uint8_t*i,const char*name,unsigned n,int boot,uint8_t*out){
 Dirent d;int rc=scan(v,i,name,n,0,&d);if(rc)return rc;
 rc=descriptor(v,d.id,out,boot);if(rc)return rc;
 if(le32(out+MDH+4)!=d.record||le32(out+MDH+12)!=d.type)return IFS_FS_CORRUPT;
 return 0;
}
static int read_catalogue(Volume*v,const uint8_t*i,Catalogue*c,uint32_t data){
 uint8_t p[512];int rc=file_read(v,i,0,p,sizeof(p),0);if(rc)return rc;
 return header(p,c,le64(i+MDH+20),data);
}
static int load_fbb(Volume*v){
 uint64_t size=le64(v->fbb_inode+MDH+20);uint8_t h[512];int rc=file_read(v,v->fbb_inode,0,h,512,0);if(rc)return rc;
 /* FBB is bitmap-only. Some revisions reserve associated item bytes;
  * accept their declared geometry, never infer file-block ownership
  * from that optional space. */
 rc=header(h,&v->fbb,size,le32(h+12));if(rc)return rc;
 if(v->fbb.total>v->size/v->block||v->fbb.total>UINT32_C(8388608))return IFS_FS_UNSUPPORTED;
 size_t bytes=(v->fbb.total+7u)/8u;v->used=v->h->allocate(v->h->context,bytes);if(!v->used)return IFS_FS_NO_MEMORY;
 zero(v->used,bytes);v->free=0;
 uint8_t p[MDH+512u];
 for(uint32_t start=0;start<v->fbb.total;){
  uint32_t entry=start/v->fbb.items,bits,free;uint64_t e,pos;
  rc=entry_offset(&v->fbb,entry,0,&e,&pos);if(rc)return rc;
  rc=file_read(v,v->fbb_inode,e,p,sizeof(p),0);if(rc)return rc;
  uint64_t position;rc=metadata_position(v,v->fbb_inode,e,sizeof(p),&position);if(rc)return rc;
  if(le64(p+4)!=position)return IFS_FS_CORRUPT;
  rc=check_entry(p,&v->fbb,entry,0x10c00002u,&bits,&free);if(rc)return rc;
  for(unsigned j=0;j<bits;j++)if(!(p[MDH+16+j/8u]&(1u<<(j%8u))))v->used[(start+j)/8u]|=(uint8_t)(1u<<((start+j)%8u));
  v->free+=free;start+=bits;
 }
 v->verified=1;return 0;
}
static void destroy(Volume*v){if(v->used)v->h->release(v->h->context,v->used);v->h->release(v->h->context,v);}
static int volume_info(void*vol,IfsFsVolumeInfoV2*i){
 Volume*v=vol;if(!v||!i||i->structure_bytes<sizeof(*i))return IFS_FS_INVALID;
 i->variant=v->version;i->block_bytes=(uint32_t)v->block;i->max_name_bytes=128;i->total_bytes=v->size;i->free_bytes=v->free*v->block;return 0;
}
static int mount_volume(const IfsFsHostRuntimeV2*h,void**out,IfsFsVolumeInfoV2*info){
 if(!out||!info||info->structure_bytes<sizeof(*info)||!ifs_fs_host_v2_valid(h))return IFS_FS_INVALID;
 *out=NULL;if((h->capabilities&(IFS_FS_HOST_READ_ONLY|IFS_FS_HOST_EXCLUSIVE))!=(IFS_FS_HOST_READ_ONLY|IFS_FS_HOST_EXCLUSIVE))return IFS_FS_ACCESS;
 uint32_t version;int rc=identify(h,&version);if(rc)return rc;
 Volume*v=h->allocate(h->context,sizeof(*v));if(!v)return IFS_FS_NO_MEMORY;zero(v,sizeof(*v));v->h=h;v->version=version;
 uint8_t p[512],check[DESCRIPTOR];
 rc=read_bytes(h,VOL+LVM_OFFSET,p,sizeof(p));if(rc)goto done;v->size=le64(p);
 rc=logical_read(v,0x200000u,p,sizeof(p));if(rc)goto done;v->block=le64(p+161);
 v->base=HB_END/v->block;if(!v->base)v->base=1;v->base*=v->block;
 rc=logical_read(v,v->base,p,sizeof(p));if(rc)goto done;
 /* Only the initial catalogue header need fit the bootstrap block;
  * its declared complete extent is validated after reading .fdc.sf. */
 rc=header(p,&v->fdc,v->size-v->base,DESCRIPTOR);if(rc)goto done;
 rc=descriptor(v,4u,v->root,1);if(rc)goto done;
 rc=lookup(v,v->root,".fdc.sf",7,1,v->fdc_inode);if(rc)goto done;
 rc=lookup(v,v->root,".fbb.sf",7,1,v->fbb_inode);if(rc)goto done;
 if(le32(v->fdc_inode+MDH+12)!=5u||le32(v->fbb_inode+MDH+12)!=5u){rc=IFS_FS_CORRUPT;goto done;}
 rc=read_catalogue(v,v->fdc_inode,&v->fdc,DESCRIPTOR);if(rc)goto done;
 rc=load_fbb(v);if(rc)goto done;
 rc=descriptor(v,4u,check,0);if(rc)goto done;
 if(!equal(v->root,check,DESCRIPTOR)){rc=IFS_FS_CORRUPT;goto done;}
 rc=lookup(v,v->root,".fdc.sf",7,0,check);if(rc)goto done;
 if(!equal(v->fdc_inode,check,DESCRIPTOR)){rc=IFS_FS_CORRUPT;goto done;}
 rc=lookup(v,v->root,".fbb.sf",7,0,check);if(rc)goto done;
 if(!equal(v->fbb_inode,check,DESCRIPTOR)){rc=IFS_FS_CORRUPT;goto done;}
 /* Re-read all FBB entries under the now established allocation map. */
 for(uint32_t start=0;start<v->fbb.total;){
  uint64_t e,pos;uint32_t entry=start/v->fbb.items,bits,free;
  uint8_t b[MDH+512u];rc=entry_offset(&v->fbb,entry,0,&e,&pos);if(rc)goto done;
  rc=file_read(v,v->fbb_inode,e,b,sizeof(b),0);if(rc)goto done;
  uint64_t position;rc=metadata_position(v,v->fbb_inode,e,sizeof(b),&position);if(rc)goto done;
  if(le64(b+4)!=position){rc=IFS_FS_CORRUPT;goto done;}
  rc=check_entry(b,&v->fbb,entry,0x10c00002u,&bits,&free);if(rc)goto done;
  start+=bits;
 }
 *out=v;return volume_info(v,info);
done:
 destroy(v);return rc;
}
static int unmount_volume(void*vol){if(!vol)return IFS_FS_INVALID;destroy(vol);return 0;}
static int resolve(Volume*v,const char*path,uint8_t*i,const char**name,unsigned*n){
 if(!v||!path||path[0]!='/')return IFS_FS_INVALID;
 for(unsigned j=0;j<DESCRIPTOR;j++)i[j]=v->root[j];
 *name=path;*n=1;const char*p=path+1;
 for(unsigned depth=0;*p;depth++){
  if(depth>=64u)return IFS_FS_UNSUPPORTED;
  unsigned len=0;while(p[len]&&p[len]!='/'){if(len==128u)return IFS_FS_INVALID;len++;}
  if(!len||(len==1u&&p[0]=='.')||(len==2u&&p[0]=='.'&&p[1]=='.'))return IFS_FS_INVALID;
  uint8_t next[DESCRIPTOR];int rc=lookup(v,i,p,len,0,next);if(rc)return rc;
  for(unsigned j=0;j<DESCRIPTOR;j++)i[j]=next[j];
  *name=p;*n=len;p+=len;if(*p=='/'){p++;if(!*p)return IFS_FS_INVALID;}
 }
 return 0;
}
static int fill(const uint8_t*i,const char*name,unsigned n,IfsFsEntryV2*e){
 if(!e||e->structure_bytes<sizeof(*e)||!e->name||n>=e->name_capacity)return IFS_FS_INVALID;
 switch(le32(i+MDH+12)){case 2:e->type=IFS_FS_ENTRY_DIRECTORY;break;case 3:case 5:e->type=IFS_FS_ENTRY_FILE;break;case 4:e->type=IFS_FS_ENTRY_LINK;break;default:return IFS_FS_UNSUPPORTED;}
 for(unsigned j=0;j<n;j++)e->name[j]=name[j];
 e->name[n]=0;e->name_bytes=n;e->size_bytes=le64(i+MDH+20);e->attributes=0;return 0;
}
static int stat_path(void*vol,const char*path,IfsFsEntryV2*e){uint8_t i[DESCRIPTOR];const char*name;unsigned n;int rc=resolve(vol,path,i,&name,&n);return rc?rc:fill(i,name,n,e);}
static int list_path(void*vol,const char*path,uint32_t index,IfsFsEntryV2*e){
 uint8_t i[DESCRIPTOR];const char*name;unsigned n;Dirent d;int rc=resolve(vol,path,i,&name,&n);
 if(!rc)rc=scan(vol,i,NULL,0,index,&d);
 if(!rc)rc=descriptor(vol,d.id,i,0);
 if(!rc&&(le32(i+MDH+4)!=d.record||le32(i+MDH+12)!=d.type))rc=IFS_FS_CORRUPT;
 return rc?rc:fill(i,d.name,d.length,e);
}
static int read_path(void*vol,const char*path,uint64_t off,void*out,uint32_t n){
 uint8_t i[DESCRIPTOR];const char*name;unsigned len;int rc=resolve(vol,path,i,&name,&len);if(rc)return rc;
 if(le32(i+MDH+12)!=3u&&le32(i+MDH+12)!=5u)return IFS_FS_UNSUPPORTED;
 return file_read(vol,i,off,out,n,1);
}
static const IfsFsVolumeOpsV2 volume_ops={.header={2,sizeof(IfsFsVolumeOpsV2),IFS_FS_GROUP_VOLUME,0,IFS_FS_VOLUME_QUERY},.query=volume_info};
