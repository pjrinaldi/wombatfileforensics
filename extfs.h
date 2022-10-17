#include "common.h"

void ParseExtForensics(std::string filename, std::string mntptstr, std::string devicestr);

// MAYBE PUT INITIAL INFORMATION IN A STRUCT, THAT I POPULATE AND CAN USE FROM RUNNING THE ParseExtInit();
struct extinfo
{
    uint32_t blocksize = 0;
    uint16_t inodesize = 0;
    //uint64_t curoffset = 0;
    uint32_t incompatflags = 0;
    //uint32_t inodestartingblock = 0;
    //uint8_t bgnumber = 0;
    uint32_t blkgrpinodecnt = 0;
    float revision = 0.0;
    std::string inodeaddrtables = "";
    //std::vector<uint64_t> inodeaddrtables;
    //std::string dirlayout = "";
};

struct sbinfo
{
    uint32_t blocksize;
    uint16_t inodesize;
    uint32_t inodesperblockgroup;
    uint32_t cflags;
    uint32_t icflags;
    uint32_t roflags;
    uint8_t extversion;
    float revision;
    uint32_t fsblockcount;
    uint32_t blockspergroup;
    uint32_t blockgroupcount;
};


void GetContentBlocks(std::ifstream* devicebuffer, uint32_t blocksize, uint64_t curoffset, uint32_t incompatflags, std::vector<uint32_t>* blocklist);
std::string ConvertBlocksToExtents(std::vector<uint32_t>* blocklist, uint32_t blocksize);
void ReturnChildInode(std::ifstream* rawcontent, sbinfo* cursb, std::string* dirlayout, uint64_t* childinode, std::string* childpath, uint64_t* inodeoffset);

//void ParseExtInit(std::ifstream* devicebuffer, extinfo* curextinfo);

void ParseSuperBlock(std::ifstream* rawcontent, sbinfo* cursb);

uint64_t ParseExtPath(std::ifstream* devicebuffer, sbinfo* cursb, uint64_t curinode, std::string childpath);
//uint64_t ParseExtPath(std::ifstream* devicebuffer, extinfo* curextinfo, uint64_t curinode, std::string childpath);

//void ParseExtFile(std::ifstream* devicebuffer, uint64_t curextinode);
void ParseExtFile(std::ifstream* rawcontent, sbinfo* cursb, uint64_t curinode, std::string filename);

/*
struct wfi_metadata
{
    uint32_t skipframeheader; // skippable frame header - 4
    uint32_t skipframesize; // skippable frame content size (not including header and this size) - 4
    uint16_t sectorsize; // raw forensic image sector size - 2
    uint16_t version; // version # for forensic image format
    uint16_t reserved1;
    uint32_t reserved2;
    //int64_t reserved; // reserved
    int64_t totalbytes; // raw forensic image total size - 8
    char casenumber[24]; // 24 character string - 24
    char evidencenumber[24]; // 24 character string - 24
    char examiner[24]; // 24 character string - 24
    char description[128]; // 128 character string - 128
    uint8_t devhash[32]; // blake3 source hash - 32
} wfimd; // 256
*/
