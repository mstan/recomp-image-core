#include "recomp_image/image_reader.hpp"
#include "recomp_image/image.h"
#include "../src/image_sha256.h"
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
namespace fs = std::filesystem;
static int failures = 0;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; ++failures; } } while (0)
static void write(const fs::path& p, const std::vector<uint8_t>& bytes) {
    std::ofstream f(p,std::ios::binary); f.write((const char*)bytes.data(),bytes.size());
}
static std::vector<uint8_t> load(const fs::path& p) {
    std::ifstream f(p,std::ios::binary); return {std::istreambuf_iterator<char>(f),{}};
}
static void le32(uint8_t* p,uint32_t n) {for(int i=0;i<4;++i)p[i]=(uint8_t)(n>>(8*i));}
static void be32(uint8_t* p,uint32_t n) {for(int i=0;i<4;++i)p[i]=(uint8_t)(n>>(8*(3-i)));}
static void be64(uint8_t* p,uint64_t n) {for(int i=0;i<8;++i)p[i]=(uint8_t)(n>>(8*(7-i)));}
static void record(uint8_t* p,uint32_t lba,uint32_t size,const char* name) {
    size_t n=std::strlen(name); p[0]=(uint8_t)(33+n+(n%2==0));
    le32(p+2,lba); le32(p+10,size); p[32]=(uint8_t)n; std::memcpy(p+33,name,n);
}
static std::vector<uint8_t> iso() {
    std::vector<uint8_t> b(24*2048);
    auto* p=b.data()+16*2048; p[0]=1; std::memcpy(p+1,"CD001",5);p[6]=1;
    std::memcpy(p+40,"SYNTHETIC",9); record(p+156,20,2048,"X");p[156+25]=2;
    record(b.data()+20*2048,21,7,"HELLO.TXT;1");
    std::memcpy(b.data()+21*2048,"fixture",7);return b;
}
static void chd_fixture(const fs::path& p) {
    constexpr uint32_t hunk=2448*4;
    std::vector<uint8_t> b(hunk*2);
    std::memcpy(b.data(),"MComprHD",8); be32(b.data()+8,124);be32(b.data()+12,5);
    be64(b.data()+32,hunk);be64(b.data()+40,124);be64(b.data()+48,128);
    be32(b.data()+56,hunk);be32(b.data()+60,2448);be32(b.data()+124,1);
    const char metadata[]="TRACK:1 TYPE:MODE2_RAW SUBTYPE:NONE FRAMES:4 PREGAP:0 PGTYPE:MODE2_RAW PGSUB:NONE POSTGAP:0";
    std::memcpy(b.data()+128,"CHT2",4);be32(b.data()+132,sizeof(metadata));
    std::memcpy(b.data()+144,metadata,sizeof(metadata));
    for(int i=0;i<4;++i) {
        auto* raw=b.data()+hunk+i*2448; raw[0]=0;
        std::memset(raw+1,255,10);raw[11]=0;raw[15]=2;
        std::memset(raw+24,0x70+i,2048);
    }
    write(p,b);
}
int main() {
    const uint8_t abc_digest[32] = {0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
    uint8_t sha[32]; image_sha256_compute((const uint8_t*)"abc",3,sha);
    CHECK(std::memcmp(sha,abc_digest,32)==0);
    auto original=iso();write("fixture.iso",original);
    recomp_image::ImageReader reader;
    CHECK(reader.Open("fixture.iso"));CHECK(reader.GetVolumeID()=="SYNTHETIC");
    std::array<uint8_t,2352> buffer{};
    CHECK(reader.ReadFile("hello.txt",buffer.data(),buffer.size())==7);
    CHECK(std::memcmp(buffer.data(),"fixture",7)==0);
    CHECK(!reader.ReadView(24,RECOMP_IMAGE_USER_2048,buffer.data(),buffer.size()));
    CHECK(!reader.ReadView(21,RECOMP_IMAGE_USER_2048,buffer.data(),2047));
    CHECK(!reader.ReadView(21,RECOMP_IMAGE_RAW_2352,buffer.data(),buffer.size()));
    std::array<uint8_t,32> before{},again{},changed{};
    CHECK(reader.Identity(before.data()));CHECK(reader.Identity(again.data()));CHECK(before==again);
    CHECK(load("fixture.iso")==original);
    reader.Close();original[21*2048]^=1;write("changed.iso",original);
    CHECK(reader.Open("changed.iso"));CHECK(reader.Identity(changed.data()));CHECK(before!=changed);
    reader.Close();
    auto malformed=iso();malformed[20*2048]=34;malformed[20*2048+32]=250;write("malformed.iso",malformed);
    CHECK(reader.Open("malformed.iso"));CHECK(reader.ListFiles().empty());reader.Close();
    malformed=iso();le32(malformed.data()+16*2048+156+10,UINT32_MAX);write("oversized.iso",malformed);
    CHECK(reader.Open("oversized.iso"));CHECK(reader.ListFiles().empty());reader.Close();
    std::vector<uint8_t> raw(2352);std::memset(raw.data()+1,255,10);raw[15]=1;
    std::memset(raw.data()+16,0x42,2048);write("mode1.bin",raw);
    CHECK(reader.Open("mode1.bin"));CHECK(reader.ReadSector(0,buffer.data()));CHECK(buffer[0]==0x42 && buffer[2047]==0x42);reader.Close();
    raw[15]=2;raw[18]=0x20;std::memset(raw.data()+24,0x35,2324);write("form2.bin",raw);
    CHECK(reader.Open("form2.bin"));CHECK(reader.ReadView(0,RECOMP_IMAGE_MODE2_FORM2_2324,buffer.data(),buffer.size()));
    CHECK(buffer[0]==0x35 && buffer[2323]==0x35);reader.Close();
    std::ofstream("invalid.cue") << "FILE \"mode1.bin\" BINARY\nTRACK 01 MODE1/2352\nINDEX 01 00:00:30\n";
    CHECK(!reader.Open("invalid.cue"));CHECK(!reader.IsOpen());
    std::ofstream("bad-time.cue") << "FILE \"mode1.bin\" BINARY\nTRACK 01 MODE1/2352\nINDEX 01 00:99:99\n";
    CHECK(!reader.Open("bad-time.cue"));
    std::ofstream("comments.cue") << "REM FILE is a comment\nFILE \"mode1.bin\" BINARY\nTRACK 01 MODE1/2352\nINDEX 01 00:00:00\n";
    CHECK(reader.Open("comments.cue"));reader.Close();
    write("tiny.iso",{1,2,3});CHECK(!reader.Open("tiny.iso"));
    chd_fixture("synthetic.chd");
    CHECK(reader.Open("synthetic.chd"));CHECK(reader.GetSectorCount()==4);
    CHECK(reader.ReadSector(3,buffer.data()));CHECK(buffer[0]==0x73);
    CHECK(reader.ReadRawSector(0,buffer.data()));CHECK(buffer[15]==2 && buffer[24]==0x70);
    CHECK(reader.Identity(before.data()));reader.Close();
    write("broken.chd",{1,2,3,4});CHECK(!reader.Open("broken.chd"));
    auto* c=recomp_image_open("fixture.iso");CHECK(c!=nullptr);
    CHECK(recomp_image_sector_count(c)==24);CHECK(recomp_image_track_count(c)==1);
    uint32_t start=99,gap=99;int audio=99;
    CHECK(recomp_image_track(c,1,&start,&gap,&audio));CHECK(start==0 && gap==0 && audio==0);
    CHECK(!recomp_image_track(c,2,&start,&gap,&audio));
    CHECK(recomp_image_read(c,21,RECOMP_IMAGE_USER_2048,buffer.data(),buffer.size()));
    CHECK(recomp_image_identity(c,before.data()));recomp_image_close(c);
    return failures ? 1 : 0;
}
