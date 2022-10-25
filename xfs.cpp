#include "xfs.h"

/*
std::string ConvertUnixTimeToHuman(uint32_t unixtime)
{
    time_t timet = (time_t)unixtime;
    struct tm* tmtime = gmtime(&timet);
    char hchar[100];
    strftime(hchar, 100, "%m/%d/%Y %I:%M:%S %p UTC", tmtime);
    
    return std::string(hchar);
}

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
*/

void ParseSuperBlock(std::ifstream* rawcontent, xfssuperblockinfo* cursb)
{
    // BLOCK SIZE
    uint8_t* bs = new uint8_t[4];
    uint32_t blocksize = 0;
    ReadContent(rawcontent, bs, 4, 4);
    ReturnUint32(&blocksize, bs);
    delete[] bs;
    blocksize = __builtin_bswap32(blocksize); 
    cursb->blocksize = blocksize;
    std::cout << "Block Size: " << blocksize << std::endl;
    // INODE SIZE
    uint8_t* is = new uint8_t[2];
    uint16_t inodesize = 0;
    ReadContent(rawcontent, is, 104, 2);
    ReturnUint16(&inodesize, is);
    delete[] is;
    inodesize = __builtin_bswap16(inodesize);
    cursb->inodesize = inodesize;
    std::cout << "Inode Size: " << inodesize << std::endl;
    // INODES PER BLOCK
    uint8_t* ipb = new uint8_t[2];
    uint16_t inodesperblock = 0;
    ReadContent(rawcontent, ipb, 106, 2);
    ReturnUint16(&inodesperblock, ipb);
    delete[] ipb;
    inodesperblock = __builtin_bswap16(inodesperblock);
    cursb->inodesperblock = inodesperblock;
    std::cout << "Inodes Per Block: " << inodesperblock << std::endl;
    // ALLOCATION GROUP BLOCKS
    uint8_t* agb = new uint8_t[4];
    uint32_t allocationgroupblocks = 0;
    ReadContent(rawcontent, agb, 84, 4);
    ReturnUint32(&allocationgroupblocks, agb);
    delete[] agb;
    allocationgroupblocks = __builtin_bswap32(allocationgroupblocks);
    cursb->allocationgroupblocks = allocationgroupblocks;
    std::cout << "Allocation Group Blocks: " << allocationgroupblocks << std::endl;
    // ALLOCATION GROUP COUNT
    uint8_t* agc = new uint8_t[4];
    uint32_t allocationgroupcount = 0;
    ReadContent(rawcontent, agc, 88, 4);
    ReturnUint32(&allocationgroupcount, agc);
    delete[] agc;
    allocationgroupcount = __builtin_bswap32(allocationgroupcount);
    cursb->allocationgroupcount = allocationgroupcount;
    std::cout << "Allocation Group Count: " << allocationgroupcount << std::endl;
    // ROOT INODE
    uint8_t* ri = new uint8_t[8];
    uint64_t rootinode = 0;
    ReadContent(rawcontent, ri, 56, 8);
    ReturnUint64(&rootinode, ri);
    rootinode = __builtin_bswap64(rootinode);
    cursb->rootinode = rootinode;
    std::cout << "Root Inode 0x" << std::hex << rootinode << std::dec << " " << rootinode << std::endl;
    // DIRECTORY BLOCK LOG (used in directory block size)
    uint8_t* dbl = new uint8_t[1];
    uint8_t directoryblocklog = 0;
    ReadContent(rawcontent, dbl, 190, 1);
    directoryblocklog = (uint8_t)dbl[0];
    delete[] dbl;
    cursb->directoryblocklog = directoryblocklog;
    std::cout << "Directory Block Log: " << (int)directoryblocklog << std::endl;
}

uint64_t ParseXfsPath(std::ifstream* rawcontent, xfssuperblockinfo* cursb, uint64_t curinode, std::string childpath)
{
    uint64_t childinode = 0;
    // INODE OFFSET
    uint64_t inodeoffset = (curinode / cursb->inodesperblock) * cursb->blocksize;
    std::cout << "Current Inode Offset: " << inodeoffset << std::endl;
    // INODE MODE
    uint8_t* im = new uint8_t[2];
    uint16_t inodemode = 0;
    ReadContent(rawcontent, im, inodeoffset + 2, 2);
    ReturnUint16(&inodemode, im);
    delete[] im;
    inodemode = __builtin_bswap16(inodemode);
    std::cout << "Inode Mode: " << inodemode << std::endl;
    // INODE VERSION
    int8_t* iv = new int8_t[1];
    int8_t inodeversion = 0;
    ReadContent(rawcontent, iv, inodeoffset + 4, 1);
    inodeversion = (int8_t)iv[0];
    delete[] iv;
    std::cout << "inode version: " << (int)inodeversion << std::endl;
    // INODE FORMAT
    int8_t* ifmt = new int8_t[1];
    int8_t inodeformat = 0;
    ReadContent(rawcontent, ifmt, inodeoffset + 5, 1);
    inodeformat = (int8_t)ifmt[0];
    delete[] ifmt;
    std::cout << "Inode Format: " << (int)inodeformat << std::endl;
    // DIRECTORY ENTRY SIZE
    int8_t* des = new int8_t[8];
    int64_t directoryentrysize = 0;
    ReadContent(rawcontent, des, inodeoffset + 56, 8);
    ReturnInt(&directoryentrysize, des, 8);
    delete[] des;
    directoryentrysize = __builtin_bswap64(directoryentrysize);
    std::cout << "Directory Entry Size: " << directoryentrysize << std::endl;
    // NUMBER OF BLOCKS FOR INODE's DATA
    uint8_t* idbc = new uint8_t[8];
    uint64_t inodedatablockcount = 0;
    ReadContent(rawcontent, idbc, inodeoffset + 72, 8);
    ReturnUint64(&inodedatablockcount, idbc);
    delete[] idbc;
    inodedatablockcount = __builtin_bswap64(inodedatablockcount);
    std::cout << "inode data block count: " << inodedatablockcount << std::endl;
    uint64_t inodedataoffset = inodeoffset;
    if(inodeversion == 3)
        inodedataoffset += 176;
    else
        inodedataoffset += 100;
    std::cout << "inode offset to directory data: " << inodedataoffset << std::endl;
    /*
    // INODE FORK OFFSET - (DATA OFFSET LENGTH = INODE FORK OFFSET - INODEDATAOFFSET)
    uint8_t* ifo = new uint8_t[1];
    uint8_t inodeforkoffset = 0;
    ReadContent(rawcontent, ifo, inodeoffset + 86, 1);
    inodeforkoffset = (uint8_t)ifo[0];
    delete[] ifo;
    std::cout << "inode fork offset: " << (unsigned int)inodeforkoffset << std::endl;
    std::cout << "data length: " << inodeforkoffset - (int)inodedataoffset << std::endl;
    */
    if(inodeformat & 0x01)
        std::cout << "local" << std::endl;
    if(inodeformat & 0x02)
        std::cout << "extents" << std::endl;
    if(inodeformat & 0x03)
        std::cout << "btree" << std::endl;
    if(inodeformat & 0x05)
        std::cout << "reverse mapping b+tree rooted in the fork." << std::endl;
    if(inodemode & 0x4000)
        std::cout << "Directory" << std::endl;
    uint16_t curoffset = 0;
    if(inodeformat & 0x01) // local xfs_dir2_sf
    {
        // ENTRY COUNT
        uint8_t* ec = new uint8_t[1];
        uint8_t entrycount = 0;
        ReadContent(rawcontent, ec, inodedataoffset, 1);
        entrycount = (uint8_t)ec[0];
        delete[] ec;
        std::cout << "entry count: " << (int)entrycount << std::endl;
        // I8COUNT
        uint8_t* i8c = new uint8_t[1];
        uint8_t i8count = 0;
        ReadContent(rawcontent, i8c, inodedataoffset + 1, 1);
        i8count = (uint8_t)i8c[0];
        delete[] i8c;
        std::cout << "i8count: " << (int)i8count << std::endl;
        // PARENT INODE
        uint64_t parentinode = 0;
        if(i8count == 0x00)
        {
            uint8_t* pi = new uint8_t[4];
            uint32_t pinode = 0;
            ReadContent(rawcontent, pi, inodedataoffset + 2, 4);
            ReturnUint32(&pinode, pi);
            delete[] pi;
            parentinode = __builtin_bswap32(pinode);
            curoffset = 6;
        }
        else
        {
            uint8_t* pi = new uint8_t[8];
            ReadContent(rawcontent, pi, inodedataoffset + 2, 8);
            ReturnUint64(&parentinode, pi);
            delete[] pi;
            parentinode = __builtin_bswap64(parentinode);
            curoffset = 10;
        }
        std::cout << "Parent Inode: " << parentinode << std::endl;
        for(uint8_t i=0; i < entrycount; i++)
        {
            // NAME LENGTH
            uint8_t* nl = new uint8_t[1];
            uint8_t namelength = 0;
            ReadContent(rawcontent, nl, inodedataoffset + curoffset, 1);
            namelength = (uint8_t)nl[0];
            delete[] nl;
            std::cout << "Name Length: " << (int)namelength << std::endl;
            // ENTRY NAME
            uint8_t* en = new uint8_t[namelength+1];
            ReadContent(rawcontent, en, inodedataoffset + curoffset + 3, namelength);
            en[namelength] = '\0';
            std::string name((char*)en);
            std::cout << "entry name: " << name << std::endl;
            // entry inode
            if(i8count == 0x00)
            {
                uint8_t* ei = new uint8_t[4];
                uint32_t entryinode = 0;
                ReadContent(rawcontent, ei, inodedataoffset + curoffset + 3 + namelength + 1, 4);
                ReturnUint32(&entryinode, ei);
                delete[] ei;
                childinode = __builtin_bswap32(entryinode);
                curoffset = curoffset + 3 + namelength + 1 + 4;
            }
            else
            {
                uint8_t* ei = new uint8_t[8];
                ReadContent(rawcontent, ei, inodedataoffset + curoffset + 3 + namelength + 1, 8);
                ReturnUint64(&childinode, ei);
                childinode = __builtin_bswap64(childinode);
                delete[] ei;
                curoffset = curoffset + 3 + namelength + 1 + 8;
            }
            std::cout << "Child Inode: " << childinode << std::endl;
            if(childpath.compare(name) == 0)
                return childinode;
        }
    }
    /*  if(format & 1)
        {
            fmt += 1;
            qDebug() << "local";
        }
        if(format & 2)
        {
            fmt += 2;
            qDebug() << "extents";
        }
        if(format & 3)
        {
            fmt += 4;
            qDebug() << "btree";
        }
        if(format & 4)
        {
            fmt += 8;
            qDebug() << "uuid";
        }
        if(format & 5)
        {
            fmt += 16;
            qDebug() << "reverse mapping b+tree rooted in the fork.";
        }

            else if(mode & 0x4000) // directory
            uint64_t dataforkoff = rootinodeoff + 176;
     *      if(fmt == 21) // local,btree,reversemap - short form directory, xfs_dir2_sf_t
            {
                uint8_t entrycount = qFromBigEndian<uint8_t>(curimg->ReadContent(dataforkoff, 1));
                uint8_t entry64cnt = qFromBigEndian<uint8_t>(curimg->ReadContent(dataforkoff + 1, 1));
                uint32_t parinode = qFromBigEndian<uint32_t>(curimg->ReadContent(dataforkoff + 2, 4));
                uint8_t curoff = 6;
                while(curoff < filesize)
                {
                    qDebug() << "curoff:" << curoff;
                    uint8_t namelen = qFromBigEndian<uint8_t>(curimg->ReadContent(dataforkoff + curoff, 1));
                    uint16_t entryoff = qFromBigEndian<uint16_t>(curimg->ReadContent(dataforkoff + curoff + 1, 2));
                    QString entryname = QString::fromStdString(curimg->ReadContent(dataforkoff + curoff + 3, namelen).toStdString());
                    uint8_t ftype = qFromBigEndian<uint8_t>(curimg->ReadContent(dataforkoff + curoff + 3 + namelen, 1)); // 1-file,2-dir,
                    uint32_t entryinode = qFromBigEndian<uint32_t>(curimg->ReadContent(dataforkoff + curoff + 4 + namelen, 4));
                    qDebug() << "name|type|inode:" << entryname << ftype << entryinode;
		    qDebug() << "entry inode offset:" << curstartsector*512 + (entryinode / inodesperblock) * blocksize;
                    curoff = curoff + 8 + namelen;
		    ParseInode(curimg, curstartsector, ptreecnt, blocksize, inodesize, inodesperblock, entryinode, parinode, "");
                }
            }
     */ 
    return childinode;
}
/*
 * Allocation Group (ag) Layout
 * superblock               - 1 sector 512 bytes
 * ag free block info       - 1 sector 512 bytes
 * ag inode b+tree info     - 1 sector 512 bytes
 * ag internal free list    - 1 sector 512 bytes
 * root of inode b+tree     - 1 blocks blksz bytes
 * root free space b+tree   - 1 blocks blksz bytes
 * free list                - 4 blocks blksz bytes
 * inodes (64)              - 64 * inodesize bytes
 * remaining space meta/data - rest up to agblocks * blocksize
*/

/*
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
    //std::cout << "block size: " << blocksize << std::endl;
    cursb->blocksize = blocksize;
    // GET INODE SIZE
    uint8_t* isize = new uint8_t[2];
    uint16_t inodesize = 0;
    ReadContent(rawcontent, isize, sboff + 88, 2);
    ReturnUint16(&inodesize, isize);
    delete[] isize;
    //std::cout << "inode size: " << inodesize << std::endl;
    cursb->inodesize = inodesize;
    // GET INODES PER BLOCK GROUP
    uint8_t* ipbg = new uint8_t[4];
    uint32_t inodesperblockgroup = 0;
    ReadContent(rawcontent, ipbg, sboff + 40, 4);
    ReturnUint32(&inodesperblockgroup, ipbg);
    cursb->inodesperblockgroup = inodesperblockgroup;
    //std::cout << "inodes per block group: " << inodesperblockgroup << std::endl;
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
    //std::cout << "ext version: EXT" << (unsigned int)extversion << std::endl;
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
    //std::cout << "revision: " << revision << std::endl;
    // GET FILE SYSTEM BLOCK COUNT
    uint8_t* fbc = new uint8_t[4];
    ReadContent(rawcontent, fbc, sboff + 4, 4);
    uint32_t fsblkcnt = 0;
    ReturnUint32(&fsblkcnt, fbc);
    delete[] fbc;
    cursb->fsblockcount = fsblkcnt;
    //std::cout << "File system block count: " << fsblkcnt << std::endl;
    // GET BLOCKS PER BLOCK GROUP
    uint8_t* bpg = new uint8_t[4];
    uint32_t blockspergroup = 0;
    ReadContent(rawcontent, bpg, sboff + 32, 4);
    ReturnUint32(&blockspergroup, bpg);
    delete[] bpg;
    cursb->blockspergroup = blockspergroup;
    //std::cout << "blocks per group: " << blockspergroup << std::endl;
    // GET NUMBER OF BLOCK GROUPS
    uint32_t blockgroupcount = fsblkcnt / blockspergroup;
    unsigned int blockgroupcountrem = fsblkcnt % blockspergroup;
    if(blockgroupcountrem > 0)
        blockgroupcount++;
    if(blockgroupcount == 0)
        blockgroupcount = 1;
    cursb->blockgroupcount = blockgroupcount;
    //std::cout << "number of block groups: " << blockgroupcount << std::endl;
}

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
        //std::cout << "curdir offset: " << curdiroffset << " curdir length: " << curdirlength << std::endl;
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
    //std::cout << "curinode: " << curinode << " child path to find: " << childpath << std::endl;
    // DETERMINE THE CURINODE BLOCK GROUP
    uint64_t curblockgroup = (curinode - 1) / cursb->inodesperblockgroup;
    //std::cout << "cur inode block group: " << curblockgroup << std::endl;
    uint64_t localinode = (curinode - 1) % cursb->inodesperblockgroup;
    //std::cout << "local inode index: " << localinode << std::endl;
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
    //std::cout << "group descriptor entry size: " << groupdescriptorentrysize << std::endl;
    uint64_t curblockdescriptortableentryoffset = 0;
    if(cursb->blocksize == 1024)
        curblockdescriptortableentryoffset = 2 * cursb->blocksize + groupdescriptorentrysize * curblockgroup;
    else
        curblockdescriptortableentryoffset = cursb->blocksize + groupdescriptorentrysize * curblockgroup;
    //std::cout << "cur block descriptor table entry offset: " << curblockdescriptortableentryoffset << std::endl;
    // +8 get's the inode table block...
    uint8_t* sbit = new uint8_t[4];
    uint32_t startingblockforinodetable = 0;
    ReadContent(rawcontent, sbit, curblockdescriptortableentryoffset + 8, 4);
    ReturnUint32(&startingblockforinodetable, sbit);
    delete[] sbit;
    //std::cout << "Starting block for inode table: " << startingblockforinodetable << std::endl;
    uint64_t curoffset = startingblockforinodetable * cursb->blocksize + cursb->inodesize * localinode;
    //std::cout << "Current Offset to Inode Table Entry for Current Directory: " << curoffset << std::endl;
    // GET INODE CONTENT BLOCKS
    std::vector<uint32_t> blocklist;
    blocklist.clear();
    //std::cout << "get content blocks\n";
    GetContentBlocks(rawcontent, cursb->blocksize, curoffset, cursb->icflags, &blocklist);
    //std::cout << "blocklist count: " << blocklist.size() << std::endl;
    //std::cout << "convert blocks to extents\n";
    std::string dirlayout = ConvertBlocksToExtents(&blocklist, cursb->blocksize);
    //std::cout << "dir layout: " << dirlayout << std::endl;
    blocklist.clear();
    ReturnChildInode(rawcontent, cursb, &dirlayout, &childinode, &childpath, &curoffset);

    return childinode;
}

std::string ParseExtFile(std::ifstream* rawcontent, sbinfo* cursb, uint64_t curinode, std::string filename)
{
    std::string extforensics = "";
    extforensics += "Ext Inode|" + std::to_string(curinode) + "\n";
    extforensics += "Name|" + filename + "\n";

    // DETERMINE THE CURINODE BLOCK GROUP
    uint64_t curblockgroup = (curinode - 1) / cursb->inodesperblockgroup;
    //std::cout << "cur inode block group: " << curblockgroup << std::endl;
    uint64_t localinode = (curinode - 1) % cursb->inodesperblockgroup;
    //std::cout << "local inode index: " << localinode << std::endl;
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
    //std::cout << "group descriptor entry size: " << groupdescriptorentrysize << std::endl;
    uint64_t curblockdescriptortableentryoffset = 0;
    if(cursb->blocksize == 1024)
        curblockdescriptortableentryoffset = 2 * cursb->blocksize + groupdescriptorentrysize * curblockgroup;
    else
        curblockdescriptortableentryoffset = cursb->blocksize + groupdescriptorentrysize * curblockgroup;
    //std::cout << "cur block descriptor table entry offset: " << curblockdescriptortableentryoffset << std::endl;
    // +8 get's the inode table block...
    uint8_t* sbit = new uint8_t[4];
    uint32_t startingblockforinodetable = 0;
    ReadContent(rawcontent, sbit, curblockdescriptortableentryoffset + 8, 4);
    ReturnUint32(&startingblockforinodetable, sbit);
    delete[] sbit;
    //std::cout << "Starting block for inode table: " << startingblockforinodetable << std::endl;
    uint64_t curoffset = startingblockforinodetable * cursb->blocksize + cursb->inodesize * localinode;
    // PARSE INODE HERE
    uint64_t logicalsize = 0;
    // LOWER SIZE FOR LOGICAL SIZE
    uint8_t* ls = new uint8_t[4];
    uint32_t lowersize = 0;
    ReadContent(rawcontent, ls, curoffset + 4, 4);
    ReturnUint32(&lowersize, ls);
    delete[] ls;
    logicalsize = lowersize;
    // FILE MODE
    uint8_t* fm = new uint8_t[2];
    uint16_t filemode = 0;
    ReadContent(rawcontent, fm, curoffset, 2);
    ReturnUint16(&filemode, fm);
    delete[] fm;
    std::string filemodestring = "---------";
    if(filemode & 0xc000) // unix socket
        filemodestring.replace(0, 1, "s");
    if(filemode & 0xa000) // symbolic link
        filemodestring.replace(0, 1, "l");
    if(filemode & 0x6000) // block device
        filemodestring.replace(0, 1, "b");
    if(filemode & 0x2000) // char device
        filemodestring.replace(0, 1, "c");
    if(filemode & 0x1000) // FIFO (pipe)
        filemodestring.replace(0, 1, "p");
    if(filemode & 0x8000) // regular file
    {
        filemodestring.replace(0, 1, "-");
        if(cursb->roflags & 0x02)
        {
            uint8_t* us = new uint8_t[4];
            uint32_t uppersize = 0;
            ReadContent(rawcontent, us, curoffset + 108, 4);
            ReturnUint32(&uppersize, us);
            delete[] us;
            logicalsize = ((uint64_t)uppersize >> 32) + lowersize;
        }
    }
    else if(filemode & 0x4000) // directory
    {
        filemodestring.replace(0, 1, "d");
    }
    if(filemode & 0x100) // user read
        filemodestring.replace(1, 1, "r");
    if(filemode & 0x080) // user write
        filemodestring.replace(2, 1, "w");
    if(filemode & 0x040) // user execute
        filemodestring.replace(3, 1, "x");
    if(filemode & 0x020) // group read
        filemodestring.replace(4, 1, "r");
    if(filemode & 0x010) // group write
        filemodestring.replace(5, 1, "w");
    if(filemode & 0x008) // group execute
        filemodestring.replace(6, 1, "x");
    if(filemode & 0x004) // other read
        filemodestring.replace(7, 1, "r");
    if(filemode & 0x002) // other write
        filemodestring.replace(8, 1, "w");
    if(filemode & 0x001) // other execute
        filemodestring.replace(9, 1, "x");
    extforensics += "Mode|" + filemodestring + "\n";
    extforensics += "Logical Size|" + std::to_string(logicalsize) + " bytes\n";
    // LOWER GROUP ID
    uint8_t* lgi = new uint8_t[2];
    uint16_t lowergroupid = 0;
    ReadContent(rawcontent, lgi, curoffset + 24, 2);
    ReturnUint16(&lowergroupid, lgi);
    delete[] lgi;
    // UPPER GROUP ID
    uint8_t* ugi = new uint8_t[2];
    uint16_t uppergroupid = 0;
    ReadContent(rawcontent, ugi, curoffset + 122, 2);
    ReturnUint16(&uppergroupid, ugi);
    delete[] ugi;
    // GROUP ID
    uint32_t groupid = ((uint32_t)uppergroupid >> 16) + lowergroupid;
    // LOWER USER ID
    uint8_t* lui = new uint8_t[2];
    uint16_t loweruserid = 0;
    ReadContent(rawcontent, lui, curoffset + 2, 2);
    ReturnUint16(&loweruserid, lui);
    delete[] lui;
    // UPPER USER ID
    uint8_t* uui = new uint8_t[2];
    uint16_t upperuserid = 0;
    ReadContent(rawcontent, uui, curoffset + 120, 2);
    ReturnUint16(&upperuserid, uui);
    delete[] uui;
    // USER ID
    uint32_t userid = ((uint32_t)upperuserid >> 16) + loweruserid;
    extforensics += "uid/gid|" + std::to_string(userid) + "/" + std::to_string(groupid) + "\n";
    // ACCESS DATE
    uint8_t* ad = new uint8_t[4];
    uint32_t accessdate = 0;
    ReadContent(rawcontent, ad, curoffset + 8, 4);
    ReturnUint32(&accessdate, ad);
    delete[] ad;
    // STATUS DATE
    uint8_t* sd = new uint8_t[4];
    uint32_t statusdate = 0;
    ReadContent(rawcontent, sd, curoffset + 12, 4);
    ReturnUint32(&statusdate, sd);
    delete[] sd;
    // MODIFY DATE
    uint8_t* md = new uint8_t[4];
    uint32_t modifydate = 0;
    ReadContent(rawcontent, md, curoffset + 16, 4);
    ReturnUint32(&modifydate, md);
    delete[] md;
    // DELETED DATE
    uint8_t* dd = new uint8_t[4];
    uint32_t deletedate = 0;
    ReadContent(rawcontent, dd, curoffset + 20, 4);
    ReturnUint32(&deletedate, dd);
    delete[] dd;

    extforensics += "Access Date|" + ConvertUnixTimeToHuman(accessdate) + "\n";
    extforensics += "Status Date|" + ConvertUnixTimeToHuman(statusdate) + "\n";
    extforensics += "Modify Date|" + ConvertUnixTimeToHuman(modifydate) + "\n";
    if(deletedate > 0)
        extforensics += "Delete Date|" + ConvertUnixTimeToHuman(deletedate) + "\n";
    // CREATE DATE
    if(cursb->extversion == 0x04)
    {
        uint8_t* rd = new uint8_t[4];
        uint32_t createdate = 0;
        ReadContent(rawcontent, rd, curoffset + 144, 4);
        ReturnUint32(&createdate, rd);
        delete[] rd;
        extforensics += "Create Date|" + ConvertUnixTimeToHuman(createdate) + "\n";
    }
    // LINK COUNT
    uint8_t* lc = new uint8_t[2];
    uint16_t linkcount = 0;
    ReadContent(rawcontent, lc, curoffset + 26, 2);
    ReturnUint16(&linkcount, lc);
    delete[] lc;
    extforensics += "Link Count|" + std::to_string(linkcount) + "\n";
    // INODE FLAGS
    uint8_t* eif = new uint8_t[4];
    uint32_t inodeflags = 0;
    ReadContent(rawcontent, eif, curoffset + 32, 4);
    ReturnUint32(&inodeflags, eif);
    delete[] eif;
    extforensics += "Attributes|";
    if(inodeflags & 0x200000)
        extforensics += "Stores a Large Extended Attribute,";
    if(inodeflags & 0x80000)
        extforensics += "Uses Extents,";
    if(inodeflags & 0x40000)
        extforensics += "Huge File,";
    if(inodeflags & 0x20000)
        extforensics += "Top of Directory,";
    if(inodeflags & 0x10000)
        extforensics += "Synchronous Data Write,";
    if(inodeflags & 0x8000)
        extforensics += "File Tail not Merged,";
    if(inodeflags & 0x4000)
        extforensics += "File Data Written through Journal,";
    if(inodeflags & 0x2000)
        extforensics += "AFS Magic Directory,";
    if(inodeflags & 0x1000)
        extforensics += "Hashed Indexed Directory,";
    if(inodeflags & 0x800)
        extforensics += "Encrypted,";
    if(inodeflags & 0x400)
        extforensics += "No Compression,";
    if(inodeflags & 0x200)
        extforensics += "Has Compression in 1 or more blocks,";
    if(inodeflags & 0x100)
        extforensics += "Dirty Compression,";
    if(inodeflags & 0x80)
        extforensics += "No Update ATIME,";
    if(inodeflags & 0x40)
        extforensics += "dump utility ignores file,";
    if(inodeflags & 0x20)
        extforensics += "Append Only,";
    if(inodeflags & 0x10)
        extforensics += "Immutable,";
    if(inodeflags & 0x08)
        extforensics += "Synchronous Writes,";
    if(inodeflags & 0x04)
        extforensics += "Compressed,";
    if(inodeflags & 0x02)
        extforensics += "Preserved for un-deletion,";
    if(inodeflags & 0x01)
        extforensics += "Requires Secure Deletion";
    if(inodeflags == 0x00)
        extforensics += "No attributes";
    extforensics += "\n";
    //std::cout << "Current Offset to Inode Table Entry for Current File: " << curoffset << std::endl;
    // GET INODE CONTENT BLOCKS
    std::vector<uint32_t> blocklist;
    blocklist.clear();
    //std::cout << "get content blocks\n";
    GetContentBlocks(rawcontent, cursb->blocksize, curoffset, cursb->icflags, &blocklist);
    //std::cout << "blocklist count: " << blocklist.size() << std::endl;
    //std::cout << "convert blocks to extents\n";
    std::string filelayout = ConvertBlocksToExtents(&blocklist, cursb->blocksize);
    extforensics+= "Layout|" + filelayout + "\n";
    blocklist.clear();

    return extforensics;
}
*/

void ParseXfsForensics(std::string filename, std::string mntptstr, std::string devicestr)
{
    std::ifstream devicebuffer(devicestr.c_str(), std::ios::in|std::ios::binary);
    //std::cout << filename << " || " << mntptstr << " || " << devicestr << std::endl;
    std::cout << std::endl << "Forensics Artifacts for Mounted File: " << filename << std::endl;
    std::cout << "Mounted File Mount Point: " << mntptstr << std::endl;
    std::cout << "Mounted File Device: " << devicestr << std::endl;
    xfssuperblockinfo cursb;
    ParseSuperBlock(&devicebuffer, &cursb);
    // NEED TO DETERMINE STARINT DIRECTORY BASED ON MOUNT POINT AND FILENAME
    std::string pathstring = "";
    if(mntptstr.compare("/") != 0)
    {
        std::size_t initdir = filename.find(mntptstr);
        pathstring = filename.substr(initdir + mntptstr.size());
        //std::cout << "initdir: " << initdir << " new path: " << pathstring << std::endl;
    }
    else
        pathstring = filename;
    std::cout << "Mounted File Internal Path: " << pathstring << std::endl;
    // SPLIT CURRENT FILE PATH INTO DIRECTORIES
    std::vector<std::string> pathvector;
    std::istringstream iss(pathstring);
    std::string s;
    while(getline(iss, s, '/'))
        pathvector.push_back(s);

    uint64_t childinode = 0;
    std::cout << "path vector size: " << pathvector.size() << std::endl;
    if(pathvector.size() > 1)
    {
        childinode = ParseXfsPath(&devicebuffer, &cursb, cursb.rootinode, pathvector.at(1)); // parse root directory
        std::cout << "Child Inode to use for child path file: " << childinode << std::endl;
        for(int i=1; i < pathvector.size() - 2; i++)
        {
            childinode = ParseXfsPath(&devicebuffer, &cursb, childinode, pathvector.at(i+1));
            std::cout << "Child Inode to use for child path/file: " << childinode << std::endl;
        }
    }
    std::cout << "now to parse xfs file: " << pathvector.at(pathvector.size() - 1) << std::endl;
    //std::string xfsforensics = ParseXfsFile(&devicebuffer, &cursb, &childinode, pathvector.at(pathvector.size() - 1));
    //std::cout << xfsforensics;
    
    devicebuffer.close();
}
