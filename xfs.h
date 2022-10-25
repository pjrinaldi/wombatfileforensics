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
};

/*
std::string ConvertUnixTimeToHuman(uint32_t unixtime);
std::string ConvertBlocksToExtents(std::vector<uint32_t>* blocklist, uint32_t blocksize);
void GetContentBlocks(std::ifstream* devicebuffer, uint32_t blocksize, uint64_t curoffset, uint32_t incompatflags, std::vector<uint32_t>* blocklist);
void ReturnChildInode(std::ifstream* rawcontent, sbinfo* cursb, std::string* dirlayout, uint64_t* childinode, std::string* childpath, uint64_t* inodeoffset);
uint64_t ParseExtPath(std::ifstream* devicebuffer, sbinfo* cursb, uint64_t curinode, std::string childpath);
std::string ParseExtFile(std::ifstream* rawcontent, sbinfo* cursb, uint64_t curinode, std::string filename);
*/

uint64_t ParseXfsPath(std::ifstream* rawcontent, xfssuperblockinfo* cursb, uint64_t curinode, std::string childpath);
std::string ParseXfsFile(std::ifstream* rawcontent, xfssuperblockinfo* cursb, uint64_t curinode, std::string filename);
void ParseSuperBlock(std::ifstream* rawcontent, xfssuperblockinfo* cursb);
void ParseXfsForensics(std::string filename, std::string mntptstr, std::string devicestr);
