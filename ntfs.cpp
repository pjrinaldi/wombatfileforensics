#include "ntfs.h"

void GetRunListLayout(std::ifstream* rawcontent, ntfsinfo* curnt, uint64_t curoffset, uint32_t attributelength, std::string* runliststr)
{
    // RUN LIST OFFSET
    uint8_t* rlo = new uint8_t[2];
    uint16_t runlistoffset = 0;
    ReadContent(rawcontent, rlo, curoffset + 32, 2);
    ReturnUint16(&runlistoffset, rlo);
    delete[] rlo;
    //std::cout << "Run List Offset: " << runlistoffset << std::endl;
    unsigned int currunoffset = curoffset + runlistoffset;
    std::vector<uint64_t> runofflist;
    std::vector<uint64_t> runlenlist;
    runofflist.clear();
    runlenlist.clear();
    //std::vector<std::string> runlist;
    //runlist.clear();
    int i = 0;
    while(currunoffset < curoffset + attributelength)
    {
        // RUN INFO
        uint8_t* ri = new uint8_t[1];
        uint8_t runinfo = 0;
        ReadContent(rawcontent, ri, currunoffset, 1);
        runinfo = (uint8_t)ri[0];
        delete[] ri;
        if(runinfo > 0)
        {
            std::bitset<8> runbits{runinfo};
            //std::cout << "run bits: " << runbits << std::endl;
            std::bitset<4> runlengthbits;
            std::bitset<4> runoffsetbits;
            for(int j=0; j < 4; j++)
            {
                runlengthbits.set(j, runbits[j]);
                runoffsetbits.set(j, runbits[j+4]);
            }
            //std::cout << "run length bits: " << runlengthbits << std::endl;
            //std::cout << "run offset bits: " << runoffsetbits << std::endl;
            unsigned int runlengthbytes = runlengthbits.to_ulong();
            unsigned int runoffsetbytes = runoffsetbits.to_ulong();
            //std::cout << "run length: " << runlengthbytes << std::endl;
            //std::cout << "run offset: " << runoffsetbytes << std::endl;
            if(runlengthbytes == 0 && runoffsetbytes == 0)
                break;
            currunoffset++;
            //std::cout << "current run offset: " << currunoffset << std::endl;
            //std::cout << "local attribute current run offset: " << currunoffset - curoffset << std::endl;
            uint64_t runlength = 0;
            uint64_t runoffset = 0;
            if(runlengthbytes == 1)
            {
                uint8_t* rl = new uint8_t[1];
                ReadContent(rawcontent, rl, currunoffset, 1);
                runlength = (uint8_t)rl[0];
                delete[] rl;
            }
            else
            {
                uint8_t* rl = new uint8_t[runlengthbytes];
                ReadContent(rawcontent, rl, currunoffset, runlengthbytes);
                ReturnUint(&runlength, rl, runlengthbytes);
            }
            //std::cout << "data run length: " << runlength << std::endl;
            runlenlist.push_back(runlength);
            if(runoffsetbits.to_ulong() == 1)
            {
                uint8_t* ro = new uint8_t[1];
                ReadContent(rawcontent, ro, currunoffset + runlengthbytes, 1);
                runoffset = (uint8_t)ro[0];
                delete[] ro;
            }
            else
            {
                uint8_t* ro = new uint8_t[runoffsetbytes];
                ReadContent(rawcontent, ro, currunoffset + runlengthbytes, runoffsetbytes);
                ReturnUint(&runoffset, ro, runoffsetbytes);
            }
            if(i > 0)
            {
                std::bitset<8> runoffsetbits{runoffset};
                if(i > 1 && runoffsetbits[0] == 1)
                    runoffset = runoffset - 0xffffffff - 1;
                runoffset = runoffset + runofflist.at(i-1);
            }
            //std::cout << "data run offset: " << runoffset << std::endl;
            runofflist.push_back(runoffset);
            i++;
            currunoffset += runlengthbytes + runoffsetbytes;
        }
        else
            break;
    }
    for(int i=0; i < runofflist.size(); i++)
        *runliststr += std::to_string(runofflist.at(i) * curnt->sectorspercluster * curnt->bytespersector) + "," + std::to_string(runlenlist.at(i) * curnt->sectorspercluster * curnt->bytespersector) + ";";
    runofflist.clear();
    runlenlist.clear();
}

std::string ConvertNtfsTimeToHuman(uint64_t ntfstime)
{
    uint64_t tmp = ntfstime / 10000000; // convert from 100ns intervals to seconds
    tmp = tmp - 11644473600LL; // subtract number of seconds between epochs
    time_t timet = (time_t)((uint32_t)tmp);
    struct tm* tmtime = gmtime(&timet);
    char hchar[100];
    strftime(hchar, 100, "%m/%d/%Y %I:%M:%S %p UTC", tmtime);
    
    return std::string(hchar);
}

std::string GetIndexAttributesLayout(std::ifstream* rawcontent, ntfsinfo* curnt, uint64_t mftentryoffset)
{
    std::string indexlayout = "";
    uint8_t* mes = new uint8_t[5];
    ReadContent(rawcontent, mes, mftentryoffset, 4);
    mes[4] = '\0';
    std::string mestr((char*)mes);
    delete[] mes;
    //std::cout << "MFT Entry 0 Signature: " << me0str << std::endl;
    if(mestr.compare("FILE") == 0) // A PROPER MFT ENTRY
    {
        // OFFSET TO THE FIRST ATTRIBUTE
        uint8_t* fao = new uint8_t[2];
        uint16_t firstattributeoffset = 0;
        ReadContent(rawcontent, fao, mftentryoffset + 20, 2);
        ReturnUint16(&firstattributeoffset, fao);
        delete[] fao;
        //std::cout << "First Attribute Offset: " << firstattributeoffset << std::endl;
        uint64_t indexrootoffset = 0;
        uint32_t indexrootlength = 0;
        std::string indexallocationlayout = "";
        // LOOP OVER ATTRIBUTES TO FIND DATA ATTRIBUTE
        uint16_t curoffset = firstattributeoffset;
        while(curoffset < curnt->mftentrysize * curnt->sectorspercluster * curnt->bytespersector)
        {
            //std::cout << "Current Offset: " << curoffset << std::endl;
            // ATTRIBUTE LENGTH
            uint8_t* al = new uint8_t[4];
            uint32_t attributelength = 0;
            ReadContent(rawcontent, al, mftentryoffset + curoffset + 4, 4);
            ReturnUint32(&attributelength, al);
            delete[] al;
            //std::cout << "Attribute Length: " << attributelength << std::endl;
            // ATTRIBUTE TYPE
            uint8_t* at = new uint8_t[4];
            uint32_t attributetype = 0;
            ReadContent(rawcontent, at, mftentryoffset + curoffset, 4);
            ReturnUint32(&attributetype, at);
            delete[] at;
            //std::cout << "Attribute Type: 0x" << std::hex << attributetype << std::dec << std::endl;
            if(attributetype == 0x90) // $INDEX_ROOT - ALWAYS RESIDENT
            {
                // ATTRIBUTE CONTENT LENGTH
                uint8_t* cl = new uint8_t[4];
                uint32_t contentlength = 0;
                ReadContent(rawcontent, cl, mftentryoffset + curoffset + 16, 4);
                ReturnUint32(&contentlength, cl);
                delete[] cl;
                indexrootlength = contentlength;
                // ATTRIBUTE CONTENT OFFSET
                uint8_t* co = new uint8_t[2];
                uint16_t contentoffset = 0;
                ReadContent(rawcontent, co, mftentryoffset + curoffset + 20, 2);
                ReturnUint16(&contentoffset, co);
                delete[] co;
                indexrootoffset = mftentryoffset + curoffset + contentoffset;
            }
            else if(attributetype == 0xa0) // $INDEX_ALLOCATION - ALWAYS NON-RESIDENT
            {
                 GetRunListLayout(rawcontent, curnt, mftentryoffset + curoffset, attributelength, &indexallocationlayout);
            }
            curoffset += attributelength;
            if(attributelength == 0 || attributetype == 0xffffffff)
                break;
        }
        indexlayout = std::to_string(indexrootoffset) + "," + std::to_string(indexrootlength) + ";";
        if(!indexallocationlayout.empty())
            indexlayout += indexallocationlayout;

        //std::cout << "Index Root Offset: " << indexrootoffset << " Length: " << indexrootlength << std::endl;
        //std::cout << "Index Allocation Layout: " << indexallocationlayout << std::endl;

        return indexlayout;
    }

    return "";

}

std::string GetStandardInformationAttribute(std::ifstream* rawcontent, ntfsinfo* curnt, uint64_t mftentryoffset)
{
    std::string siforensics = "";
    uint8_t* mes = new uint8_t[5];
    ReadContent(rawcontent, mes, mftentryoffset, 4);
    mes[4] = '\0';
    std::string mesigstr((char*)mes);
    delete[] mes;
    //std::cout << "MFT Entry Signature: " << mesigstr << std::endl;
    if(mesigstr.compare("FILE") == 0) // A PROPER MFT ENTRY
    {
        // OFFSET TO THE FIRST ATTRIBUTE
        uint8_t* fao = new uint8_t[2];
        uint16_t firstattributeoffset = 0;
        ReadContent(rawcontent, fao, mftentryoffset + 20, 2);
        ReturnUint16(&firstattributeoffset, fao);
        delete[] fao;
        //std::cout << "First Attribute Offset: " << firstattributeoffset << std::endl;
        // LOOP OVER ATTRIBUTES TO FIND DATA ATTRIBUTE
        uint16_t curoffset = firstattributeoffset;
        while(curoffset < curnt->mftentrysize * curnt->sectorspercluster * curnt->bytespersector)
        {
            //std::cout << "Current Offset: " << curoffset << std::endl;
            /*
            // IS RESIDENT/NON-RESIDENT
            uint8_t* rf = new uint8_t[1];
            uint8_t isnonresident = 0; // 0 - Resident | 1 - Non-Resident
            ReadContent(rawcontent, rf, mftentryoffset + curoffset + 8, 1);
            isnonresident = (uint8_t)rf[0];
            delete[] rf;
            //std::cout << "Is None Resident: " << (int)isnonresident << std::endl;
            */
            // ATTRIBUTE LENGTH
            uint8_t* al = new uint8_t[4];
            uint32_t attributelength = 0;
            ReadContent(rawcontent, al, mftentryoffset + curoffset + 4, 4);
            ReturnUint32(&attributelength, al);
            delete[] al;
            //std::cout << "Attribute Length: " << attributelength << std::endl;
            // ATTRIBUTE TYPE
            uint8_t* at = new uint8_t[4];
            uint32_t attributetype = 0;
            ReadContent(rawcontent, at, mftentryoffset + curoffset, 4);
            ReturnUint32(&attributetype, at);
            delete[] at;
            //std::cout << "Attribute Type: 0x" << std::hex << attributetype << std::dec << std::endl;
	    if(attributetype == 0x10) // STANDARD_INFORMATION ATTRIBUTE - always resident
	    {
                // CONTENT SIZE
		uint8_t* cs = new uint8_t[4];
		uint32_t contentsize = 0;
		ReadContent(rawcontent, cs, mftentryoffset + curoffset + 16, 4);
		ReturnUint32(&contentsize, cs);
		delete[] cs;
                // CONTENT OFFSET
		uint8_t* co = new uint8_t[2];
		uint16_t contentoffset = 0;
		ReadContent(rawcontent, co, mftentryoffset + curoffset + 20, 2);
		ReturnUint16(&contentoffset, co);
		delete[] co;
                // CREATE DATE
                uint8_t* cd = new uint8_t[8];
		uint64_t createdate = 0;
		ReadContent(rawcontent, cd, mftentryoffset + curoffset + contentoffset, 8);
		ReturnUint64(&createdate, cd);
		delete[] cd;
		siforensics += "Create Date|" + ConvertNtfsTimeToHuman(createdate) + "\n";
                // MODIFY DATE
		uint8_t* md = new uint8_t[8];
		uint64_t modifydate = 0;
		ReadContent(rawcontent, md, mftentryoffset + curoffset + contentoffset + 8, 8);
		ReturnUint64(&modifydate, md);
		delete[] md;
		siforensics += "Modify Date|" + ConvertNtfsTimeToHuman(modifydate) + "\n";
                // STATUS DATE
                uint8_t* sd = new uint8_t[8];
                uint64_t statusdate = 0;
                ReadContent(rawcontent, sd, mftentryoffset + curoffset + contentoffset + 16, 8);
                ReturnUint64(&statusdate, sd);
                delete[] sd;
                siforensics += "Status Date|" + ConvertNtfsTimeToHuman(statusdate) + "\n";
                // ACCESS DATE
                uint8_t* ad = new uint8_t[8];
                uint64_t accessdate = 0;
                ReadContent(rawcontent, ad, mftentryoffset + curoffset + contentoffset + 24, 8);
                ReturnUint64(&accessdate, ad);
                delete[] ad;
                siforensics += "Access Date|" + ConvertNtfsTimeToHuman(accessdate) + "\n";
                // OWNER ID
                uint8_t* oi = new uint8_t[4];
                uint32_t ownerid = 0;
                ReadContent(rawcontent, oi, mftentryoffset + curoffset + contentoffset + 48, 4);
                ReturnUint32(&ownerid, oi);
                delete[] oi;
                siforensics += "Owner ID|" + std::to_string(ownerid) + "\n";
                // SECURITY ID
                uint8_t* si = new uint8_t[4];
                uint32_t securityid = 0;
                ReadContent(rawcontent, si, mftentryoffset + curoffset + contentoffset + 52, 4);
                ReturnUint32(&securityid, si);
                delete[] si;
                siforensics += "Security ID|" + std::to_string(securityid) + "\n";
                // ACCESS FLAGS
                uint8_t* af = new uint8_t[4];
                uint32_t accessflags = 0;
                ReadContent(rawcontent, af, mftentryoffset + curoffset + contentoffset + 32, 4);
                ReturnUint32(&accessflags, af);
                delete[] af;
                siforensics += "Attributes|";
                if(accessflags & 0x01)
                    siforensics += "Read Only,";
                if(accessflags & 0x02)
                    siforensics += "Hidden,";
                if(accessflags & 0x04)
                    siforensics += "System,";
                if(accessflags & 0x20)
                    siforensics += "Archive,";
                if(accessflags & 0x40)
                    siforensics += "Device,";
                if(accessflags & 0x80)
                    siforensics += "Normal,";
                if(accessflags & 0x100)
                    siforensics += "Temporary,";
                if(accessflags & 0x200)
                    siforensics += "Sparse File,";
                if(accessflags & 0x400)
                    siforensics += "Reparse Point,";
                if(accessflags & 0x800)
                    siforensics += "Compressed,";
                if(accessflags & 0x1000)
                    siforensics += "Offline,";
                if(accessflags & 0x2000)
                    siforensics += "Not Indexed,";
                if(accessflags & 0x4000)
                    siforensics += "Encrypted,";
                siforensics += "\n";

		return siforensics;
	    }
            if(attributelength == 0 || attributetype == 0xffffffff)
                break;
	    curoffset += attributelength;
	}
    }

    return siforensics;
}

std::string GetFileNameAttribute(std::ifstream* rawcontent, ntfsinfo* curnt, uint64_t mftentryoffset)
{
    std::string fnforensics = "";
    uint8_t* mes = new uint8_t[5];
    ReadContent(rawcontent, mes, mftentryoffset, 4);
    mes[4] = '\0';
    std::string mesigstr((char*)mes);
    delete[] mes;
    //std::cout << "MFT Entry Signature: " << mesigstr << std::endl;
    if(mesigstr.compare("FILE") == 0) // A PROPER MFT ENTRY
    {
        // OFFSET TO THE FIRST ATTRIBUTE
        uint8_t* fao = new uint8_t[2];
        uint16_t firstattributeoffset = 0;
        ReadContent(rawcontent, fao, mftentryoffset + 20, 2);
        ReturnUint16(&firstattributeoffset, fao);
        delete[] fao;
        //std::cout << "First Attribute Offset: " << firstattributeoffset << std::endl;
        // LOOP OVER ATTRIBUTES TO FIND DATA ATTRIBUTE
        uint16_t curoffset = firstattributeoffset;
        while(curoffset < curnt->mftentrysize * curnt->sectorspercluster * curnt->bytespersector)
        {
            //std::cout << "Current Offset: " << curoffset << std::endl;
            /*
            // IS RESIDENT/NON-RESIDENT
            uint8_t* rf = new uint8_t[1];
            uint8_t isnonresident = 0; // 0 - Resident | 1 - Non-Resident
            ReadContent(rawcontent, rf, mftentryoffset + curoffset + 8, 1);
            isnonresident = (uint8_t)rf[0];
            delete[] rf;
            //std::cout << "Is None Resident: " << (int)isnonresident << std::endl;
            */
            // ATTRIBUTE LENGTH
            uint8_t* al = new uint8_t[4];
            uint32_t attributelength = 0;
            ReadContent(rawcontent, al, mftentryoffset + curoffset + 4, 4);
            ReturnUint32(&attributelength, al);
            delete[] al;
            //std::cout << "Attribute Length: " << attributelength << std::endl;
            // ATTRIBUTE TYPE
            uint8_t* at = new uint8_t[4];
            uint32_t attributetype = 0;
            ReadContent(rawcontent, at, mftentryoffset + curoffset, 4);
            ReturnUint32(&attributetype, at);
            delete[] at;
            //std::cout << "Attribute Type: 0x" << std::hex << attributetype << std::dec << std::endl;
	    if(attributetype == 0x30) // FILE_NAME ATTRIBUTE - always resident
	    {
                // CONTENT SIZE
		uint8_t* cs = new uint8_t[4];
		uint32_t contentsize = 0;
		ReadContent(rawcontent, cs, mftentryoffset + curoffset + 16, 4);
		ReturnUint32(&contentsize, cs);
		delete[] cs;
                // CONTENT OFFSET
		uint8_t* co = new uint8_t[2];
		uint16_t contentoffset = 0;
		ReadContent(rawcontent, co, mftentryoffset + curoffset + 20, 2);
		ReturnUint16(&contentoffset, co);
		delete[] co;
                // CREATE DATE
                uint8_t* cd = new uint8_t[8];
		uint64_t createdate = 0;
		ReadContent(rawcontent, cd, mftentryoffset + curoffset + contentoffset + 8, 8);
		ReturnUint64(&createdate, cd);
		delete[] cd;
		fnforensics += "Create Date|" + ConvertNtfsTimeToHuman(createdate) + "\n";
                // MODIFY DATE
                uint8_t* md = new uint8_t[8];
		uint64_t modifydate = 0;
		ReadContent(rawcontent, md, mftentryoffset + curoffset + contentoffset + 16, 8);
		ReturnUint64(&modifydate, md);
		delete[] md;
		fnforensics += "Modify Date|" + ConvertNtfsTimeToHuman(modifydate) + "\n";
                // STATUS DATE
                uint8_t* sd = new uint8_t[8];
		uint64_t statusdate = 0;
		ReadContent(rawcontent, sd, mftentryoffset + curoffset + contentoffset + 24, 8);
		ReturnUint64(&statusdate, sd);
		delete[] sd;
		fnforensics += "Status Date|" + ConvertNtfsTimeToHuman(statusdate) + "\n";
                // ACCESS DATE
                uint8_t* ad = new uint8_t[8];
		uint64_t accessdate = 0;
		ReadContent(rawcontent, ad, mftentryoffset + curoffset + contentoffset + 32, 8);
		ReturnUint64(&accessdate, ad);
		delete[] ad;
		fnforensics += "Access Date|" + ConvertNtfsTimeToHuman(accessdate) + "\n";
                // FILE NAMESPACE
                uint8_t* fns = new uint8_t[1];
                uint8_t filenamespace = 0;
                ReadContent(rawcontent, fns, mftentryoffset + curoffset + contentoffset + 65, 1);
                filenamespace = (uint8_t)fns[0];
                delete[] fns;
                if(filenamespace != 0x02)
                {
                    // NAME LENGTH
                    uint8_t* nl = new uint8_t[1];
                    uint8_t namelength = 0;
                    ReadContent(rawcontent, nl, mftentryoffset + curoffset + contentoffset + 64, 1);
                    namelength = (uint8_t)nl[0];
                    delete[] nl;
                    // FILE NAME
                    std::string filename = "";
                    for(uint8_t j=0; j < namelength; j++)
                    {
                        uint8_t* sl = new uint8_t[2];
                        uint16_t singleletter = 0;
                        ReadContent(rawcontent, sl, mftentryoffset + curoffset + contentoffset + 66 + j*2, 2);
                        ReturnUint16(&singleletter, sl);
                        delete[] sl;
                        filename += (char)singleletter;
                    }
                    fnforensics += "\nName|" + filename + "\n";
                }
            }
            if(attributelength == 0 || attributetype == 0xffffffff)
                break;
            curoffset += attributelength;
        }
    }
    
    return fnforensics;
}

std::string GetDataAttribute(std::ifstream* rawcontent, ntfsinfo* curnt, uint64_t mftentryoffset)
{
    std::string dataforensics = "";
    std::string runliststr = "";
    uint8_t* mes = new uint8_t[5];
    ReadContent(rawcontent, mes, mftentryoffset, 4);
    mes[4] = '\0';
    std::string mesigstr((char*)mes);
    delete[] mes;
    //std::cout << "MFT Entry Signature: " << mesigstr << std::endl;
    if(mesigstr.compare("FILE") == 0) // A PROPER MFT ENTRY
    {
        // OFFSET TO THE FIRST ATTRIBUTE
        uint8_t* fao = new uint8_t[2];
        uint16_t firstattributeoffset = 0;
        ReadContent(rawcontent, fao, mftentryoffset + 20, 2);
        ReturnUint16(&firstattributeoffset, fao);
        delete[] fao;
        //std::cout << "First Attribute Offset: " << firstattributeoffset << std::endl;
        // LOOP OVER ATTRIBUTES TO FIND DATA ATTRIBUTE
        uint16_t curoffset = firstattributeoffset;
        while(curoffset < curnt->mftentrysize * curnt->sectorspercluster * curnt->bytespersector)
        {
            //std::cout << "Current Offset: " << curoffset << std::endl;
            // IS RESIDENT/NON-RESIDENT
            uint8_t* rf = new uint8_t[1];
            uint8_t isnonresident = 0; // 0 - Resident | 1 - Non-Resident
            ReadContent(rawcontent, rf, mftentryoffset + curoffset + 8, 1);
            isnonresident = (uint8_t)rf[0];
            delete[] rf;
            //std::cout << "Is None Resident: " << (int)isnonresident << std::endl;
            // ATTRIBUTE LENGTH
            uint8_t* al = new uint8_t[4];
            uint32_t attributelength = 0;
            ReadContent(rawcontent, al, mftentryoffset + curoffset + 4, 4);
            ReturnUint32(&attributelength, al);
            delete[] al;
            //std::cout << "Attribute Length: " << attributelength << std::endl;
            // ATTRIBUTE TYPE
            uint8_t* at = new uint8_t[4];
            uint32_t attributetype = 0;
            ReadContent(rawcontent, at, mftentryoffset + curoffset, 4);
            ReturnUint32(&attributetype, at);
            delete[] at;
            //std::cout << "Attribute Type: 0x" << std::hex << attributetype << std::dec << std::endl;
	    if(attributetype == 0x80) // DATA ATTRIBUTE - resident/non-resident
	    {
                uint8_t* anl = new uint8_t[1];
                uint8_t attributenamelength = 0;
                ReadContent(rawcontent, anl, mftentryoffset + curoffset + 9, 1);
                attributenamelength = (uint8_t)anl[0];
                delete[] anl;
                //std::cout << "Attribute Name Length: " << (int)attributenamelength << std::endl;
                if(attributenamelength == 0) // DEFAULT DATA ENTRY
                {
                    dataforensics += "Default||Layout|";
                }
                else // Alternate data stream
                {
                    std::string adsname = "";
                    uint8_t* no = new uint8_t[2];
                    uint16_t nameoffset = 0;
                    ReadContent(rawcontent, no, mftentryoffset + curoffset + 10, 2);
                    ReturnUint16(&nameoffset, no);
                    delete[] no;
                    for(int j=0; j < attributenamelength; j++)
                    {
                        uint8_t* sl = new uint8_t[2];
                        uint16_t singleletter = 0;
                        ReadContent(rawcontent, sl, mftentryoffset + curoffset + nameoffset + j*2, 2);
                        ReturnUint16(&singleletter, sl);
                        delete[] sl;
                        adsname += (char)singleletter;
                    }
                    dataforensics += "Alternate||Name|" + adsname + "||Layout|";
                }
                    if(isnonresident == 1)
                    {
                        // GET RUN LIST AND RETURN LAYOUT
                        uint64_t totalmftsize = 0;
                        GetRunListLayout(rawcontent, curnt, mftentryoffset + curoffset, attributelength, &runliststr);
                        dataforensics += runliststr + "\n";
                        //std::cout << "Run List Layout: " << runliststr << std::endl;
                        break;
                    }
                    else // is resident 0
                    {
                        // CONTENT SIZE
                        uint8_t* cs = new uint8_t[4];
                        uint32_t contentsize = 0;
                        ReadContent(rawcontent, cs, mftentryoffset + curoffset + 16, 4);
                        ReturnUint32(&contentsize, cs);
                        delete[] cs;
                        // CONTENT OFFSET
                        uint8_t* co = new uint8_t[2];
                        uint16_t contentoffset = 0;
                        ReadContent(rawcontent, co, mftentryoffset + curoffset + 20, 2);
                        ReturnUint16(&contentoffset, co);
                        delete[] co;
                        dataforensics += std::to_string(mftentryoffset + curoffset + contentoffset) + "," + std::to_string(contentsize) + ";" + "\n";
                    }
            }
            if(attributelength == 0 || attributetype == 0xffffffff)
                break;
            curoffset += attributelength;
        }
    }

    return dataforensics;
}

// will need to fix this so it accounts for resident and non-resident...
// right now it is non-resident only since the mft is always non-resident..
std::string GetDataAttributeLayout(std::ifstream* rawcontent, ntfsinfo* curnt, uint64_t mftoffset)
{
    uint8_t* me0s = new uint8_t[5];
    ReadContent(rawcontent, me0s, mftoffset, 4);
    me0s[4] = '\0';
    std::string me0str((char*)me0s);
    delete[] me0s;
    //std::cout << "MFT Entry 0 Signature: " << me0str << std::endl;
    if(me0str.compare("FILE") == 0) // A PROPER MFT ENTRY
    {
        std::string runliststr = "";
        // OFFSET TO THE FIRST ATTRIBUTE
        uint8_t* fao = new uint8_t[2];
        uint16_t firstattributeoffset = 0;
        ReadContent(rawcontent, fao, mftoffset + 20, 2);
        ReturnUint16(&firstattributeoffset, fao);
        delete[] fao;
        //std::cout << "First Attribute Offset: " << firstattributeoffset << std::endl;
        // LOOP OVER ATTRIBUTES TO FIND DATA ATTRIBUTE
        uint16_t curoffset = firstattributeoffset;
        while(curoffset < curnt->mftentrysize * curnt->sectorspercluster * curnt->bytespersector)
        {
            //std::cout << "Current Offset: " << curoffset << std::endl;
            // IS RESIDENT/NON-RESIDENT
            uint8_t* rf = new uint8_t[1];
            uint8_t isnonresident = 0; // 0 - Resident | 1 - Non-Resident
            ReadContent(rawcontent, rf, mftoffset + curoffset + 8, 1);
            isnonresident = (uint8_t)rf[0];
            delete[] rf;
            //std::cout << "Is None Resident: " << (int)isnonresident << std::endl;
            // ATTRIBUTE LENGTH
            uint8_t* al = new uint8_t[4];
            uint32_t attributelength = 0;
            ReadContent(rawcontent, al, mftoffset + curoffset + 4, 4);
            ReturnUint32(&attributelength, al);
            delete[] al;
            //std::cout << "Attribute Length: " << attributelength << std::endl;
            // ATTRIBUTE TYPE
            uint8_t* at = new uint8_t[4];
            uint32_t attributetype = 0;
            ReadContent(rawcontent, at, mftoffset + curoffset, 4);
            ReturnUint32(&attributetype, at);
            delete[] at;
            //std::cout << "Attribute Type: 0x" << std::hex << attributetype << std::dec << std::endl;
            if(attributetype == 0x80) // DATA ATTRIBUTE
            {
                uint8_t* anl = new uint8_t[1];
                uint8_t attributenamelength = 0;
                ReadContent(rawcontent, anl, mftoffset + curoffset + 9, 1);
                attributenamelength = (uint8_t)anl[0];
                delete[] anl;
                //std::cout << "Attribute Name Length: " << (int)attributenamelength << std::endl;
                if(attributenamelength == 0) // DEFAULT DATA ENTRY
                {
                    if(isnonresident == 1)
                    {
                        // GET RUN LIST AND RETURN LAYOUT
                        uint64_t totalmftsize = 0;
                        GetRunListLayout(rawcontent, curnt, mftoffset + curoffset, attributelength, &runliststr);
                        //std::cout << "Run List Layout: " << runliststr << std::endl;
                        break;
                    }
                    else // is resident 0
                    {
                        // CONTENT SIZE
                        uint8_t* cs = new uint8_t[4];
                        uint32_t contentsize = 0;
                        ReadContent(rawcontent, cs, mftoffset + curoffset + 16, 4);
                        ReturnUint32(&contentsize, cs);
                        delete[] cs;
                        // CONTENT OFFSET
                        uint8_t* co = new uint8_t[2];
                        uint16_t contentoffset = 0;
                        ReadContent(rawcontent, co, mftoffset + curoffset + 20, 2);
                        ReturnUint16(&contentoffset, co);
                        delete[] co;
                        runliststr = std::to_string(mftoffset + curoffset + contentoffset) + "," + std::to_string(contentsize) + ";";
                    }
                }
            }
            curoffset += attributelength;
            if(attributelength == 0 || attributetype == 0xffffffff)
                break;
        }
        return runliststr;
    }
    else
        return "";
}

void ParseNtfsInfo(std::ifstream* rawcontent, ntfsinfo* curnt)
{
    // BYTES PER SECTOR
    uint8_t* bps = new uint8_t[2];
    uint16_t bytespersector = 0;
    ReadContent(rawcontent, bps, 11, 2);
    ReturnUint16(&bytespersector, bps);
    delete[] bps;
    curnt->bytespersector = bytespersector;
    //std::cout << "Bytes Per Sector: " << bytespersector << std::endl;
    // SECTORS PER CLUSTER
    uint8_t* spc = new uint8_t[1];
    uint8_t sectorspercluster = 0;
    ReadContent(rawcontent, spc, 13, 1);
    sectorspercluster = (uint8_t)spc[0];
    delete[] spc;
    curnt->sectorspercluster = sectorspercluster;
    //std::cout << "Sectors per cluster: " << (unsigned int)sectorspercluster << std::endl;
    // MFT STARTING CLUSTER
    uint8_t* msc = new uint8_t[8];
    uint64_t mftstartingcluster = 0;
    ReadContent(rawcontent, msc, 48, 8);
    ReturnUint64(&mftstartingcluster, msc);
    delete[] msc;
    curnt->mftstartingcluster = mftstartingcluster;
    //std::cout << "MFT Starting Cluster|Offset: " << mftstartingcluster << " | " << mftstartingcluster * sectorspercluster * bytespersector << std::endl;
    // MFT ENTRY SIZE
    uint8_t* mes = new uint8_t[1];
    uint8_t mftentrysize = 0;
    ReadContent(rawcontent, mes, 64, 1);
    mftentrysize = (uint8_t)mes[0];
    curnt->mftentrysize = mftentrysize;
    //std::cout << "MFT Entry Size (cluster, bytes): " << (int)mftentrysize << ", " << mftentrysize * sectorspercluster * bytespersector << std::endl;
    // MFT LAYOUT
    uint64_t mftoffset = mftstartingcluster * sectorspercluster * bytespersector;
    std::string mftlayout = GetDataAttributeLayout(rawcontent, curnt, mftoffset);
    //curnt->mftlayout = mftlayout;
    std::vector<std::string> mftlayoutlist;
    mftlayoutlist.clear();
    std::istringstream mll(mftlayout);
    std::string mls;
    uint64_t mftsize = 0;
    while(getline(mll, mls, ';'))
        mftlayoutlist.push_back(mls);
    for(int i=0; i < mftlayoutlist.size(); i++)
    {
        std::size_t layoutsplit = mftlayoutlist.at(i).find(",");
        mftsize += std::stoull(mftlayoutlist.at(i).substr(layoutsplit+1));
    }
    //std::cout << "MFT Size: " << mftsize << std::endl;
    uint64_t maxmftentrycount = mftsize / (curnt->mftentrysize * curnt->sectorspercluster * curnt->bytespersector);
    curnt->maxmftentrycount = maxmftentrycount;
    //std::cout << "Max MFT Entry Count: " << maxmftentrycount << std::endl;
}

uint64_t ParseNtfsPath(std::ifstream* rawcontent, ntfsinfo* curnt, uint64_t ntinode, std::string childpath)
{
    uint64_t childntinode = 0;
    uint64_t mftentryoffset = 0;
    uint64_t mftoffset = curnt->mftstartingcluster * curnt->sectorspercluster * curnt->bytespersector;
    uint64_t mftlength = 0;
    std::string mftlayout = GetDataAttributeLayout(rawcontent, curnt, mftoffset);
    std::vector<std::string> mftlayoutlist;
    mftlayoutlist.clear();
    std::istringstream mll(mftlayout);
    std::string mls;
    uint64_t relativentinode = ntinode;
    uint64_t mftsize = 0;
    while(getline(mll, mls, ';'))
        mftlayoutlist.push_back(mls);
    for(int i=0; i < mftlayoutlist.size(); i++)
    {
        std::size_t layoutsplit = mftlayoutlist.at(i).find(",");
        mftoffset = std::stoull(mftlayoutlist.at(i).substr(0, layoutsplit));
        mftlength = std::stoull(mftlayoutlist.at(i).substr(layoutsplit+1));
        uint64_t curmaxntinode = mftlength / (curnt->mftentrysize * curnt->sectorspercluster * curnt->bytespersector);
        if(relativentinode < curmaxntinode)
            break;
        else
            relativentinode = relativentinode - curmaxntinode;
    }
    mftentryoffset = mftoffset + relativentinode * curnt->mftentrysize * curnt->sectorspercluster * curnt->bytespersector;
    //std::cout << "MFT Entry Offset: " << mftentryoffset << std::endl;
    std::string indexlayout = GetIndexAttributesLayout(rawcontent, curnt, mftentryoffset);
    //std::cout << "Index Layout: " << indexlayout << std::endl;
    // PARSE INDEX ROOT AND ALLOCATION TO DETERMINE THE DIR/FILE NAME/INODE AND SEE IF THEY MATCH
    std::vector<std::string> indexlayoutlist;
    indexlayoutlist.clear();
    std::istringstream ill(indexlayout);
    std::string ils;
    while(getline(ill, ils, ';'))
        indexlayoutlist.push_back(ils);
    uint32_t indexrecordsize = 0;
    for(int i=0; i < indexlayoutlist.size(); i++)
    {
        std::size_t layoutsplit = indexlayoutlist.at(i).find(",");
        uint64_t indexoffset = std::stoull(indexlayoutlist.at(i).substr(0, layoutsplit));
        uint64_t indexlength = std::stoull(indexlayoutlist.at(i).substr(layoutsplit+1));
        if(i == 0) // $INDEX_ROOT
        {
            // INDEX RECORD SIZE (bytes)
            uint8_t* irs = new uint8_t[4];
            ReadContent(rawcontent, irs, indexoffset + 8, 4);
            ReturnUint32(&indexrecordsize, irs);
            delete[] irs;
            //std::cout << "Index Record Size: " << indexrecordsize << std::endl;
            // STARTING OFFSET
            uint8_t* so = new uint8_t[4];
            uint32_t startoffset = 0;
            ReadContent(rawcontent, so, indexoffset + 16, 4);
            ReturnUint32(&startoffset, so);
            delete[] so;
            //std::cout << "start offset: " << startoffset << std::endl;
            // END OFFSET
            uint8_t* eo = new uint8_t[4];
            uint32_t endoffset = 0;
            ReadContent(rawcontent, eo, indexoffset + 20, 4);
            ReturnUint32(&endoffset, eo);
            delete[] eo;
            //std::cout << "End Offset: " << endoffset << std::endl;
            /*
            // ALLOCATION OFFSET - DELETED ENTRIES
            uint8_t* ao = new uint8_t[4];
            uint32_t allocationoffset = 0;
            ReadContent(rawoffset, ao, indexoffset + 24, 4);
            ReturnUint32(&allocationoffset, ao);
            delete[] ao;
            */
            unsigned int curpos = startoffset;
            while(curpos < endoffset)
            {
                // INDEX ENTRY LENGTH
                uint8_t* iel = new uint8_t[2];
                uint16_t indexentrylength = 0;
                ReadContent(rawcontent, iel, indexoffset + 16 + curpos + 8, 2);
                ReturnUint16(&indexentrylength, iel);
                delete[] iel;
                //std::cout << "Index Entry Length: " << indexentrylength << std::endl;
                // FILE NAME ATTRIBUTE LENGTH
                uint8_t* fnl = new uint8_t[2];
                uint16_t filenamelength = 0;
                ReadContent(rawcontent, fnl, indexoffset + 16 + curpos + 10, 2);
                ReturnUint16(&filenamelength, fnl);
                delete[] fnl;
                //std::cout << "File Name Attribute Length: " << filenamelength << std::endl;
                uint8_t* ief = new uint8_t[4];
                uint32_t indexentryflags = 0;
                ReadContent(rawcontent, ief, indexoffset + 16 + curpos + 12, 4);
                ReturnUint32(&indexentryflags, ief);
                delete[] ief;
                //std::cout << "Index Entry Flags: 0x" << std::hex << indexentryflags << std::dec << std::endl;
                if(indexentryflags & 0x02)
                    break;
                else
                {
                    if(indexentrylength > 0 && filenamelength > 66 && filenamelength < indexentrylength)
                    {
                        // I30 SEQUENCE ID
                        uint8_t* i3si = new uint8_t[2];
                        uint16_t i30seqid = 0;
                        ReadContent(rawcontent, i3si, indexoffset + 16 + curpos + 6, 2);
                        ReturnUint16(&i30seqid, i3si);
                        delete[] i3si;
                        //std::cout << "I30 Sequence ID: " << i30seqid << std::endl;
                        // CHILD NT INODE
                        uint8_t* cni = new uint8_t[6];
                        uint64_t childntinode = 0;
                        ReadContent(rawcontent, cni, indexoffset + 16 + curpos, 6);
                        ReturnUint(&childntinode, cni, 6);
                        delete[] cni;
                        childntinode = childntinode & 0x00ffffffffffffff;
                        //std::cout << "Child NT Inode: " << childntinode << std::endl;
                        if(childntinode <= curnt->maxmftentrycount)
                        {
                            curpos = curpos + 16; // STARTING ON FILE_NAME ATTRIBUTE
                            // FILE NAME TYPE
                            uint8_t* fnt = new uint8_t[1];
                            uint8_t fntype = 0;
                            ReadContent(rawcontent, fnt, indexoffset + 16 + curpos + 65, 1);
                            fntype = (uint8_t)fnt[0];
                            delete[] fnt;
                            //std::cout << "file name type: " << (int)fntype << std::endl;
                            if(fntype != 0x02)
                            {
                                // FILE NAME LENGTH
                                uint8_t* nl = new uint8_t[1];
                                uint8_t namelength = 0;
                                ReadContent(rawcontent, nl, indexoffset + 16 + curpos + 64, 1);
                                namelength = (uint8_t)nl[0];
                                delete[] nl;
                                //std::cout << "name length: " << (int)namelength << std::endl;
                                // FILE NAME
                                std::string filename = "";
                                for(uint8_t j=0; j < namelength; j++)
                                {
                                    uint8_t* sl = new uint8_t[2];
                                    uint16_t singleletter = 0;
                                    ReadContent(rawcontent, sl, indexoffset + 16 + curpos + 66 + j*2, 2);
                                    ReturnUint16(&singleletter, sl);
                                    delete[] sl;
                                    filename += (char)singleletter;
                                }
                                //std::cout << "File Name to compare to child name: " << filename << " " << childpath << std::endl;
                                if(filename.compare(childpath) == 0)
                                    return childntinode;
                            }
                        }
                    }
                }
                curpos += indexentrylength;
            }
        }
        else // $INDEX_ALLOCATION
        {
            uint32_t indexrecordcount = indexlength / indexrecordsize;
            //std::cout << "Index Record Count: " << indexrecordcount << std::endl;
            uint64_t curpos = indexoffset;
            for(uint32_t j=0; j < indexrecordcount; j++)
            {
                // START OFFSET
                uint8_t* so = new uint8_t[4];
                uint32_t startoffset = 0;
                ReadContent(rawcontent, so, curpos + 24, 4);
                ReturnUint32(&startoffset, so);
                delete[] so;
                //std::cout << "Start Offset: " << startoffset << std::endl;
                // END OFFSET
                uint8_t* eo = new uint8_t[4];
                uint32_t endoffset = 0;
                ReadContent(rawcontent, eo, curpos + 28, 4);
                ReturnUint32(&endoffset, eo);
                delete[] eo;
                //std::cout << "End Offset: " << endoffset << std::endl;
                curpos = curpos + 24 + startoffset + j*indexrecordsize;
		//std::cout << "before while loop - curpos: " << curpos << " indexsize: " << indexoffset + j*indexrecordsize + indexrecordsize << std::endl;
                while(curpos < indexoffset + j*indexrecordsize + indexrecordsize)
                {
		    // INDEX ENTRY LENGTH
		    uint8_t* iel = new uint8_t[2];
		    uint16_t indexentrylength = 0;
		    ReadContent(rawcontent, iel, curpos + 8, 2);
		    ReturnUint16(&indexentrylength, iel);
		    delete[] iel;
		    //std::cout << "Index Entry Length: " << indexentrylength << std::endl;
		    // FILE NAME ATTRIBUTE LENGTH
		    uint8_t* fnl = new uint8_t[2];
		    uint16_t filenamelength = 0;
		    ReadContent(rawcontent, fnl, curpos + 10, 2);
		    ReturnUint16(&filenamelength, fnl);
		    delete[] fnl;
		    //std::cout << "File Name Attribute Length: " << filenamelength << std::endl;
		    uint8_t* ief = new uint8_t[4];
		    if(indexentrylength > 0 && filenamelength > 66 && filenamelength < indexentrylength)
		    {
			// I30 SEQUENCE ID
			uint8_t* i3si = new uint8_t[2];
			uint16_t i30seqid = 0;
			ReadContent(rawcontent, i3si, curpos + 6, 2);
			ReturnUint16(&i30seqid, i3si);
			delete[] i3si;
			//std::cout << "I30 Sequence ID: " << i30seqid << std::endl;
			// CHILD NT INODE
			uint8_t* cni = new uint8_t[6];
			uint64_t childntinode = 0;
			ReadContent(rawcontent, cni, curpos, 6);
			ReturnUint(&childntinode, cni, 6);
			delete[] cni;
			childntinode = childntinode & 0x00ffffffffffffff;
			//std::cout << "Child NT Inode: " << childntinode << std::endl;
			if(childntinode <= curnt->maxmftentrycount)
			{
                            curpos = curpos + 16; // STARTING ON FILE_NAME ATTRIBUTE
                            // FILE NAME TYPE
                            uint8_t* fnt = new uint8_t[1];
                            uint8_t fntype = 0;
                            ReadContent(rawcontent, fnt, curpos + 65, 1);
                            fntype = (uint8_t)fnt[0];
                            delete[] fnt;
                            //std::cout << "file name type: " << (int)fntype << std::endl;
                            if(fntype != 0x02)
                            {
                                // FILE NAME LENGTH
                                uint8_t* nl = new uint8_t[1];
                                uint8_t namelength = 0;
                                ReadContent(rawcontent, nl, curpos + 64, 1);
                                namelength = (uint8_t)nl[0];
                                delete[] nl;
                                //std::cout << "name length: " << (int)namelength << std::endl;
                                // FILE NAME
                                std::string filename = "";
                                for(uint8_t k=0; k < namelength; k++)
                                {
                                    uint8_t* sl = new uint8_t[2];
                                    uint16_t singleletter = 0;
                                    ReadContent(rawcontent, sl, curpos + 66 + k*2, 2);
                                    ReturnUint16(&singleletter, sl);
                                    delete[] sl;
                                    filename += (char)singleletter;
                                }
                                //std::cout << "File Name to compare to child name: " << filename << " " << childpath << std::endl;
                                if(filename.compare(childpath) == 0)
                                    return childntinode;
                            }
			}
		    }
		    curpos += indexentrylength - 16;
                }
            }
        }
    }

    return childntinode;
}

std::string ParseNtfsFile(std::ifstream* rawcontent, ntfsinfo* curnt, uint64_t ntinode, std::string childfile)
{
    std::string fileforensics = "";
    //std::cout << "parent ntinode: " << ntinode << " child file to analyze: " << childfile << std::endl;
    // NEED TO GET MFT ENTRY CONTENTS FOR CHILD FILE
    uint64_t mftentryoffset = 0;
    uint64_t mftoffset = curnt->mftstartingcluster * curnt->sectorspercluster * curnt->bytespersector;
    uint64_t mftlength = 0;
    std::string mftlayout = GetDataAttributeLayout(rawcontent, curnt, mftoffset);
    std::vector<std::string> mftlayoutlist;
    mftlayoutlist.clear();
    std::istringstream mll(mftlayout);
    std::string mls;
    uint64_t relativentinode = ntinode;
    uint64_t mftsize = 0;
    while(getline(mll, mls, ';'))
        mftlayoutlist.push_back(mls);
    for(int i=0; i < mftlayoutlist.size(); i++)
    {
        std::size_t layoutsplit = mftlayoutlist.at(i).find(",");
        mftoffset = std::stoull(mftlayoutlist.at(i).substr(0, layoutsplit));
        mftlength = std::stoull(mftlayoutlist.at(i).substr(layoutsplit+1));
        uint64_t curmaxntinode = mftlength / (curnt->mftentrysize * curnt->sectorspercluster * curnt->bytespersector);
        if(relativentinode < curmaxntinode)
            break;
        else
            relativentinode = relativentinode - curmaxntinode;
    }
    mftentryoffset = mftoffset + relativentinode * curnt->mftentrysize * curnt->sectorspercluster * curnt->bytespersector;
    //std::cout << "MFT Entry Offset: " << mftentryoffset << std::endl;
    std::cout << "NT Inode|" << ntinode << std::endl << std::endl;
    std::string standardinformation = GetStandardInformationAttribute(rawcontent, curnt, mftentryoffset);
    std::string filename = GetFileNameAttribute(rawcontent, curnt, mftentryoffset);
    std::string dataattribute = GetDataAttribute(rawcontent, curnt, mftentryoffset);
    std::cout << "Standard Information" << std::endl << "--------------------" << std::endl << standardinformation << std::endl;
    std::cout << "File Name" << std::endl << "---------" << std::endl << filename << std::endl;
    std::cout << "Data Stream" << std::endl << "-----------" << std::endl << dataattribute << std::endl;

    return fileforensics;
}

void ParseNtfsForensics(std::string filename, std::string mntptstr, std::string devicestr, uint8_t ftype)
{
    std::ifstream devicebuffer(devicestr.c_str(), std::ios::in|std::ios::binary);
    //std::cout << filename << " || " << mntptstr << " || " << devicestr << " || " << (int)ftype << std::endl;
    std::cout << std::endl << "Forensics Artifacts for Mounted File: " << filename << std::endl;
    std::cout << "Mounted File Mount Point: " << mntptstr << std::endl;
    std::cout << "Mounted File Device: " << devicestr << std::endl;

    ntfsinfo curnt;
    uint64_t childntinode = 0;
    // GET NTFS FILESYSTEM INFO
    ParseNtfsInfo(&devicebuffer, &curnt);
    std::string pathstring = "";
    if(mntptstr.compare("/") == 0)
        pathstring = filename;
    else
    {
        std::size_t initpos = filename.find(mntptstr);
        pathstring = filename.substr(initpos + mntptstr.size());
    }
    //std::cout << "pathstring: " << pathstring << std::endl;
    std::cout << "Mounted File Internal Path: " << pathstring << std::endl;
    // SPLIT CURRENT FILE PATH INTO DIRECTORY STEPS
    std::vector<std::string> pathvector;
    std::istringstream iss(pathstring);
    std::string pp;
    while(getline(iss, pp, '/'))
        pathvector.push_back(pp);

    if(pathvector.size() > 1)
    {
        childntinode = ParseNtfsPath(&devicebuffer, &curnt, 5, pathvector.at(1)); // parse root directory for child directory
	//std::cout << "Child NT Inode to use for child path/file: " << childntinode << std::endl;
        //std::cout << "path vector size: " << pathvector.size() << std::endl;
	for(int i=1; i < pathvector.size() - 2; i++)
        {
	    childntinode = ParseNtfsPath(&devicebuffer, &curnt, childntinode, pathvector.at(i+1));
	    //std::cout << "Child NT Inode to use for child path/file: " << childntinode << std::endl;
	}
    }
    //std::cout << "now to parse ntfs file: " << pathvector.at(pathvector.size() - 1) << std::endl;
    std::string forensicsinfo = ParseNtfsFile(&devicebuffer, &curnt, childntinode, pathvector.at(pathvector.size() - 1));
    std::cout << forensicsinfo;
}
