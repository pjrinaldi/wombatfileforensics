#include "common.h"

struct ntfsinfo
{
    uint16_t bytespersector = 0;
    uint8_t sectorspercluster = 0;
    uint8_t mftentrysize = 0;
    uint64_t mftstartingcluster = 0;
    uint64_t maxmftentrycount = 0;
};

std::string ConvertNtfsTimeToHuman(uint64_t ntfstime);

std::string GetDataAttributeLayout(std::ifstream* rawcontent, ntfsinfo* curnt, uint64_t mftoffset);
std::string GetIndexAttributesLayout(std::ifstream* rawcontent, ntfsinfo* curnt, uint64_t mftentryoffset);
std::string GetStandardInformationAttribute(std::ifstream* rawcontent, ntfsinfo* curnt, uint64_t mftentryoffset);
std::string GetFileNameAttribute(std::ifstream* rawcontent, ntfsinfo* curnt, uint64_t mftentryoffset);
std::string GetDataAttribute(std::ifstream* rawcontent, ntfsinfo* curnt, uint64_t mftentryoffset);

uint64_t ParseNtfsPath(std::ifstream* rawcontent, ntfsinfo* curnt, uint64_t ntinode, std::string childpath);
std::string ParseNtfsFile(std::ifstream* rawcontent, ntfsinfo* curnt, uint64_t ntinode, std::string childfile);

void GetRunListLayout(std::ifstream* rawcontent, ntfsinfo* curnt, uint64_t curoffset, uint32_t attributelength, std::string* runliststr);

void ParseNtfsInfo(std::ifstream* rawcontent, ntfsinfo* curnt);
void ParseNtfsForensics(std::string filename, std::string mntptstr, std::string devicestr, uint8_t ftype);
