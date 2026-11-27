// SPDX-License-Identifier: MIT
#ifdef RECOMP_IMAGE_SATURN_BINDING
#include "saturnllr/disc.h"
using Disc=saturnllr::Disc;
#else
#include "recomp_image/cached_cue.hpp"
using Disc=recomp_image::CachedCueDisc;
#endif
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
static void check(bool v){if(!v)throw std::runtime_error("cached CUE/ISO contract failed");}
static void both(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;++i){p[i]=uint8_t(n>>(8*i));p[4+i]=uint8_t(n>>(24-8*i));}}
static unsigned record(uint8_t *p,uint32_t lba,uint32_t size,const char *name,unsigned n,uint8_t flags,bool xa=false){
    const unsigned system=33+(n|1),length=system+(xa?14:0);p[0]=uint8_t(length);both(p+2,lba);both(p+10,size);p[25]=flags;p[32]=uint8_t(n);std::memcpy(p+33,name,n);
    if(xa){p[26]=3;p[27]=4;p[system+4]=0x68;p[system+6]='X';p[system+7]='A';p[system+8]=9;}return length;
}
int main(){try{
    std::vector<uint8_t> raw(32*2352),audio(2*2352,0x72);
    for(unsigned i=0;i<32;++i){auto p=raw.data()+i*2352;std::memset(p+1,255,10);p[15]=1;}
    auto data=[&](unsigned lba){return raw.data()+lba*2352+16;};
    std::memset(data(0),' ',2048);std::memcpy(data(0),"SEGA SEGASATURN",14);data(0)[0xe0]=data(0)[0xe1]=data(0)[0xe3]=0;data(0)[0xe2]=0x10;
    auto p=data(16);p[0]=1;std::memcpy(p+1,"CD001",5);p[6]=1;p[129]=p[130]=8;
    record(p+156,20,4096,"\0",1,2);
    unsigned at=0;at+=record(data(20)+at,20,4096,"\0",1,2);at+=record(data(20)+at,20,4096,"\1",1,2);
    at+=record(data(20)+at,24,11,"FIRST.BIN;1",11,0);
    record(data(21),25,12,"SECOND.BIN;1",12,0,true);
    std::memcpy(data(24),"first-bytes",11);
    {std::ofstream f("track.bin",std::ios::binary);f.write(reinterpret_cast<const char*>(raw.data()),raw.size());}
    {std::ofstream f("audio.bin",std::ios::binary);f.write(reinterpret_cast<const char*>(audio.data()),audio.size());}
    std::ofstream("disc.cue")<<"FILE \"track.bin\" BINARY\nTRACK 01 MODE1/2352\nINDEX 01 00:00:00\nFILE \"audio.bin\" BINARY\nTRACK 02 AUDIO\nINDEX 00 00:00:00\nINDEX 01 00:00:01\n";
    Disc d("disc.cue");check(d.tracks().size()==2 && d.tracks()[1].start_lba==33);
    check(d.view_raw(24)==nullptr);check(d.read_raw(32)[0]==0x72);
    const auto root=d.root_directory();auto first=d.read_directory(root.lba,root.size,2,3);check(first.size()==3 && first[2].name=="FIRST.BIN;1");
    auto next=d.read_directory(root.lba,root.size,3,3);check(next.size()==3 && next[2].name=="SECOND.BIN;1");
    check(next[2].attributes==0x68 && next[2].unit==3 && next[2].gap==4 && next[2].number==9);
    d.cache_tracks();const auto view=d.view_raw(24);check(view && view[16]=='f');
    d.read_raw(33);d.read_mode1(21);d.cache_tracks();check(d.view_raw(24)==view && view[16]=='f');
#ifdef RECOMP_IMAGE_SATURN_BINDING
    check(d.read_saturn_header().initial_program_size==0x1000 && d.first_read_file().lba==24);
    SaturnCue *cue=saturn_cue_open("disc.cue");check(cue);const auto source=saturn_cue_source(cue);check(source && source->layout.leadout_fad==184);
    uint8_t sector[2352];check(source->read_sector(source->user,174,sector) && sector[16]=='f');
    SaturnDiscFileInfo entries[256]{};unsigned count=0;bool more=true;check(source->read_directory(source->user,nullptr,2,entries,&count,&more));check(count==2 && !more && entries[3].attributes==0x68);
    if(source->view_sector){const auto v=source->view_sector(source->user,174);check(v && v[16]=='f');}
    saturn_cue_close(cue);
#endif
    return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
