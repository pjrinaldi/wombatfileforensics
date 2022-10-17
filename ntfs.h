#include "common.h"


struct ntfsinfo
{
    uint16_t bytespersector = 0;
    uint8_t sectorspercluster = 0;
    uint8_t mftentrysize = 0;
    uint64_t mftstartingcluster = 0;
    uint64_t maxmftentrycount = 0;
    //std::string mftlayout = "";
};
/*
std::string ConvertDosTimeToHuman(uint16_t* dosdate, uint16_t* dostime);
std::string ConvertExFatTimeToHuman(uint16_t* dosdate, uint16_t* dostime, uint8_t* timezone);
void GetNextCluster(std::ifstream* rawcontent, uint32_t clusternum, fatinfo* curfat, std::vector<uint32_t>* clusterlist);
std::string ConvertClustersToExtents(std::vector<uint32_t>* clusterlist, fatinfo* curfat);
std::string ParseFatPath(std::ifstream* rawcontent, fatinfo* curfat, std::string childpath);
std::string ParseFatFile(std::ifstream* rawcontent, fatinfo* curfat, std::string childfile);

*/

std::string GetDataAttributeLayout(std::ifstream* rawcontent, ntfsinfo* curnt, uint64_t mftoffset);
std::string GetIndexAttributesLayout(std::ifstream* rawcontent, ntfsinfo* curnt, uint64_t mftentryoffset);

uint64_t ParseNtfsPath(std::ifstream* rawcontent, ntfsinfo* curnt, uint64_t ntinode, std::string childpath);

void GetRunListLayout(std::ifstream* rawcontent, ntfsinfo* curnt, uint64_t curoffset, uint32_t attributelength, std::string* runliststr);

void ParseNtfsInfo(std::ifstream* rawcontent, ntfsinfo* curnt);
void ParseNtfsForensics(std::string filename, std::string mntptstr, std::string devicestr, uint8_t ftype);
