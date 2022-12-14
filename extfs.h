#include "common.h"

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

std::string ConvertBlocksToExtents(std::vector<uint32_t>* blocklist, uint32_t blocksize);

void GetContentBlocks(std::ifstream* devicebuffer, uint32_t blocksize, uint64_t curoffset, uint32_t incompatflags, std::vector<uint32_t>* blocklist);
void ReturnChildInode(std::ifstream* rawcontent, sbinfo* cursb, std::string* dirlayout, uint64_t* childinode, std::string* childpath, uint64_t* inodeoffset);

void ParseSuperBlock(std::ifstream* rawcontent, sbinfo* cursb);
uint64_t ParseExtPath(std::ifstream* devicebuffer, sbinfo* cursb, uint64_t curinode, std::string childpath);
std::string ParseExtFile(std::ifstream* rawcontent, sbinfo* cursb, uint64_t curinode, std::string filename);
void ParseExtForensics(std::string filename, std::string mntptstr, std::string devicestr);
