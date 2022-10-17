#include "common.h"

struct fatinfo
{
    uint16_t bytespersector = 0;
    uint8_t sectorspercluster = 0;
    uint8_t fattype = 0;
    uint32_t fatoffset = 0;
    uint32_t fatsize = 0;
    uint64_t clusterareastart = 0;
    std::string rootdirlayout = "";
    std::string curdirlayout = "";
};

std::string ConvertDosTimeToHuman(uint16_t* dosdate, uint16_t* dostime);
std::string ConvertExFatTimeToHuman(uint16_t* dosdate, uint16_t* dostime, uint8_t* timezone);
std::string ConvertClustersToExtents(std::vector<uint32_t>* clusterlist, fatinfo* curfat);
std::string ParseFatPath(std::ifstream* rawcontent, fatinfo* curfat, std::string childpath);
std::string ParseFatFile(std::ifstream* rawcontent, fatinfo* curfat, std::string childfile);

void GetNextCluster(std::ifstream* rawcontent, uint32_t clusternum, fatinfo* curfat, std::vector<uint32_t>* clusterlist);
void ParseFatInfo(std::ifstream* rawcontent, fatinfo* curfat);
void ParseFatForensics(std::string filename, std::string mntptstr, std::string devicestr, uint8_t ftype);
