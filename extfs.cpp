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
    /*
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
     */

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

void ParseExtForensics(std::string filename, std::string mntptstr, std::string devicestr)
{
    std::ifstream devicebuffer(devicestr.c_str(), std::ios::in|std::ios::binary);
    //std::cout << filename << " || " << mntptstr << " || " << devicestr << std::endl;
    std::cout << std::endl << "Forensics Artifacts for Mounted File: " << filename << std::endl;
    std::cout << "Mounted File Mount Point: " << mntptstr << std::endl;
    std::cout << "Mounted File Device: " << devicestr << std::endl;
    sbinfo cursb;
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

    //std::cout << "path vector size: " << pathvector.size() << std::endl;
    // PARSE ROOT DIRECTORY AND GET INODE FOR THE NEXT DIRECTORY IN PATH VECTOR
    uint32_t returninode = 0;
    returninode = ParseExtPath(&devicebuffer, &cursb, 2, pathvector.at(1));

    //std::cout << "Inode for " << pathvector.at(1) << ": " << returninode << std::endl;
    //std::cout << "Loop Over the Remaining Paths\n";
    if(pathvector.size() > 0)
    {
        for(int i=1; i < pathvector.size() - 2; i++)
        {
            returninode = ParseExtPath(&devicebuffer, &cursb, returninode, pathvector.at(i));
            //std::cout << "Inode for " << pathvector.at(i) << ": " << returninode << std::endl;
            //std::cout << "i: " << i << " Next Directory to Parse: " << pathvector.at(i+1) << std::endl;
        }
    } 
    std::string extforensics = ParseExtFile(&devicebuffer, &cursb, returninode, pathvector.at(pathvector.size() - 1));
    std::cout << extforensics << std::endl;

    devicebuffer.close();
}

