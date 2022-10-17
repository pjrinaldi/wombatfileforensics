#include "extfs.h"

void GetContentBlocks(std::ifstream* devicebuffer, uint32_t blocksize, uint64_t curoffset, uint32_t incompatflags, std::vector<uint32_t>* blocklist)
{
    uint8_t* iflags = new uint8_t[4];
    devicebuffer->seekg(curoffset + 32);
    devicebuffer->read((char*)iflags, 4);
    uint32_t inodeflags = (uint32_t)iflags[0] | (uint32_t)iflags[1] << 8 | (uint32_t)iflags[2] << 16 | (uint32_t)iflags[3] << 24;
    delete[] iflags;
    if(incompatflags & 0x40 && inodeflags & 0x80000) // FS USES EXTENTS AND INODE USES EXTENTS
    {
        //std::cout << "uses extents\n";
        uint8_t* eentry = new uint8_t[2];
        devicebuffer->seekg(curoffset + 42);
        devicebuffer->read((char*)eentry, 2);
        uint16_t extententries = (uint16_t)eentry[0] | (uint16_t)eentry[1] << 8;
        delete[] eentry;
        uint8_t* edepth = new uint8_t[2];
        devicebuffer->seekg(curoffset + 46);
        devicebuffer->read((char*)edepth, 2);
        uint16_t extentdepth = (uint16_t)edepth[0] | (uint16_t)edepth[1] << 8;
        delete[] edepth;
        if(extentdepth == 0) // use ext4_extent
        {
            for(uint16_t i=0; i < extententries; i++)
            {
                uint8_t* strtblk = new uint8_t[6];
                devicebuffer->seekg(curoffset + 58 + i*12);
                devicebuffer->read((char*)strtblk, 6);
                uint16_t starthi = (uint16_t)strtblk[0] | (uint16_t)strtblk[1] << 8;
                uint32_t startlo = (uint32_t)strtblk[2] | (uint32_t)strtblk[3] << 8 | (uint32_t)strtblk[4] << 16 | (uint32_t)strtblk[5] << 24;
                delete[] strtblk;
                //std::cout << "start block: " << ((uint64_t)starthi >> 32) + startlo << std::endl;
                blocklist->push_back(((uint64_t)starthi >> 32) + startlo); // block #, not bytes
            }
        }
        else // use ext4_extent_idx
        {
            std::vector<uint32_t> leafnodes;
            leafnodes.clear();
            for(uint16_t i=0; i < extententries; i++)
            {
                uint8_t* lnode = new uint8_t[4];
                devicebuffer->seekg(curoffset + 56 + i*12);
                devicebuffer->read((char*)lnode, 4);
                leafnodes.push_back((uint32_t)lnode[0] | (uint32_t)lnode[1] << 8 | (uint32_t)lnode[2] << 16 | (uint32_t)lnode[3] << 24);
                delete[] lnode;
            }
            for(int i=0; i < leafnodes.size(); i++)
            {
                uint8_t* lentry = new uint8_t[2];
                devicebuffer->seekg(leafnodes.at(i) * blocksize + 2);
                devicebuffer->read((char*)lentry, 2);
                uint16_t lextententries = (uint16_t)lentry[0] | (uint16_t)lentry[1] << 8;
                delete[] lentry;
                uint8_t* ldepth = new uint8_t[2];
                devicebuffer->seekg(leafnodes.at(i) * blocksize + 6);
                devicebuffer->read((char*)ldepth, 2);
                uint16_t lextentdepth = (uint16_t)ldepth[0] | (uint16_t)ldepth[1] << 8;
                delete[] ldepth;
                if(extentdepth == 0) // use ext4_extent
                {
                    for(uint16_t j=0; j < lextententries; j++)
                    {
                        uint8_t* strtblk = new uint8_t[6];
                        devicebuffer->seekg(leafnodes.at(i) * blocksize + 18 + j*12);
                        devicebuffer->read((char*)strtblk, 6);
                        uint16_t starthi = (uint16_t)strtblk[0] | (uint16_t)strtblk[1] << 8;
                        uint32_t startlo = (uint32_t)strtblk[2] | (uint32_t)strtblk[3] << 8 | (uint32_t)strtblk[4] << 16 | (uint32_t)strtblk[5] << 24;
                        delete[] strtblk;
                        //std::cout << "start block: " << ((uint64_t)starthi >> 32) + startlo << std::endl;
                        blocklist->push_back(((uint64_t)starthi >> 32) + startlo); // block #, not bytes
                    }
                }
                else // use ext4_extent_idx
                {
                    std::cout << "repeat leafnode execise here...";
                    break;
                }
            }
        }
    }
    else // USES DIRECT AND INDIRECT BLOCKS
    {
        //std::cout << "uses direct and indirect blocks\n";
        for(int i=0; i < 12; i++)
        {
            uint8_t* dirblk = new uint8_t[4];
            devicebuffer->seekg(curoffset + 40 + i*4);
            devicebuffer->read((char*)dirblk, 4);
            uint32_t curdirblk = (uint32_t)dirblk[0] | (uint32_t)dirblk[1] << 8 | (uint32_t)dirblk[2] << 16 | (uint32_t)dirblk[3] << 24;
            delete[] dirblk;
            if(curdirblk > 0)
                blocklist->push_back(curdirblk);
        }
        uint8_t* indirect = new uint8_t[12];
        devicebuffer->seekg(curoffset + 88);
        devicebuffer->read((char*)indirect, 12);
        uint32_t sindirect = (uint32_t)indirect[0] | (uint32_t)indirect[1] << 8 | (uint32_t)indirect[2] << 16 | (uint32_t)indirect[3] << 24;
        uint32_t dindirect = (uint32_t)indirect[4] | (uint32_t)indirect[5] << 8 | (uint32_t)indirect[6] << 16 | (uint32_t)indirect[7] << 24;
        uint32_t tindirect = (uint32_t)indirect[8] | (uint32_t)indirect[9] << 8 | (uint32_t)indirect[10] << 16 | (uint32_t)indirect[11] << 24;
        delete[] indirect;
        if(sindirect > 0)
        {
            for(unsigned int i=0; i < blocksize / 4; i++)
            {
                uint8_t* csd = new uint8_t[4];
                devicebuffer->seekg(sindirect * blocksize + i*4);
                devicebuffer->read((char*)csd, 4);
                uint32_t cursdirect = (uint32_t)csd[0] | (uint32_t)csd[1] << 8 | (uint32_t)csd[2] << 16 | (uint32_t)csd[3] << 24;
                delete[] csd;
                if(cursdirect > 0)
                    blocklist->push_back(cursdirect);
            }
        }
        if(dindirect > 0)
        {
            std::vector<uint32_t> sinlist;
            sinlist.clear();
            for(unsigned int i=0; i < blocksize / 4; i++)
            {
                uint8_t* sind = new uint8_t[4];
                devicebuffer->seekg(dindirect * blocksize + i*4);
                devicebuffer->read((char*)sind, 4);
                uint32_t sindirect = (uint32_t)sind[0] | (uint32_t)sind[1] << 8 | (uint32_t)sind[2] << 16 | (uint32_t)sind[3] << 24;
                delete[] sind;
                if(sindirect > 0)
                    sinlist.push_back(sindirect);
            }
            for(int i=0; i < sinlist.size(); i++)
            {
                for(unsigned int j=0; j < blocksize / 4; j++)
                {
                    uint8_t* sd = new uint8_t[4];
                    devicebuffer->seekg(sinlist.at(i) * blocksize + j*4);
                    devicebuffer->read((char*)sd, 4);
                    uint32_t sdirect = (uint32_t)sd[0] | (uint32_t)sd[1] << 8 | (uint32_t)sd[2] << 16 | (uint32_t)sd[3] << 24;
                    delete[] sd;
                    if(sdirect > 0)
                        blocklist->push_back(sdirect);
                }
            }
            sinlist.clear();
        }
        if(tindirect > 0)
        {
            std::vector<uint32_t> dinlist;
            std::vector<uint32_t> sinlist;
            dinlist.clear();
            sinlist.clear();
            for(unsigned int i=0; i < blocksize / 4; i++)
            {
                uint8_t* did = new uint8_t[4];
                devicebuffer->seekg(tindirect * blocksize + i*4);
                devicebuffer->read((char*)did, 4);
                uint32_t dindirect = (uint32_t)did[0] | (uint32_t)did[1] << 8 | (uint32_t)did[2] << 16 | (uint32_t)did[3] << 24;
                delete[] did;
                if(dindirect > 0)
                    dinlist.push_back(dindirect);
            }
            for(int i=0; i < dinlist.size(); i++)
            {
                for(unsigned int j=0; j < blocksize / 4; j++)
                {
                    uint8_t* sid = new uint8_t[4];
                    devicebuffer->seekg(dinlist.at(i) * blocksize + j*4);
                    devicebuffer->read((char*)sid, 4);
                    uint32_t sindirect = (uint32_t)sid[0] | (uint32_t)sid[1] << 8 | (uint32_t)sid[2] << 16 | (uint32_t)sid[3] << 24;
                    delete[] sid;
                    if(sindirect > 0)
                        sinlist.push_back(sindirect);
                }
                for(int j=0; j < sinlist.size(); j++)
                {
                    for(unsigned int k=0; k < blocksize / 4; k++)
                    {
                        uint8_t* sd = new uint8_t[4];
                        devicebuffer->seekg(sinlist.at(j) * blocksize + k*4);
                        devicebuffer->read((char*)sd, 4);
                        uint32_t sdirect = (uint32_t)sd[0] | (uint32_t)sd[1] << 8 | (uint32_t)sd[2] << 16 | (uint32_t)sd[3] << 24;
                        delete[] sd;
                        if(sdirect > 0)
                            blocklist->push_back(sdirect);
                    }
                }
            }
        }
    }
}

std::string ConvertBlocksToExtents(std::vector<uint32_t>* blocklist, uint32_t blocksize)
{
    std::string extentstr = "";
    int blkcnt = 1;
    unsigned int startvalue = blocklist->at(0);
    for(int i=1; i < blocklist->size(); i++)
    {
        unsigned int oldvalue = blocklist->at(i-1);
        unsigned int newvalue = blocklist->at(i);
        if(newvalue - oldvalue == 1)
            blkcnt++;
        else
        {
            extentstr += std::to_string(startvalue * blocksize) + "," + std::to_string(blkcnt * blocksize) + ";";
            startvalue = blocklist->at(i);
            blkcnt = 1;
        }
        if(i == blocklist->size() - 1)
        {
            extentstr += std::to_string(startvalue * blocksize) + "," + std::to_string(blkcnt * blocksize) + ";";
            startvalue = blocklist->at(i);
            blkcnt = 1;
        }
    }
    if(blocklist->size() == 1)
    {
        extentstr += std::to_string(startvalue * blocksize) + "," + std::to_string(blkcnt * blocksize) + ";";
    }

    return extentstr;
}


void ParseSuperBlock(std::ifstream* rawcontent, sbinfo* cursb)
{
    uint64_t sboff = 1024; // start of superblock
    // GET BLOCK SIZE
    uint8_t* bsize = new uint8_t[4];
    uint32_t bsizepow = 0;
    ReadContent(rawcontent, bsize, sboff + 24, 4);
    ReturnUint32(&bsizepow, bsize);
    delete[] bsize;
    uint32_t blocksize = 1024 * pow(2, bsizepow);
    std::cout << "block size: " << blocksize << std::endl;
    cursb->blocksize = blocksize;
    // GET INODE SIZE
    uint8_t* isize = new uint8_t[2];
    uint16_t inodesize = 0;
    ReadContent(rawcontent, isize, sboff + 88, 2);
    ReturnUint16(&inodesize, isize);
    delete[] isize;
    std::cout << "inode size: " << inodesize << std::endl;
    cursb->inodesize = inodesize;
    // GET INODES PER BLOCK GROUP
    uint8_t* ipbg = new uint8_t[4];
    uint32_t inodesperblockgroup = 0;
    ReadContent(rawcontent, ipbg, sboff + 40, 4);
    ReturnUint32(&inodesperblockgroup, ipbg);
    cursb->inodesperblockgroup = inodesperblockgroup;
    std::cout << "inodes per block group: " << inodesperblockgroup << std::endl;
    // GET EXT FLAGS AND EXT VERSION (2/3/4)
    uint8_t extversion  = 2;
    uint8_t* flags = new uint8_t[4];
    uint32_t compatflags = 0;
    uint32_t incompatflags = 0;
    uint32_t readonlyflags = 0;
    ReadContent(rawcontent, flags, sboff + 92, 4);
    ReturnUint32(&compatflags, flags);
    ReadContent(rawcontent, flags, sboff + 96, 4);
    ReturnUint32(&incompatflags, flags);
    ReadContent(rawcontent, flags, sboff + 100, 4);
    ReturnUint32(&readonlyflags, flags);
    delete[] flags;
    if((compatflags & 0x00000200UL != 0) || (incompatflags & 0x0001f7c0UL != 0) || (readonlyflags & 0x00000378UL != 0))
        extversion = 4;
    else if((compatflags & 0x00000004UL != 0) || (incompatflags & 0x0000000cUL != 0))
        extversion = 3;
    std::cout << "ext version: EXT" << (unsigned int)extversion << std::endl;
    cursb->cflags = compatflags;
    cursb->icflags = incompatflags;
    cursb->roflags = readonlyflags;
    cursb->extversion = extversion;
    // GET EXTFS REVISION
    std::string revstr;
    uint8_t* rbig = new uint8_t[4];
    uint32_t revbig = 0;
    ReadContent(rawcontent, rbig, sboff + 76, 4);
    ReturnUint32(&revbig, rbig);
    delete[] rbig;
    uint8_t* rsml = new uint8_t[2];
    uint16_t revsml = 0;
    ReadContent(rawcontent, rsml, sboff + 62, 2);
    ReturnUint16(&revsml, rsml);
    delete[] rsml;
    revstr.append(std::to_string(revbig));
    revstr.append(".");
    revstr.append(std::to_string(revsml));
    float revision = std::stof(revstr);
    cursb->revision = revision;
    std::cout << "revision: " << revision << std::endl;
    // GET FILE SYSTEM BLOCK COUNT
    uint8_t* fbc = new uint8_t[4];
    ReadContent(rawcontent, fbc, sboff + 4, 4);
    uint32_t fsblkcnt = 0;
    ReturnUint32(&fsblkcnt, fbc);
    delete[] fbc;
    cursb->fsblockcount = fsblkcnt;
    std::cout << "File system block count: " << fsblkcnt << std::endl;
    // GET BLOCKS PER BLOCK GROUP
    uint8_t* bpg = new uint8_t[4];
    uint32_t blockspergroup = 0;
    ReadContent(rawcontent, bpg, sboff + 32, 4);
    ReturnUint32(&blockspergroup, bpg);
    delete[] bpg;
    cursb->blockspergroup = blockspergroup;
    std::cout << "blocks per group: " << blockspergroup << std::endl;
    // GET NUMBER OF BLOCK GROUPS
    uint32_t blockgroupcount = fsblkcnt / blockspergroup;
    unsigned int blockgroupcountrem = fsblkcnt % blockspergroup;
    if(blockgroupcountrem > 0)
        blockgroupcount++;
    if(blockgroupcount == 0)
        blockgroupcount = 1;
    cursb->blockgroupcount = blockgroupcount;
    std::cout << "number of block groups: " << blockgroupcount << std::endl;
}

/*
void ParseExtInit(std::ifstream* devicebuffer, extinfo* curextinfo)
{
    //uint64_t curextinode = 2;
    // IF THERE IS COMMON ELEMENTS MOVE INTO ParseExtInit();
    // MOVE A LOT OF THIS INTO ParseExtPath();
    // MOVE A LOT OF THIS INTO ParseExtFile();
    //uint32_t blocksize = 0;
    uint32_t bsizepow = 0;
    uint8_t* bsize = new uint8_t[4];
    devicebuffer->seekg(1024 + 24);
    devicebuffer->read((char*)bsize, 4);
    bsizepow = (uint32_t)bsize[0] | (uint32_t)bsize[1] << 8 | (uint32_t)bsize[2] << 16 | (uint32_t)bsize[3] << 24;
    delete[] bsize;
    //blocksize = 1024 * pow(2, bsizepow);
    curextinfo->blocksize = 1024 * pow(2, bsizepow);
    //std::cout << "block size: " << bsizepow << std::endl;
    //std::cout << "blocksize: " << curextinfo->blocksize << std::endl;
    uint8_t* isize = new uint8_t[2];
    //uint16_t inodesize = 0;
    devicebuffer->seekg(1024 + 88);
    devicebuffer->read((char*)isize, 2);
    //inodesize = (uint16_t)isize[0] | (uint16_t)isize[1] << 8;
    curextinfo->inodesize = (uint16_t)isize[0] | (uint16_t)isize[1] << 8;
    delete[] isize;
    //std::cout << "inode size: " << curextinfo->inodesize << std::endl;
    //uint32_t blkgrpinodecnt = 0;
    uint8_t* bgicnt = new uint8_t[4];
    devicebuffer->seekg(1024 + 40);
    devicebuffer->read((char*)bgicnt, 4);
    curextinfo->blkgrpinodecnt = (uint32_t)bgicnt[0] | (uint32_t)bgicnt[1] << 8 | (uint32_t)bgicnt[2] << 16 | (uint32_t)bgicnt[3] << 24;
    delete[] bgicnt;
    //std::cout << "block group inode count: " << blkgrpinodecnt << std::endl;
    uint32_t rootinodetableaddress = 0;
    uint8_t* ritaddr = new uint8_t[4];
    if(curextinfo->blkgrpinodecnt > 2)
        devicebuffer->seekg(1024 + 1032);
    else
        devicebuffer->seekg(1024 + 1064);
    devicebuffer->read((char*)ritaddr, 4);
    rootinodetableaddress = (uint32_t)ritaddr[0] | (uint32_t)ritaddr[1] << 8 | (uint32_t)ritaddr[2] << 16 | (uint32_t)ritaddr[3] << 24;
    delete[] ritaddr;
    //std::cout << "root inode table address: " << rootinodetableaddress << std::endl;
    uint8_t exttype = 2;
    uint8_t* extflags = new uint8_t[12];
    devicebuffer->seekg(1024 + 92);
    devicebuffer->read((char*)extflags, 12);
    uint32_t compatflags = (uint32_t)extflags[0] | (uint32_t)extflags[1] << 8 | (uint32_t)extflags[2] << 16 | (uint32_t)extflags[3] << 24;
    curextinfo->incompatflags = (uint32_t)extflags[4] | (uint32_t)extflags[5] << 8 | (uint32_t)extflags[6] << 16 | (uint32_t)extflags[7] << 24;
    uint32_t readonlyflags = (uint32_t)extflags[8] | (uint32_t)extflags[9] << 8 | (uint32_t)extflags[10] << 16 | (uint32_t)extflags[11] << 24;
    delete[] extflags;
    if((compatflags & 0x00000200UL != 0) || (curextinfo->incompatflags & 0x0001f7c0UL != 0) || (readonlyflags & 0x00000378UL != 0))
        exttype = 4;
    else if((compatflags & 0x00000004UL != 0) || (curextinfo->incompatflags & 0x0000000cUL != 0))
        exttype = 3;
    //std::cout << "exttype: EXT" << (unsigned int)exttype << std::endl;
    std::string revstr;
    uint8_t* revbig = new uint8_t[4];
    devicebuffer->seekg(1100);
    devicebuffer->read((char*)revbig, 4);
    revstr.append(std::to_string((uint32_t)revbig[0] | (uint32_t)revbig[1] << 8 | (uint32_t)revbig[2] << 16 | (uint32_t)revbig[3] << 24));
    delete[] revbig;
    revstr.append(".");
    uint8_t* revsml = new uint8_t[2];
    devicebuffer->seekg(1086);
    devicebuffer->read((char*)revsml, 2);
    revstr.append(std::to_string((uint16_t)revsml[0] | (uint16_t)revsml[1] << 8));
    delete[] revsml;
    curextinfo->revision = std::stof(revstr);
    //std::cout << "revstr: " << revstr << std::endl;
    //std::cout << "rev float: " << revision << std::endl;
    uint16_t grpdescsize = 32;
    if(curextinfo->incompatflags & 0x80)
    {
        uint8_t* grpdesc = new uint8_t[2];
        devicebuffer->seekg(1024 + 254);
        devicebuffer->read((char*)grpdesc, 2);
        grpdescsize = (uint16_t)grpdesc[0] | (uint16_t)grpdesc[1] << 8;
        delete[] grpdesc;
    }
    //std::cout << "group descriptor size: " << grpdescsize << std::endl;
    uint8_t* fsblk = new uint8_t[4];
    devicebuffer->seekg(1024 + 4);
    devicebuffer->read((char*)fsblk, 4);
    uint32_t fsblkcnt = (uint32_t)fsblk[0] | (uint32_t)fsblk[1] << 8 | (uint32_t)fsblk[2] << 16 | (uint32_t)fsblk[3] << 24;
    delete[] fsblk;
    std::cout << "fs block cnt:" << fsblkcnt << std::endl;
    uint8_t* blkgrp = new uint8_t[4];
    devicebuffer->seekg(1024 + 32);
    devicebuffer->read((char*)blkgrp, 4);
    uint32_t blkgrpblkcnt = (uint32_t)blkgrp[0] | (uint32_t)blkgrp[1] << 8 | (uint32_t)blkgrp[2] << 16 | (uint32_t)blkgrp[3] << 24;
    delete[] blkgrp;
    std::cout << "block group block count: " << blkgrpblkcnt << std::endl;
    uint32_t blockgroupcount = fsblkcnt / blkgrpblkcnt;
    unsigned int blkgrpcntrem = fsblkcnt % blkgrpblkcnt;
    if(blkgrpcntrem > 0)
        blockgroupcount++;
    if(blockgroupcount == 0)
        blockgroupcount = 1;
    std::cout << "block group count: " << blockgroupcount << std::endl;
    //std::vector<uint32_t> inodeaddrtables;
    //curextinfo->inodeaddrtables.clear();
    for(unsigned int i=0; i < blockgroupcount; i++)
    {
        uint8_t* iaddr = new uint8_t[4];
        if(curextinfo->blocksize == 1024)
            devicebuffer->seekg(2*curextinfo->blocksize + i * grpdescsize + 8);
        else
            devicebuffer->seekg(curextinfo->blocksize + i * grpdescsize + 8);
        devicebuffer->read((char*)iaddr, 4);
        curextinfo->inodeaddrtables += std::to_string((uint32_t)iaddr[0] | (uint32_t)iaddr[1] << 8 | (uint32_t)iaddr[2] << 16 | (uint32_t)iaddr[3] << 24);
        if(i < blockgroupcount - 1)
            curextinfo->inodeaddrtables += ",";
        //curextinfo->inodeaddrtables.push_back((uint32_t)iaddr[0] | (uint32_t)iaddr[1] << 8 | (uint32_t)iaddr[2] << 16 | (uint32_t)iaddr[3] << 24);
    }
    //std::cout << "inode address tables:" << curextinfo->inodeaddrtables << std::endl;
}
*/

//uint64_t ParseExtPath(std::ifstream* devicebuffer, extinfo* curextinfo, uint64_t nextinode, std::string childpath)

void ReturnChildInode(std::ifstream* rawcontent, sbinfo* cursb, std::string* dirlayout, uint64_t* childinode, std::string* childpath, uint64_t* inodeoffset)
{
    // GET INODE FLAGS
    uint8_t* iflags = new uint8_t[4];
    uint32_t inodeflags = 0;
    ReadContent(rawcontent, iflags, *inodeoffset + 32, 4);
    ReturnUint32(&inodeflags, iflags);
    delete[] iflags;
    // GET THE DIRECTORY CONTENT OFFSETS/LENGTHS AND THEN LOOP OVER THEM
    std::vector<std::string> dirlaylist;
    dirlaylist.clear();
    std::istringstream dll(*dirlayout);
    std::string dls;
    while(getline(dll, dls, ';'))
        dirlaylist.push_back(dls);
    for(int i=0; i < dirlaylist.size(); i++)
    {
        std::size_t found = dirlaylist.at(i).find(",");
        uint64_t curdiroffset = std::stoull(dirlaylist.at(i).substr(0, found));
        uint64_t curdirlength = std::stoull(dirlaylist.at(i).substr(found+1));
        std::cout << "curdir offset: " << curdiroffset << " curdir length: " << curdirlength << std::endl;
        uint64_t coff = curdiroffset + 24; // SKIP . AND .. ENTRIES WHICH ARE ALWAYS 1ST 2 AND 12 BYTES EACH
        if(inodeflags & 0x1000) // hash trees in use
        {
            coff = curdiroffset + 40; // THIS SHOULD ACCOUNT FOR HASH TREE DEPT OF 0, NEED TO TEST FOR 1 - 3
            std::cout << "hash trees are in use\n";
        }
        while(coff < curdiroffset + curdirlength - 8)
        {
            // GET CHILD INODE NUMBER
            uint8_t* ei = new uint8_t[4];
            uint32_t extinode = 0;
            ReadContent(rawcontent, ei, coff, 4);
            ReturnUint32(&extinode, ei);
            delete[] ei;
            //std::cout << "ext inode: " << extinode << std::endl;
            if(extinode > 0)
            {
                // GET CHILD ENTRY LENGTH
                uint8_t* el = new uint8_t[2];
                uint16_t entrylength = 0;
                ReadContent(rawcontent, el, coff + 4, 2);
                ReturnUint16(&entrylength, el);
                delete[] el;
                // GET CHILD NAME LENGTH
                uint16_t namelength = 0;
                if(cursb->icflags & 0x02 || cursb->revision > 0.4)
                {
                    uint8_t* nl = new uint8_t[1];
                    ReadContent(rawcontent, nl, coff + 6, 1);
                    namelength = (uint16_t)nl[0];
                    delete[] nl;
                }
                else
                {
                    uint8_t* nl = new uint8_t[2];
                    ReadContent(rawcontent, nl, coff + 6, 2);
                    ReturnUint16(&namelength, nl);
                    delete[] nl;
                }
                //std::cout << "namelength: " << namelength << std::endl;
                // GET FILENAME
                uint8_t* fname = new uint8_t[namelength+1];
                ReadContent(rawcontent, fname, coff + 8, namelength);
                fname[namelength] = '\0';
                std::string filename((char*)fname);
                //std::cout << "fname: " << (char*)fname << std::endl;
                //std::cout << "filename: " << filename << std::endl;
                if(filename.find(*childpath) != std::string::npos)
                    *childinode = extinode;
                
                coff += entrylength;
            }
            else
                break;
        }
    }
}

uint64_t ParseExtPath(std::ifstream* rawcontent, sbinfo* cursb, uint64_t curinode, std::string childpath)
{
    uint64_t childinode = 0;
    std::cout << "curinode: " << curinode << " child path to find: " << childpath << std::endl;
    // DETERMINE THE CURINODE BLOCK GROUP
    uint64_t curblockgroup = (curinode - 1) / cursb->inodesperblockgroup;
    std::cout << "cur inode block group: " << curblockgroup << std::endl;
    uint64_t localinode = (curinode - 1) % cursb->inodesperblockgroup;
    std::cout << "local inode index: " << localinode << std::endl;
    // DETERMINE THE GROUP DESCRIPTOR ENTRY SIZE FOR GROUP DESCRIPTOR TABLE
    uint16_t groupdescriptorentrysize = 32;
    if(cursb->icflags & 0x80)
    {
        uint8_t* gdes = new uint8_t[2];
        ReadContent(rawcontent, gdes, 1024 + 254, 2);
        ReturnUint16(&groupdescriptorentrysize, gdes);
        delete[] gdes;
    }
    // GET TO THE GROUP DESCRIPTOR BLOCK TABLE STARTING POINT, WHICH IS 4096 + groupdescriptorentrysize * curinodeblockgroup
    std::cout << "group descriptor entry size: " << groupdescriptorentrysize << std::endl;
    uint64_t curblockdescriptortableentryoffset = cursb->blocksize + groupdescriptorentrysize * curblockgroup;
    std::cout << "cur block descriptor table entry offset: " << curblockdescriptortableentryoffset << std::endl;
    // +8 get's the inode table block...
    uint8_t* sbit = new uint8_t[4];
    uint32_t startingblockforinodetable = 0;
    ReadContent(rawcontent, sbit, curblockdescriptortableentryoffset + 8, 4);
    ReturnUint32(&startingblockforinodetable, sbit);
    delete[] sbit;
    std::cout << "Starting block for inode table: " << startingblockforinodetable << std::endl;
    uint64_t curoffset = startingblockforinodetable * cursb->blocksize + cursb->inodesize * localinode;
    std::cout << "Current Offset to Inode Table Entry for Current Directory: " << curoffset << std::endl;
    // GET INODE CONTENT BLOCKS
    std::vector<uint32_t> blocklist;
    blocklist.clear();
    std::cout << "get content blocks\n";
    GetContentBlocks(rawcontent, cursb->blocksize, curoffset, cursb->icflags, &blocklist);
    std::cout << "blocklist count: " << blocklist.size() << std::endl;
    std::cout << "convert blocks to extents\n";
    std::string dirlayout = ConvertBlocksToExtents(&blocklist, cursb->blocksize);
    std::cout << "dir layout: " << dirlayout << std::endl;
    blocklist.clear();
    // 4571136 or 4571392 is byte offset to the root directory inode
    // 38125568 is byte offset to the root directory content
    ReturnChildInode(rawcontent, cursb, &dirlayout, &childinode, &childpath, &curoffset);


    // GET GROUP DESCRIPTOR ENTRY FOR CURRENT BLOCK GROUP... (group descriptor table starts at byte offset 4096)
    // THIS IS WRONG, I NEED TO BE WITHIN THE GROUP DESCRIPTOR TABLE... AND FIND THE OFFSET TO THE WRITE BLOCK GROUP, THEN
    // GET THE 8 BYTES I NEED...TO GET THE INODE TABLE BLOCK
    
    /*
    uint64_t curblockdescripttableentryoffset = cursb->blocksize + groupdescriptorentrysize * curblockgroup + 8;
    std::cout << "curblockdescriptortableentryoffset: " << curblockdescripttableentryoffset << std::endl;
    uint64_t curblockgroupoffset = curblockdescripttableentryoffset;
    //uint64_t curblockgroupoffset = curblockgroup * cursb->blockspergroup;
    if(curblockgroup == 0)
    {
        curblockgroupoffset = cursb->blocksize;
        //curblockgroup = 1;
    }
    std::cout << "cur offset to block group: " << curblockgroupoffset << std::endl;
    // GET STARTING BLOCK FOR INODE TABLE FOR THIS BLOCK GROUP
    std::cout << "starting block for inode table for this block group: " << curblockgroupoffset << std::endl;
    uint8_t* sbit = new uint8_t[4];
    uint32_t startingblockforinodetable = 0;
    ReadContent(rawcontent, sbit, curblockgroupoffset, 4);
    ReturnUint32(&startingblockforinodetable, sbit);
    delete[] sbit;
    std::cout << "Starting block for inode table: " << startingblockforinodetable << std::endl;
    //GET INODE ENTRY TO DETERMINE THE DIRECTORY CONTENTS
    uint64_t curoffset = startingblockforinodetable * cursb->blocksize * cursb->blockspergroup + cursb->inodesize * localinode;
    std::cout << "Current Offset to Inode Table Entry for Current Directory: " << curoffset << std::endl;
    // GET INODE CONTENT BLOCKS
    std::vector<uint32_t> blocklist;
    blocklist.clear();
    std::cout << "get content blocks\n";
    GetContentBlocks(rawcontent, cursb->blocksize, curoffset, cursb->icflags, &blocklist);
    std::cout << "convert blocks to extents\n";
    std::string dirlayout = ConvertBlocksToExtents(&blocklist, cursb->blocksize);
    std::cout << "dir layout: " << dirlayout << std::endl;
    blocklist.clear();
    // 4571136 or 4571392 is byte offset to the root directory inode
    // 38125568 is byte offset to the root directory content
    ReturnChildInode(rawcontent, cursb, &dirlayout, &childinode, &childpath, &curoffset);
    */

    return childinode;
}
    /*
    unsigned int bgnumber = 0;
    uint64_t inodestartingblock = 0;
    std::cout << "curinode: " << nextinode << std::endl;
    std::vector<uint32_t> inodeaddrlist;
    inodeaddrlist.clear();
    std::istringstream ial(curextinfo->inodeaddrtables);
    std::string iaddr;
    while(getline(ial, iaddr, ','))
            inodeaddrlist.push_back(stoull(iaddr));
    std::cout << "inode address table size:" << inodeaddrlist.size() << std::endl;
    for(int i=1; i < inodeaddrlist.size(); i++)
    {
        if(nextinode < i*curextinfo->blkgrpinodecnt)
        {
            std::cout << "curinode: " << nextinode << " blkgrp: " << i << " inode count: " << i*curextinfo->blkgrpinodecnt << std::endl;
            //inodestartingblock = inodeaddrlist.at(i);
            inodestartingblock = inodeaddrlist.at(i-1);
            bgnumber = i-1;
            //bgnumber = i;
            break;
        }
    }
    std::cout << "inode starting block: " << inodestartingblock << std::endl;
    std::cout << "block group number: " << (unsigned int)bgnumber << std::endl;
    std::cout << "blkgrpinodecnt: " << curextinfo->blkgrpinodecnt << std::endl;
    uint64_t relcurinode = nextinode - 1  - (bgnumber * curextinfo->blkgrpinodecnt);
    //quint64 curoffset = curstartsector * 512 + inodestartingblock * blocksize + inodesize * relcurinode;
    uint64_t curoffset = inodestartingblock * curextinfo->blocksize + curextinfo->inodesize * relcurinode;
    std::cout << "relcurinode: " << relcurinode << std::endl;
    std::cout << "curoffset: " << curoffset << std::endl;
    std::vector<uint32_t> blocklist;
    blocklist.clear();
    std::cout << "get content blocks\n";
    GetContentBlocks(devicebuffer, curextinfo->blocksize, curoffset, curextinfo->incompatflags, &blocklist);
    std::cout << "convert blocks to extents\n";
    std::string dirlayout = ConvertBlocksToExtents(&blocklist, curextinfo->blocksize);
    std::cout << "dir layout: " << dirlayout << std::endl;
    blocklist.clear();

    std::cout << "nextinode: " << nextinode << " child path to match: " << childpath << std::endl;
    //std::cout << "dirlayout: " << curextinfo.dirlayout << std::endl;
    uint8_t* iflags = new uint8_t[4];
    devicebuffer->seekg(curoffset + 32);
    devicebuffer->read((char*)iflags, 4);
    uint32_t inodeflags = (uint32_t)iflags[0] | (uint32_t)iflags[1] << 8 | (uint32_t)iflags[2] << 16 | (uint32_t)iflags[3] << 24;
    delete[] iflags;
    //uint64_t childinode = 0;

    std::vector<std::string> dirlaylist;
    dirlaylist.clear();
    std::istringstream dll(dirlayout);
    std::string s;
    while(getline(dll, s, ';'))
        dirlaylist.push_back(s);
    for(int i=0; i < dirlaylist.size(); i++)
    {
        std::size_t found = dirlaylist.at(i).find(",");
        uint64_t curdiroffset = std::stoull(dirlaylist.at(i).substr(0, found));
        uint64_t curdirlength = std::stoull(dirlaylist.at(i).substr(found+1));
        std::cout << "curdir offset: " << curdiroffset << " curdir length: " << curdirlength << std::endl;
        uint64_t coffset = curdiroffset + 24; // SKIP THE . AND .. ENTRIES WHICH ARE ALWAYS THE 1ST TWO ENTRIES AND 12 BYTES EACH
        std::cout << "coffset: " << coffset << std::endl;
        if(inodeflags & 0x1000) // hash trees in use
        {
            coffset = curdiroffset + 40; // THIS SHOULD ACCOUNT FOR HASH TREE DEPTH OF 0, NEED TO TEST FOR 1 - 3
            std::cout << "cur dir inode uses hashed trees rather than linear direntry reading" << std::endl;
        }
        while(coffset < curdiroffset + curdirlength - 8)
        {
            //std::cout << "coffset: " << coffset << std::endl;
            uint16_t entrylength = 4;
            uint8_t* ei = new uint8_t[4];
            devicebuffer->seekg(coffset);
            devicebuffer->read((char*)ei, 4);
            uint32_t extinode = (uint32_t)ei[0] | (uint32_t)ei[1] << 8 | (uint32_t)ei[2] << 16 | (uint32_t)ei[3] << 24;
            delete[] ei;
            std::cout << "extinode: " << extinode << std::endl;
            if(extinode > 0)
            {
                uint8_t* el = new uint8_t[2];
                devicebuffer->seekg(coffset + 4);
                devicebuffer->read((char*)el, 2);
                entrylength = (uint16_t)el[0] | (uint16_t)el[1] << 8;
                std::cout << "entry length: " << entrylength << std::endl;
                delete[] el;

                uint16_t namelength = 0;
                devicebuffer->seekg(coffset + 6);
                if(curextinfo->incompatflags & 0x02 || curextinfo->revision > 0.4)
                {
                    //std::cout << "name length is 1 byte long.\n";
                    uint8_t* nl = new uint8_t[1];
                    devicebuffer->read((char*)nl, 1);
                    namelength = (uint16_t)nl[0];
                    delete[] nl;
                }
                else
                {
                    //std::cout << "name length is 2 bytes long.\n";
                    uint8_t* nl = new uint8_t[2];
                    devicebuffer->read((char*)nl, 2);
                    namelength = (uint16_t)nl[0] | (uint16_t)nl[1] << 8;
                    delete[] nl;
                }
                std::cout << "namelength: " << namelength << std::endl;
                char* fname = new char[namelength];
                devicebuffer->seekg(coffset + 8);
                devicebuffer->read(fname, namelength);
                std::string filename(fname);
                std::cout << "filename: " << filename << std::endl;
                if(filename.find(childpath) != std::string::npos)
                    return extinode;
            }
            else
            {
                break;
                //entrylength = 4;
            }
            coffset += entrylength;
            //coffset += newlength;
        }
    }
    return 0;
    */
    /*
     *
    
    for(int i=0; i < dirlayout.split(";", Qt::SkipEmptyParts).count(); i++)
    {
        bool nextisdeleted = false;
        while(coffset < curdiroffset + curdirlength - 8)
        {
                uint16_t namelength = 0;
                int filetype =  -1;
                entrylength = qFromLittleEndian<uint16_t>(curimg->ReadContent(coffset + 4, 2));
                if(incompatflags.contains("Directory Entries record file type,") || revision > 0.4)
                {
                    namelength = qFromLittleEndian<uint8_t>(curimg->ReadContent(coffset + 6, 1));
                    filetype = qFromLittleEndian<uint8_t>(curimg->ReadContent(coffset + 7, 1));
                }
                else
                {
                    namelength = qFromLittleEndian<uint16_t>(curimg->ReadContent(coffset + 6, 2));
                }
		//qDebug() << "namelength:" << namelength;
		//qDebug() << "newlength:" << newlength << "entrylength:" << entrylength << "namelength:" << QString::number(namelength, 16);
                QString filename = QString::fromStdString(curimg->ReadContent(coffset + 8, namelength).toStdString()).toLatin1();
                //qDebug() << "filename:" << filename;
                uint8_t isdeleted = 0;
                if(nextisdeleted)
                    isdeleted = 1;
                if(newlength < entrylength)
                    nextisdeleted = true;
                else
                    nextisdeleted = false;
                //itemtype = itemnode->itemtype; // node type 5=file, 3=dir, 4-del file, 10=vir file, 11=vir dir, -1=not file (evid image, vol, part, fs), 15=carved file
                uint8_t itemtype = 0;
                if(filetype == 0) // unknown type
                    itemtype = 0;
                else if(filetype == 1) // FILE
                {
                    itemtype = 5;
                    if(isdeleted == 1)
                        itemtype = 4;
                }
                else if(filetype == 2) // DIR
                {
                    itemtype = 3;
                    if(isdeleted == 1)
                        itemtype = 2;
                }
                else if(filetype == 3) // CHARACTER
                    itemtype = 6;
                else if(filetype == 4) // BLOCK
                    itemtype = 7;
                else if(filetype == 5) // FIFO
                    itemtype = 8;
                else if(filetype == 6) // UNIX SOCKET
                    itemtype = 9;
                else if(filetype == 7) // SYMBOLIC LINK
                    itemtype = 12;
		//qDebug() << "itemtype:" << itemtype;
                // DETERMINE WHICH BLOCK GROUP # THE CURINODE IS IN SO I CAN READ IT'S INODE'S CONTENTS AND GET THE NECCESARY METADATA
                quint64 curinodetablestartblock = 0;
                uint8_t blockgroupnumber = 0;
		//qDebug() << "inodeaddresstable:" << inodeaddresstable;
                for(int j=1; j <= inodeaddresstable.split(",", Qt::SkipEmptyParts).count(); j++)
                {
                    if(extinode < j * blkgrpinodecnt)
                    {
                        curinodetablestartblock = inodeaddresstable.split(",", Qt::SkipEmptyParts).at(j-1).toULongLong();
                        blockgroupnumber = j - 1;
                        break;
                    }
                }
                //qDebug() << "extinode:" << extinode << "block group number:" << blockgroupnumber << "curinodetablestartblock:" << curinodetablestartblock;
                quint64 logicalsize = 0;
                quint64 curinodeoffset = curstartsector * 512 + curinodetablestartblock * blocksize + inodesize * (extinode - 1 - blockgroupnumber * blkgrpinodecnt);
                uint16_t filemode = qFromLittleEndian<uint16_t>(curimg->ReadContent(curinodeoffset, 2));
                QString filemodestr = "---------";
                if(filemode & 0xc000) // unix socket
		{
                    filemodestr.replace(0, 1, "s");
		    itemtype = 9;
		}
                if(filemode & 0xa000) // symbolic link
		{
                    filemodestr.replace(0, 1, "l");
		    itemtype = 12;
		}
                if(filemode & 0x6000) // block device
		{
                    filemodestr.replace(0, 1, "b");
		    itemtype = 7;
		}
                if(filemode & 0x2000) // char device
		{
                    filemodestr.replace(0, 1, "c");
		    itemtype = 6;
		}
                if(filemode & 0x1000) // FIFO (pipe)
		{
                    filemodestr.replace(0, 1, "p");
		    itemtype = 8;
		}
                if(filemode & 0x8000) // regular file
                {
                    if(readonlyflags.contains("Allow storing files larger than 2GB,")) // LARGE FILE SUPPORT
                    {
                        uint32_t lowersize = qFromLittleEndian<uint32_t>(curimg->ReadContent(curinodeoffset + 4, 4));
                        uint32_t uppersize = qFromLittleEndian<uint32_t>(curimg->ReadContent(curinodeoffset + 108, 4));
                        logicalsize = ((quint64)uppersize >> 32) + lowersize;
                    }
                    else
                    {
                        logicalsize = qFromLittleEndian<uint32_t>(curimg->ReadContent(curinodeoffset + 4, 4));
                    }
                    filemodestr.replace(0, 1, "-");
		    itemtype = 5;
		    if(isdeleted == 1)
			itemtype = 4;
                }
                else if(filemode & 0x4000) // directory
                {
                    logicalsize = qFromLittleEndian<uint32_t>(curimg->ReadContent(curinodeoffset + 4, 4));
                    filemodestr.replace(0, 1, "d");
		    itemtype = 3;
		    if(isdeleted == 1)
			itemtype = 2;
                }
                if(filemode & 0x100) // user read
                    filemodestr.replace(1, 1, "r");
                if(filemode & 0x080) // user write
                    filemodestr.replace(2, 1, "w");
                if(filemode & 0x040) // user execute
                    filemodestr.replace(3, 1, "x");
                if(filemode & 0x020) // group read
                    filemodestr.replace(4, 1, "r");
                if(filemode & 0x010) // group write
                    filemodestr.replace(5, 1, "w");
                if(filemode & 0x008) // group execute
                    filemodestr.replace(6, 1, "x");
                if(filemode & 0x004) // other read
                    filemodestr.replace(7, 1, "r");
                if(filemode & 0x002) // other write
                    filemodestr.replace(8, 1, "w");
                if(filemode & 0x001) // other execute
                    filemodestr.replace(9, 1, "x");
                //qDebug() << "filemodestr:" << filemodestr;
		out << "Mode|" << filemodestr << "|Unix Style Permissions. r - file, d - directory, l - symbolic link, c - character device, b - block device, p - named pipe, v - virtual file created by the forensic tool; r - read, w - write, x - execute, s - set id and executable, S - set id, t - sticky bit executable, T - sticky bit. format is type/user/group/other - [rdlcbpv]/rw[sSx]/rw[sSx]/rw[tTx]." << Qt::endl;
		//qDebug() << "itemtype attempt 2:" << itemtype;

                // STILL NEED TO DO FILE ATTRIBUTES, EXTENDED ATTRIBUTE BLOCK
                uint16_t lowergroupid = qFromLittleEndian<uint16_t>(curimg->ReadContent(curinodeoffset + 24, 2));
                uint16_t uppergroupid = qFromLittleEndian<uint16_t>(curimg->ReadContent(curinodeoffset + 122, 2));
                uint32_t groupid = ((uint32_t)uppergroupid >> 16) + lowergroupid;
                uint16_t loweruserid = qFromLittleEndian<uint16_t>(curimg->ReadContent(curinodeoffset + 2, 2));
                uint16_t upperuserid = qFromLittleEndian<uint16_t>(curimg->ReadContent(curinodeoffset + 120, 2));
                uint32_t userid = ((uint32_t)upperuserid >> 16) + loweruserid;
		out << "uid / gid|" << QString(QString::number(userid) + " / " + QString::number(groupid)) << "|User ID and Group ID." << Qt::endl;
                uint32_t accessdate = qFromLittleEndian<uint32_t>(curimg->ReadContent(curinodeoffset + 8, 4));
                uint32_t statusdate = qFromLittleEndian<uint32_t>(curimg->ReadContent(curinodeoffset + 12, 4));
                uint32_t modifydate = qFromLittleEndian<uint32_t>(curimg->ReadContent(curinodeoffset + 16, 4));
                uint32_t deletedate = qFromLittleEndian<uint32_t>(curimg->ReadContent(curinodeoffset + 20, 4));
		if(deletedate > 0)
		    out << "Deleted Time|" << QDateTime::fromSecsSinceEpoch(deletedate, QTimeZone::utc()).toString("MM/dd/yyyy hh:mm:ss AP") << "|Deleted time for the file." << Qt::endl;
                uint16_t linkcount = qFromLittleEndian<uint16_t>(curimg->ReadContent(curinodeoffset + 26, 2));
		out << "Link Count|" << QString::number(linkcount) << "|Number of files pointing to this file." << Qt::endl;
                uint32_t createdate = 0;
                if(fstype.startsWith("EXT4"))
                    uint32_t createdate = qFromLittleEndian<uint32_t>(curimg->ReadContent(curinodeoffset + 144, 4));
                uint32_t curinodeflags = qFromLittleEndian<uint32_t>(curimg->ReadContent(curinodeoffset + 32, 4));
		//qDebug() << "curinodeflags:" << QString::number(curinodeflags, 16);
                QString attrstr = "";
                if(curinodeflags & 0x200000)
                    attrstr += "Stores a Large Extended Attribute,";
                if(curinodeflags & 0x80000)
                    attrstr += "Uses Extents,";
                if(curinodeflags & 0x40000)
                    attrstr += "Huge File,";
                if(curinodeflags & 0x20000)
                    attrstr += "Top of Directory,";
                if(curinodeflags & 0x10000)
                    attrstr += "Synchronous Data Write,";
                if(curinodeflags & 0x8000)
                    attrstr += "File Tail not Merged,";
                if(curinodeflags & 0x4000)
                    attrstr += "File Data Written through Journal,";
                if(curinodeflags & 0x2000)
                    attrstr += "AFS Magic Directory,";
                if(curinodeflags & 0x1000)
                    attrstr += "Hashed Indexed Directory,";
                if(curinodeflags & 0x800)
                    attrstr += "Encrypted,";
                if(curinodeflags & 0x400)
                    attrstr += "No Compression,";
                if(curinodeflags & 0x200)
                    attrstr += "Has Compression in 1 or more blocks,";
                if(curinodeflags & 0x100)
                    attrstr += "Dirty Compression,";
                if(curinodeflags & 0x80)
                    attrstr += "No Update ATIME,";
                if(curinodeflags & 0x40)
                    attrstr += "dump utility ignores file,";
                if(curinodeflags & 0x20)
                    attrstr += "Append Only,";
                if(curinodeflags & 0x10)
                    attrstr += "Immutable,";
                if(curinodeflags & 0x08)
                    attrstr += "Synchronous Writes,";
                if(curinodeflags & 0x04)
                    attrstr += "Compressed,";
                if(curinodeflags & 0x02)
                    attrstr += "Preserved for un-deletion,";
                if(curinodeflags & 0x01)
                    attrstr += "Requires Secure Deletion";
		//if(curinodeflags == 0x00)
		//    attrstr = "No attributes";
		if(!attrstr.isEmpty())
		    out << "File Attributes|" << attrstr << "|Attributes list for the file." << Qt::endl;

                QList<uint32_t> curblklist;
                curblklist.clear();
                GetContentBlocks(curimg, curstartsector, blocksize, curinodeoffset, &incompatflags, &curblklist);
                //qDebug() << "curblklist:" << curblklist;
                QString curlayout = "";
                if(curblklist.count() > 0)
                    curlayout = ConvertBlocksToExtents(curblklist, blocksize);
		out << "Layout|" << curlayout << "|File layout in offset,size; format." << Qt::endl;
                //qDebug() << "Curlayout:" << curlayout;
                quint64 physicalsize = 0;
                for(int j=0; j < curlayout.split(";", Qt::SkipEmptyParts).count(); j++)
                {
                    physicalsize += curlayout.split(";", Qt::SkipEmptyParts).at(j).split(",").at(1).toULongLong();
                }
                //int phyblkcnt = physicalsize / blocksize;
                int phyremcnt = physicalsize % blocksize;
                if(phyremcnt > 0)
                    physicalsize += blocksize;
		out << "Physical Size|" << QString::number(physicalsize) << "|Size of the blocks the file takes up in bytes." << Qt::endl;
		out << "Logical Size|" << QString::number(logicalsize) << "|Size in Bytes for the file." << Qt::endl;

		//qDebug() << "curlayout:" << curlayout;
                curblklist.clear();
		QHash<QString, QVariant> nodedata;
		nodedata.clear();
		nodedata.insert("name", QByteArray(filename.toUtf8()).toBase64());
                nodedata.insert("path", QByteArray(filepath.toUtf8()).toBase64());
                nodedata.insert("size", logicalsize);
                nodedata.insert("create", createdate);
                nodedata.insert("access", accessdate);
                nodedata.insert("modify", modifydate);
                //nodedata.insert("status", "0");
                //nodedata.insert("hash", "0");
		if(logicalsize > 0) // Get Category/Signature
		{
		    if(itemtype == 3 && isdeleted == 0)
                    {
			nodedata.insert("cat", "Directory");
                        nodedata.insert("sig", "Directory");
                    }
		    else
		    {
			QString catsig = GenerateCategorySignature(curimg, filename, curlayout.split(";").at(0).split(",").at(0).toULongLong());
			nodedata.insert("cat", catsig.split("/").first());
                        nodedata.insert("sig", catsig.split("/").last());
		    }
		}
		else
                {
		    nodedata.insert("cat", "Empty");
                    nodedata.insert("sig", "Zero File");
                }
		//nodedata.insert("tag", "0");
                nodedata.insert("id", QString("e" + curimg->MountPath().split("/").last().split("-e").last() + "-p" + QString::number(ptreecnt) + "-f" + QString::number(inodecnt)));
		//nodedata.insert("match", 0);
		QString parentstr = QString("e" + curimg->MountPath().split("/").last().split("-e").last() + "-p" + QString::number(ptreecnt));
		if(parinode > 0)
		    parentstr += QString("-f" + QString::number(parinode));
		mutex.lock();
		treenodemodel->AddNode(nodedata, parentstr, itemtype, isdeleted);
		mutex.unlock();
		if(nodedata.value("id").toString().split("-").count() == 3)
		{
		    listeditems.append(nodedata.value("id").toString());
		    filesfound++;
		    isignals->ProgUpd();
		}
		inodecnt++;
		nodedata.clear();
		out.flush();
		fileprop.close();
                if(filemode & 0x4000) // directory, so recurse
		{
		    //qDebug() << "sub dir, should recurse...";
                    inodecnt = ParseExtDirectory(curimg, curstartsector, ptreecnt, extinode, inodecnt - 1, QString(filepath + filename + "/"), curlayout);
		}
            }
            coffset += newlength;
        }
    }
    return inodecnt;


}
     */

void ParseExtFile(std::ifstream* rawcontent, sbinfo* cursb, uint64_t curinode, std::string filename)
{
    std::cout << "parse forensic contents for " << filename << std::endl;
}

//void ParseExtFile(std::ifstream* devicebuffer, uint64_t curextinode)
//{
//    std::cout << "do file stuff here...\n";
//}

// may want to replace filename with the current path array value and build the filename from the loop of this function
//void ParseExtForensics(std::string filename, std::string mntptstr, std::string devicestr, uint64_t curextinode)
void ParseExtForensics(std::string filename, std::string mntptstr, std::string devicestr)
{
    //uint64_t nextinode = 2;
    std::ifstream devicebuffer(devicestr.c_str(), std::ios::in|std::ios::binary);
    std::cout << filename << " || " << mntptstr << " || " << devicestr << std::endl;
    // MAY WANT THIS IN HERE TO LOOP AND PARSE THE RESPECTIVE DIRECTORIES TO GET TO THE FILE...
    extinfo curextinfo;
    sbinfo cursb;
    ParseSuperBlock(&devicebuffer, &cursb);
    // NEED TO DETERMINE STARINT DIRECTORY BASED ON MOUNT POINT AND FILENAME
    std::string pathstring = "";
    if(mntptstr.compare("/") != 0)
    {
        std::size_t initdir = filename.find(mntptstr);
        pathstring = filename.substr(initdir + mntptstr.size());
        std::cout << "initdir: " << initdir << " new path: " << pathstring << std::endl;
    }
    else
        pathstring = filename;
    // SPLIT CURRENT FILE PATH INTO DIRECTORIES
    std::vector<std::string> pathvector;
    std::istringstream iss(pathstring);
    //std::istringstream iss(filename);
    std::string s;
    while(getline(iss, s, '/'))
        pathvector.push_back(s);

    std::cout << "path vector size: " << pathvector.size() << std::endl;
    // PARSE ROOT DIRECTORY AND GET INODE FOR THE NEXT DIRECTORY IN PATH VECTOR
    uint32_t returninode = 0;
    returninode = ParseExtPath(&devicebuffer, &cursb, 2, pathvector.at(1));

    std::cout << "Inode for " << pathvector.at(1) << ": " << returninode << std::endl;
    std::cout << "Loop Over the Remaining Paths\n";

    for(int i=1; i < pathvector.size() - 2; i++)
    {
        returninode = ParseExtPath(&devicebuffer, &cursb, returninode, pathvector.at(i));
        std::cout << "Inode for " << pathvector.at(i) << ": " << returninode << std::endl;
        //std::cout << "i: " << i << " Next Directory to Parse: " << pathvector.at(i+1) << std::endl;
    }
    
    ParseExtFile(&devicebuffer, &cursb, returninode, pathvector.at(pathvector.size() - 1));

    //ParseExtInit(&devicebuffer, &curextinfo);
    /*
    std::vector<std::string> pathvector;
    std::istringstream iss(filename);
    std::string s;
    while(getline(iss, s, '/'))
        pathvector.push_back(s);
    std::cout << "i: 0 " << "root inode:" << 2 << " parse root directory." << std::endl;
    //nextinode = ParseExtPath(&devicebuffer, &curextinfo, 2, pathvector.at(1));
    std::cout << "next inode: " << nextinode << std::endl;
    for(int i=1; i < pathvector.size(); i++)
    {
        if(nextinode == 0)
            break;
        std::cout << "current path: " << pathvector.at(i) << std::endl;
        //if(i == 0)
        //{
        //    std::cout << i << " nextinode: " << nextinode << " Parse Ext Root Directory inode 2\n";
        //    //nextinode = ParseExtPath(&devicebuffer, &curextinfo, nextinode, pathvector.at(i+1));
        //}
        if(i == pathvector.size() - 1)
        {
            std::cout << i << " nextinode: " << nextinode << " Parse Ext File Forensics\n";
            //ParseExtFile(&devicebuffer, nextinode);
        }
        else
        {
            std::cout << i << " nextinode: " << nextinode << " Parse File's Next Directory " << pathvector.at(i+1) << std::endl;
            //nextinode = ParseExtPath(&devicebuffer, &curextinfo, nextinode, pathvector.at(i+1));
        }
    }
    */
    /*
    uint64_t curextinode = 2;
    // IF THERE IS COMMON ELEMENTS MOVE INTO ParseExtInit();
    // MOVE A LOT OF THIS INTO ParseExtPath();
    // MOVE A LOT OF THIS INTO ParseExtFile();
    uint32_t blocksize = 0;
    uint32_t bsizepow = 0;
    uint8_t* bsize = new uint8_t[4];
    devicebuffer.seekg(1048);
    devicebuffer.read((char*)bsize, 4);
    bsizepow = (uint32_t)bsize[0] | (uint32_t)bsize[1] << 8 | (uint32_t)bsize[2] << 16 | (uint32_t)bsize[3] << 24;
    delete[] bsize;
    blocksize = 1024 * pow(2, bsizepow);
    //std::cout << "block size: " << bsizepow << std::endl;
    std::cout << "blocksize: " << blocksize << std::endl;
    uint8_t* isize = new uint8_t[2];
    uint16_t inodesize = 0;
    devicebuffer.seekg(1112);
    devicebuffer.read((char*)isize, 2);
    inodesize = (uint16_t)isize[0] | (uint16_t)isize[1] << 8;
    delete[] isize;
    std::cout << "inode size: " << inodesize << std::endl;
    uint32_t blkgrpinodecnt = 0;
    uint8_t* bgicnt = new uint8_t[4];
    devicebuffer.seekg(1064);
    devicebuffer.read((char*)bgicnt, 4);
    blkgrpinodecnt = (uint32_t)bgicnt[0] | (uint32_t)bgicnt[1] << 8 | (uint32_t)bgicnt[2] << 16 | (uint32_t)bgicnt[3] << 24;
    delete[] bgicnt;
    std::cout << "block group inode count: " << blkgrpinodecnt << std::endl;
    uint32_t rootinodetableaddress = 0;
    uint8_t* ritaddr = new uint8_t[4];
    if(blkgrpinodecnt > 2)
        devicebuffer.seekg(2056);
    else
        devicebuffer.seekg(2088);
    devicebuffer.read((char*)ritaddr, 4);
    rootinodetableaddress = (uint32_t)ritaddr[0] | (uint32_t)ritaddr[1] << 8 | (uint32_t)ritaddr[2] << 16 | (uint32_t)ritaddr[3] << 24;
    delete[] ritaddr;
    std::cout << "root inode table address: " << rootinodetableaddress << std::endl;
    uint8_t exttype = 2;
    uint8_t* extflags = new uint8_t[12];
    devicebuffer.seekg(1116);
    devicebuffer.read((char*)extflags, 12);
    uint32_t compatflags = (uint32_t)extflags[0] | (uint32_t)extflags[1] << 8 | (uint32_t)extflags[2] << 16 | (uint32_t)extflags[3] << 24;
    uint32_t incompatflags = (uint32_t)extflags[4] | (uint32_t)extflags[5] << 8 | (uint32_t)extflags[6] << 16 | (uint32_t)extflags[7] << 24;
    uint32_t readonlyflags = (uint32_t)extflags[8] | (uint32_t)extflags[9] << 8 | (uint32_t)extflags[10] << 16 | (uint32_t)extflags[11] << 24;
    delete[] extflags;
    if((compatflags & 0x00000200UL != 0) || (incompatflags & 0x0001f7c0UL != 0) || (readonlyflags & 0x00000378UL != 0))
        exttype = 4;
    else if((compatflags & 0x00000004UL != 0) || (incompatflags & 0x0000000cUL != 0))
        exttype = 3;
    std::cout << "exttype: EXT" << (unsigned int)exttype << std::endl;
    std::string revstr;
    uint8_t* revbig = new uint8_t[4];
    devicebuffer.seekg(1100);
    devicebuffer.read((char*)revbig, 4);
    revstr.append(std::to_string((uint32_t)revbig[0] | (uint32_t)revbig[1] << 8 | (uint32_t)revbig[2] << 16 | (uint32_t)revbig[3] << 24));
    delete[] revbig;
    revstr.append(".");
    uint8_t* revsml = new uint8_t[2];
    devicebuffer.seekg(1086);
    devicebuffer.read((char*)revsml, 2);
    revstr.append(std::to_string((uint16_t)revsml[0] | (uint16_t)revsml[1] << 8));
    delete[] revsml;
    float revision = std::stof(revstr);
    std::cout << "revstr: " << revstr << std::endl;
    std::cout << "rev float: " << revision << std::endl;
    uint16_t grpdescsize = 32;
    if(incompatflags & 0x80)
    {
        uint8_t* grpdesc = new uint8_t[2];
        devicebuffer.seekg(1278);
        devicebuffer.read((char*)grpdesc, 2);
        grpdescsize = (uint16_t)grpdesc[0] | (uint16_t)grpdesc[1] << 8;
        delete[] grpdesc;
    }
    std::cout << "group descriptor size: " << grpdescsize << std::endl;
    uint8_t* fsblk = new uint8_t[4];
    devicebuffer.seekg(1028);
    devicebuffer.read((char*)fsblk, 4);
    uint32_t fsblkcnt = (uint32_t)fsblk[0] | (uint32_t)fsblk[1] << 8 | (uint32_t)fsblk[2] << 16 | (uint32_t)fsblk[3] << 24;
    delete[] fsblk;
    std::cout << "fs block cnt:" << fsblkcnt << std::endl;
    uint8_t* blkgrp = new uint8_t[4];
    devicebuffer.seekg(1056);
    devicebuffer.read((char*)blkgrp, 4);
    uint32_t blkgrpblkcnt = (uint32_t)blkgrp[0] | (uint32_t)blkgrp[1] << 8 | (uint32_t)blkgrp[2] << 16 | (uint32_t)blkgrp[4] << 24;
    delete[] blkgrp;
    std::cout << "block group block count: " << blkgrpblkcnt << std::endl;
    uint32_t blockgroupcount = fsblkcnt / blkgrpblkcnt;
    unsigned int blkgrpcntrem = fsblkcnt % blkgrpblkcnt;
    if(blkgrpcntrem > 0)
        blockgroupcount++;
    if(blockgroupcount == 0)
        blockgroupcount = 1;
    std::cout << "block group count: " << blockgroupcount << std::endl;
    std::vector<uint32_t> inodeaddrtables;
    inodeaddrtables.clear();
    for(unsigned int i=0; i < blockgroupcount; i++)
    {
        uint8_t* iaddr = new uint8_t[4];
        if(blocksize == 1024)
            devicebuffer.seekg(2*blocksize + i * grpdescsize + 8);
        else
            devicebuffer.seekg(blocksize + i * grpdescsize + 8);
        devicebuffer.read((char*)iaddr, 4);
        inodeaddrtables.push_back((uint32_t)iaddr[0] | (uint32_t)iaddr[1] << 8 | (uint32_t)iaddr[2] << 16 | (uint32_t)iaddr[3] << 24);
    }
    uint8_t bgnumber = 0;
    uint32_t inodestartingblock = 0;
    for(int i=1; i <= inodeaddrtables.size(); i++)
    {
        if(curextinode < i*blkgrpinodecnt) // if i generalize function, then 2 would be replaced with curextinode as a passed variable
        {
            inodestartingblock = inodeaddrtables.at(i-1);
            bgnumber = i-1;
            break;
        }
    }
    std::cout << "inode starting block: " << inodestartingblock << std::endl;
    std::cout << "block group number: " << (unsigned int)bgnumber << std::endl;
    uint64_t relcurinode = curextinode - 1  - (bgnumber * blkgrpinodecnt);
    uint64_t curoffset = inodestartingblock * blocksize + inodesize * relcurinode;
    std::cout << "relcurinode: " << relcurinode << std::endl;
    std::cout << "curoffset: " << curoffset << std::endl;
    std::vector<uint32_t> blocklist;
    blocklist.clear();
    GetContentBlocks(&devicebuffer, blocksize, curoffset, &incompatflags, &blocklist);
    std::string dirlayout = ConvertBlocksToExtents(&blocklist, blocksize);
    std::cout << "dir layout: " << dirlayout << std::endl;
    blocklist.clear();
    */



    // IF IT's DIRECTORY, JUST GET VALUES I NEED TO RERUN PARSEEXTFORENSICS WITH THE NEW PATH VALUE
    // IF IT'S FILE, THEN GET ALL THE FORENSIC VALUES I NEED TO DISPLAY OUT...
    /*
    if(filemode & 0x4000) // directory, so recurse
    {
        //qDebug() << "sub dir, should recurse...";
        inodecnt = ParseExtDirectory(curimg, curstartsector, ptreecnt, extinode, inodecnt - 1, QString(filepath + filename + "/"), curlayout);
    }
    */
    // TAKE WHAT I NEED FROM HERE TO GET TO THE NEXT DIRECTORY OR THE FILE
    // MAY MOVE THIS INTO AN PARSEEXTINODE FUNCTION
    /*
     *
     uint32_t inodeflags = qFromLittleEndian<uint32_t>(curimg->ReadContent(curoffset + 32, 4));
    
    for(int i=0; i < dirlayout.split(";", Qt::SkipEmptyParts).count(); i++)
    {
        quint64 curdiroffset = dirlayout.split(";", Qt::SkipEmptyParts).at(i).split(",").at(0).toULongLong();
        quint64 curdirlength = dirlayout.split(";", Qt::SkipEmptyParts).at(i).split(",").at(1).toULongLong();
        quint64 coffset = curstartsector * 512 + curdiroffset + 24; // SKIP THE . AND .. ENTRIES WHICH ARE ALWAYS THE 1ST TWO ENTRIES AND ARE 12 BYTES LONG EACH
	//qDebug() << "initial coffset:" << coffset;
        if(inodeflags & 0x1000) // hash trees in use
        {
            coffset = curstartsector * 512 + curdiroffset + 40; // THIS SHOULD ACCOUNT FOR HASH TREE DEPTH OF 0, NEED TO TEST FOR 1 - 3
            qDebug() << "cur directory inode uses hashed btree rather than linear direntry reading";
        }
        bool nextisdeleted = false;
        while(coffset < curdiroffset + curdirlength - 8)
        {
            uint16_t entrylength = 0;
            int lengthdiv = (8 + qFromLittleEndian<uint8_t>(curimg->ReadContent(coffset + 6, 1))) / 4;
            int lengthrem = (8 + qFromLittleEndian<uint8_t>(curimg->ReadContent(coffset + 6, 1))) % 4;
            int newlength = 0;
            if(lengthrem == 0)
                newlength = lengthdiv * 4;
            else
                newlength = lengthdiv * 4 + 4;
            int32_t extinode = qFromLittleEndian<int32_t>(curimg->ReadContent(coffset, 4));
            if(extinode > 0)
            {
		//qDebug() << "cur coffset:" << coffset;
		//qDebug() << "extinode:" << extinode;
                QTextStream out;
                QFile fileprop(curimg->MountPath() + "/p" + QString::number(ptreecnt) + "/f" + QString::number(inodecnt) + ".prop");
                if(!fileprop.isOpen())
                    fileprop.open(QIODevice::Append | QIODevice::Text);
                out.setDevice(&fileprop);

		out << "EXTFS Inode|" << QString::number(extinode) << "|EXTFS inode value to locate file in the filesystem." << Qt::endl;
                QString filepath = "/";
                if(!parfilename.isEmpty())
                {
                    filepath = parfilename;
                }
                uint16_t namelength = 0;
                int filetype =  -1;
                entrylength = qFromLittleEndian<uint16_t>(curimg->ReadContent(coffset + 4, 2));
                if(incompatflags.contains("Directory Entries record file type,") || revision > 0.4)
                {
                    namelength = qFromLittleEndian<uint8_t>(curimg->ReadContent(coffset + 6, 1));
                    filetype = qFromLittleEndian<uint8_t>(curimg->ReadContent(coffset + 7, 1));
                }
                else
                {
                    namelength = qFromLittleEndian<uint16_t>(curimg->ReadContent(coffset + 6, 2));
                }
		//qDebug() << "namelength:" << namelength;
		//qDebug() << "newlength:" << newlength << "entrylength:" << entrylength << "namelength:" << QString::number(namelength, 16);
                QString filename = QString::fromStdString(curimg->ReadContent(coffset + 8, namelength).toStdString()).toLatin1();
                //qDebug() << "filename:" << filename;
                uint8_t isdeleted = 0;
                if(nextisdeleted)
                    isdeleted = 1;
                if(newlength < entrylength)
                    nextisdeleted = true;
                else
                    nextisdeleted = false;
                //itemtype = itemnode->itemtype; // node type 5=file, 3=dir, 4-del file, 10=vir file, 11=vir dir, -1=not file (evid image, vol, part, fs), 15=carved file
                uint8_t itemtype = 0;
                if(filetype == 0) // unknown type
                    itemtype = 0;
                else if(filetype == 1) // FILE
                {
                    itemtype = 5;
                    if(isdeleted == 1)
                        itemtype = 4;
                }
                else if(filetype == 2) // DIR
                {
                    itemtype = 3;
                    if(isdeleted == 1)
                        itemtype = 2;
                }
                else if(filetype == 3) // CHARACTER
                    itemtype = 6;
                else if(filetype == 4) // BLOCK
                    itemtype = 7;
                else if(filetype == 5) // FIFO
                    itemtype = 8;
                else if(filetype == 6) // UNIX SOCKET
                    itemtype = 9;
                else if(filetype == 7) // SYMBOLIC LINK
                    itemtype = 12;
		//qDebug() << "itemtype:" << itemtype;
                // DETERMINE WHICH BLOCK GROUP # THE CURINODE IS IN SO I CAN READ IT'S INODE'S CONTENTS AND GET THE NECCESARY METADATA
                quint64 curinodetablestartblock = 0;
                uint8_t blockgroupnumber = 0;
		//qDebug() << "inodeaddresstable:" << inodeaddresstable;
                for(int j=1; j <= inodeaddresstable.split(",", Qt::SkipEmptyParts).count(); j++)
                {
                    if(extinode < j * blkgrpinodecnt)
                    {
                        curinodetablestartblock = inodeaddresstable.split(",", Qt::SkipEmptyParts).at(j-1).toULongLong();
                        blockgroupnumber = j - 1;
                        break;
                    }
                }
                //qDebug() << "extinode:" << extinode << "block group number:" << blockgroupnumber << "curinodetablestartblock:" << curinodetablestartblock;
                quint64 logicalsize = 0;
                quint64 curinodeoffset = curstartsector * 512 + curinodetablestartblock * blocksize + inodesize * (extinode - 1 - blockgroupnumber * blkgrpinodecnt);
                uint16_t filemode = qFromLittleEndian<uint16_t>(curimg->ReadContent(curinodeoffset, 2));
                QString filemodestr = "---------";
                if(filemode & 0xc000) // unix socket
		{
                    filemodestr.replace(0, 1, "s");
		    itemtype = 9;
		}
                if(filemode & 0xa000) // symbolic link
		{
                    filemodestr.replace(0, 1, "l");
		    itemtype = 12;
		}
                if(filemode & 0x6000) // block device
		{
                    filemodestr.replace(0, 1, "b");
		    itemtype = 7;
		}
                if(filemode & 0x2000) // char device
		{
                    filemodestr.replace(0, 1, "c");
		    itemtype = 6;
		}
                if(filemode & 0x1000) // FIFO (pipe)
		{
                    filemodestr.replace(0, 1, "p");
		    itemtype = 8;
		}
                if(filemode & 0x8000) // regular file
                {
                    if(readonlyflags.contains("Allow storing files larger than 2GB,")) // LARGE FILE SUPPORT
                    {
                        uint32_t lowersize = qFromLittleEndian<uint32_t>(curimg->ReadContent(curinodeoffset + 4, 4));
                        uint32_t uppersize = qFromLittleEndian<uint32_t>(curimg->ReadContent(curinodeoffset + 108, 4));
                        logicalsize = ((quint64)uppersize >> 32) + lowersize;
                    }
                    else
                    {
                        logicalsize = qFromLittleEndian<uint32_t>(curimg->ReadContent(curinodeoffset + 4, 4));
                    }
                    filemodestr.replace(0, 1, "-");
		    itemtype = 5;
		    if(isdeleted == 1)
			itemtype = 4;
                }
                else if(filemode & 0x4000) // directory
                {
                    logicalsize = qFromLittleEndian<uint32_t>(curimg->ReadContent(curinodeoffset + 4, 4));
                    filemodestr.replace(0, 1, "d");
		    itemtype = 3;
		    if(isdeleted == 1)
			itemtype = 2;
                }
                if(filemode & 0x100) // user read
                    filemodestr.replace(1, 1, "r");
                if(filemode & 0x080) // user write
                    filemodestr.replace(2, 1, "w");
                if(filemode & 0x040) // user execute
                    filemodestr.replace(3, 1, "x");
                if(filemode & 0x020) // group read
                    filemodestr.replace(4, 1, "r");
                if(filemode & 0x010) // group write
                    filemodestr.replace(5, 1, "w");
                if(filemode & 0x008) // group execute
                    filemodestr.replace(6, 1, "x");
                if(filemode & 0x004) // other read
                    filemodestr.replace(7, 1, "r");
                if(filemode & 0x002) // other write
                    filemodestr.replace(8, 1, "w");
                if(filemode & 0x001) // other execute
                    filemodestr.replace(9, 1, "x");
                //qDebug() << "filemodestr:" << filemodestr;
		out << "Mode|" << filemodestr << "|Unix Style Permissions. r - file, d - directory, l - symbolic link, c - character device, b - block device, p - named pipe, v - virtual file created by the forensic tool; r - read, w - write, x - execute, s - set id and executable, S - set id, t - sticky bit executable, T - sticky bit. format is type/user/group/other - [rdlcbpv]/rw[sSx]/rw[sSx]/rw[tTx]." << Qt::endl;
		//qDebug() << "itemtype attempt 2:" << itemtype;

                // STILL NEED TO DO FILE ATTRIBUTES, EXTENDED ATTRIBUTE BLOCK
                uint16_t lowergroupid = qFromLittleEndian<uint16_t>(curimg->ReadContent(curinodeoffset + 24, 2));
                uint16_t uppergroupid = qFromLittleEndian<uint16_t>(curimg->ReadContent(curinodeoffset + 122, 2));
                uint32_t groupid = ((uint32_t)uppergroupid >> 16) + lowergroupid;
                uint16_t loweruserid = qFromLittleEndian<uint16_t>(curimg->ReadContent(curinodeoffset + 2, 2));
                uint16_t upperuserid = qFromLittleEndian<uint16_t>(curimg->ReadContent(curinodeoffset + 120, 2));
                uint32_t userid = ((uint32_t)upperuserid >> 16) + loweruserid;
		out << "uid / gid|" << QString(QString::number(userid) + " / " + QString::number(groupid)) << "|User ID and Group ID." << Qt::endl;
                uint32_t accessdate = qFromLittleEndian<uint32_t>(curimg->ReadContent(curinodeoffset + 8, 4));
                uint32_t statusdate = qFromLittleEndian<uint32_t>(curimg->ReadContent(curinodeoffset + 12, 4));
                uint32_t modifydate = qFromLittleEndian<uint32_t>(curimg->ReadContent(curinodeoffset + 16, 4));
                uint32_t deletedate = qFromLittleEndian<uint32_t>(curimg->ReadContent(curinodeoffset + 20, 4));
		if(deletedate > 0)
		    out << "Deleted Time|" << QDateTime::fromSecsSinceEpoch(deletedate, QTimeZone::utc()).toString("MM/dd/yyyy hh:mm:ss AP") << "|Deleted time for the file." << Qt::endl;
                uint16_t linkcount = qFromLittleEndian<uint16_t>(curimg->ReadContent(curinodeoffset + 26, 2));
		out << "Link Count|" << QString::number(linkcount) << "|Number of files pointing to this file." << Qt::endl;
                uint32_t createdate = 0;
                if(fstype.startsWith("EXT4"))
                    uint32_t createdate = qFromLittleEndian<uint32_t>(curimg->ReadContent(curinodeoffset + 144, 4));
                uint32_t curinodeflags = qFromLittleEndian<uint32_t>(curimg->ReadContent(curinodeoffset + 32, 4));
		//qDebug() << "curinodeflags:" << QString::number(curinodeflags, 16);
                QString attrstr = "";
                if(curinodeflags & 0x200000)
                    attrstr += "Stores a Large Extended Attribute,";
                if(curinodeflags & 0x80000)
                    attrstr += "Uses Extents,";
                if(curinodeflags & 0x40000)
                    attrstr += "Huge File,";
                if(curinodeflags & 0x20000)
                    attrstr += "Top of Directory,";
                if(curinodeflags & 0x10000)
                    attrstr += "Synchronous Data Write,";
                if(curinodeflags & 0x8000)
                    attrstr += "File Tail not Merged,";
                if(curinodeflags & 0x4000)
                    attrstr += "File Data Written through Journal,";
                if(curinodeflags & 0x2000)
                    attrstr += "AFS Magic Directory,";
                if(curinodeflags & 0x1000)
                    attrstr += "Hashed Indexed Directory,";
                if(curinodeflags & 0x800)
                    attrstr += "Encrypted,";
                if(curinodeflags & 0x400)
                    attrstr += "No Compression,";
                if(curinodeflags & 0x200)
                    attrstr += "Has Compression in 1 or more blocks,";
                if(curinodeflags & 0x100)
                    attrstr += "Dirty Compression,";
                if(curinodeflags & 0x80)
                    attrstr += "No Update ATIME,";
                if(curinodeflags & 0x40)
                    attrstr += "dump utility ignores file,";
                if(curinodeflags & 0x20)
                    attrstr += "Append Only,";
                if(curinodeflags & 0x10)
                    attrstr += "Immutable,";
                if(curinodeflags & 0x08)
                    attrstr += "Synchronous Writes,";
                if(curinodeflags & 0x04)
                    attrstr += "Compressed,";
                if(curinodeflags & 0x02)
                    attrstr += "Preserved for un-deletion,";
                if(curinodeflags & 0x01)
                    attrstr += "Requires Secure Deletion";
		//if(curinodeflags == 0x00)
		//    attrstr = "No attributes";
		if(!attrstr.isEmpty())
		    out << "File Attributes|" << attrstr << "|Attributes list for the file." << Qt::endl;

                QList<uint32_t> curblklist;
                curblklist.clear();
                GetContentBlocks(curimg, curstartsector, blocksize, curinodeoffset, &incompatflags, &curblklist);
                //qDebug() << "curblklist:" << curblklist;
                QString curlayout = "";
                if(curblklist.count() > 0)
                    curlayout = ConvertBlocksToExtents(curblklist, blocksize);
		out << "Layout|" << curlayout << "|File layout in offset,size; format." << Qt::endl;
                //qDebug() << "Curlayout:" << curlayout;
                quint64 physicalsize = 0;
                for(int j=0; j < curlayout.split(";", Qt::SkipEmptyParts).count(); j++)
                {
                    physicalsize += curlayout.split(";", Qt::SkipEmptyParts).at(j).split(",").at(1).toULongLong();
                }
                //int phyblkcnt = physicalsize / blocksize;
                int phyremcnt = physicalsize % blocksize;
                if(phyremcnt > 0)
                    physicalsize += blocksize;
		out << "Physical Size|" << QString::number(physicalsize) << "|Size of the blocks the file takes up in bytes." << Qt::endl;
		out << "Logical Size|" << QString::number(logicalsize) << "|Size in Bytes for the file." << Qt::endl;

		//qDebug() << "curlayout:" << curlayout;
                curblklist.clear();
		QHash<QString, QVariant> nodedata;
		nodedata.clear();
		nodedata.insert("name", QByteArray(filename.toUtf8()).toBase64());
                nodedata.insert("path", QByteArray(filepath.toUtf8()).toBase64());
                nodedata.insert("size", logicalsize);
                nodedata.insert("create", createdate);
                nodedata.insert("access", accessdate);
                nodedata.insert("modify", modifydate);
                //nodedata.insert("status", "0");
                //nodedata.insert("hash", "0");
		if(logicalsize > 0) // Get Category/Signature
		{
		    if(itemtype == 3 && isdeleted == 0)
                    {
			nodedata.insert("cat", "Directory");
                        nodedata.insert("sig", "Directory");
                    }
		    else
		    {
			QString catsig = GenerateCategorySignature(curimg, filename, curlayout.split(";").at(0).split(",").at(0).toULongLong());
			nodedata.insert("cat", catsig.split("/").first());
                        nodedata.insert("sig", catsig.split("/").last());
		    }
		}
		else
                {
		    nodedata.insert("cat", "Empty");
                    nodedata.insert("sig", "Zero File");
                }
		//nodedata.insert("tag", "0");
                nodedata.insert("id", QString("e" + curimg->MountPath().split("/").last().split("-e").last() + "-p" + QString::number(ptreecnt) + "-f" + QString::number(inodecnt)));
		//nodedata.insert("match", 0);
		QString parentstr = QString("e" + curimg->MountPath().split("/").last().split("-e").last() + "-p" + QString::number(ptreecnt));
		if(parinode > 0)
		    parentstr += QString("-f" + QString::number(parinode));
		mutex.lock();
		treenodemodel->AddNode(nodedata, parentstr, itemtype, isdeleted);
		mutex.unlock();
		if(nodedata.value("id").toString().split("-").count() == 3)
		{
		    listeditems.append(nodedata.value("id").toString());
		    filesfound++;
		    isignals->ProgUpd();
		}
		inodecnt++;
		nodedata.clear();
		out.flush();
		fileprop.close();
                if(filemode & 0x4000) // directory, so recurse
		{
		    //qDebug() << "sub dir, should recurse...";
                    inodecnt = ParseExtDirectory(curimg, curstartsector, ptreecnt, extinode, inodecnt - 1, QString(filepath + filename + "/"), curlayout);
		}
            }
            coffset += newlength;
        }
    }
    return inodecnt;

     */


    devicebuffer.close();
    //for(int i=0; i < pathvector.size(); i++)
    //    std::cout << "path part: " << pathvector.at(i) << "\n";
    // Parse Root Directory to find the file
    // root directory inode is 2

    /*
        uint32_t blkgrp0startblk = qFromLittleEndian<uint32_t>(curimg->ReadContent(curstartsector * 512 + 1044, 4));
        out << "Block Group 0 Start Block|" << QString::number(blkgrp0startblk) << "|Starting block number for block group 0." << Qt::endl;

    if(!parfilename.isEmpty())
	inodecnt = parinode + 1;
     */ 
}

