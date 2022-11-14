#include "common.h"

struct isosuperblockinfo
{
    uint32_t blocksize;
    uint8_t pvcount;
    uint8_t svcount;
    std::vector<uint32_t> pvrootdirblk;
    std::vector<uint32_t> pvrootdirlen;
    std::vector<uint32_t> svrootdirblk;
    std::vector<uint32_t> svrootdirlen;
};

/*
#define MASK(n)	((1UL << n) - 1)

std::string ConvertBlocksToExtents(std::vector<uint32_t>* blocklist, uint32_t blocksize);
void GetContentBlocks(std::ifstream* devicebuffer, uint32_t blocksize, uint64_t curoffset, uint32_t incompatflags, std::vector<uint32_t>* blocklist);

uint64_t ParseXfsPath(std::ifstream* rawcontent, xfssuperblockinfo* cursb, uint64_t curinode, std::string childpath);
std::string ParseXfsFile(std::ifstream* rawcontent, xfssuperblockinfo* cursb, uint64_t curinode, std::string filename);
void ParseSuperBlock(std::ifstream* rawcontent, xfssuperblockinfo* cursb);
*/

void ParseSuperBlock(std::ifstream* rawcontent, isosuperblockinfo* cursb);
void ParseIsoForensics(std::string filename, std::string mntptstr, std::string devicestr);
