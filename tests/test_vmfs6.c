/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _POSIX_C_SOURCE 200809L
#include <infiltratr/filesystem_module_v2.h>
#include <assert.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define SIZE (301989888u)
static uint8_t *image;
struct context{int fd,fail;uint32_t sector,allocs;uint64_t bytes;};
static int rd(void *p,uint64_t off,void *out,uint32_t n){
 struct context *c=p;assert(n&&n<=4096u&&!(off%c->sector)&&!(n%c->sector)&&off<=c->bytes&&n<=c->bytes-off);
 if(c->fail)return -1;
 if(c->fd>=0)return pread(c->fd,out,n,(off_t)off)==(ssize_t)n?0:-1;
 memcpy(out,image+off,n);return 0;
}
static void *allocate(void *p,size_t n){struct context*c=p;void *m=malloc(n);if(m)c->allocs++;return m;}
static void release(void *p,void *m){struct context*c=p;assert(m&&c->allocs);c->allocs--;free(m);}
static inline void put16(uint8_t*p,uint32_t n){p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static inline void put32(uint8_t*p,uint32_t n){put16(p,n);put16(p+2,n>>16);}
static inline void put64(uint8_t*p,uint64_t n){put32(p,(uint32_t)n);put32(p+4,(uint32_t)(n>>32));}
static inline uint32_t crc32(uint32_t c,const uint8_t*p,uint32_t n){
 while(n--){c^=*p++;for(unsigned i=0;i<8u;i++)c=(c>>1)^((0u-(c&1u))&0xedb88320u);}return c;
}


#define VOL 0x100000u
#define DATA 0x1100000u
#define FDC (DATA+0x700000u)
#define HEAD 65536u
#define STRIDE 8192u
#define MDH 4096u
#define ROOT (FDC+HEAD+STRIDE*2u)
static void fixture(void){
 uint8_t *p=image+VOL;put32(p,0xc001d00d);put32(p+4,6);put32(p+95,SIZE/256u);p[130]=9;
 p+=0x1000u;put64(p,0x10000000u);put64(p+8,2);p[84]=3;put32(p+116,1);put32(p+144,1);
 p=image+DATA+0x200000u;put32(p,0x2fabf15e);p[9]=5;p[177]=3;put64(p+161,0x100000u);put32(p+217,65536);
 p=image+FDC;put32(p,8);put32(p+4,2);put32(p+8,HEAD);put32(p+12,8192u);
 put32(p+16,2u*STRIDE+16u*8192u);put32(p+20,16);put32(p+24,1);
 p=image+FDC+HEAD;put32(p,0x10c00005);put64(p+4,FDC+HEAD-DATA);
 p+=MDH;put32(p+4,8);put32(p+8,7);put32(p+12,1);p[16]=254;
 p=image+ROOT;put32(p,0x10c00001);put64(p+4,ROOT-DATA);
 p+=MDH;put32(p,4);put32(p+8,2);put32(p+12,2);put64(p+20,280);put64(p+28,0x100000);put64(p+36,1);
}
static void corrupt(void){image[FDC+HEAD+MDH+16]|=1;}
static void absent(void){image[VOL]=0;}

#define BLK 0x100000u
#define FBB (DATA+9u*BLK)
#define ROOT_DATA (DATA+10u*BLK)
#define FILE_DATA (DATA+11u*BLK)
static void desc(unsigned item,unsigned type,uint64_t size,unsigned data){
 uint8_t*p=image+ROOT+item*8192u;
 put32(p,0x10c00001);put64(p+4,ROOT+item*8192u-DATA);
 p+=MDH;put32(p,4u+(item<<22));put32(p+4,item+100u);put32(p+8,1);put32(p+12,type);
 put64(p+20,size);put64(p+28,BLK);put64(p+36,1);put32(p+68,1);
 put64(p+(5632u-MDH),((uint64_t)data<<51)|1u);
}
static void dirent(unsigned offset,unsigned item,unsigned type,const char*name){
 uint8_t*p=image+ROOT_DATA+offset;put32(p,type);put32(p+4,4u+(item<<22));put32(p+8,item+100u);memcpy(p+24u,name,strlen(name));
}
static void populated(void){
 fixture();
 uint8_t*p=image+FDC+HEAD+MDH;put32(p+8,4);put32(p+12,4);p[16]=0xf0;
 /* The second descriptor bitmap entry represents the remaining 8
  * free items of the native catalogue. */
 p=image+FDC+HEAD+STRIDE;put32(p,0x10c00005);put64(p+4,FDC+HEAD+STRIDE-DATA);
 p+=MDH;put32(p,1);put32(p+4,8);put32(p+8,8);p[16]=255;
 desc(0,2,0x12000u,10);desc(1,5,BLK,7u);desc(2,5,BLK,9);desc(3,3,5,11);
 image[ROOT_DATA+0x10040]=1;image[ROOT_DATA+0x10041]=0x80;
 dirent(0x3b8,0,2,".");dirent(0x4d8,0,2,"..");
 dirent(0x11040,1,5,".fdc.sf");dirent(0x11160,2,5,".fbb.sf");dirent(0x11280,3,3,"hello.txt");
 p=image+FBB;put32(p,256);put32(p+4,1);put32(p+8,512);put32(p+12,0);put32(p+16,STRIDE);put32(p+20,256);put32(p+24,1);
 p+=512;put32(p,0x10c00002);put64(p+4,FBB+512-DATA);p+=MDH;put32(p+4,256);put32(p+8,244);put32(p+12,12);
 memset(p+16,255,32);p[16]=0;p[17]=0xf0;
 memcpy(image+FILE_DATA,"hello",5);
}
static void mount_fixture(const IfsFsCoreOpsV2*core,IfsFsHostRuntimeV2*h,struct context*c){
 memset(image,0,SIZE);populated();
 void*v=NULL;IfsFsVolumeInfoV2 info={.structure_bytes=sizeof(info)};
 assert(core->mount(h,&v,&info)==IFS_FS_ACCESS&&!v);
 h->capabilities|=IFS_FS_HOST_EXCLUSIVE;
 int rc=core->mount(h,&v,&info);if(rc)fprintf(stderr,"native VMFS fixture mount %d\n",rc);
 assert(rc==0&&v&&info.free_bytes==244ull*BLK);
 char name[129],out[5];IfsFsEntryV2 e={.structure_bytes=sizeof(e),.name=name,.name_capacity=sizeof(name)};
 assert(core->stat(v,"/",&e)==0&&e.type==IFS_FS_ENTRY_DIRECTORY);
 assert(core->stat(v,"/hello.txt",&e)==0&&e.size_bytes==5&&e.type==IFS_FS_ENTRY_FILE);
 assert(core->directory_entry(v,"/",2,&e)==0&&!strcmp(name,"hello.txt"));
 assert(core->read(v,"/hello.txt",0,out,5)==0&&!memcmp(out,"hello",5));
 assert(core->read(v,"/hello.txt",5,out,1)==IFS_FS_INVALID);
 assert(core->unmount(v)==0&&!c->allocs);
 /* Free the actual payload block without changing its stale bytes;
  * a read must refuse, not expose bytes merely because they exist. */
 uint8_t*p=image+FBB+512+MDH;p[17]|=8;put32(p+8,245);put32(p+12,11);
 assert(core->mount(h,&v,&info)==0&&v);
 assert(core->read(v,"/hello.txt",0,out,5)==IFS_FS_CORRUPT);
 assert(core->unmount(v)==0&&!c->allocs);
 p=image+FBB+512+MDH;put32(p+8,244);
 assert(core->mount(h,&v,&info)==IFS_FS_CORRUPT&&!v&&!c->allocs);
 /* Recompute no checksum: these fields are independent identity
  * constraints, not checksummed fields. A relocated stale descriptor
  * or bitmap record must fail even if the surrounding data is intact. */
 memset(image,0,SIZE);populated();put64(image+ROOT+3u*8192u+4,ROOT-DATA);
 assert(core->mount(h,&v,&info)==0&&v);
 assert(core->stat(v,"/hello.txt",&e)==IFS_FS_CORRUPT);
 assert(core->unmount(v)==0&&!c->allocs);
 memset(image,0,SIZE);populated();put64(image+FBB+512+4,FBB-DATA);
 assert(core->mount(h,&v,&info)==IFS_FS_CORRUPT&&!v&&!c->allocs);
 memset(image,0,SIZE);populated();
 p=image+FBB+512+MDH;p[17]|=8;put32(p+8,245);put32(p+12,11);
 put64(image+ROOT+3u*8192u+5632u,(UINT64_C(11)<<51)|0x81u);
 assert(core->mount(h,&v,&info)==0&&v);
 assert(core->read(v,"/hello.txt",0,out,5)==IFS_FS_CORRUPT);
 assert(core->unmount(v)==0&&!c->allocs);
 h->capabilities&=~IFS_FS_HOST_EXCLUSIVE;
}


/* A second native directory page, fragmented file, sparse allocation
 * and inline payload exercise actual 64-bit direct mapping rather than
 * just one small data block. The fixture is structural evidence only. */
static void extended_fixture(void){
 memset(image,0,SIZE);populated();
 uint8_t*p=image+FDC+HEAD+MDH;put32(p+8,1);put32(p+12,7);p[16]=0x80;
 put64(image+ROOT+MDH+20,0x13000u);
 image[ROOT_DATA+0x10041]=0x18;
 dirent(0x12040,4,3,"fragment.bin");
 dirent(0x12160,5,3,"sparse.bin");
 dirent(0x12280,6,3,"inline.txt");
 desc(4,3,BLK+9u,12);
 put64(image+ROOT+4u*8192u+MDH+36,2);
 put64(image+ROOT+4u*8192u+5632u+8u,(UINT64_C(15)<<51)|1u);
 memset(image+DATA+12u*BLK,'A',BLK);memcpy(image+DATA+15u*BLK,"fragment!",9);
 desc(5,3,BLK+5u,0);
 put64(image+ROOT+5u*8192u+5632u,0);
 put64(image+ROOT+5u*8192u+5632u+8u,(UINT64_C(13)<<51)|1u);
 memcpy(image+DATA+13u*BLK,"tail!",5);
 desc(6,3,6,0);
 put32(image+ROOT+6u*8192u+MDH+68,4305);
 memcpy(image+ROOT+6u*8192u+MDH+512,"inline",6);
 p=image+FBB+512+MDH;p[17]=0x40;put32(p+8,241);put32(p+12,14);
}
static void check_extended(const IfsFsCoreOpsV2*core,IfsFsHostRuntimeV2*h,struct context*c){
 extended_fixture();h->capabilities|=IFS_FS_HOST_EXCLUSIVE;
 void*v=NULL;IfsFsVolumeInfoV2 info={.structure_bytes=sizeof(info)};
 assert(core->mount(h,&v,&info)==0&&v&&info.free_bytes==241ull*BLK);
 char name[129];uint8_t out[16];IfsFsEntryV2 e={.structure_bytes=sizeof(e),.name=name,.name_capacity=sizeof(name)};
 assert(core->directory_entry(v,"/",3,&e)==0&&!strcmp(name,"fragment.bin"));
 assert(core->directory_entry(v,"/",4,&e)==0&&!strcmp(name,"sparse.bin"));
 assert(core->directory_entry(v,"/",5,&e)==0&&!strcmp(name,"inline.txt"));
 assert(core->directory_entry(v,"/",6,&e)==IFS_FS_END);
 assert(core->stat(v,"/fragment.bin",&e)==0&&e.size_bytes==BLK+9u);
 assert(core->read(v,"/fragment.bin",BLK-3u,out,12)==0&&!memcmp(out,"AAAfragment!",12));
 assert(core->read(v,"/sparse.bin",BLK-3u,out,8)==0&&!out[0]&&!out[1]&&!out[2]&&!memcmp(out+3,"tail!",5));
 assert(core->read(v,"/inline.txt",0,out,6)==0&&!memcmp(out,"inline",6));
 assert(core->unmount(v)==0&&!c->allocs);
 extended_fixture();put32(image+FBB+512+MDH+12,16);
 assert(core->mount(h,&v,&info)==IFS_FS_CORRUPT&&!v&&!c->allocs);
 extended_fixture();put32(image+ROOT_DATA+0x3b8+8,999);
 assert(core->mount(h,&v,&info)==IFS_FS_CORRUPT&&!v&&!c->allocs);
 extended_fixture();put32(image+ROOT+4u*8192u+36,1);
 assert(core->mount(h,&v,&info)==0&&v);
 assert(core->stat(v,"/fragment.bin",&e)==IFS_FS_CORRUPT);
 assert(core->unmount(v)==0&&!c->allocs);
 h->capabilities&=~IFS_FS_HOST_EXCLUSIVE;
}

static void expect(const IfsFsCoreOpsV2 *core,IfsFsHostRuntimeV2*h,struct context*c,int want){
 uint32_t v=99;int rc=core->probe(h,&v);
 if(rc!=want)fprintf(stderr,"expected %d got %d at sector %u\n",want,rc,c->sector);
 assert(rc==want&&(!rc?v!=0:v==0)&&!c->allocs);
}

/* Qualification on a genuine external VMFS6 partition image. The
 * expected bytes are independently obtained from the original
 * datastore before capture. Neither the structural fixture nor an
 * older VMFS3 image is permitted to substitute for this test. */
static void real_media(const IfsFsCoreOpsV2*core,IfsFsHostRuntimeV2*h,struct context*c,const char*path,const char*expected)
{
 int fd=open(expected,O_RDONLY);struct stat st;assert(fd>=0&&!fstat(fd,&st)&&st.st_size>0);
 h->capabilities|=IFS_FS_HOST_EXCLUSIVE;
 void*v=NULL;IfsFsVolumeInfoV2 info={.structure_bytes=sizeof(info)};
 int rc=core->mount(h,&v,&info);if(rc)fprintf(stderr,"VMFS6 independent mount %d\n",rc);
 assert(rc==0&&v&&info.variant==6u&&info.total_bytes&&info.free_bytes<=info.total_bytes);
 char name[129];IfsFsEntryV2 e={.structure_bytes=sizeof(e),.name=name,.name_capacity=sizeof(name)};
 assert(core->stat(v,path,&e)==0&&e.type==IFS_FS_ENTRY_FILE&&e.size_bytes==(uint64_t)st.st_size);
 uint8_t want[4096],got[4096];
 for(uint64_t off=0;off<(uint64_t)st.st_size;){
  uint32_t n=(uint64_t)st.st_size-off>sizeof(want)?sizeof(want):(uint32_t)((uint64_t)st.st_size-off);
  assert(pread(fd,want,n,(off_t)off)==(ssize_t)n);
  assert(core->read(v,path,off,got,n)==0&&!memcmp(want,got,n));
  off+=n;
 }
 assert(core->unmount(v)==0&&!c->allocs&&!close(fd));
 h->capabilities&=~IFS_FS_HOST_EXCLUSIVE;
 puts("Independent VMFS6 payload comparison PASS");
}
int main(int argc,char **argv){
 assert(argc==2||argc==3||argc==5);void *lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);assert(lib);
 IfsFsModuleApiEntryV2 get=(IfsFsModuleApiEntryV2)dlsym(lib,IFS_FS_COMMON_EXPORT_V2);assert(get);
 const IfsFsModuleApiV2*a=get();assert(ifs_fs_api_v2_valid(a,"vmfs6"));
 const IfsFsCoreOpsV2*core=(const void*)ifs_fs_group_v2(a,IFS_FS_GROUP_CORE);
 assert(core&&core->mount&&core->read&&!core->write&&!ifs_fs_group_v2(a,IFS_FS_GROUP_DEFRAG));
 struct context c={.fd=-1,.bytes=SIZE};
 if(argc>=3){struct stat st;c.fd=open(argv[2],O_RDONLY);assert(c.fd>=0&&!fstat(c.fd,&st)&&st.st_size>0);c.bytes=(uint64_t)st.st_size;}
 else {image=calloc(1,SIZE);assert(image);}
 IfsFsHostRuntimeV2 h={.abi_version=2,.structure_bytes=sizeof(h),.capabilities=IFS_FS_HOST_READ_ONLY,.volume_bytes=c.bytes,.maximum_transfer_bytes=4096,.context=&c,.read_at=rd,.allocate=allocate,.release=release};
 for(c.sector=512;c.sector<=4096;c.sector*=8){
  h.logical_sector_bytes=h.physical_sector_bytes=c.sector;
  if(argc>=3){expect(core,&h,&c,IFS_FS_OK);if(argc==5)real_media(core,&h,&c,argv[3],argv[4]);continue;}
  memset(image,0,SIZE);fixture();expect(core,&h,&c,IFS_FS_OK);
  c.fail=1;expect(core,&h,&c,IFS_FS_IO_ERROR);c.fail=0;
  corrupt();expect(core,&h,&c,IFS_FS_CORRUPT);
  memset(image,0,SIZE);fixture();absent();expect(core,&h,&c,IFS_FS_NOT_FOUND);
  memset(image,0,SIZE);fixture();put32(image+VOL+4,5);expect(core,&h,&c,IFS_FS_UNSUPPORTED);
  memset(image,0,SIZE);fixture();put32(image+VOL+4096+144,2);expect(core,&h,&c,IFS_FS_UNSUPPORTED);
  memset(image,0,SIZE);fixture();image[DATA+0x200000+177]^=1;expect(core,&h,&c,IFS_FS_CORRUPT);
  memset(image,0,SIZE);fixture();put32(image+FDC+4,UINT32_MAX);expect(core,&h,&c,IFS_FS_CORRUPT);
  memset(image,0,SIZE);fixture();put32(image+ROOT,0);expect(core,&h,&c,IFS_FS_CORRUPT);

  mount_fixture(core,&h,&c);
  check_extended(core,&h,&c);
 }
 if(c.fd>=0)assert(!close(c.fd));
 free(image);assert(!dlclose(lib));
 puts("VMFS6 native recognition, bounded reads and corruption refusal PASS");return 0;
}
