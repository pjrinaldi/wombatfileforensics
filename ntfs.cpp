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
        /*
        // NEXT ATTRIBUTE ID
        uint8_t* nai = new uint8_t[2];
        uint16_t nextattributeid = 0;
        ReadContent(rawcontent, nai, mftoffset + 40, 2);
        ReturnUint16(&nextattributeid, nai);
        delete[] nai;
        std::cout << "Next Attribute ID: " << nextattributeid << std::endl;
        */
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

std::string GetStandardInformationAttributeLayout(std::ifstream* rawcontent, ntfsinfo* curnt, uint64_t mftentryoffset)
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
            std::cout << "Current Offset: " << curoffset << std::endl;
            // IS RESIDENT/NON-RESIDENT
            uint8_t* rf = new uint8_t[1];
            uint8_t isnonresident = 0; // 0 - Resident | 1 - Non-Resident
            ReadContent(rawcontent, rf, mftentryoffset + curoffset + 8, 1);
            isnonresident = (uint8_t)rf[0];
            delete[] rf;
            std::cout << "Is None Resident: " << (int)isnonresident << std::endl;
            // ATTRIBUTE LENGTH
            uint8_t* al = new uint8_t[4];
            uint32_t attributelength = 0;
            ReadContent(rawcontent, al, mftentryoffset + curoffset + 4, 4);
            ReturnUint32(&attributelength, al);
            delete[] al;
            std::cout << "Attribute Length: " << attributelength << std::endl;
            // ATTRIBUTE TYPE
            uint8_t* at = new uint8_t[4];
            uint32_t attributetype = 0;
            ReadContent(rawcontent, at, mftentryoffset + curoffset, 4);
            ReturnUint32(&attributetype, at);
            delete[] at;
            std::cout << "Attribute Type: 0x" << std::hex << attributetype << std::dec << std::endl;
	    if(attributetype == 0x10) // STANDARD_INFORMATION ATTRIBUTE - always resident
	    {
		uint8_t* cs = new uint8_t[4];
		uint32_t contentsize = 0;
		ReadContent(rawcontent, cs, mftentryoffset + curoffset + 16, 4);
		ReturnUint32(&contentsize, cs);
		delete[] cs;
		uint8_t* co = new uint8_t[2];
		uint16_t contentoffset = 0;
		ReadContent(rawcontent, co, mftentryoffset + curoffset + 20, 2);
		ReturnUint16(&contentoffset, co);
		delete[] co;
		uint8_t* cd = new uint8_t[8];
		uint64_t createdate = 0;
		ReadContent(rawcontent, cd, mftentryoffset + curoffset + contentoffset, 8);
		ReturnUint(&createdate, cd, 8);
		delete[] cd;
		siforensics += "Create Date|" + ConvertNtfsTimeToHuman(createdate) + "\n";
		uint8_t* md = new uint8_t[8];
		uint64_t modifydate = 0;
		ReadContent(rawcontent, md, mftentryoffset + curoffset + contentoffset + 8, 8);
		ReturnUint(&modifydate, md, 8);
		delete[] md;
		siforensics += "Modify Date|" + ConvertNtfsTimeToHuman(modifydate) + "\n";

		return siforensics;
	    }
            if(attributelength == 0 || attributetype == 0xffffffff)
                break;
	    curoffset += attributelength;
	}
    }

    return siforensics;
    /*
     *          if(attrtype == 0x10) // $STANDARD_INFORMATION - always resident, treenode timestamps
                {
		    createdate = ConvertNtfsTimeToUnixTime(qFromLittleEndian<uint64_t>(curimg->ReadContent(curoffset + 24, 8)));
		    modifydate = ConvertNtfsTimeToUnixTime(qFromLittleEndian<uint64_t>(curimg->ReadContent(curoffset + 32, 8)));
		    statusdate = ConvertNtfsTimeToUnixTime(qFromLittleEndian<uint64_t>(curimg->ReadContent(curoffset + 40, 8)));
		    accessdate = ConvertNtfsTimeToUnixTime(qFromLittleEndian<uint64_t>(curimg->ReadContent(curoffset + 48, 8)));
		    accessflags = qFromLittleEndian<uint16_t>(curimg->ReadContent(curoffset + 56, 4));
		    attrstr = "";
		    if(attrflags == 0x00) // unallocated file
			attrstr += "Not Allocated,";
		    else if(attrflags == 0x01) // allocated file
			attrstr += "Allocated,";
		    else if(attrflags == 0x02) // unallocated directory
			attrstr += "Not Allocated,";
		    else if(attrflags == 0x03) // allocated directory
			attrstr += "Allocated,";
		    if(accessflags & 0x01) // READ ONLY
			attrstr += "Read Only,";
		    if(accessflags & 0x02) // Hidden
			attrstr += "Hidden,";
		    if(accessflags & 0x04) // System
			attrstr += "System,";
		    if(accessflags & 0x20) // Archive
			attrstr += "Archive,";
		    if(accessflags & 0x40) // Device
			attrstr += "Device,";
		    if(accessflags & 0x80) // Normal
			attrstr += "Normal,";
		    if(accessflags & 0x100) // Temporary
			attrstr += "Temporary,";
		    if(accessflags & 0x200) // Sparse File
			attrstr += "Sparse File,";
		    if(accessflags & 0x400) // Reparse Point
			attrstr += "Reparse Point,";
		    if(accessflags & 0x800) // Compressed
			attrstr += "Compressed,";
		    if(accessflags & 0x1000) // Offline
			attrstr += "Offline,";
		    if(accessflags & 0x2000) // Not Indexed
			attrstr += "Not Indexed,";
		    if(accessflags & 0x4000) // Encrypted
			attrstr += "Encrypted,";
                    // ADD ATTRSTR TO RETURN FOR THE PROPERTIES FILE
		    out << "File Attributes|" << attrstr << "|Attributes list for the file." << Qt::endl;
                    if(curpos > endoffset)
                    {
                        if(attrflags == 0x00) // unalloc file
                        {
                            if(accessflags & 0x4000) // encrypted
                                itemtype = 13;
                            else
                                itemtype = 4;
                            isdeleted = 1;
                        }
                        else if(attrflags == 0x02) // unalloc dir
                        {
                            if(accessflags & 0x4000) // encrypted
                                itemtype = 13;
                            else
                                itemtype = 2;
                            isdeleted = 1;
                        }
                        else
                        {
                            itemtype = 4;
                            isdeleted = 1;
                        }
                    }
                    else if(!parfilename.isEmpty())
                    {
                        if(parntinode != parentntinode)
                        {
                            if(attrflags == 0x00) // unalloc file
                            {
                                if(accessflags & 0x4000) // encrypted
                                    itemtype = 13;
                                else
                                    itemtype = 4;
                                isdeleted = 1;
                            }
                            else if(attrflags == 0x02) // unalloc dir
                            {
                                if(accessflags & 0x4000) // encrypted
                                    itemtype = 13;
                                else
                                    itemtype = 2;
                                isdeleted = 1;
                            }
                            else
                            {
                                itemtype = 4;
                                isdeleted = 1;
                            }
                        }
                        else
                        {
                            if(attrflags == 0x01) // alloc file
                            {
                                if(accessflags & 0x4000) // encrypted
                                    itemtype = 13;
                                else
                                    itemtype = 5;
                                isdeleted = 0;
                            }
                            else if(attrflags == 0x03) // alloc dir
                            {
                                if(accessflags & 0x4000) // encrypted
                                    itemtype = 13;
                                else
                                    itemtype = 3;
                                isdeleted = 0;
                            }
                            else
                            {
                                itemtype = 5;
                                isdeleted = 0;
                            }
                        }
                    }
                    else
                    {
                        if(attrflags == 0x01) // alloc file
                        {
                            if(accessflags & 0x4000) // encrypted
                                itemtype = 13;
                            else
                                itemtype = 5;
                            isdeleted = 0;
                        }
                        else if(attrflags == 0x03) // alloc dir
                        {
                            if(accessflags & 0x4000) // encrypted
                                itemtype = 13;
                            else
                                itemtype = 3;
                            isdeleted = 0;
                        }
                        else
                        {
                            itemtype = 5;
                            isdeleted = 0;
                        }
                    }
                }

     */ 
}

std::string GetFileNameAttributeLayout(std::ifstream* rawcontent, ntfsinfo* curnt, uint64_t mftentryoffset)
{
    std::string fnlayout = "";
    
    return fnlayout;
    /*
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
                    }
                }
            }
            curoffset += attributelength;
            if(attributelength == 0)
                break;
        }
        return runliststr;
    }
    else
        return "";

     */ 
	 /*
	  *     else if(attrtype == 0x30) // $FILE_NAME - always resident
                {
		    uint8_t filenamespace = qFromLittleEndian<uint8_t>(curimg->ReadContent(curoffset + 89, 1));
		    if(filenamespace != 0x02)
		    {
			uint64_t filecreate = qFromLittleEndian<uint64_t>(curimg->ReadContent(curoffset + 32, 8));
			uint64_t filemodify = qFromLittleEndian<uint64_t>(curimg->ReadContent(curoffset + 40, 8));
			uint64_t filestatus = qFromLittleEndian<uint64_t>(curimg->ReadContent(curoffset + 48, 8));
			uint64_t fileaccess = qFromLittleEndian<uint64_t>(curimg->ReadContent(curoffset + 56, 8));
                        out << "$FILE_NAME Create|" << ConvertWindowsTimeToUnixTimeUTC(filecreate) << "|File creation time as recorded in the $FILE_NAME attribute." << Qt::endl;
                        out << "$FILE_NAME Modify|" << ConvertWindowsTimeToUnixTimeUTC(filemodify) << "|File modification time as recorded in the $FILE_NAME attribute." << Qt::endl;
                        out << "$FILE_NAME Status Changed|" << ConvertWindowsTimeToUnixTimeUTC(filestatus) << "|File status changed time as recorded in the $FILE_NAME attribute." << Qt::endl;
                        out << "$FILE_NAME Accessed|" << ConvertWindowsTimeToUnixTimeUTC(fileaccess) << "|File accessed time as recorded in the $FILE_NAME attribute." << Qt::endl;
		    }
                }

	  */

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
        /*
        // NEXT ATTRIBUTE ID
        uint8_t* nai = new uint8_t[2];
        uint16_t nextattributeid = 0;
        ReadContent(rawcontent, nai, mftoffset + 40, 2);
        ReturnUint16(&nextattributeid, nai);
        delete[] nai;
        std::cout << "Next Attribute ID: " << nextattributeid << std::endl;
        */
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
    /*
     *		    if(namelength == 0) // main file content - not alternate data stream
		    {
			logicalsize = 0;
			quint64 physicalsize = 0;
			layout = "";
			if(resflags == 0x00) // resident
			{
			    uint32_t contentsize = qFromLittleEndian<uint32_t>(curimg->ReadContent(curoffset + 16, 4));
			    uint16_t contentoffset = qFromLittleEndian<uint16_t>(curimg->ReadContent(curoffset + 20, 2));
			    logicalsize = contentsize;
			    physicalsize = contentsize;
			    dirlayout = QString(QString::number(curoffset + contentoffset) + "," + QString::number(contentsize) + ";");
                            // RETURN LOGICALSIZE FOR NODE DATA, PHYSICALSIZE AND LAYOUT FOR PROPERTIES FILE
			}
			else if(resflags == 0x01) // non-resident
			{
			    logicalsize = qFromLittleEndian<uint64_t>(curimg->ReadContent(curoffset + 48, 8));
			    GetRunListLayout(curimg, curstartsector, bytespercluster, mftentrybytes, curoffset, &dirlayout);
			    //qDebug() << "layout:" << layout;
			    for(int j=0; j < dirlayout.split(";", Qt::SkipEmptyParts).count(); j++)
			    {
				physicalsize += dirlayout.split(";", Qt::SkipEmptyParts).at(j).split(",").at(1).toULongLong();
			    }
                            // RETURN LOGICALSIZE FOR THE NODE DATA, PHYSICALSIZE AND LAYOUT FOR PROPERTIES FILE
			}
                        out << "Physical Size|" << QString::number(physicalsize) << "|Physical size in bytes for the file." << Qt::endl;
			out << "Logical Size|" << QString::number(logicalsize) << "|Size in Bytes for the file." << Qt::endl;
                        out << "Layout|" << dirlayout << "|File layout in bytes and formatted as offset,size; entries." << Qt::endl;
		    }
		    else // alternate data stream
		    {
			attrname = "";
			for(int k=0; k < namelength; k++)
			    attrname += QString(QChar(qFromLittleEndian<uint16_t>(curimg->ReadContent(curoffset + nameoffset + k*2, 2))));
			//qDebug() << "ads:" << QString("$DATA:" + attrname);
			quint64 logicalsize = 0;
			quint64 physicalsize = 0;
			layout = "";
			if(resflags == 0x00) // resident
			{
			    uint32_t contentsize = qFromLittleEndian<uint32_t>(curimg->ReadContent(curoffset + 16, 4));
			    uint16_t contentoffset = qFromLittleEndian<uint16_t>(curimg->ReadContent(curoffset + 20, 2));
			    logicalsize = contentsize;
			    physicalsize = contentsize;
			    layout = QString(QString::number(curoffset + contentoffset) + "," + QString::number(contentsize) + ";");
			}
			else if(resflags == 0x01) // non-resident
			{
			    logicalsize = qFromLittleEndian<uint64_t>(curimg->ReadContent(curoffset + 48, 8));
			    GetRunListLayout(curimg, curstartsector, bytespercluster, mftentrybytes, curoffset, &layout);
			    for(int j=0; j < layout.split(";", Qt::SkipEmptyParts).count(); j++)
				physicalsize += layout.split(";", Qt::SkipEmptyParts).at(j).split(",").at(1).toULongLong();
			}
                        QList<QVariant> tmpnode;
                        tmpnode.clear();
                        QList<QString> tmpprop;
                        tmpprop.clear();
                        tmpnode << QString("$DATA:" + attrname) << logicalsize;
                        tmpprop.append(QString("Physical Size|" + QString::number(physicalsize) + "|Physical size for the file in bytes."));
			tmpprop.append(QString("Logical Size|" + QString::number(logicalsize) + "|Logical size for the file in bytes."));
                        tmpprop.append(QString("Layout|" + layout + "|File layout in bytes as offset,size;."));
                        adsnodelist.append(tmpnode);
                        adsproplist.append(tmpprop);
                        tmpnode.clear();
                        tmpprop.clear();
                        // RETURN ATTRNAME, LOGICALSIZE FOR ADS NODE DATA AND PHYSICALSIZE AND LAYOUT FOR PROPERTIES FILE
			// NEED TO DO SOMETHING WITH THE ADS PROPERTIES AS WELL AS THE FILE PROPERTIES...
		    }

     */ 
}

/*
std::string ConvertDosTimeToHuman(uint16_t* dosdate, uint16_t* dostime)
{
    std::string humanstring = "";
    if(((*dosdate & 0x1e0) >> 5) < 10)
        humanstring += "0";
    humanstring += std::to_string(((*dosdate & 0x1e0) >> 5)); // MONTH
    humanstring += "/"; // DIVIDER
    if(((*dosdate & 0x1f) >> 0) < 10)
        humanstring += "0";
    humanstring += std::to_string(((*dosdate & 0x1f) >> 0)); // MONTH
    humanstring += "/"; // DIVIDER
    humanstring += std::to_string(((*dosdate & 0xfe00) >> 9) + 1980); // YEAR
    humanstring += " ";
    if(dostime == NULL)
	humanstring += "00:00:00";
    else
    {
        if(((*dostime & 0xf800) >> 11) < 10)
            humanstring += "0";
	humanstring += std::to_string(((*dostime & 0xf800) >> 11)); // HOUR
	humanstring += ":";
        if(((*dostime & 0x7e0) >> 5) < 10)
            humanstring += "0";
	humanstring += std::to_string(((*dostime & 0x7e0) >> 5)); // MINUTE
	humanstring += ":";
        if((((*dostime & 0x1f) >> 0) * 2) < 10)
            humanstring += "0";
	humanstring += std::to_string(((*dostime & 0x1f) >> 0) * 2); // SECOND
    }
    humanstring += " (UTC)";
    
    return humanstring;
}

std::string ConvertExFatTimeToHuman(uint16_t* dosdate, uint16_t* dostime, uint8_t* timezone)
{
    std::string humanstring = ConvertDosTimeToHuman(dosdate, dostime);
    std::bitset<8> zonebits{*timezone};
    if(zonebits[0] == 1) // apply utc offset
    {
        // CALCULATE OFFSET AND REPLACE (UTC) IN STRING WITH HOW TO WRITE TIME ZONE OFFSET PIECE
        // OR INSERT HUMANSTRING.END()-1, IF(<0) - ELSE +, OFFSETSECS/3600 float = (OFFSETSECS%3600)/3600) (HOUR::MIN)
        // IF FLOAT = 0, THEN 00 = 0.25, THEN 15, = 0.50, THEN 30, =0.75 THEN 45
        zonebits.set(0, 0); // set switch bit to zero so i can get the offset without that value
        int offsetsecs = (int)zonebits.to_ulong() * 15 * 60;
        std::cout << "offset secs:" << std::hex << offsetsecs << std::endl;
        int offhour = offsetsecs / 3600;
        float offmin = (abs(offsetsecs) % 3600 ) / 3600;
        std::string offstring = std::to_string(offhour) + ":";
        if(offmin == 0.00)
            offstring += "00";
        else if(offmin == 0.25)
            offstring += "15";
        else if(offmin == 0.50)
            offstring += "30";
        else if(offmin == 0.75)
            offstring += "45";
        else
            offstring += "00";
        std::cout << "offset secs: " << offstring << std::endl;
    }

    return humanstring;
}

void GetNextCluster(std::ifstream* rawcontent, uint32_t clusternum, fatinfo* curfat, std::vector<uint32_t>* clusterlist)
{
    uint32_t curcluster = 0;
    int fatbyte1 = 0;
    if(curfat->fattype == 1) // FAT 12
    {
	fatbyte1 = clusternum + (clusternum / 2);
	uint8_t* cc = new uint8_t[2];
	uint16_t ccluster = 0;
	ReadContent(rawcontent, cc, curfat->fatoffset + fatbyte1, 2);
	ReturnUint16(&ccluster, cc);
	delete[] cc;
	if(clusternum & 0x0001) // ODD
	{
	    curcluster = ccluster >> 4;
	}
	else // EVEN
	{
	    curcluster = ccluster & 0x0FFF;
	}
	if(curcluster < 0x0FF7 && curcluster >= 2)
	{
	    clusterlist->push_back(curcluster);
	    GetNextCluster(rawcontent, curcluster, curfat, clusterlist);
	}
    }
    else if(curfat->fattype == 2) // FAT16
    {
        if(clusternum >= 2)
        {
            fatbyte1 = clusternum * 2;
            uint8_t* cc = new uint8_t[2];
            ReadContent(rawcontent, cc, curfat->fatoffset + fatbyte1, 2);
            ReturnUint32(&curcluster, cc);
            delete[] cc;
            if(curcluster < 0xFFF7 && curcluster >= 2)
            {
                clusterlist->push_back(curcluster);
                GetNextCluster(rawcontent, curcluster, curfat, clusterlist);
            }
        }
    }
    else if(curfat->fattype == 3) // FAT32
    {
        if(clusternum >= 2)
        {
            fatbyte1 = clusternum * 4;
            uint8_t* cc = new uint8_t[4];
            uint32_t ccluster = 0;
            ReadContent(rawcontent, cc, curfat->fatoffset + fatbyte1, 4);
            ReturnUint32(&ccluster, cc);
            delete[] cc;
            curcluster = ccluster & 0x0FFFFFFF;
            if(curcluster < 0x0FFFFFF7 && curcluster >= 2)
            {
                clusterlist->push_back(curcluster);
                GetNextCluster(rawcontent, curcluster, curfat, clusterlist);
            }
        }
    }
    else if(curfat->fattype == 4) // EXFAT
    {
        if(clusternum >= 2)
        {
	    curcluster = 0;
            fatbyte1 = clusternum * 4;
            uint8_t* cc = new uint8_t[4];
            ReadContent(rawcontent, cc, curfat->fatoffset + fatbyte1, 4);
            ReturnUint32(&curcluster, cc);
            delete[] cc;
            if(curcluster < 0xFFFFFFF7 && curcluster >= 2)
            {
                clusterlist->push_back(curcluster);
                GetNextCluster(rawcontent, curcluster, curfat, clusterlist);
            }
        }
    }
}

std::string ConvertClustersToExtents(std::vector<uint32_t>* clusterlist, fatinfo* curfat)
{
    uint32_t clustersize = curfat->sectorspercluster * curfat->bytespersector;
    uint64_t rootdiroffset = curfat->clusterareastart * curfat->bytespersector;

    std::string extentstring = "";
    int blkcnt = 1;
    uint32_t startvalue = clusterlist->at(0);
    for(int i=1; i < clusterlist->size(); i++)
    {
	uint32_t oldvalue = clusterlist->at(i-1);
	uint32_t newvalue = clusterlist->at(i);
	if(newvalue - oldvalue == 1)
	    blkcnt++;
	else
	{
	    if(rootdiroffset > 0)
		extentstring += std::to_string(rootdiroffset + ((startvalue - 2) * clustersize)) + "," + std::to_string(blkcnt * clustersize) + ";";
	    else
		extentstring += std::to_string(startvalue * clustersize) + "," + std::to_string(blkcnt * clustersize) + ";";
	    startvalue = clusterlist->at(i);
	    blkcnt = 1;
	}
	if(i == clusterlist->size() - 1)
	{
	    if(rootdiroffset > 0)
		extentstring += std::to_string(rootdiroffset + ((startvalue - 2) * clustersize)) + "," + std::to_string(blkcnt * clustersize) + ";";
	    else
		extentstring += std::to_string(startvalue * clustersize) + "," + std::to_string(blkcnt * clustersize) + ";";
	    startvalue = clusterlist->at(i);
	    blkcnt = 1;
	}
    }
    if(clusterlist->size() == 1)
    {
	if(rootdiroffset > 0)
	    extentstring += std::to_string(rootdiroffset + ((startvalue - 2) * clustersize)) + "," + std::to_string(blkcnt * clustersize) + ";";
	else
	    extentstring += std::to_string(startvalue * clustersize) + "," + std::to_string(blkcnt * clustersize) + ";";
    }

    return extentstring;
}
*/

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
    /*
    // LOGICAL FILE SIZE
    uint8_t* ls = new uint8_t[8];
    uint64_t logicalsize = 0;
    ReadContent(rawcontent, ls, mftoffset + curoffset + 48, 8);
    ReturnUint64(&logicalsize, ls);
    delete[] ls;
    std::cout << "Logical Size: " << logicalsize << std::endl;
    */
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
    std::cout << "parent ntinode: " << ntinode << " child file to analyze: " << childfile << std::endl;
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
    std::cout << "MFT Entry Offset: " << mftentryoffset << std::endl;
    std::string datalayout = GetDataAttributeLayout(rawcontent, curnt, mftentryoffset);
    std::string standardinformationlayout = GetStandardInformationAttributeLayout(rawcontent, curnt, mftentryoffset);
    std::string filenamelayout = GetFileNameAttributeLayout(rawcontent, curnt, mftentryoffset);
    std::cout << "Data layout: " << datalayout << std::endl;
    std::cout << "Standard Information" << std::endl << standardinformationlayout << std::endl;
    std::cout << "File Name layout: " << filenamelayout << std::endl;

    return fileforensics;
}

void ParseNtfsForensics(std::string filename, std::string mntptstr, std::string devicestr, uint8_t ftype)
{
    std::ifstream devicebuffer(devicestr.c_str(), std::ios::in|std::ios::binary);
    std::cout << filename << " || " << mntptstr << " || " << devicestr << " || " << (int)ftype << std::endl;

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
    std::cout << "pathstring: " << pathstring << std::endl;
    // SPLIT CURRENT FILE PATH INTO DIRECTORY STEPS
    std::vector<std::string> pathvector;
    std::istringstream iss(pathstring);
    std::string pp;
    while(getline(iss, pp, '/'))
        pathvector.push_back(pp);

    if(pathvector.size() > 1)
    {
        childntinode = ParseNtfsPath(&devicebuffer, &curnt, 5, pathvector.at(1)); // parse root directory for child directory
	std::cout << "Child NT Inode to use for child path/file: " << childntinode << std::endl;
        //std::string nextdirlayout = "";
	//nextdirlayout = ParseFatPath(&devicebuffer, &curfat, pathvector.at(1));
        //curfat.curdirlayout = nextdirlayout;
	//std::cout << "child path: " << pathvector.at(1) << "'s layout: " << nextdirlayout << std::endl;

        //std::cout << "path vector size: " << pathvector.size() << std::endl;
	for(int i=1; i < pathvector.size() - 2; i++)
        {
	    childntinode = ParseNtfsPath(&devicebuffer, &curnt, childntinode, pathvector.at(i+1));
	    std::cout << "Child NT Inode to use for child path/file: " << childntinode << std::endl;
            //std::cout << "get next directory..";
	    //nextdirlayout = ParseFatPath(&devicebuffer, &curfat, pathvector.at(i+1));
	    //curfat.curdirlayout = nextdirlayout;
	    //std::cout << "child path: " << pathvector.at(i+1) << "'s layout: " << nextdirlayout << std::endl;
	}
    }
    std::cout << "now to parse ntfs file: " << pathvector.at(pathvector.size() - 1) << std::endl;
    std::string forensicsinfo = ParseNtfsFile(&devicebuffer, &curnt, childntinode, pathvector.at(pathvector.size() - 1));
    //std::string forensicsinfo = ParseFatFile(&devicebuffer, &curfat, pathvector.at(pathvector.size() - 1));
    std::cout << forensicsinfo << std::endl;

    /*
     *
        QHash<quint64, quint64> dirntinodehash;
	QHash<quint64, uint16_t> ntinodehash;
	dirntinodehash.clear();
        ntinodehash.clear();
	curinode = ParseNtfsDirectory(curimg, curstartsector, ptreecnt, 5, 0, "", "", &dirntinodehash, &ntinodehash);
     */ 
}

/*
std::string ParseFatPath(std::ifstream* rawcontent, fatinfo* curfat, std::string childpath)
{
    uint8_t isrootdir = 0;
    // NEED TO DETERMINE IF IT'S ROOT DIRECTORY OR NOT AND HANDLE ACCORDINGLY
    if(curfat->curdirlayout.compare(curfat->rootdirlayout) == 0)
    {
	isrootdir = 1;
	//std::cout << "root dir layout matches curdirlayout" << std::endl;
    }
    else
    {
	isrootdir = 0;
	//std::cout << "curdirlayout is not rootdirlayout" << std::endl;
    }

    //std::cout << "child path to find: " << childpath << std::endl;
    // GET THE DIRECTORY CONTENT OFFSETS/LENGTHS AND THEN LOOP OVER THEM
    std::vector<std::string> dirlayoutlist;
    dirlayoutlist.clear();
    std::istringstream rdll(curfat->curdirlayout);
    std::string rdls;
    while(getline(rdll, rdls, ';'))
	dirlayoutlist.push_back(rdls);
    for(int i=0; i < dirlayoutlist.size(); i++)
    {
	uint64_t diroffset = 0;
	uint64_t dirlength = 0;
	std::size_t layoutsplit = dirlayoutlist.at(i).find(",");
	if(i == 0)
	{
	    diroffset = std::stoull(dirlayoutlist.at(i).substr(0, layoutsplit));
	    dirlength = std::stoull(dirlayoutlist.at(i).substr(layoutsplit+1));
	    if(isrootdir == 0) // sub directory
	    {
		diroffset = diroffset + 64; // skip . and .. directories
		dirlength = dirlength - 64; // adjust read size for the 64 byte skip
	    }
	}
	//std::cout << "dir offset: " << diroffset << " dir length: " << dirlength << std::endl;
	unsigned int direntrycount = dirlength / 32;
	//std::cout << "dir entry count: " << direntrycount << std::endl;
	// PARSE DIRECTORY ENTRIES
	std::string longnamestring = "";
	for(unsigned int j=0; j < direntrycount; j++)
	{
	    std::string filename = "";
	    uint32_t clusternum = 0;
	    uint64_t physicalsize = 0;
	    // FIRST CHARACTER
	    uint8_t* fc = new uint8_t[1];
	    uint8_t firstchar = 0;
	    ReadContent(rawcontent, fc, diroffset + j*32, 1);
	    firstchar = (uint8_t)fc[0];
	    delete[] fc;
	    //std::cout << "first char: " << (int)firstchar << std::endl;
            if(firstchar == 0xe5) // deleted entry, skip
            {
            }
            else if(firstchar == 0x00) // entry is free and all remaining are free
                break;
	    else if(firstchar == 0x85) // EXFAT File/Dir Directory Entry
	    {
		uint8_t* sc = new uint8_t[1];
		uint8_t secondarycount = 0;
		ReadContent(rawcontent, sc, diroffset + j*32 + 1, 1);
		secondarycount = (uint8_t)sc[0];
		delete[] sc;
		//std::cout << "secondary count: " << secondarycount << std::endl;
		// FILE ATTRIBUTE
		uint8_t* fa = new uint8_t[1];
		uint8_t fileattr = 0;
		ReadContent(rawcontent, fa, diroffset + j*32 + 4, 1);
		fileattr = (uint8_t)fa[0];
		delete[] fa;
		//std::cout << "file attr: " << (int)fileattr << std::endl;
		if(fileattr & 0x10) // Sub Directory
		{
		    int fatchain = 0;
		    uint8_t namelength = 0;
		    uint8_t curlength = 0;
		    for(uint8_t k=1; k <= secondarycount; k++)
		    {
			uint8_t* set = new uint8_t[1];
			uint8_t subentrytype = 0;
			ReadContent(rawcontent, set, diroffset + j*32 + k*32, 1);
			subentrytype = (uint8_t)set[0];
			delete[] set;
			//std::cout << "Sub entry type: 0x" << std::hex << (int)subentrytype << std::endl;
			if(subentrytype == 0xc0) // Stream Extension Directory Entry
			{
			    // NAME LENGTH
			    uint8_t* nl = new uint8_t[1];
			    ReadContent(rawcontent, nl, diroffset + (j+k)*32 + 3, 1);
			    namelength = (uint8_t)nl[0];
			    delete[] nl;
			    // FLAGS
			    uint8_t* ff = new uint8_t[1];
			    ReadContent(rawcontent, ff, diroffset + (j+k)*32 + 1, 1);
			    std::bitset<8> flagbits{(uint8_t)ff[0]};
			    delete[] ff;
			    //std::cout << "flagbits: " << flagbits << std::endl;
			    // FAT CHAIN
			    int fatchain = flagbits[1];
			    //std::cout << "fatchain: " << fatchain << std::endl;
			    // CLUSTER NUM
			    uint8_t* cn = new uint8_t[4];
			    //uint32_t clusternum = 0;
			    ReadContent(rawcontent, cn, diroffset + (j+k)*32 + 20, 4);
			    ReturnUint32(&clusternum, cn);
			    delete[] cn;
			    //std::cout << "Cluster Number: " << clusternum << std::endl;
			    // PHYSICAL SIZE
			    uint8_t* ps = new uint8_t[8];
			    //uint64_t physicalsize = 0;
			    ReadContent(rawcontent, ps, diroffset + (j+k)*32 + 24, 8);
			    ReturnUint64(&physicalsize, ps);
			    delete[] ps;
			    //std::cout << "Physical Size: " << physicalsize << std::endl;
			}
			else if(subentrytype == 0xc1) // File Name Directory Entry
			{
			    // GET FILE NAME
			    curlength += 15;
			    if(curlength <= namelength)
			    {
				for(int m=1; m < 16; m++)
				{
				    uint8_t* sl = new uint8_t[2];
				    uint16_t singleletter = 0;
				    ReadContent(rawcontent, sl, diroffset + (j+k)*32 + m*2, 2);
				    ReturnUint16(&singleletter, sl);
				    delete[] sl;
				    filename += (char)singleletter;
				}
			    }
			    else
			    {
				int remaining = namelength + 16 - curlength;
				for(int m=1; m < remaining; m++)
				{
				    uint8_t* sl = new uint8_t[2];
				    uint16_t singleletter = 0;
				    ReadContent(rawcontent, sl, diroffset + (j+k)*32 + m*2, 2);
				    ReturnUint16(&singleletter, sl);
				    delete[] sl;
				    filename += (char)singleletter;
				}
			    }
			}
		    }
		    if(filename.find(childpath) != std::string::npos)
		    {
			std::string layout = "";
			//std::cout << "cur file name: " << filename << std::endl;
			// GET DIRECTORY LAYOUT
			if(fatchain == 0 && clusternum > 1)
			{
			    std::vector<uint32_t> clusterlist;
			    clusterlist.clear();
			    clusterlist.push_back(clusternum);
			    GetNextCluster(rawcontent, clusternum, curfat, &clusterlist);
			    layout = ConvertClustersToExtents(&clusterlist, curfat);
			    clusterlist.clear();
			}
			else if(fatchain == 1)
			{
			    uint clustercout = (uint)ceil((float)physicalsize / (curfat->bytespersector * curfat->sectorspercluster));
			    layout = std::to_string(curfat->clusterareastart * curfat->bytespersector + ((clusternum - 2) * curfat->bytespersector * curfat->sectorspercluster)) + "," + std::to_string(clustercout * curfat->bytespersector * curfat->sectorspercluster) + ";";
			}
			
			return layout;
		    }
		}
	    }
            else
            {
                uint8_t* fa = new uint8_t[1];
                uint8_t fileattr = 0;
                ReadContent(rawcontent, fa, diroffset + j*32 + 11, 1);
                fileattr = (uint8_t)fa[0];
                delete[] fa;
                if(fileattr == 0x0f || fileattr == 0x3f) // Long Directory Name
                {
                    unsigned int lsn = ((int)firstchar & 0x0f);
                    if(lsn <= 20)
                    {
                        // process long file name part here... then add to the long file name...
                        std::string longname = "";
                        int arr[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};
                        for(int k=0; k < 13; k++)
                        {
                            uint16_t longletter = 0;
                            uint8_t* ll = new uint8_t[3];
                            ReadContent(rawcontent, ll, diroffset + j*32 + arr[k], 2);
                            ReturnUint16(&longletter, ll);
                            delete[] ll;
                            if(longletter < 0xFFFF)
                            {
                                longname += (char)longletter;
                            }
                        }
                        longnamestring.insert(0, longname);
                    }
                }
		else
		{
		    if(fileattr == 0x10)
		    {
			//std::cout << "long name: " << longnamestring << "|" << std::endl;
			if(longnamestring.find(childpath) != std::string::npos)
			{
			    uint8_t* hcn = new uint8_t[2];
			    uint16_t hiclusternum = 0;
			    ReadContent(rawcontent, hcn, diroffset + j*32 + 20, 2); // always zero for fat12/16
			    ReturnUint16(&hiclusternum, hcn);
			    delete[] hcn;
			    uint8_t* lcn = new uint8_t[2];
			    uint16_t loclusternum = 0;
			    ReadContent(rawcontent, lcn, diroffset + j*32 + 26, 2);
			    ReturnUint16(&loclusternum, lcn);
			    delete[] lcn;
			    uint32_t clusternum = ((uint32_t)hiclusternum >> 16) + loclusternum;
			    std::vector<uint32_t> clusterlist;
			    clusterlist.clear();
			    //std::cout << "first cluster: " << clusternum << std::endl;
			    if(clusternum >= 2)
			    {
				clusterlist.push_back(clusternum);
				GetNextCluster(rawcontent, clusternum, curfat, &clusterlist);
			    }
			    std::string layout = "";
			    if(clusterlist.size() > 0)
			    {
				layout = ConvertClustersToExtents(&clusterlist, curfat);
			    }
			    clusterlist.clear();
			    //std::cout << "layout: " << layout << std::endl;
			    return layout;
			}
		    }
		    longnamestring = "";
		}
            }
	}
    }

    return "";
}

std::string ParseFatFile(std::ifstream* rawcontent, fatinfo* curfat, std::string childfile)
{
    // NEED TO SPIT OUT ORIGINAL INFORMATION FOR FILE WHICH IS IN || || PORTIONS
    std::string fileforensics = "";
    //std::cout << "child file to find: " << childfile << std::endl;
    //std::cout << "parent directory layout: " << curfat->curdirlayout << std::endl;

    uint8_t isrootdir = 0;
    if(curfat->curdirlayout.compare(curfat->rootdirlayout) == 0)
	isrootdir = 1;
    else
	isrootdir = 0;

    std::vector<std::string> dirlayoutlist;
    dirlayoutlist.clear();
    std::istringstream dll(curfat->curdirlayout);
    std::string dls;
    while(getline(dll, dls, ';'))
	dirlayoutlist.push_back(dls);
    for(int i=0; i < dirlayoutlist.size(); i++)
    {
	uint64_t diroffset = 0;
	uint64_t dirlength = 0;
	std::size_t layoutsplit = dirlayoutlist.at(i).find(",");
	if(i == 0)
	{
	    diroffset = std::stoull(dirlayoutlist.at(i).substr(0, layoutsplit));
	    dirlength = std::stoull(dirlayoutlist.at(i).substr(layoutsplit+1));
	    if(isrootdir == 0) // sub directory
	    {
		diroffset = diroffset + 64; // skip . and .. directories
		dirlength = dirlength - 64; // adjust read size for the 64 byte skip
	    }
	}
	//std::cout << "dir offset: " << diroffset << " dir length: " << dirlength << std::endl;
	unsigned int direntrycount = dirlength / 32;
	//std::cout << "dir entry count: " << direntrycount << std::endl;
	// PARSE DIRECTORY ENTRIES
	std::string longnamestring = "";
	for(unsigned int j=0; j < direntrycount; j++)
	{
	    uint8_t* fc = new uint8_t[1];
	    uint8_t firstchar = 0;
	    ReadContent(rawcontent, fc, diroffset + j*32, 1);
	    firstchar = (uint8_t)fc[0];
	    delete[] fc;
	    if(firstchar == 0xe5) // deleted entry, skip
	    {
	    }
	    else if(firstchar == 0x00) // entry is free and all remaining are free
		break;
	    else if(firstchar == 0x85) // EXFAT File Entry
	    {
		std::string filename = "";
		uint32_t clusternum = 0;
		uint64_t logicalsize = 0;
		uint64_t physicalsize = 0;
		// SECONDARY COUNT
		uint8_t* sc = new uint8_t[1];
		uint8_t secondarycount = 0;
		ReadContent(rawcontent, sc, diroffset + j*32 + 1, 1);
		secondarycount = (uint8_t)sc[0];
		delete[] sc;
		//std::cout << "secondary count: " << secondarycount << std::endl;
		// FILE ATTRIBUTE
		uint8_t* fa = new uint8_t[1];
		uint8_t fileattr = 0;
		ReadContent(rawcontent, fa, diroffset + j*32 + 4, 1);
		fileattr = (uint8_t)fa[0];
		delete[] fa;
		//std::cout << "file attr: " << (int)fileattr << std::endl;
		int fatchain = 0;
		uint8_t namelength = 0;
		uint8_t curlength = 0;
		for(uint8_t k=1; k <= secondarycount; k++)
		{
		    uint8_t* set = new uint8_t[1];
		    uint8_t subentrytype = 0;
		    ReadContent(rawcontent, set, diroffset + j*32 + k*32, 1);
		    subentrytype = (uint8_t)set[0];
		    delete[] set;
		    //std::cout << "Sub entry type: 0x" << std::hex << (int)subentrytype << std::endl;
		    if(subentrytype == 0xc0) // Stream Extension Directory Entry
		    {
			// NAME LENGTH
			uint8_t* nl = new uint8_t[1];
			ReadContent(rawcontent, nl, diroffset + (j+k)*32 + 3, 1);
			namelength = (uint8_t)nl[0];
			delete[] nl;
			// FLAGS
			uint8_t* ff = new uint8_t[1];
			ReadContent(rawcontent, ff, diroffset + (j+k)*32 + 1, 1);
			std::bitset<8> flagbits{(uint8_t)ff[0]};
			delete[] ff;
			//std::cout << "flagbits: " << flagbits << std::endl;
			// FAT CHAIN
			fatchain = flagbits[1];
			//std::cout << "fatchain: " << fatchain << std::endl;
			// LOGICAL SIZE
			uint8_t* ls = new uint8_t[8];
			ReadContent(rawcontent, ls, diroffset + (j+k)*32 + 8, 8);
			ReturnUint64(&logicalsize, ls);
			delete[] ls;
			//std::cout << "Logical Size: " << logicalsize << std::endl;
			// CLUSTER NUM
			uint8_t* cn = new uint8_t[4];
			ReadContent(rawcontent, cn, diroffset + (j+k)*32 + 20, 4);
			ReturnUint32(&clusternum, cn);
			delete[] cn;
			//std::cout << "Cluster Number: " << clusternum << std::endl;
			// PHYSICAL SIZE
			uint8_t* ps = new uint8_t[8];
			ReadContent(rawcontent, ps, diroffset + (j+k)*32 + 24, 8);
			ReturnUint64(&physicalsize, ps);
			delete[] ps;
			//std::cout << "Physical Size: " << physicalsize << std::endl;
		    }
		    else if(subentrytype == 0xc1) // File Name Directory Entry
		    {
			// GET FILE NAME
			curlength += 15;
			if(curlength <= namelength)
			{
			    for(int m=1; m < 16; m++)
			    {
				uint8_t* sl = new uint8_t[2];
				uint16_t singleletter = 0;
				ReadContent(rawcontent, sl, diroffset + (j+k)*32 + m*2, 2);
				ReturnUint16(&singleletter, sl);
				delete[] sl;
				filename += (char)singleletter;
			    }
			}
			else
			{
			    int remaining = namelength + 16 - curlength;
			    for(int m=1; m < remaining; m++)
			    {
				uint8_t* sl = new uint8_t[2];
				uint16_t singleletter = 0;
				ReadContent(rawcontent, sl, diroffset + (j+k)*32 + m*2, 2);
				ReturnUint16(&singleletter, sl);
				delete[] sl;
				filename += (char)singleletter;
			    }
			}
		    }
		}
		if(filename.find(childfile) != std::string::npos)
		{
		    //std::cout << "cur file name: " << filename << std::endl;
		    fileforensics += "File Name|" + filename + "\n";
		    std::string layout = "";
		    // GET FILE PROPERTIES
		    fileforensics += "Attributes|";
		    if(fileattr & 0x01)
			fileforensics += "Read Only,";
		    if(fileattr & 0x02)
			fileforensics += "Hidden File,";
		    if(fileattr & 0x04)
			fileforensics += "System File,";
		    if(fileattr & 0x10)
			fileforensics += "Sub Directory,";
		    if(fileattr & 0x20)
			fileforensics += "Archive File,\n";
                    // GET LOGICAL SIZE
                    fileforensics += "Logical Size|" + std::to_string(logicalsize) + " bytes\n";
		    // GET DIRECTORY LAYOUT (PHYSICAL SIZE)
		    if(fatchain == 0 && clusternum > 1)
		    {
			std::vector<uint32_t> clusterlist;
			clusterlist.clear();
			clusterlist.push_back(clusternum);
			GetNextCluster(rawcontent, clusternum, curfat, &clusterlist);
			layout = ConvertClustersToExtents(&clusterlist, curfat);
			clusterlist.clear();
		    }
		    else if(fatchain == 1)
		    {
			uint clustercount = (uint)ceil((float)physicalsize / ((float)(curfat->bytespersector * curfat->sectorspercluster)));
			layout = std::to_string(curfat->clusterareastart * curfat->bytespersector + ((clusternum - 2) * curfat->bytespersector * curfat->sectorspercluster)) + "," + std::to_string(clustercount * curfat->bytespersector * curfat->sectorspercluster) + ";";
		    }
		    fileforensics += "Physical Layout|" + layout + " (offset,size;) [bytes]\n";
		    //std::cout << "Layout: " << layout << std::endl;
                    // GET DATE/TIME's
                    uint8_t* cd = new uint8_t[2];
                    uint16_t createdate = 0;
                    ReadContent(rawcontent, cd, diroffset + j*32 + 10, 2);
                    ReturnUint16(&createdate, cd);
                    delete[] cd;
                    uint8_t* ct = new uint8_t[2];
                    uint16_t createtime = 0;
                    ReadContent(rawcontent, ct, diroffset + j*32 + 8, 2);
                    ReturnUint16(&createtime, ct);
                    delete[] ct;
                    uint8_t* cz = new uint8_t[1];
                    uint8_t createzone = 0;
                    ReadContent(rawcontent, cz, diroffset + j*32 + 22, 1);
                    createzone = (uint8_t)cz[0];
                    delete[] cz;
                    fileforensics += "Create Date|" + ConvertExFatTimeToHuman(&createdate, &createtime, &createzone) + "\n";
                    uint8_t* md = new uint8_t[2];
                    uint16_t modifydate = 0;
                    ReadContent(rawcontent, md, diroffset + j*32 + 14, 2);
                    ReturnUint16(&modifydate, md);
                    delete[] md;
                    uint8_t* mt = new uint8_t[2];
                    uint16_t modifytime = 0;
                    ReadContent(rawcontent, mt, diroffset + j*32 + 12, 2);
                    ReturnUint16(&modifytime, mt);
                    delete[] mt;
                    uint8_t* mz = new uint8_t[1];
                    uint8_t modifyzone = 0;
                    ReadContent(rawcontent, mz, diroffset + j*32 + 23, 1);
                    modifyzone = (uint8_t)mz[0];
                    delete[] mz;
                    fileforensics += "Modify Date|" + ConvertExFatTimeToHuman(&modifydate, &modifytime, &modifyzone) + "\n";
                    uint8_t* ad = new uint8_t[2];
                    uint16_t accessdate = 0;
                    ReadContent(rawcontent, ad, diroffset + j*32 + 18, 2);
                    ReturnUint16(&accessdate, ad);
                    delete[] ad;
                    uint8_t* at = new uint8_t[2];
                    uint16_t accesstime = 0;
                    ReadContent(rawcontent, at, diroffset + j*32 + 16, 2);
                    ReturnUint16(&accesstime, at);
                    delete[] at;
                    uint8_t* az = new uint8_t[1];
                    uint8_t accesszone = 0;
                    ReadContent(rawcontent, az, diroffset + j*32 + 24, 1);
                    accesszone = (uint8_t)az[0];
                    delete[] az;
                    fileforensics += "Access Date|" + ConvertExFatTimeToHuman(&accessdate, &accesstime, &accesszone) + "\n";
		}
	    }
	    else
	    {
                uint8_t* fa = new uint8_t[1];
                uint8_t fileattr = 0;
                ReadContent(rawcontent, fa, diroffset + j*32 + 11, 1);
                fileattr = (uint8_t)fa[0];
                delete[] fa;
                if(fileattr == 0x0f || fileattr == 0x3f) // Long Directory Name
                {
                    unsigned int lsn = ((int)firstchar & 0x0f);
                    if(lsn <= 20)
                    {
                        // process long file name part here... then add to the long file name...
                        std::string longname = "";
                        int arr[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};
                        for(int k=0; k < 13; k++)
                        {
                            uint16_t longletter = 0;
                            uint8_t* ll = new uint8_t[3];
                            ReadContent(rawcontent, ll, diroffset + j*32 + arr[k], 2);
                            ReturnUint16(&longletter, ll);
                            delete[] ll;
                            if(longletter < 0xFFFF)
                            {
                                longname += (char)longletter;
                            }
                        }
                        longnamestring.insert(0, longname);
                    }
                }
		else // parse file contents here...
		{
		    //std::cout << "long name: " << longnamestring << "|" << std::endl;
		    if(longnamestring.find(childfile) != std::string::npos)
		    {
			// PRINT OUT LONG NAME
			fileforensics += "Long Name|" + longnamestring + "\n";
			//std::cout << "child file found, parse here..." << std::endl;
			// GET ALIAS NAME AND PRINT OUT
			uint8_t* rname = new uint8_t[8];
			ReadContent(rawcontent, rname, diroffset + j*32 + 1, 7);
			rname[7] = '\0';
			std::string restname((char*)rname);
			delete[] rname;
			//std::cout << "file name: " << (char)firstchar << restname << std::endl;
			uint8_t* ename = new uint8_t[4];
			ReadContent(rawcontent, ename, diroffset + j*32 + 8, 3);
			ename[3] = '\0';
			std::string extname((char*)ename);
			delete[] ename;
			std::size_t findempty = 0;
			while(extname.size() > 0)
			{
			    findempty = extname.find(" ", 0);
			    if(findempty != std::string::npos)
				extname.erase(findempty, 1);
			    else
				break;
			}
			//std::cout << "extname after find/erase loop: " << extname << "|" << std::endl;
			fileforensics += "Alias Name|" + std::string(1, (char)firstchar) + restname;
			if(extname.size() > 0)
			    fileforensics += "." + extname + "\n";
			else
			    fileforensics += "\n";
			// GET FILE PROPERTIES
			fileforensics += "Attributes|";
			if(fileattr & 0x01)
			    fileforensics += "Read Only,";
			if(fileattr & 0x02)
			    fileforensics += "Hidden File,";
			if(fileattr & 0x04)
			    fileforensics += "System File,";
			if(fileattr & 0x08)
			    fileforensics += "Volume ID,";
			if(fileattr & 0x10)
			    fileforensics += "Sub Directory,";
			if(fileattr & 0x20)
			    fileforensics += "Archive File,";
			fileforensics += "\n";
			// GET LOGICAL FILE SIZE
			uint8_t* ls = new uint8_t[4];
			uint32_t logicalsize = 0;
			ReadContent(rawcontent, ls, diroffset + j*32 + 28, 4);
			ReturnUint32(&logicalsize, ls);
			delete[] ls;
			fileforensics += "Logical Size|" + std::to_string(logicalsize) + " bytes\n";
			// GET PHYSICAL FILE SIZE (SHOULD BE LAYOUT)
			uint8_t* hcn = new uint8_t[2];
			uint16_t hiclusternum = 0;
			ReadContent(rawcontent, hcn, diroffset + j*32 + 20, 2); // always zero for fat12/16
			ReturnUint16(&hiclusternum, hcn);
			delete[] hcn;
			uint8_t* lcn = new uint8_t[2];
			uint16_t loclusternum = 0;
			ReadContent(rawcontent, lcn, diroffset + j*32 + 26, 2);
			ReturnUint16(&loclusternum, lcn);
			delete[] lcn;
			uint32_t clusternum = ((uint32_t)hiclusternum >> 16) + loclusternum;
			std::vector<uint32_t> clusterlist;
			clusterlist.clear();
			//std::cout << "first cluster: " << clusternum << std::endl;
			if(clusternum >= 2)
			{
			    clusterlist.push_back(clusternum);
			    GetNextCluster(rawcontent, clusternum, curfat, &clusterlist);
			}
			std::string layout = "";
			if(clusterlist.size() > 0)
			{
			    layout = ConvertClustersToExtents(&clusterlist, curfat);
			}
			clusterlist.clear();
			fileforensics += "Physical Layout|" + layout + " (offset,size;) [bytes]\n";
			//std::cout << "layout: " << layout << std::endl;
			// GET DATE/TIME's
			uint8_t* cd = new uint8_t[2];
			uint16_t createdate = 0;
			ReadContent(rawcontent, cd, diroffset + j*32 + 16, 2);
			ReturnUint16(&createdate, cd);
			delete[] cd;
			uint8_t* ct = new uint8_t[2];
			uint16_t createtime = 0;
			ReadContent(rawcontent, ct, diroffset + j*32 + 14, 2);
			ReturnUint16(&createtime, ct);
			fileforensics += "Create Date|" + ConvertDosTimeToHuman(&createdate, &createtime) + "\n";
			uint8_t* ad = new uint8_t[2];
			uint16_t accessdate = 0;
			ReadContent(rawcontent, ad, diroffset + j*32 + 18, 2);
			ReturnUint16(&accessdate, ad);
			delete[] ad;
			fileforensics += "Access Date|" + ConvertDosTimeToHuman(&accessdate, NULL) + "\n";
			uint8_t* md = new uint8_t[2];
			uint16_t modifydate = 0;
			ReadContent(rawcontent, md, diroffset + j*32 + 24, 2);
			ReturnUint16(&modifydate, md);
			delete[] md;
			uint8_t* mt = new uint8_t[2];
			uint16_t modifytime = 0;
			ReadContent(rawcontent, mt, diroffset + j*32 + 22, 2);
			ReturnUint16(&modifytime, mt);
			delete[] mt;
			fileforensics += "Modify Date|" + ConvertDosTimeToHuman(&modifydate, &modifytime) + "\n";
		    }
		    longnamestring = "";
		}
	    }
	}
    }

    return fileforensics;
}
*/
