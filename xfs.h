#include "common.h"

struct xfssuperblockinfo
{
    uint32_t blocksize;
    uint16_t inodesize;
    uint16_t inodesperblock;
    uint32_t allocationgroupblocks;
    uint32_t allocationgroupcount;
    uint64_t rootinode;
    uint8_t directoryblocklog;
    uint8_t allocationgroupblocklog; // 124
    uint8_t inodeperblocklog; // 123
};

#define MASK(n)	((1UL << n) - 1)

std::string ConvertXfsTimeToHuman(uint32_t unixtime);
/*
std::string ConvertBlocksToExtents(std::vector<uint32_t>* blocklist, uint32_t blocksize);
void GetContentBlocks(std::ifstream* devicebuffer, uint32_t blocksize, uint64_t curoffset, uint32_t incompatflags, std::vector<uint32_t>* blocklist);
*/

uint64_t ParseXfsPath(std::ifstream* rawcontent, xfssuperblockinfo* cursb, uint64_t curinode, std::string childpath);
std::string ParseXfsFile(std::ifstream* rawcontent, xfssuperblockinfo* cursb, uint64_t curinode, std::string filename);
void ParseSuperBlock(std::ifstream* rawcontent, xfssuperblockinfo* cursb);
void ParseXfsForensics(std::string filename, std::string mntptstr, std::string devicestr);
