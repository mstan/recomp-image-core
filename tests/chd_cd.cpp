// SPDX-License-Identifier: MIT
#include "recomp_image/chd_cd.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using recomp_image::ChdCd;
static void be32(uint8_t *p,uint32_t n) { for(unsigned i=0;i<4;++i)p[i]=uint8_t(n>>(24-8*i)); }
static void be64(uint8_t *p,uint64_t n) { for(unsigned i=0;i<8;++i)p[i]=uint8_t(n>>(56-8*i)); }
static void write(const std::vector<uint8_t> &b) {
    std::ofstream f("raw.chd",std::ios::binary|std::ios::trunc);
    f.write(reinterpret_cast<const char *>(b.data()),std::streamsize(b.size()));
}
static void check(bool value) { if(!value)throw std::runtime_error("verified CD contract failed"); }
template<class F> static void rejects(F f) { bool rejected=false;try {f();}catch(const std::exception &){rejected=true;}check(rejected); }
int main() { try {
    constexpr uint32_t hunk=2448*4;
    std::vector<uint8_t> b(hunk*3);
    std::memcpy(b.data(),"MComprHD",8);be32(b.data()+8,124);be32(b.data()+12,5);
    be64(b.data()+32,hunk*2);be64(b.data()+40,124);be64(b.data()+48,132);
    be32(b.data()+56,hunk);be32(b.data()+60,2448);
    be32(b.data()+124,1);be32(b.data()+128,2);
    for(unsigned i=0;i<20;++i){b[64+i]=uint8_t(i+1);b[84+i]=uint8_t(i+31);}
    const char meta[]="TRACK:1 TYPE:MODE1_RAW SUBTYPE:NONE FRAMES:7 PREGAP:0 PGTYPE:MODE1_RAW PGSUB:NONE POSTGAP:0";
    std::memcpy(b.data()+132,"CHT2",4);be32(b.data()+136,sizeof(meta));
    std::memcpy(b.data()+148,meta,sizeof(meta));
    for(unsigned i=0;i<8;++i){auto p=b.data()+hunk+i*2448;std::memset(p+1,255,10);p[15]=1;std::memset(p+16,0x40+i,2048);p[2352]=uint8_t(i+10);}
    write(b);
    { ChdCd disc("raw.chd");const auto &info=disc.info();
      check(info.frames==7 && info.hunk_count==2 && info.hunk_bytes==hunk && info.logical_bytes==2*hunk);
      for(unsigned i=0;i<20;++i)check(info.raw_sha1[i]==i+1 && info.combined_sha1[i]==i+31);
      const auto first=disc.read_hunk(0);check(first.size()==hunk && first[16]==0x40);
      check(first.data()==disc.read_hunk(0).data());
      check(disc.read_frame(6)[2352]==16 && disc.read_mode1(6)[2047]==0x46);
      check(disc.read_mode1(0)[0]==0x40);
      rejects([&]{disc.read_hunk(2);});rejects([&]{disc.read_frame(7);});
    }
    auto bad=b;be32(bad.data()+60,2048);write(bad);rejects([]{ChdCd d("raw.chd");});
    bad=b;bad[104]=1;write(bad);rejects([]{ChdCd d("raw.chd");});
    bad=b;be32(bad.data()+56,17*1024*1024);write(bad);rejects([]{ChdCd d("raw.chd");});
    write(b);return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;} }
