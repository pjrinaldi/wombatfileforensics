#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <filesystem>

#include "blake3.h"

#include "extfs.h"
#include "fatfs.h"
#include "ntfs.h"

void ShowUsage(int outtype)
{
    if(outtype == 0)
    {
        printf("Provide forensic file properties for each of the FILES for any mounted filesystem.\n\n");
        printf("Usage :\n");
        printf("\twombatfileforensics [args] FILES\n\n");
        printf("FILES\t: a device to image such as /dev/sdX\n");
        printf("Arguments :\n");
	printf("-r\t: Recurse subdirectories.\n");
	printf("-c\t: Calculate the hash for the file(s).\n");
	printf("-w\t: Include file slack in output.\n");
	printf("-o FILE\t: Output file's contents to FILE.\n");
        printf("-V\t: Prints Version information\n");
        printf("-h\t: Prints help information\n\n");
        printf("Example Usage :\n");
        printf("wombatfileforensics item1 dir/ file1 file2\n");
    }
    else if(outtype == 1)
    {
        printf("wombatfileforensics v0.1\n");
	printf("License CC0-1.0: Creative Commons Zero v1.0 Universal\n");
        printf("This software is in the public domain\n");
        printf("There is NO WARRANTY, to the extent permitted by law.\n\n");
        printf("Written by Pasquale Rinaldi\n");
    }
};

void ParseDirectory(std::filesystem::path dirpath, std::vector<std::filesystem::path>* filelist)
{
    for(auto const& dir_entry : std::filesystem::recursive_directory_iterator(dirpath))
    {
        if(std::filesystem::is_regular_file(dir_entry))
            filelist->push_back(dir_entry);
    }
}

void DetermineFileSystem(std::string devicestring, int* fstype)
{
    std::ifstream devicebuffer(devicestring.c_str(), std::ios::in|std::ios::binary);
    unsigned char* extsig = new unsigned char[2];
    unsigned char* winsig = new unsigned char[2];
    unsigned char* refsig = new unsigned char[8];
    unsigned char* f2fsig = new unsigned char[4];
    unsigned char* zfssig = new unsigned char[8];
    unsigned char* bcfsig = new unsigned char[16];
    unsigned char* zonsig = new unsigned char[4];
    char* bfssig = new char[4];
    char* apfsig = new char[4];
    char* hfssig = new char[2];
    char* xfssig = new char[4];
    char* btrsig = new char[8];
    char* btlsig = new char[8];
    char* isosig = new char[5];
    char* udfsig = new char[5];
    // get ext2,3,4 signature
    devicebuffer.seekg(1080);
    devicebuffer.read((char*)extsig, 2); // 0x53, 0xef
    // get windows mbr signature (FAT, NTFS, BFS)
    devicebuffer.seekg(510);
    devicebuffer.read((char*)winsig, 2); // 0x55, 0xaa
    // get BFS signature
    devicebuffer.seekg(544);
    devicebuffer.read(bfssig, 4);
    std::string bfsigstr(bfssig);
    delete[] bfssig;
    // get apfs signature
    devicebuffer.seekg(32);
    devicebuffer.read(apfsig, 4);
    std::string apfsigstr(apfsig);
    delete[] apfsig;
    // get hfs signature
    devicebuffer.seekg(1024);
    devicebuffer.read(hfssig, 2);
    std::string hfssigstr(hfssig);
    delete[] hfssig;
    // get xfs signature
    devicebuffer.seekg(0);
    devicebuffer.read(xfssig, 4);
    std::string xfssigstr(xfssig);
    delete[] xfssig;
    // get btrfs signature
    devicebuffer.seekg(65600);
    devicebuffer.read(btrsig, 8);
    std::string btrsigstr(btrsig);
    delete[] btrsig;
    // get bitlocker signature
    devicebuffer.seekg(0);
    devicebuffer.read(btlsig, 8);
    std::string btlsigstr(btlsig);
    delete[] btlsig;
    // get iso signature
    devicebuffer.seekg(32769);
    devicebuffer.read(isosig, 5);
    std::string isosigstr(isosig);
    delete[] isosig;
    // get udf signature
    devicebuffer.seekg(40961);
    devicebuffer.read(udfsig, 5);
    std::string udfsigstr(udfsig);
    delete[] udfsig;
    // get refs signature
    devicebuffer.seekg(3);
    devicebuffer.read((char*)refsig, 8);
    // get f2fs signature
    devicebuffer.seekg(1024);
    devicebuffer.read((char*)f2fsig, 4);
    // get zfs signature
    devicebuffer.seekg(135168);
    devicebuffer.read((char*)zfssig, 4);
    // get bcachefs signature
    devicebuffer.seekg(4120);
    devicebuffer.read((char*)bcfsig, 16);
    // get zonefs signature
    devicebuffer.seekg(0);
    devicebuffer.read((char*)zonsig, 4);
    //std::cout << "compare:" << bfsigstr.substr(0,4).compare("1SFB") << std::endl;
    //std::cout << "extsig1 array: " << std::hex << static_cast<int>((unsigned char)extsig[1]) << std::endl;
    //std::cout << "extsig1 " << std::hex << static_cast<int>((unsigned char)extsig1) << std::endl;
    if(extsig[0] == 0x53 && extsig[1] == 0xef) // EXT2,3,4 SIGNATURE == 0
    {
        *fstype = 0;
    }
    else if(winsig[0] == 0x55 && winsig[1] == 0xaa && bfsigstr.find("1SFB") == std::string::npos) // FAT NTFS, BFS SIGNATURE
    {
        char* exfatbuf = new char[5];
        char* fatbuf = new char[5];
        char* fat32buf = new char[5];
        devicebuffer.seekg(3);
        devicebuffer.read(exfatbuf, 5);
        std::string exfatstr(exfatbuf);
        delete[] exfatbuf;
        devicebuffer.seekg(54);
        devicebuffer.read(fatbuf, 5);
        std::string fatstr(fatbuf);
        delete[] fatbuf;
        devicebuffer.seekg(82);
        devicebuffer.read(fat32buf, 5);
        std::string fat32str(fat32buf);
        delete[] fat32buf;
        if(fatstr.find("FAT12") != std::string::npos)
            *fstype = 1;
        else if(fatstr.find("FAT16") != std::string::npos)
            *fstype = 2;
        else if(fat32str.find("FAT32") != std::string::npos)
            *fstype = 3;
        else if(exfatstr.find("EXFAT") != std::string::npos)
            *fstype = 4;
        else if(exfatstr.find("NTFS") != std::string::npos)
            *fstype = 5;
    }
    else if(apfsigstr.find("NXSB") != std::string::npos) // APFS
        *fstype = 6;
    else if(hfssigstr.find("H+") != std::string::npos) // HFS+
        *fstype = 7;
    else if(hfssigstr.find("HX") != std::string::npos) // HFSX
        *fstype = 8;
    else if(xfssigstr.find("XFSB") != std::string::npos) // XFS
        *fstype = 9;
    else if(btrsigstr.find("_BHRfS_M") != std::string::npos) // BTRFS
        *fstype = 10;
    else if(btlsigstr.find("-FVE-FS-") != std::string::npos) // BTILOCKER
        *fstype = 11;
    else if(bfsigstr.find("1SFB") != std::string::npos) // BFS
        *fstype = 12;
    else if(f2fsig[0] == 0x10 && f2fsig[1] == 0x20 && f2fsig[3] == 0xf5 && f2fsig[3] == 0xf2) // F2FS
        *fstype = 13;
    else if(isosigstr.find("CD001") != std::string::npos && udfsigstr.find("BEA01") == std::string::npos) // ISO9660
        *fstype = 14;
    else if(isosigstr.find("CD001") != std::string::npos && udfsigstr.find("BEA01") != std::string::npos) // UDF
        *fstype = 15;
    else if(hfssigstr.find("BD") != std::string::npos) // Legacy HFS
        *fstype = 16;
    else if(zfssig[0] == 0x0c && zfssig[1] == 0xb1 && zfssig[2] == 0xba && zfssig[3] == 0x00) // ZFS
        *fstype = 17;
    else if(refsig[0] == 0x00 && refsig[1] == 0x00 && refsig[2] == 0x00 && refsig[3] == 0x00 && refsig[4] == 0x53 && refsig[5] == 0x46 && refsig[6] == 0x65 && refsig[7] == 0x52) // ReFS
        *fstype = 18;
    else if(f2fsig[0] == 0xe2 && f2fsig[1] == 0xe1 && f2fsig[2] == 0x5e && f2fsig[3] == 0x0f) // EROFS
        *fstype = 19;
    else if(bcfsig[0] == 0xc6 && bcfsig[1] == 0x85 && bcfsig[2] == 0x73 && bcfsig[3] == 0xf6 && bcfsig[4] == 0x4e && bcfsig[5] == 0x1a && bcfsig[6] == 0x45 && bcfsig[7] == 0xca && bcfsig[8] == 0x82 && bcfsig[9] == 0x65 && bcfsig[10] == 0xf5 && bcfsig[11] == 0x7f && bcfsig[12] == 0x48 && bcfsig[13] == 0xba && bcfsig[14] == 0x6d && bcfsig[15] == 0x81) // BCACHEFS
        *fstype = 20;
    else if(zonsig[0] == 0x5a && zonsig[1] == 0x4f && zonsig[2] == 0x46 && zonsig[3] == 0x53) // ZONEFS
        *fstype = 21;
    else // UNKNOWN FILE SYSTEM SO FAR
        *fstype = 50; 
    devicebuffer.close();
    delete[] extsig;
    delete[] winsig;
    delete[] refsig;
    delete[] f2fsig;
    delete[] zfssig;
    delete[] bcfsig;
    delete[] zonsig;
}

    /*
    // WILL WRITE FILE SYSTEM INFORMATION IN THIS FUNCTION AND ONLY RETURN THE QSTRING(FILESYSTEMNAME,FILESYSTEMTYPE) TO BE USED BY THE PARTITION
    if(winsig == 0xaa55 && bfssig != "1SFB") // FAT OR NTFS OR BFS
    {
	if(exfatstr.startsWith("NTFS")) // NTFS
	{
	    qInfo() << "NTFS File System Found. Parsing...";
	    out << "File System Type Int|5|Internal File System Type represented as an integer." << Qt::endl;
	    out << "File System Type|NTFS|File System Type String." << Qt::endl;
	    out << "Bytes Per Sector|" << QString::number(qFromLittleEndian<uint16_t>(curimg->ReadContent(curstartsector*512 + 11, 2))) << "|Number of bytes per sector, usually 512." << Qt::endl;
	    out << "Sectors Per Cluster|" << QString::number(qFromLittleEndian<uint8_t>(curimg->ReadContent(curstartsector*512 + 13, 1))) << "|Number of sectors per cluster." << Qt::endl;
	    out << "Total Sectors|" << QString::number(qFromLittleEndian<qulonglong>(curimg->ReadContent(curstartsector*512 + 40, 8))) << "|Number of sectors in the file system." << Qt::endl;
	    out << "Bytes Per Cluster|" << QString::number(qFromLittleEndian<uint16_t>(curimg->ReadContent(curstartsector*512 + 11, 2)) * qFromLittleEndian<uint8_t>(curimg->ReadContent(curstartsector*512 + 13, 1))) << "|Number of bytes per cluster" << Qt::endl;
	    out << "MFT Starting Cluster|" << QString::number(qFromLittleEndian<qulonglong>(curimg->ReadContent(curstartsector*512 + 48, 8))) << "|Starting cluster number for the MFT" << Qt::endl;
	    out << "MFT Starting Offset|" << QString::number(curstartsector*512 + qFromLittleEndian<qulonglong>(curimg->ReadContent(curstartsector*512 + 48, 8)) * qFromLittleEndian<uint16_t>(curimg->ReadContent(curstartsector*512 + 11, 2)) * qFromLittleEndian<uint8_t>(curimg->ReadContent(curstartsector*512 + 13, 1))) << "|Starting byte for the MFT" << Qt::endl;
	    out << "MFT Entry Size|" << QString::number(qFromLittleEndian<uint8_t>(curimg->ReadContent(curstartsector*512 + 64, 1))) << "|Entry size in clusters for an MFT Entry" << Qt::endl;
            out << "MFT Entry Bytes|1024| Entry size in bytes for an MFT Entry" << Qt::endl; // entrysize is stored at offset 64, then it should be entrysize * bytespercluster
	    out << "Serial Number|" << QString("0x" + QString::number(qFromLittleEndian<qulonglong>(curimg->ReadContent(curstartsector*512 + 72, 8)), 16)) << "|Serial number for the file system volume" << Qt::endl;
            out << "Index Record Size|" << QString::number(qFromLittleEndian<uint8_t>(curimg->ReadContent(curstartsector*512 + 68, 1))) << "|Index record size for an index record." << Qt::endl;

            uint bytespercluster = qFromLittleEndian<uint16_t>(curimg->ReadContent(curstartsector*512 + 11, 2)) * qFromLittleEndian<uint8_t>(curimg->ReadContent(curstartsector*512 + 13, 1));
            qint64 mftoffset = curstartsector*512 + qFromLittleEndian<qulonglong>(curimg->ReadContent(curstartsector*512 + 48, 8)) * bytespercluster;
            // GET THE MFT LAYOUT TO WRITE TO PROP FILE
            if(QString::fromStdString(curimg->ReadContent(mftoffset, 4).toStdString()) == "FILE") // a proper MFT entry
            {
                int curoffset = qFromLittleEndian<uint16_t>(curimg->ReadContent(mftoffset + 20, 2)); // mft offset + offset to first attribute
                for(int i=0; i < qFromLittleEndian<uint16_t>(curimg->ReadContent(mftoffset + 40, 2)); i++) // loop over attributes until hit attribute before the next attribute id
                {
                    if(qFromLittleEndian<uint32_t>(curimg->ReadContent(mftoffset + curoffset, 4)) == 0x80 && qFromLittleEndian<uint8_t>(curimg->ReadContent(mftoffset + curoffset + 9, 1)) == 0) // attrtype | namelength > default$DATA attribute to parse
                        break;
                    curoffset += qFromLittleEndian<uint32_t>(curimg->ReadContent(mftoffset + curoffset + 4, 4)); // attribute length
                }
                QString runliststr = "";
                quint64 mftsize = 0;
                GetRunListLayout(curimg, curstartsector, bytespercluster, 1024, mftoffset + curoffset, &runliststr);
                //qDebug() << "runliststr for MFT Layout:" << runliststr;
		for(int j=0; j < runliststr.split(";", Qt::SkipEmptyParts).count(); j++)
		{
		    mftsize += runliststr.split(";", Qt::SkipEmptyParts).at(j).split(",").at(1).toULongLong();
		}
                //qDebug() << "runliststr for MFT Layout:" << runliststr << "mft size:" << mftsize;
	        out << "MFT Layout|" << runliststr << "|Layout for the MFT in starting offset, size; format" << Qt::endl;
                out << "Max MFT Entries|" << QString::number((mftsize)/1024) << "|Max MFT Entries allowed in the MFT" << Qt::endl;
                runliststr = "";
                mftsize = 0;
            }
            // GET VOLUME LABEL FROM THE $VOLUME_NAME SYSTEM FILE
            if(QString::fromStdString(curimg->ReadContent(mftoffset + 3 * 1024, 4).toStdString()) == "FILE") // a proper MFT entry
            {
                int curoffset = qFromLittleEndian<uint16_t>(curimg->ReadContent(mftoffset + 3*1024 + 20, 2)); // offset to first attribute
                for(uint i=0; i < qFromLittleEndian<uint16_t>(curimg->ReadContent(mftoffset + 3*1024 + 40, 2)); i++) // loop over attributes until get to next attribute id
                {
                    if(qFromLittleEndian<uint32_t>(curimg->ReadContent(mftoffset + 3*1024 + curoffset, 4)) == 0x60) // $VOLUME_NAME attribute to parse (always resident)
                        break;
                    curoffset += qFromLittleEndian<uint32_t>(curimg->ReadContent(mftoffset + 3*1024 + curoffset + 4, 4));
                }
                for(uint k=0; k < qFromLittleEndian<uint32_t>(curimg->ReadContent(mftoffset + 3*1024 + curoffset + 16, 4))/2; k++)
                    partitionname += QString(QChar(qFromLittleEndian<uint16_t>(curimg->ReadContent(mftoffset + 3*1024 + curoffset + qFromLittleEndian<uint16_t>(curimg->ReadContent(mftoffset + 3*1024 + curoffset + 20, 2)) + k*2, 2))));
                out << "Volume Label|" << partitionname << "|Volume Label for the file system." << Qt::endl;
                partitionname += " [NTFS]";
            }
	}
    }
    else if(extsig == 0xef53) // EXT2/3/4
    {
        out << "File System Type Int|6|Internal File System Type represented as an integer." << Qt::endl;
        for(int i=0; i < 16; i++)
        {
            if(qFromBigEndian<uint8_t>(curimg->ReadContent(curstartsector*512 + 1144 + i, 1)) == 0x00)
                break;
            else
                partitionname += QString::fromStdString(curimg->ReadContent(curstartsector*512 + 1144 + i, 1).toStdString());
        }
        //qDebug() << "partition name:" << partitionname;
        out << "Volume Label|" << partitionname << "|Volume Label for the file system." << Qt::endl;
        out << "Created Time|" << QDateTime::fromSecsSinceEpoch(qFromLittleEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1288, 4)), QTimeZone::utc()).toString("MM/dd/yyyy hh:mm:ss AP") << "|Creation time for the file system." << Qt::endl;
        out << "Mount Time|" << QDateTime::fromSecsSinceEpoch(qFromLittleEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1068, 4)), QTimeZone::utc()).toString("MM/dd/yyyy hh:mm:ss AP") << "|Mount time for the file system." << Qt::endl;
        out << "Write Time|" << QDateTime::fromSecsSinceEpoch(qFromLittleEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1072, 4)), QTimeZone::utc()).toString("MM/dd/yyyy hh:mm:ss AP") << "|Write time for the file system." << Qt::endl;
        out << "Last Check Time|" << QDateTime::fromSecsSinceEpoch(qFromLittleEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1088, 4)), QTimeZone::utc()).toString("MM/dd/yyyy hh:mm:ss AP") << "|Last check time for the file system." << Qt::endl;
        out << "Current State|";
        if(qFromLittleEndian<uint16_t>(curimg->ReadContent(curstartsector*512 + 1082, 2)) == 0x01)
            out << "Cleanly unmounted";
        else
            out << "Errors detected";
        out << "|Condition of the file system at lsat unmount." << Qt::endl;
        out << "Compatible Features|";
        uint32_t compatflags = qFromLittleEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1116, 4));
        if(compatflags & 0x200)
            out << "Sparse Super Block,";
        if(compatflags & 0x100)
            out << "Exclude Bitmap,";
        if(compatflags & 0x80)
            out << "Exclude Inodes,";
        if(compatflags & 0x40)
            out << "Lazy Block Groups,";
        if(compatflags & 0x20)
            out << "Indexed Directories,";
        if(compatflags & 0x10)
            out << "Reserved GDT,";
        if(compatflags & 0x08)
            out << "Extended Attributes,";
        if(compatflags & 0x04)
            out << "Journal,";
        if(compatflags & 0x02)
            out << "Imagic Inodes,";
        if(compatflags & 0x01)
            out << "Directory preallocation";
        out << "|File system compatible feature set." << Qt::endl;
        uint32_t incompatflags = qFromLittleEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1120, 4));
        out << "Incompatible Features|";
	if(incompatflags & 0x10000)
	    out << "Encrypted inodes,";
	if(incompatflags & 0x8000)
	    out << "Data in inode,";
	if(incompatflags & 0x4000)
	    out << "Large Directory >2GB or 3-level htree,";
	if(incompatflags & 0x2000)
	    out << "Metadata checksum seed in superblock,";
	if(incompatflags & 0x1000)
	    out << "Data in Directory Entry,";
	if(incompatflags & 0x400)
	    out << "Inodes can store large extended attributes,";
	if(incompatflags & 0x200)
	    out << "Flexible block groups,";
	if(incompatflags & 0x100)
	    out << "Multiple mount protection,";
	if(incompatflags & 0x80)
	    out << "FS size over 2^32 blocks,";
	if(incompatflags & 0x40)
	    out << "Files use Extents,";
	if(incompatflags & 0x10)
	    out << "Meta block groups,";
	if(incompatflags & 0x08)
	    out << "Seperate Journal device,";
	if(incompatflags & 0x04)
	    out << "FS needs journal recovery,";
	if(incompatflags & 0x02)
	    out << "Directory entries record file type,";
	if(incompatflags & 0x01)
	    out << "Compression";
        out << "|File system incompatible Feature set." << Qt::endl;
        uint32_t readonlyflags = qFromLittleEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1124, 4));
        out << "Read Only Compatible Features|";
	if(readonlyflags & 0x2000)
	    out << "Tracks project quotas,";
	if(readonlyflags & 0x1000)
	    out << "Read only FS,";
	if(readonlyflags & 0x800)
	    out << "Replica support,";
	if(readonlyflags & 0x400)
	    out << "Metadata Checksumming support,";
	if(readonlyflags & 0x200)
	    out << "Bigalloc support,";
	if(readonlyflags & 0x100)
	    out << "Quota is handled transactionally with journal,";
	if(readonlyflags & 0x80)
	    out << "Snapshot,";
	if(readonlyflags & 0x40)
	    out << "Large Inodes exist,";
	if(readonlyflags & 0x20)
	    out << "EXT3 32k subdir limit doesn't exist,";
	if(readonlyflags & 0x10)
	    out << "Group descriptors have checksums,";
	if(readonlyflags & 0x08)
	    out << "Space usage stored in i_blocks in units of fs_blocks, not 512-byte sectors,";
	if(readonlyflags & 0x04)
	    out << "Was intented for use with htree directories,";
	if(readonlyflags & 0x02)
	    out << "Allow storing files larger than 2GB,";
	if(readonlyflags & 0x01)
	    out << "Sparse superblocks";
        out << "|File system read only compatible feature set." << Qt::endl;
        out << "File System Type|";
        if(((compatflags & 0x00000200UL) != 0) || ((incompatflags & 0x0001f7c0UL) != 0) || ((readonlyflags & 0x00000378UL) != 0))
        {
	    qInfo() << "EXT4 File System Found. Parsing...";
            out << "EXT4";
            partitionname += QString(" [EXT4]");
        }
        else if(((compatflags & 0x00000004UL) != 0) || ((incompatflags & 0x0000000cUL) != 0))
        {
	    qInfo() << "EXT3 File System Found. Parsing...";
            out << "EXT3";
            partitionname += QString(" [EXT3]");
        }
        else
        {
	    qInfo() << "EXT2 File System Found. Parsing...";
            out << "EXT2";
            partitionname += QString(" [EXT2]");
        }
        //qDebug() << "partition name with fstype:" << partitionname;
        out << "|File system type string." << Qt::endl;
        uint16_t grpdescsize = 32;
	if(incompatflags & 0x80)
        {
            grpdescsize = qFromLittleEndian<uint16_t>(curimg->ReadContent(curstartsector * 512 + 1278, 2));
        }
        //qDebug() << "grpdescsize:" << grpdescsize;
        out << "Block Group Descriptor Size|" << QString::number(grpdescsize) << "|Size in bytes of the block group descriptor table entry." << Qt::endl;
	uint32_t fsblockcnt = qFromLittleEndian<uint32_t>(curimg->ReadContent(curstartsector * 512 + 1028, 4));
	uint32_t blkgrpblkcnt = qFromLittleEndian<uint32_t>(curimg->ReadContent(curstartsector * 512 + 1056, 4));
	uint32_t blocksize = 1024 * pow(2, qFromLittleEndian<uint32_t>(curimg->ReadContent(curstartsector * 512 + 1048, 4)));
        uint32_t blkgrp0startblk = qFromLittleEndian<uint32_t>(curimg->ReadContent(curstartsector * 512 + 1044, 4));
        out << "File System Inode Count|" << QString::number(qFromLittleEndian<uint32_t>(curimg->ReadContent(curstartsector * 512 + 1024, 4))) << "|Number of inodes within the file system." << Qt::endl;
        out << "File System Block Count|" << QString::number(fsblockcnt) << "|Number of blocks within the file system." << Qt::endl;
        out << "Block Group 0 Start Block|" << QString::number(blkgrp0startblk) << "|Starting block number for block group 0." << Qt::endl;
        out << "Block Size|" << QString::number(blocksize) << "|Block size in bytes." << Qt::endl;
	out << "Fragment Size|" << QString::number(1024 * pow(2, qFromLittleEndian<uint32_t>(curimg->ReadContent(curstartsector * 512 + 1052, 4)))) << "|Fragment size in bytes." << Qt::endl;
	out << "Block Group Block Count|" << QString::number(blkgrpblkcnt) << "|Number of blocks within a block group." << Qt::endl;
	out << "Block Group Fragment Count|" << QString::number(qFromLittleEndian<uint32_t>(curimg->ReadContent(curstartsector * 512 + 1060, 4))) << "|Number of fragments within a block group." << Qt::endl;
	uint32_t blkgrpinodecnt = qFromLittleEndian<uint32_t>(curimg->ReadContent(curstartsector * 512 + 1064, 4));
	out << "Block Group Inode Count|" << QString::number(blkgrpinodecnt) << "|Number of inodes within a block group." << Qt::endl;
	uint32_t creatoros = qFromLittleEndian<uint32_t>(curimg->ReadContent(curstartsector * 512 + 1096, 4));
	out << "Creator OS|";
	if(creatoros == 0x00)
	    out << "Linux";
	else if(creatoros == 0x03)
	    out << "FreeBSD";
	out << "|Operating System used to create the file system." << Qt::endl;
	out << "Inode Size|" << QString::number(qFromLittleEndian<uint16_t>(curimg->ReadContent(curstartsector * 512 + 1112, 2))) << "|Size of an inode in bytes." << Qt::endl;
	out << "Last Mounted Path|" << QString::fromStdString(curimg->ReadContent(curstartsector * 512 + 1160, 64).toStdString()) << "|Path where file system was last mounted." << Qt::endl;
	uint32_t blockgroupcount = fsblockcnt / blkgrpblkcnt;
	uint blkgrpcntrem = fsblockcnt % blkgrpblkcnt;
	if(blkgrpcntrem > 0)
	    blockgroupcount++;
	if(blockgroupcount == 0)
	    blockgroupcount = 1;
        //qDebug() << "curstartsector:" << curstartsector;
        //qDebug() << "blkgrp0startblock:" << blkgrp0startblk;
	QString inodeaddresstable = "";
        //qDebug() << "blockgroupcount:" << blockgroupcount;
        //qDebug() << "blocksize:" << blocksize;
	for(uint i=0; i < blockgroupcount; i++)
	{
	    if(blocksize == 1024)
		inodeaddresstable += QString::number(qFromLittleEndian<uint32_t>(curimg->ReadContent(curstartsector * 512 + 2 * blocksize + i * grpdescsize + 8, 4))) + ",";
	    else
		inodeaddresstable += QString::number(qFromLittleEndian<uint32_t>(curimg->ReadContent(curstartsector * 512 + blocksize + i * grpdescsize + 8, 4))) + ",";
	}
	//qDebug() << "inodeaddresstable:" << inodeaddresstable;
	out << "Inode Address Table|" << inodeaddresstable << "|Table of the Inode addresses for a block group." << Qt::endl;
	out << "Root Inode Table Address|";
	if(blkgrpinodecnt > 2)
	    out << QString::number(qFromLittleEndian<uint32_t>(curimg->ReadContent(curstartsector * 512 + 2056, 4)));
	else
	    out << QString::number(qFromLittleEndian<uint32_t>(curimg->ReadContent(curstartsector * 512 + 2088, 4)));
	out << "|Starting address for the Root Directory Inode Table." << Qt::endl;
	out << "Revision Level|" << QString::number(qFromLittleEndian<uint32_t>(curimg->ReadContent(curstartsector * 512 + 1100, 4))) + "." + QString::number(qFromLittleEndian<uint16_t>(curimg->ReadContent(curstartsector * 512 + 1086, 2))) << "|File system revision level." << Qt::endl;
	// NEED TO IMPLEMENT THESE FS PROPERTIES SO I CAN USE THEM WHEN I PARSE THE EXT2/3/4 FS
        fsinfo.insert("partoffset", QVariant((qulonglong)(512 * partoffset)));
        //qDebug() << "INODE SIZE ACCORDING TO SUPERBLOCK:" << fsinfo.value("inodesize").toUInt();
        //qDebug() << "compatflags:" << fsinfo.value("compatflags").toUInt() << "incompatflags:" << fsinfo.value("incompatflags").toUInt() << "readonlyflags:" << fsinfo.value("readonlyflags").toUInt();
        //if(fsinfo.value("incompatflags").toUInt() & 0x40)
        //    qDebug() << "fs uses extents.";
        
	//qDebug() << "blocks for group descriptor table:" << (32 * (fsinfo.value("fsblockcnt").toUInt() / fsinfo.value("blockgroupblockcnt").toUInt())) / fsinfo.value("blocksize").toUInt();
    }
    else if(apfsig == "NXSB") // APFS Container
    {
	qInfo() << "APFS Container Found. Parsing...";
        uint64_t nxoffset = curstartsector*512;
        // THIS POSSIBLY NEEDS TO OCCUR WITHIN THE FOR LOOP OF THE CHECKPOINT DESCRIPTOR LOOP
        uint64_t nxoid = qFromLittleEndian<uint64_t>(curimg->ReadContent(nxoffset + 8, 8));
        uint64_t nxxid = qFromLittleEndian<uint64_t>(curimg->ReadContent(nxoffset + 16, 8));
        //qDebug() << "nxoid:" << nxoid << "nxxid:" << nxxid;
        uint32_t blocksize = qFromLittleEndian<uint32_t>(curimg->ReadContent(nxoffset + 36, 4));
        uint32_t nxcmapblkcnt = qFromLittleEndian<uint32_t>(curimg->ReadContent(nxoffset + 104, 4));
        int64_t nxcmapblk = qFromLittleEndian<int64_t>(curimg->ReadContent(nxoffset + 112, 8));
        //qDebug() << "nxcmapblk:" << nxcmapblk << "nxcmapblkcnt:" << nxcmapblkcnt;
        uint64_t nxcmapoffset = nxcmapblk * blocksize + curstartsector*512;
        //qDebug() << "nxcmapoffset:" << nxcmapoffset;
        uint64_t oldxid = nxxid;
        for(int i=0; i <= nxcmapblkcnt; i++)
        {
            uint64_t curoid = qFromLittleEndian<uint64_t>(curimg->ReadContent(nxcmapoffset + i*blocksize + 8, 8));
            uint64_t curxid = qFromLittleEndian<uint64_t>(curimg->ReadContent(nxcmapoffset + i*blocksize + 16, 8));
            uint32_t curtype = qFromLittleEndian<uint32_t>(curimg->ReadContent(nxcmapoffset + i*blocksize + 24, 4));
            //qDebug() << "i:" << i << "curoid:" << curoid << "curxid:" << curxid << "curtype:" << QString::number(curtype, 16);
            
            if(curoid == 0 && curxid == 0 && curtype == 0)
                break; // break out of for loop
            switch(curtype)
            {
                case 0x4000000c: // PHYSICAL CHECKPOINT MAP
                    // NEED TO IMPLEMENT THIS PART OF THE CODE NEXT...
                    qDebug() << "use checkpoint map to get the latest superblock with whcih to parse";
                    qDebug() << "cmap offset:" << nxcmapoffset + i*blocksize;
                    qDebug() << "curoid:" << curoid << "curxid:" << curxid << "curtype:" << QString::number(curtype, 16);
                    break;
                case 0x80000001: // EPHEMERAL SUPERBLOCK
                    //qDebug() << "it's a superblock so see if it's newer and use it.";
                    //qDebug() << "i:" << i << "curxid:" << curxid << "oldxid:" << oldxid;
                    if(curxid > oldxid)
                    {
                        oldxid = curxid;
                        nxoffset = nxcmapoffset + i*blocksize;
                        //qDebug() << "curoid:" << curoid << "curxid:" << curxid << "curtype:" << QString::number(curtype, 16);
                    }
                    break;
                default:
                    break;
            }
        }
        //qDebug() << "new superblock oid/offset:" << nxoid << "/" << nxoffset;
        // NOW TO DO THE OBJECT MAP PARSING FOR WHAT I NEED
        //uint64_t nxomapoid = qFromLittleEndian<uint64_t>(curimg->ReadContent(nxoffset + 160, 8));
        //uint64_t nxomapoffset = curstartsector*512 + nxomapoid * blocksize;
        //qDebug() << "nxomapoid:" << nxomapoid << "nxomapoffset:" << nxomapoffset;
        uint64_t omapbtreeoid = qFromLittleEndian<uint64_t>(curimg->ReadContent(nxomapoffset + 48, 8));
        uint32_t omapobjtype = qFromLittleEndian<uint32_t>(curimg->ReadContent(nxomapoffset + 24, 4));
        if(omapobjtype != 0x4000000b) // PHYSICAL OBJECT MAP
        {
            qDebug() << "error, not a valid object map..";
        }
        // ONCE I GET THE ABOVE RIGHT, I WILL NEED TO SET THE NEW SUPERBLOCK OFFSET, SO THE BELOW PULLS THE INFORMATION
        // FROM THE CORRECT SUPERBLOCK
        //qDebug() << "superblock checksum:" << CheckChecksum(curimg, curstartsector*512 + 8, qFromLittleEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 36, 4)) - 8, qFromLittleEndian<uint64_t>(curimg->ReadContent(curstartsector*512, 8)));
        out << "File System Type Int|7|Internal File System Type represented as an integer." << Qt::endl;
        out << "File System Type|APFS|File System Type String." << Qt::endl;
        //out << "Fletcher Checksum|" << QString::fromStdString(curimg->ReadContent(nxoffset, 8.toHex()).toStdString()) << "|Fletcher checksum value." << Qt::endl;
        out << "SuperBlock Offset|" << QString::number(nxoffset) << "|Byte offset to the current superblock." << Qt::endl;
        out << "Object ID|" << QString::number(qFromLittleEndian<uint64_t>(curimg->ReadContent(nxoffset + 8, 8))) << "|APFS object id." << Qt::endl;
        out << "Transaction ID|" << QString::number(qFromLittleEndian<uint64_t>(curimg->ReadContent(nxoffset + 16, 8))) << "|APFS transaction id." << Qt::endl;
        // NEED TO PROCESS PROPERLY AND DO AN IF & THING FOR IT...
        //out << "Object Type|" << QString::number(qFromLittleEndian<uint16_t>(curimg->ReadContent(nxoffset + 24, 2))) << "|APFS object type: 1 - container super block, 2 - btree, 3 btree node, 5 - spaceman, 11 - object map (OMAP), 13 - file system (volume super block), 17 - reaper." << Qt::endl;
        //out << "Object Flags|" << QString::number(qFromLittleEndian<uint16_t>(curimg->ReadContent(nxoffset + 26, 2))) << "|APFS object flags: 0 - OBJ_VIRTUAL, 80 - OBJ_EPHEMERAL, 40 - OBJ_PHYSICAL, 20 - OBJ_NOHEADER, 10 - OBJ_ENCRYPTED, 08 - OBJ_NONPERSISTENT." << Qt::endl;
        //out << "Object SubType|" << QString::number(qFromLittleEndian<uint32_t>(curimg->ReadContent(nxoffset + 28, 4))) << "|APFS object subtype: 0 - none, 11 - object map (OMAP), 14 - file system tree." << Qt::endl;
        out << "Block Size|" << QString::number(qFromLittleEndian<uint32_t>(curimg->ReadContent(nxoffset + 36, 4))) << "|APFS block size in bytes, usually 4096." << Qt::endl;
        //qDebug() << "block size:" << qFromLittleEndian<uint32_t>(curimg->ReadContent(nxoffset + 36, 4));
        out << "Block Count|" << QString::number(qFromLittleEndian<uint64_t>(curimg->ReadContent(nxoffset + 40, 8))) << "|Number of blocks for the AFPS container." << Qt::endl;
        // NEED TO DO AN IF & THING FOR THE 3 FEATURES SET AS WELL
        //out << "Container Features|" << QString::number(qFromLittleEndian<uint64_t>(curimg->ReadContent(nxoffset + 48, 8))) << "|Container features options uint64_t." << Qt::endl;
        //out << "Container Read-Only Compatible Features|" << QString::number(qFromLittleEndian<uint64_t>(curimg->ReadContent(nxoffset + 56, 8))) << "|Container read-only compatible features options uint64_t." << Qt::endl;
        //out << "Container Incompatible Features|" << QString::number(qFromLittleEndian<uint64_t>(curimg->ReadContent(nxoffset + 64, 8))) << "|Container incompatible features options uint64_t." << Qt::endl;
        out << "Container UUID|" << QString::fromStdString(curimg->ReadContent(nxoffset + 72, 16).toHex().toStdString()) << "|Container's universal unique id." << Qt::endl;
        // NEED TO DISPLAY THE CONTAINER ID IN THE PROPER FORMAT BELOW
	//qDebug() << "container uuid:" << (fsinfo.value("containeruuid").toString().left(8) + "-" + fsinfo.value("containeruuid").toString().mid(8, 4) + "-" + fsinfo.value("containeruuid").toString().mid(12, 4) + "-" + fsinfo.value("containeruuid").toString().mid(16, 4) + "-" + fsinfo.value("containeruuid").toString().right(12));
        out << "Next Object ID|" << QString::number(qFromLittleEndian<uint64_t>(curimg->ReadContent(nxoffset + 88, 8))) << "|Next object id." << Qt::endl;
        out << "Next Transaction ID|" << QString::number(qFromLittleEndian<uint64_t>(curimg->ReadContent(nxoffset + 96, 8))) << "|Next transaction id." << Qt::endl;
        //out << "CheckPoint Descriptor Flag|" << QString::fromStdString(curimg->ReadContent(nxoffset + 107, 1).toStdString()) << "|Flag for the checkpoint descriptor." << Qt::endl;
        out << "CheckPoint Description Blocks|" << QString::number(qFromLittleEndian<uint32_t>(curimg->ReadContent(nxoffset + 104, 4))) << "|Number of blocks used by the checkpoint descriptor area." << Qt::endl;
        out << "CheckPoint Descriptor Base|" << QString::number(qFromLittleEndian<int64_t>(curimg->ReadContent(nxoffset + 112, 8))) << "|Base address of the checkpoint descriptor area or the physical object identifier of a tree that contains address information." << Qt::endl;
        out << "Container Object Map Object ID|" << QString::number(qFromLittleEndian<uint64_t>(curimg->ReadContent(nxoffset + 160, 8))) << "|Object id for the container's object map." << Qt::endl;
        out << "Maximum Container Volumes|" << QString::number(qFromLittleEndian<uint32_t>(curimg->ReadContent(nxoffset + 180, 4))) << "|Maximum number of volumes in the APFS container." << Qt::endl;
        //qDebug() << "max containers:" << qFromLittleEndian<uint32_t>(curimg->ReadContent(nxoffset + 180, 4));
        out << "Volume Object ID List|";
        for(int i=0; i < qFromLittleEndian<uint32_t>(curimg->ReadContent(nxoffset + 180, 4)); i++)
        {
            uint64_t apfsvoloid = qFromLittleEndian<uint64_t>(curimg->ReadContent(nxoffset + 184 + i*8, 8));
            if(apfsvoloid > 0)
                out << QString::number(apfsvoloid) << ",";
            //qDebug() << QString("fs [" + QString::number(i) + "] objid:") << qFromLittleEndian<uint64_t>(curimg->ReadContent(nxoffset + 184 + i*8, 8));
        }
        // NEED TO GET THE PROPERTIES NX_KEYLOCKER FOR KEYBAG LOCATION AND NX_FLAGS FOR WHETHER IT IS SOFTWARE KEY
        out << "|List of object id's for each volume within the container." << Qt::endl;
        partitionname += "APFS Container [APFS]";
        fsinfo.insert("descblocks", QVariant(qFromLittleEndian<uint32_t>(partbuf.mid(104, 4))));
        fsinfo.insert("datablocks", QVariant(qFromLittleEndian<uint32_t>(partbuf.mid(108, 4))));
        fsinfo.insert("descbase", QVariant(qFromLittleEndian<qulonglong>(partbuf.mid(112, 8))));
        fsinfo.insert("database", QVariant(qFromLittleEndian<qulonglong>(partbuf.mid(120, 8))));
        fsinfo.insert("descnext", QVariant(qFromLittleEndian<uint32_t>(partbuf.mid(128, 4))));
        fsinfo.insert("datanext", QVariant(qFromLittleEndian<uint32_t>(partbuf.mid(132, 4))));
        fsinfo.insert("descindex", QVariant(qFromLittleEndian<uint32_t>(partbuf.mid(136, 4))));
        fsinfo.insert("desclen", QVariant(qFromLittleEndian<uint32_t>(partbuf.mid(140, 4))));
        fsinfo.insert("dataindex", QVariant(qFromLittleEndian<uint32_t>(partbuf.mid(144, 4))));
        fsinfo.insert("datalen", QVariant(qFromLittleEndian<uint32_t>(partbuf.mid(148, 4))));
        fsinfo.insert("maxfilesystems", QVariant(qFromLittleEndian<uint32_t>(partbuf.mid(180, 4))));
	qDebug() << "max filesystems:" << fsinfo.value("maxfilesystems").toUInt();
	//while(fsinfo.value("maxfilesystems").toUInt() < 100)
	for(uint i=0; i < fsinfo.value("maxfilesystems").toUInt(); i++)
	{
	    qDebug() << "fs entryid:" << qFromLittleEndian<qulonglong>(partbuf.mid(184 + i*8, 8));
	}
        qDebug() << "desc blocks:" << fsinfo.value("descblocks").toUInt() << "descbase:" << fsinfo.value("descbase").toUInt();
        qDebug() << "desc index:" << fsinfo.value("descindex").toUInt() << "desc length:" << fsinfo.value("descindex").toUInt();
	// to determine volume information for each volume...
	// libfsapfs_object_map_descriptor_t *object_map_descriptor = NULL;
	// libfsapfs_object_map_btree_get_descriptor_by_object_identifier(internal_container->object_map_btree,internal_container->file_io_handle,internal_container->superblock->volume_object_identifiers[ volume_index ],&object_map_descriptor,error )
	// file_offset = (off64_t) ( object_map_descriptor->physical_address * internal_container->io_handle->block_size )
    }
    else if(hfssig == "H+" || hfssig == "HX") // HFS+/HFSX
    {
        out << "File System Type Int|8|Internal File System Type represented as an integer." << Qt::endl;
        //out << "Volume Label|" << partitionname << "|Volume Label for the file system." << Qt::endl;

        // VOLUME LABEL IS THE FILE NAME FOR THE ROOT DIRECTORY, CATALOG ID: 2...
        // NEED TO GET THE CATALOG START BLOCK * BLOCK SIZE + (CNID - 1)*4096 = 991232 + (2-1)*4096 = 995328
        // THIS IS THE START OF THE CATALOG LEAF NODE FOR THE ROOT FOLDER (CNID 2)
        // FIRST 14 BYTES ARE NODE DESCRIPTOR [BYTE 8 IS THE KIND AND SHOULD BE 0xFF], [BYTE 9 IS THE HEIGHT AND SHOULD BE 1 FOR LEAF NODE]
        // NEXT 2 BYTES ARE THE KEY LENGTH, 4 BYTES ARE PARENT CNID [SHOULD BE 0x00000001]
        // NEXT 4 BYTES ARE THE DATA LENGTH NODE FOR THE KEY DATA, KEY DATA IS THE VOLUME NAME
        quint32 catalogstartoffset = qFromBigEndian<quint32>(curimg->ReadContent(curstartsector*512 + 1312, 4));
        //qDebug() << "catalog start offset:" << catalogstartoffset;
        quint64 catalogstartinbytes = catalogstartoffset * 4096 + 4096; // second 4096 is (2-1) * 4096
        //qDebug() << "catalog start offset for root folder:" << catalogstartinbytes << "should be <995328>";
        //qDebug() << "THE KIND:" << curimg->ReadContent(catalogstartinbytes + 8, 1).toHex();
        //qDebug() << "HEIGHT:" << curimg->ReadContent(catalogstartinbytes + 9, 1).toHex();
        quint32 keydatalength = qFromBigEndian<quint16>(curimg->ReadContent(catalogstartinbytes + 20, 2));
        //qDebug() << "KEY DATA LENGTH:" << keydatalength;
        for(uint8_t i=0; i < keydatalength; i++)
            partitionname += QString(QChar(qFromBigEndian<uint16_t>(curimg->ReadContent(catalogstartinbytes + 22 + i*2, 2))));
        if(hfssig == "H+")
        {
	    qInfo() << "HFS+ File System Found. Parsing...";
            partitionname += " [HFS+]";
            out << "File System Type|HFS+|File System Type String." << Qt::endl;
        }
        else if(hfssig == "HX")
        {
	    qInfo() << "HFSX File System Found. Parsing...";
            partitionname += " [HFSX]";
            out << "File System Type|HFSX|File System Type String." << Qt::endl;
        }
        // NEED TO CONVERT THE 2 DATES LISTED TO ACTUAL HUMAN READABLE DATES
        out << "Volume Creation Date|" << ConvertUnixTimeToString(ConvertHfsTimeToUnixTime(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1040, 4)))) << "|Creation Date of the volume stored in local time." << Qt::endl;
        out << "Last Modification Date|" << ConvertUnixTimeToString(ConvertHfsTimeToUnixTime(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1044, 4)))) << "|UTC Last modification date of the volume." << Qt::endl;
        out << "File Count|" << QString::number(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1056, 4))) << "|Number of files on the volume." << Qt::endl;
        out << "Folder Count|" << QString::number(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1060, 4))) << "|Number of folders on the volume." << Qt::endl;
        out << "Cluster Size|" << QString::number(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1064, 4))) << "|Allocation Cluster Size, usually 4096 bytes." << Qt::endl;
        out << "Next Catalog ID|" << QString::number(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1088, 4))) << "|Next available catalog ID (node id)." << Qt::endl;
        // ALLOCATION FILE INFORMATION
        out << "Allocation Logical Size|" << QString::number(qFromBigEndian<quint64>(curimg->ReadContent(curstartsector*512 + 1136, 8))) << "|Logical size for the allocation file." << Qt::endl;
        out << "Allocation Total Blocks|" << QString::number(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1148, 4))) << "|Total number of blocks for the allocation file." << Qt::endl;
        out << "Allocation Extents Start Block Array|";
        for(int i=0; i < 8; i++)
        {
            if(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1152 + i*8, 4)) > 0)
                out << QString::number(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1152 + i*8, 4))) << ",";
        }
        out << "|Start block for each extent for allocation file." << Qt::endl;
        out << "Allocation Extents Block Count Array|";
        for(int i=0; i < 8; i++)
        {
            if(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1156 + i*8, 4)) > 0)
                out << QString::number(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1156 + i*8, 4))) << ",";
        }
        out << "|Block count for each extent for allocation file." << Qt::endl;
        // EXTENTS OVERFLOW FILE INFORMATION
        out << "Extents Overflow Logical Size|" << QString::number(qFromBigEndian<quint64>(curimg->ReadContent(curstartsector*512 + 1216, 8))) << "|Logical size for the extents overflow file." << Qt::endl;
        out << "Extents Overflow Total Blocks|" << QString::number(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1228, 4))) << "|Total number of blocks for the extents overflow file." << Qt::endl;
        out << "Extents Overflow Extents Start Block Array|";
        for(int i=0; i < 8; i++)
        {
            if(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1232 + i*8, 4)) > 0)
                out << QString::number(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1232 + i*8, 4))) << ",";
        }
        out << "|Start block for each extent for extents overflow file." << Qt::endl;
        out << "Extents Overflow Extents Block Count Array|";
        for(int i=0; i < 8; i++)
        {
            if(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1236 + i*8, 4)) > 0)
                out << QString::number(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1236 + i*8, 4))) << ",";
        }
        out << "|Block count for each extent for extents overflow file." << Qt::endl;
        // CATALOG FILE INFORMATION
        out << "Catalog Logical Size|" << QString::number(qFromBigEndian<quint64>(curimg->ReadContent(curstartsector*512 + 1296, 8))) << "|Logical size for the catalog file." << Qt::endl;
        out << "Catalog Total Blocks|" << QString::number(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1308, 4))) << "|Total number of blocks for the Catalog file." << Qt::endl;
        out << "Catalog Extents Start Block Array|";
        for(int i=0; i < 8; i++)
        {
            if(qFromBigEndian<quint32>(curimg->ReadContent(curstartsector*512 + 1312 + i*8, 4)) > 0)
                out << QString::number(qFromBigEndian<quint32>(curimg->ReadContent(curstartsector*512 + 1312 + i*8, 4))) << ",";
        }
        out << "|Start block for each extent for Catalog file." << Qt::endl;
        out << "Catalog Extents Block Count Array|";
        for(int i=0; i < 8; i++)
        {
            if(qFromBigEndian<quint32>(curimg->ReadContent(curstartsector*512 + 1316 + i*8, 4)) > 0)
                out << QString::number(qFromBigEndian<quint32>(curimg->ReadContent(curstartsector*512 + 1316 + i*8, 4))) << ",";
        }
        out << "|Block count for each extent for Catalog file." << Qt::endl;

        // ATTRIBUTES FILE INFORMATION
        out << "Attributes Logical Size|" << QString::number(qFromBigEndian<quint64>(curimg->ReadContent(curstartsector*512 + 1376, 8))) << "|Logical size for the attributes file." << Qt::endl;
        out << "Attributes Total Blocks|" << QString::number(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1388, 4))) << "|Total number of blocks for the attributes file." << Qt::endl;
        out << "Attributes Extents Start Block Array|";
        for(int i=0; i < 8; i++)
        {
            if(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1392 + i*8, 4)) > 0)
                out << QString::number(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1392 + i*8, 4))) << ",";
        }
        out << "|Start block for each extent for attributes file." << Qt::endl;
        out << "Attributes Extents Block Count Array|";
        for(int i=0; i < 8; i++)
        {
            if(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1396 + i*8, 4)) > 0)
                out << QString::number(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1396 + i*8, 4))) << ",";
        }
        out << "|Block count for each extent for attributes file." << Qt::endl;
        // STARTUP FILE INFORMATION
        out << "Startup Logical Size|" << QString::number(qFromBigEndian<quint64>(curimg->ReadContent(curstartsector*512 + 1456, 8))) << "|Logical size for the startup file." << Qt::endl;
        out << "Startup Total Blocks|" << QString::number(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1468, 4))) << "|Total number of blocks for the startup file." << Qt::endl;
        out << "Startup Extents Start Block Array|";
        for(int i=0; i < 8; i++)
        {
            if(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1472 + i*8, 4)) > 0)
                out << QString::number(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1472 + i*8, 4))) << ",";
        }
        out << "|Start block for each extent for startup file." << Qt::endl;
        out << "Startup Extents Block Count Array|";
        for(int i=0; i < 8; i++)
        {
            if(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1476 + i*8, 4)) > 0)
                out << QString::number(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1476 + i*8, 4))) << ",";
        }
        out << "|Block count for each extent for startup file." << Qt::endl;
    }
    else if(xfssig == "XFSB") // XFS
    {
	qInfo() << "XFS File System Found. Parsing...";
        out << "File System Type Int|9|Internal File System Type represented as an integer." << Qt::endl;
        out << "File System Type|XFS|File System Type String." << Qt::endl;
        partitionname += QString::fromStdString(curimg->ReadContent(curstartsector*512 + 108, 12).toStdString());
        out << "Volume Label|" << partitionname << "|Volume Label for the file system." << Qt::endl;
        partitionname += " [XFS]";
        out << "Block Size|" << QString::number(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 4, 4))) << "|Size of block in bytes." << Qt::endl;
        out << "Data Blocks|" << QString::number(qFromBigEndian<quint64>(curimg->ReadContent(curstartsector*512 + 8, 8))) << "|Total number of blocks available for data." << Qt::endl;
        out << "Real Time Blocks|" << QString::number(qFromBigEndian<quint64>(curimg->ReadContent(curstartsector*512 + 16, 8))) << "|Number of blocks in the real time device." << Qt::endl;
        out << "Real Time Extents|" << QString::number(qFromBigEndian<quint64>(curimg->ReadContent(curstartsector*512 + 24, 8))) << "|Number of extents on the real time device." << Qt::endl;
        out << "UUID|" << QString::fromStdString(curimg->ReadContent(curstartsector*512 + 32, 16).toStdString()) << "Universal unique id for the file system." << Qt::endl;
        out << "Root Inode|" << QString::number(qFromBigEndian<quint64>(curimg->ReadContent(curstartsector*512 + 56, 8))) << "|Root inode number for the filesystem." << Qt::endl;
        out << "Allocation Group Blocks|" << QString::number(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 84, 4))) << "|Size of each allocation group in blocks." << Qt::endl;
        out << "Allocation Group Count|" << QString::number(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 88, 4))) << "|Number of allocation groups in the filesystem." << Qt::endl;
        out << "Inode Size|" << QString::number(qFromBigEndian<uint16_t>(curimg->ReadContent(curstartsector*512 + 104, 2))) << "|Size of an inode in bytes." << Qt::endl;
        out << "Inodes Per Block|" << QString::number(qFromBigEndian<uint16_t>(curimg->ReadContent(curstartsector*512 + 106, 2))) << "|Number of inodes per block." << Qt::endl;
    }
    else if(btrsig == "_BHRfS_M") // BTRFS
    {
	qInfo() << "BTRFS File System Found. Parsing...";
        out << "File System Type Int|10|Internal File System Type represented as an integer." << Qt::endl;
        out << "File System Type|BTRFS|File System Type String." << Qt::endl;
        partitionname += QString::fromStdString(curimg->ReadContent(curstartsector*512 + 65536 + 0x12b, 100).toStdString());
        out << "Volume Label|" << partitionname << "|Volume Label for the file system." << Qt::endl;
        partitionname += " [BTRFS]";
        int curoffset = 65536;
        fsinfo.insert("type", QVariant(10));
        fsinfo.insert("typestr", QVariant("BTRFS"));
        fsinfo.insert("rootaddr", QVariant(qFromLittleEndian<qulonglong>(partbuf.mid(curoffset + 0x50, 8))));
        fsinfo.insert("chunkrootaddr", QVariant(qFromLittleEndian<qulonglong>(partbuf.mid(curoffset + 0x58, 8))));
        fsinfo.insert("rootdirobjid", QVariant(qFromLittleEndian<qulonglong>(partbuf.mid(curoffset + 0x80, 8))));
        fsinfo.insert("sectorsize", QVariant(qFromLittleEndian<uint32_t>(partbuf.mid(curoffset + 0x90, 4))));
        fsinfo.insert("nodesize", QVariant(qFromLittleEndian<uint32_t>(partbuf.mid(curoffset + 0x94, 4))));
        fsinfo.insert("vollabel", QVariant(QString::fromStdString(partbuf.mid(curoffset + 0x12b, 100).toStdString())));
        qDebug() << "sectorsize:" << fsinfo.value("sectorsize").toUInt();
        qDebug() << "rootdirobjid:" << fsinfo.value("rootdirobjid").toUInt();
    }
    else if(btlsig == "-FVE-FS-") // BITLOCKER
    {
	qInfo() << "Bitlocker Encryption Found. Analyzing...";
        out << "File System Type Int|11|Internal File System Type represented as an integer." << Qt::endl;
        out << "File System Type|BITLOCKER|File System Type String." << Qt::endl;
        partitionname += QString::fromStdString(curimg->ReadContent(curstartsector*512 + 118, qFromLittleEndian<uint16_t>(curimg->ReadContent(curstartsector*512 + 112, 2)) - 8).toStdString());
        out << "Volume Label|" << partitionname << "|Volume Label for the file system." << Qt::endl;
        partitionname += " [BITLOCKER]";
        // HERE IS WHERE I WILL GET THE KEY(S) INFORMATION AND ALSO GET THE USER PASSWORD AND/OR RECOVERY KEY FROM THE EXAMINER
        // PLACE THAT PROMPT HERE BASED ON THE KEY INFORMATION I READ FROM THE ENCRYPTED VOLUME
        // ATTEMPTED TO USE LIBBDE.H, BUT IT REQUIRES A FILE, AND I DON'T HAVE A FILE PER SE
        // NEED AN IMAGE TO TEST BITLOCKER
        fsinfo.insert("type", QVariant(11));
        fsinfo.insert("typestr", QVariant("BITLOCKER"));
        fsinfo.insert("metadatasize", QVariant(qFromLittleEndian<uint16_t>(partbuf.mid(8, 2))));
        fsinfo.insert("meta1offset", QVariant(qFromLittleEndian<qulonglong>(partbuf.mid(32, 8))));
        fsinfo.insert("meta2offset", QVariant(qFromLittleEndian<qulonglong>(partbuf.mid(40, 8))));
        fsinfo.insert("meta3offset", QVariant(qFromLittleEndian<qulonglong>(partbuf.mid(48, 8))));
        fsinfo.insert("mftmirrorcluster", QVariant(qFromLittleEndian<qulonglong>(partbuf.mid(56, 8))));
        fsinfo.insert("sizeminusheader", QVariant(qFromLittleEndian<uint32_t>(partbuf.mid(64, 4))));
        fsinfo.insert("nextcounter", QVariant(qFromLittleEndian<uint32_t>(partbuf.mid(96, 4))));
        fsinfo.insert("algorithm", QVariant(qFromLittleEndian<uint32_t>(partbuf.mid(100, 4))));
        fsinfo.insert("timeenabled", QVariant(qFromLittleEndian<qulonglong>(partbuf.mid(104, 8))));
        fsinfo.insert("volnamelength", QVariant(qFromLittleEndian<uint16_t>(partbuf.mid(112, 2))));
        fsinfo.insert("vollabel", QVariant(QString::fromStdString(partbuf.mid(118, fsinfo.value("volnamelength").toUInt() - 8).toStdString())));
        qDebug() << "When Bitlocker was Enabled:" << ConvertWindowsTimeToUnixTimeUTC(fsinfo.value("timeenabled").toULongLong());
        uint encryptalgo = fsinfo.value("algorithm").toUInt();
        if(encryptalgo == 0x1000)
            qDebug() << "stretch";
        else if(encryptalgo >= 0x2000 || encryptalgo <= 0x2005)
            fsinfo.insert("algoname", QVariant("256-bit AES-CCM"));
        else if(encryptalgo == 0x8000)
            fsinfo.insert("algoname", QVariant("128-bit AES + Elephant"));
        else if(encryptalgo == 0x8001)
            fsinfo.insert("algoname", QVariant("256-bit AES + Elephant"));
        else if(encryptalgo == 0x8002)
            fsinfo.insert("algoname", QVariant("128-bit AES"));
        else if(encryptalgo == 0x8003)
            fsinfo.insert("algoname", QVariant("256-bit AES"));
        else
            fsinfo.insert("algoname", QVariant("NOT DEFINED"));
    }
    else if(bfssig == "1SFB") // BFS
    {
	qInfo() << "BFS File System Found. Parsing...";
        out << "File System Type Int|12|Internal File System Type represented as an integer." << Qt::endl;
        out << "File System Type|BFS|File System Type String." << Qt::endl;
        partitionname += QString::fromStdString(curimg->ReadContent(curstartsector*512 + 512, 32).toStdString());
        out << "Volume Label|" << partitionname << "|Volume Label for the file system." << Qt::endl;
	uint32_t blocksize = qFromLittleEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 552, 4));
	//qDebug() << "blocksize:" << blocksize;
	out << "Block Size|" << QString::number(blocksize) << "|Size in bytes for a file system block." << Qt::endl;
        out << "Block Shift|" << QString::number(qFromLittleEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 556, 4))) << "|Number of bits needed to shift a block number by to get a byte address." << Qt::endl;
	out << "Number of Blocks|" << QString::number(qFromLittleEndian<uint64_t>(curimg->ReadContent(curstartsector*512 + 560, 8))) << "|Number of blocks in the file system." << Qt::endl;
	out << "Used Blocks|" << QString::number(qFromLittleEndian<uint64_t>(curimg->ReadContent(curstartsector*512 + 568, 8))) << "|Number of blocks in use by the file system." << Qt::endl;
	out << "Inode Size|" << QString::number(qFromLittleEndian<int32_t>(curimg->ReadContent(curstartsector*512 + 576, 4))) << "|Size in bytes for an inode." << Qt::endl;
	out << "Blocks per Allocation Group|" << QString::number(qFromLittleEndian<int32_t>(curimg->ReadContent(curstartsector*512 + 584, 4))) << "|Number of blocks in each allocation group." << Qt::endl;
        out << "Allocation Shift|" << QString::number(qFromLittleEndian<int32_t>(curimg->ReadContent(curstartsector*512 + 588, 4))) << "|Number of bits to shift an allocation group number by when converting a block run address to a byte offset." << Qt::endl;
        out << "Number of Allocation Groups|" << QString::number(qFromLittleEndian<int32_t>(curimg->ReadContent(curstartsector*512 + 592, 4))) << "|Number of allocation groups in the file system." << Qt::endl;
        // need to implement properly...
        //out << "Flags|" << QString::number(qFromLittleEndian<int32_t>(curimg->ReadContent(curstartsector*512 + 596, 4))) << "|What flags means here.." << Qt::endl;
        out << "Root Directory Allocation Group|" << QString::number(qFromLittleEndian<int32_t>(curimg->ReadContent(curstartsector*512 + 628, 4))) << "|Allocation group for the root directory." << Qt::endl;
	out << "Root Directory Start Block|" << QString::number(qFromLittleEndian<uint16_t>(curimg->ReadContent(curstartsector*512 + 632, 2))) << "|Starting block number for the root directory." << Qt::endl;
        out << "Root Indices Allocation Group|" << QString::number(qFromLittleEndian<int32_t>(curimg->ReadContent(curstartsector*512 + 636, 4))) << "|Allocation group for the root directory indices." << Qt::endl;
	out << "Root Indices Start Block|" << QString::number(qFromLittleEndian<uint16_t>(curimg->ReadContent(curstartsector*512 + 640, 2))) << "|Starting block number for the root indices." << Qt::endl;
        partitionname += " [BFS]";
        fsinfo.insert("vollabel", QVariant(QString::fromStdString(partbuf.mid(512, 32).toStdString())));
        fsinfo.insert("fsbyteorder", QVariant(qFromLittleEndian<int32_t>(partbuf.mid(548, 4))));
        //qDebug() << "fsbyteorder:" << QString::number(fsinfo.value("fsbyteorder").toInt(), 16);
    }
    else if(f2fsig == 0xf2f52010) // F2FS
    {
	qInfo() << "F2FS File System Found. Parsing not yet implemented.";
        out << "File System Type Int|13|Internal File System Type represented as an integer." << Qt::endl;
        out << "File System Type|F2FS|File System Type String." << Qt::endl;
        //out << "Volume Label|" << partitionname << "|Volume Label for the file system." << Qt::endl;
        partitionname += " [F2FS]";
    }
    else if(isosig == "CD001" && udfsig != "BEA01") // ISO9660
    {
	qInfo() << "ISO9660 File System Found. Parsing...";
        out << "File System Type Int|14|Internal File System Type represented as an integer." << Qt::endl;
        out << "File System Type|ISO9660|File System Type String." << Qt::endl;
	int8_t pvindx = 1;
	int8_t svdindx = 1;
	int8_t vpdindx = 1;
	int8_t brindx = 1;
	// NEED A FOR LOOP, WHICH STARTS AT BLOCK 16 AND GOES BY 2048 UNTIL WE GET TO THE VOLUME TERMINATOR WITH TYPE FF
	for(int i=16; i < (curimg->Size() / 2048) - 15; i++)
	{
	    uint64_t curoffset = curstartsector*512 + 2048*i;
	    uint8_t voldesctype = qFromLittleEndian<uint8_t>(curimg->ReadContent(curoffset, 1));
	    if(voldesctype == 0x00) // BOOT RECORD
	    {
		//qDebug() << "Boot Record";
		out << "BR" << QString::number(brindx) << " Volume Descriptor Type|" << QString::number(qFromLittleEndian<uint8_t>(curimg->ReadContent(curoffset, 1))) << "|Value for voluem descriptor type, 0 - Boot Record, 1 - Primary, 2 - Supplementary or Enhanced, 3 - Partition, 4-254 - Reserved, 255 - Set Terminator." << Qt::endl;
		out << "BR" << QString::number(brindx) << " Boot System Identifier|" << QString::fromStdString(curimg->ReadContent(curoffset + 7, 31).toStdString()) << "|System identification which can recognize and act upon the boot system use fields." << Qt::endl;
		out << "BR" << QString::number(brindx) << " Boot Indentifier|" << QString::fromStdString(curimg->ReadContent(curoffset + 39, 31).toStdString()) << "|Identification of the boot system." << Qt::endl;
		brindx++;
	    }
	    else if(voldesctype == 0x01) // PRIMARY VOLUME DESCRIPTOR
	    {
		//qDebug() << "Primary Volume Descriptor" << pvindx;
		// Primary Volume Descriptor
		//out << "Primary Volume Descriptor|" << QString::number(pvindx) << "|Identifier for this primary volume descriptor." << Qt::endl;
		out << "PV" << QString::number(pvindx) << " Volume Descriptor Type|" << QString::number(qFromLittleEndian<uint8_t>(curimg->ReadContent(curoffset, 1))) << "|Value for volume descriptor type, 0 - Boot Record, 1 - Primary, 2 - Supplementary or Enhanced, 3 - Partition, 4-254 - Reserved, 255 - Set Terminator." << Qt::endl;
		partitionname += QString::fromStdString(curimg->ReadContent(curoffset + 40, 31).toStdString());
		out << "PV" << QString::number(pvindx) << " Volume Label|" << partitionname << "|Name of the volume." << Qt::endl;
		partitionname += " [ISO9660]";
		out << "PV" << QString::number(pvindx) << " Volume Space Size|" << QString::number(qFromLittleEndian<uint32_t>(curimg->ReadContent(curoffset + 80, 4))) << "|Number of logical blocks in which the volume space is recorded." << Qt::endl;
		out << "PV" << QString::number(pvindx) << " Volume Set Size|" << QString::number(qFromLittleEndian<uint16_t>(curimg->ReadContent(curoffset + 120, 2))) << "|Volume set size of the volume in bytes." << Qt::endl;
		out << "PV" << QString::number(pvindx) << " Volume Sequence Number|" << QString::number(qFromLittleEndian<uint16_t>(curimg->ReadContent(curoffset + 124, 2))) << "|Ordinal number of the volume in the volume set which the volume is a member." << Qt::endl;
		out << "PV" << QString::number(pvindx) << " Logical Block Size|" << QString::number(qFromLittleEndian<uint16_t>(curimg->ReadContent(curoffset + 128, 2))) << "|Size in bytes of a logical block." << Qt::endl;
		out << "PV" << QString::number(pvindx) << " Path Table Size|" << QString::number(qFromLittleEndian<uint32_t>(curimg->ReadContent(curoffset + 132, 4))) << "|Length in bytes of a recorded occurence of the path table identified by the volume descriptor." << Qt::endl;
		out << "PV" << QString::number(pvindx) << " Location of Occurrence of Type L Path Table|" << QString::number(qFromLittleEndian<uint32_t>(curimg->ReadContent(curoffset + 140, 4))) << "|Logical block number of the first logical block allocated to the extent which contains an occurrence of the path table." << Qt::endl;
		out << "PV" << QString::number(pvindx) << " Location of Optional Occurrence of Type L Path Table|" << QString::number(qFromLittleEndian<uint32_t>(curimg->ReadContent(curoffset + 144, 4))) << "|Logical block number of the first logical block allocated to the extent which contains an optional occurence of the path table. If 0, it means the extent shall not be expected to be recorded." << Qt::endl;
		// Dir Record for Root Directory - 34 bytes
		out << "PV" << QString::number(pvindx) << " Root Directory Record Length|" << QString::number(qFromLittleEndian<uint8_t>(curimg->ReadContent(curoffset + 156, 1))) << "|Length in bytes of the root directory record." << Qt::endl;
		out << "PV" << QString::number(pvindx) << " Extended Attribute Record Length|" << QString::number(qFromLittleEndian<uint8_t>(curimg->ReadContent(curoffset + 157, 1))) << "|Length in bytes of the extended attribute record, if recorded, otherwise 0." << Qt::endl;
		out << "PV" << QString::number(pvindx) << " Extent Location|" << QString::number(qFromLittleEndian<uint32_t>(curimg->ReadContent(curoffset + 158, 4))) << "|Logical block number of the first logical block allocated to the extent." << Qt::endl;
		out << "PV" << QString::number(pvindx) << " Data Length|" << QString::number(qFromLittleEndian<uint32_t>(curimg->ReadContent(curoffset + 166, 4))) << "|Length in bytes of the data for the file section." << Qt::endl;
		uint16_t recyear = 1900 + qFromLittleEndian<uint8_t>(curimg->ReadContent(curoffset + 174, 1));
		uint8_t recmonth = qFromLittleEndian<uint8_t>(curimg->ReadContent(curoffset + 175, 1));
		uint8_t recday = qFromLittleEndian<uint8_t>(curimg->ReadContent(curoffset + 176, 1));
		uint8_t rechr = qFromLittleEndian<uint8_t>(curimg->ReadContent(curoffset + 177, 1));
		uint8_t recmin = qFromLittleEndian<uint8_t>(curimg->ReadContent(curoffset + 178, 1));
		uint8_t recsec = qFromLittleEndian<uint8_t>(curimg->ReadContent(curoffset + 179, 1));
		uint8_t recutc = qFromLittleEndian<int8_t>(curimg->ReadContent(curoffset + 180, 1));
		// skipped recutc for now.
		out << "PV" << QString::number(pvindx) << " Recording Date and Time|" << QString(QString::number(recmonth) + "/" + QString::number(recday) + "/" + QString::number(recyear) + " " + QString::number(rechr) + ":" + QString::number(recmin) + ":" + QString::number(recsec)) << "|Date and time which the information in the extent of the directory record was recorded." << Qt::endl;
		// NEED TO FIX THE FILE FLAGS SO IT READS PROPERLY...
		out << "PV" << QString::number(pvindx) << " File Flags|" << QString::number(qFromLittleEndian<uint8_t>(curimg->ReadContent(curoffset + 181, 1)), 2) << "|Flags for the file." << Qt::endl;
		out << "PV" << QString::number(pvindx) << " File Unit Size|" << QString::number(qFromLittleEndian<uint8_t>(curimg->ReadContent(curoffset + 182, 1))) << "|Assigned file unit size for the file section if the file section is recorded in interleaved mode, otherwise 0." << Qt::endl;
		//out << "PV" << QString::number(pvindx) << " Volume Set Identifier|" << QString::fromStdString(curimg->ReadContent(curoffset + 190, 128)) << "|complicatted." << Qt::end;
		out << "PV" << QString::number(pvindx) << " Publisher|" << QString::fromStdString(curimg->ReadContent(curoffset + 318, 128).toStdString()) << "|User who specified what should be recorded." << Qt::endl;
		out << "PV" << QString::number(pvindx) << " Data Preparer|" << QString::fromStdString(curimg->ReadContent(curoffset + 446, 128).toStdString()) << "|Person or other entity which controls prepration of the data to be recorded." << Qt::endl;
		out << "PV" << QString::number(pvindx) << " Application|" << QString::fromStdString(curimg->ReadContent(curoffset + 574, 128).toStdString()) << "|How the data was recorded." << Qt::endl;
		// skipped copyright, abstract, and bilbiographic (112 bytes)
		// creation date, skipped hundreths of a second and the utc offset
		out << "PV" << QString::number(pvindx) << " Volume Creation Date|" << QString::fromStdString(curimg->ReadContent(curoffset + 813, 4).toStdString()) << "-" << QString::fromStdString(curimg->ReadContent(curoffset + 817, 2).toStdString()) << "-" << QString::fromStdString(curimg->ReadContent(curoffset + 819, 2).toStdString()) << " " << QString::fromStdString(curimg->ReadContent(curoffset + 821, 2).toStdString()) << ":" << QString::fromStdString(curimg->ReadContent(curoffset + 823, 2).toStdString()) << ":" << QString::fromStdString(curimg->ReadContent(curoffset + 825, 2).toStdString()) << "|Creation date and time." << Qt::endl;
		out << "PV" << QString::number(pvindx) << " Volume Modification Date|" << QString::fromStdString(curimg->ReadContent(curoffset + 830, 4).toStdString()) << "-" << QString::fromStdString(curimg->ReadContent(curoffset + 834, 2).toStdString()) << "-" << QString::fromStdString(curimg->ReadContent(curoffset + 836, 2).toStdString()) << " " << QString::fromStdString(curimg->ReadContent(curoffset + 838, 2).toStdString()) << ":" << QString::fromStdString(curimg->ReadContent(curoffset + 840, 2).toStdString()) << ":" << QString::fromStdString(curimg->ReadContent(curoffset + 842, 2).toStdString()) << "|Modification date and time." << Qt::endl;
		pvindx++;
	    }
	    else if(voldesctype == 0x02) // SUPPLEMENTARY/ENHANCED VOLUME DESCRIPTOR
	    {
		//qDebug() << "Supplementary/Enhanced Volume Descriptor" << svdindx;
		out << "SV" << QString::number(svdindx) << " Volume Descriptor Type|" << QString::number(qFromLittleEndian<uint8_t>(curimg->ReadContent(curoffset, 1))) << "|Value for volume descriptor type, 0 - Boot Record, 1 - Primary, 2 - Supplementary or Enhanced, 3 - Partition, 4-254 - Reserved, 255 - Set Terminator." << Qt::endl;
		out << "SV" << QString::number(svdindx) << " Volume Descriptor Version|" << QString::number(qFromLittleEndian<uint8_t>(curimg->ReadContent(curoffset + 6, 1))) << "|Value for volume descriptor version, 1 - Supplementary, 2 - Enhanced." << Qt::endl;
		out << "SV" << QString::number(svdindx) << " Escape Sequences|";
		uint32_t escapeseq = qFromLittleEndian<uint32_t>(curimg->ReadContent(curoffset + 88, 4));
		if(escapeseq == 0x00452f25)
		    out << "Joliet Name Encoding: USC-2 Level 3";
		else if(escapeseq == 0x00432f25)
		    out << "Joliet Name Encoding: USC-2 Level 2";
		else if(escapeseq == 0x00402f25)
		    out << "Joliet Name Encoding: USC-2 Level 1";
		out << "|Escape sequence which defines standard. Joliet Level 1: %/@, Level 2: %/C, Level 3: %/E." << Qt::endl;
		out << "SV" << QString::number(svdindx) << " Path Table Size|" << QString::number(qFromLittleEndian<uint32_t>(curimg->ReadContent(curoffset + 132, 4))) << "|Length in bytes of a recorded occurrence of the path table identified by this volume descriptor." << Qt::endl;
		out << "SV" << QString::number(svdindx) << " Location of Occurrence of Type L Path Table|" << QString::number(qFromLittleEndian<uint32_t>(curimg->ReadContent(curoffset + 140, 4))) << "|Logical block number of the first logical block allocated to the extent which  contains an occurrence of the path table." << Qt::endl;
		out << "SV" << QString::number(svdindx) << " Location of Optional Occurrence of Type L Path Table|" << QString::number(qFromLittleEndian<uint32_t>(curimg->ReadContent(curoffset + 144, 4))) << "|Logical block number of the first logical block allocated to the extent which contains an optional occurence of the path table. If 0, it means the extent shall not be expected to be recorded." << Qt::endl;
		// Dir Record for Root Directory - 34 bytes
		out << "SV" << QString::number(svdindx) << " Root Directory Record Length|" << QString::number(qFromLittleEndian<uint8_t>(curimg->ReadContent(curoffset + 156, 1))) << "|Length in bytes of the root directory record." << Qt::endl;
		out << "SV" << QString::number(svdindx) << " Extent Location|" << QString::number(qFromLittleEndian<uint32_t>(curimg->ReadContent(curoffset + 158, 4))) << "|Logical block number of the first logical block allocated to the extent." << Qt::endl;
		out << "SV" << QString::number(svdindx) << " Data Length|" << QString::number(qFromLittleEndian<uint32_t>(curimg->ReadContent(curoffset + 166, 4))) << "|Length in bytes of the data for the file section." << Qt::endl;
		svdindx++;
	    }
	    else if(voldesctype == 0x03) // VOLUME PARTITION DESCRIPTOR
	    {
		qDebug() << "Volume Partition Descriptor" << vpdindx;
		out << "VP" << QString::number(vpdindx) << " Volume Partition Identifier|" << QString::fromStdString(curimg->ReadContent(curoffset + 40, 31).toStdString()) << "|Indentification of the volume partition." << Qt::endl;
		out << "VP" << QString::number(vpdindx) << " Volume Partition Location|" << QString::number(qFromLittleEndian<uint32_t>(curimg->ReadContent(curoffset + 72, 4))) << "|Logical block number of the first logical block allocated to the volume partition." << Qt::endl;
		out << "VP" << QString::number(vpdindx) << " Volume Partition Size|" << QString::number(qFromLittleEndian<uint32_t>(curimg->ReadContent(curoffset + 80, 4))) << "|Number of logical blocks in which the volume partition is recorded." << Qt::endl;
		vpdindx++;
	    }
	    else if(voldesctype == 0xFF) // VOLUME DESCRIPTOR SET TERMINATOR
	    {
		//qDebug() << "Volume Descriptor Set Terminator";
	    }
	    if(voldesctype == 0xFF)
	    {
		//qDebug() << "i:" << i;
		break;
	    }
	}
	out << "Primary Volume Count|" << QString::number(pvindx - 1) << "|Number of Primary volumes." << Qt::endl;
	out << "Supplementary/Enhanced Volume Count|" << QString::number(svdindx - 1) << "|Number of Supplemnentary/Enhanded volumes." << Qt::endl;
    }
    else if(isosig == "CD001" && udfsig == "BEA01") // UDF
    {
	qInfo() << "UDF File System Found. Parsing not yet implemented.";
        out << "File System Type Int|15|Internal File System Type represented as an integer." << Qt::endl;
	out << "File System Type|UDF|File System Type String." << Qt::endl;
        //out << "Volume Label|" << partitionname << "|Volume Label for the file system." << Qt::endl;
        partitionname += " [UDF]";
    }
    else if(hfssig == "BD") // legacy HFS
    {
	qInfo() << "HFS File System Found. Parsing not yet implemented.";
        out << "File System Type Int|16|Internal File System Type represented as an integer." << Qt::endl;
        out << "File System Type|HFS|File System Type String." << Qt::endl;
        uint8_t volnamelength = qFromBigEndian<uint8_t>(curimg->ReadContent(curstartsector*512 + 1060, 1));
        partitionname += QString::fromStdString(curimg->ReadContent(curstartsector*512 + 1061, volnamelength).toStdString());
        partitionname += " [HFS]";
        out << "Volume Label|" << partitionname << "|Volume Label for the file system." << Qt::endl;
        
        // NEED TO CONVERT THE 2 DATES LISTED TO ACTUAL HUMAN READABLE DATES
        out << "Volume Creation Date|" << QString::number(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1040, 4))) << "|Creation Date of the volume stored in local time." << Qt::endl;
        out << "Last Modification Date|" << QString::number(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1044, 4))) << "|UTC Last modification date of the volume." << Qt::endl;
        out << "File Count|" << QString::number(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1056, 4))) << "|Number of files on the volume." << Qt::endl;
        out << "Folder Count|" << QString::number(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1060, 4))) << "|Number of folders on the volume." << Qt::endl;
        out << "Cluster Size|" << QString::number(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1064, 4))) << "|Allocation Cluster Size, usually 4096 bytes." << Qt::endl;
        out << "Next Catalog ID|" << QString::number(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1088, 4))) << "|Next available catalog ID (node id)." << Qt::endl;
        out << "Catalog Logical Size|" << QString::number(qFromBigEndian<quint64>(curimg->ReadContent(curstartsector*512 + 1296, 8))) << "|Logical size for the catalog file." << Qt::endl;
        out << "Catalog Total Blocks|" << QString::number(qFromBigEndian<uint32_t>(curimg->ReadContent(curstartsector*512 + 1308, 4))) << "|Total number of blocks for the Catalog file." << Qt::endl;
        out << "Catalog Extents Start Block Array|";
        for(int i=0; i < 8; i++)
        {
            if(qFromBigEndian<quint32>(curimg->ReadContent(curstartsector*512 + 1312 + i*8, 4)) > 0)
                out << QString::number(qFromBigEndian<quint32>(curimg->ReadContent(curstartsector*512 + 1312 + i*8, 4))) << ",";
        }
        out << "|Start block for each extent for Catalog file." << Qt::endl;
        out << "Catalog Extents Block Count Array|";
        for(int i=0; i < 8; i++)
        {
            if(qFromBigEndian<quint32>(curimg->ReadContent(curstartsector*512 + 1316 + i*8, 4)) > 0)
                out << QString::number(qFromBigEndian<quint32>(curimg->ReadContent(curstartsector*512 + 1316 + i*8, 4))) << ",";
        }
        out << "|Block count for each extent for Catalog file." << Qt::endl;
    }
    else if(zfssig == 0x00bab10c) // ZFS
    {
	qInfo() << "ZFS File System Found. Parsing not yet implemeneted.";
        out << "File System Type Int|17|Internal File System Type represented as an integer." << Qt::endl;
        out << "File System Type|ZFS|File System Type String." << Qt::endl;
        partitionname += " [ZFS]";
    }
    else if(refsig == 0x5265465300000000) // ReFS
    {
	qInfo() << "ReFS File System Found. Parsing not yet implemeneted.";
        out << "File System Type Int|18|Internal File System Type represented as an integer." << Qt::endl;
        out << "File System Type|RES|File System Type String." << Qt::endl;
        //out << "Volume Label|" << partitionname << "|Volume Label for the file system." << Qt::endl;
        partitionname += " [REFS]";
    }
    else if(f2fsig == 0xe0f5e1e2) // EROFS
    {
	qInfo() << "EROFS File System Found. Parsing not yet implemeneted.";
        out << "File System Type Int|19|Internal File System Type represented as an integer." << Qt::endl;
        out << "File System Type|EROFS|File System Type String." << Qt::endl;
        partitionname += " [EROFS]";
    }
    else if(bcfsig1 == 0xc68573f64e1a45ca && bcfsig2 == 0x8265f57f48ba6d81) // BCACHEFS
    {
	qInfo() << "BCACHEFS File System Found. Parsing not yet implemeneted.";
	out << "File System Type Int|20|Internal File System Type represented as an integer." << Qt::endl;
	out << "File System Type|BCACHEFS|File System Type String." << Qt::endl;
	partitionname += "[BCACHEFS]";
    }
    else if(zonesig == 0x5a4f4653) // ZONEFS
    {
        qInfo() << "ZoneFS File System Found. Parsing not yet implemented.";
        out << "File System Type Int|21|Internal File System Type represented as an integer." << Qt::endl;
        out << "File System Type|ZONEFS|File System Type String." << Qt::endl;
        partitionname += "[ZONEFS]";
    }
    // need to implement bfs-12, udf-15, hfs-16, zfs-17, refs-18, erofs-19, bcachefs-20, zonefs-21
    out.flush();
    propfile.close();

    return partitionname;
}

     *
     */ 

/*
void HashFile(std::string filename, std::string whlfile)
{
    std::ifstream fin(filename.c_str());
    char tmpchar[65536];
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    while(fin)
    {
        fin.read(tmpchar, 65536);
        size_t cnt = fin.gcount();
        blake3_hasher_update(&hasher, tmpchar, cnt);
        if(!cnt)
            break;
    }
    uint8_t output[BLAKE3_OUT_LEN];
    blake3_hasher_finalize(&hasher, output, BLAKE3_OUT_LEN);
    std::stringstream ss;
    for(int i=0; i < BLAKE3_OUT_LEN; i++)
        ss << std::hex << (int)output[i];
    std::string srcmd5 = ss.str();
    std::string whlstr = srcmd5 + "," + filename + "\n";
    FILE* whlptr = NULL;
    whlptr = fopen(whlfile.c_str(), "a");
    fwrite(whlstr.c_str(), strlen(whlstr.c_str()), 1, whlptr);
    fclose(whlptr);
}
 */ 

/*
 * GET LIST OF MOUNT POINTS BY DEVICE MNTPT
#include <stdio.h>
#include <stdlib.h>
#include <mntent.h>

int main(void)
{
  struct mntent *ent;
  FILE *aFile;

  aFile = setmntent("/proc/mounts", "r");
  if (aFile == NULL) {
    perror("setmntent");
    exit(1);
  }
  while (NULL != (ent = getmntent(aFile))) {
    printf("%s %s\n", ent->mnt_fsname, ent->mnt_dir);
  }
  endmntent(aFile);
}
*/

/* c++ EXAMPLE TO GET MNTPT AND COMPARE TO PROVIDED PATH STRING
 * CAN MODIFY THIS TO USE STRING.CONTAINS RATHER THAN ==

#include <string_view>
#include <fstream>
#include <optional>

std::optional<std::string> get_device_of_mount_point(std::string_view path)
{
   std::ifstream mounts{"/proc/mounts"};
   std::string mountPoint;
   std::string device;

   while (mounts >> device >> mountPoint)
   {
      if (mountPoint == path)
      {
         return device;
      }
   }

   return std::nullopt;
}
if (const auto device = get_device_of_mount_point("/"))
   std::cout << *device << "\n";
else
   std::cout << "Not found\n";

*/

int main(int argc, char* argv[])
{
    uint8_t isrecursive = 0;
    uint8_t ishash = 0;
    uint8_t isoutput = 0;
    uint8_t iswithslack = 0;

    std::string outfile = "";

    std::string imagepath;
    std::string logpath;
    std::vector<std::filesystem::path> filevector;
    filevector.clear();
    std::vector<std::filesystem::path> filelist;
    filelist.clear();
    std::vector<std::string> fileinfovector;
    fileinfovector.clear();

    if(argc == 1)
    {
	ShowUsage(0);
	return 1;
    }

    int i;
    while((i=getopt(argc, argv, "cho:rwV")) != -1)
    {
        switch(i)
        {
            case 'h':
                ShowUsage(0);
                return 1;
            case 'V':
                ShowUsage(1);
                return 1;
	    case 'c':
                ishash = 0;
		printf("Calculate hash and store in logical image.\n");
		break;
	    case 'r':
		isrecursive = 1;
		break;
            case 'o':
                isoutput = 1;
                outfile = optarg;
                break;
            case 'w':
                iswithslack = 1;
                break;
        }
    }
    // get all the input strings... then run parse directory to get the list of files...
    for(int i=optind; i < argc; i++)
    {
        filevector.push_back(std::filesystem::canonical(argv[i]));
    }
    // recursively parse the files/dirs and add them to the correct list
    for(int i=0; i < filevector.size(); i++)
    {
	if(std::filesystem::is_regular_file(filevector.at(i)))
	{
	    filelist.push_back(filevector.at(i));
	}
	else if(std::filesystem::is_directory(filevector.at(i)))
	{
	    if(isrecursive)
		ParseDirectory(filevector.at(i), &filelist);
	    else
		printf("Directory %s skipped. Use -r to recurse directory\n", filevector.at(i).c_str());
	}
    }
    // Determine file info needed to forensically parse the file
    for(int i=0; i < filelist.size(); i++)
    {
	std::ifstream mounts{"/proc/mounts"};
	std::string mntpt;
	std::string device;
	std::string fileinfo;
	while(mounts >> device >> mntpt)
	{
	    std::size_t found = filelist.at(i).string().find(mntpt);
	    if(found != std::string::npos)
	    {
		fileinfo = filevector.at(i).string() + "|" + mntpt + "|" + device;
	    }
	}
	//std::cout << fileinfo << "\n";
	fileinfovector.push_back(fileinfo);
    }

    // Begin forensic parsing for each file
    for(int i=0; i < fileinfovector.size(); i++)
    {
        int fstype = -1;
        std::string filename;
	std::string devicestr;
        std::string mntptstr;
        std::size_t lfound = fileinfovector.at(i).find("|");
        filename = fileinfovector.at(i).substr(0, lfound);
	std::size_t rfound = fileinfovector.at(i).rfind("|");
        mntptstr = fileinfovector.at(i).substr(lfound+1, rfound - lfound-1);
	devicestr = fileinfovector.at(i).substr(rfound+1);
        // GET FILE SYSTEM TYPE
	DetermineFileSystem(devicestr, &fstype);
        //std::cout << filename << " " << fstype << std::endl;
        switch(fstype)
        {
            case 0:
                //std::cout << "EXTFS\n";
                ParseExtForensics(filename, mntptstr, devicestr);
                break;
            case 1:
                //std::cout << "FAT12\n";
                ParseFatForensics(filename, mntptstr, devicestr, 1);
                break;
            case 2:
                //std::cout << "FAT16\n";
		ParseFatForensics(filename, mntptstr, devicestr, 2);
                break;
            case 3:
                //std::cout << "FAT32\n";
		ParseFatForensics(filename, mntptstr, devicestr, 3);
                break;
            case 4:
                //std::cout << "EXFAT\n";
		ParseFatForensics(filename, mntptstr, devicestr, 4);
                break;
            case 5:
                //std::cout << "NTFS\n";
                ParseNtfsForensics(filename, mntptstr, devicestr, 5);
                break;
            case 6:
                std::cout << "APFS\n";
                break;
            case 7:
                std::cout << "HFS+\n";
                break;
            case 8:
                std::cout << "HFSX\n";
                break;
            case 9:
                std::cout << "XFS\n";
                break;
            case 10:
                std::cout << "BTRFS\n";
                break;
            case 11:
                std::cout << "BITLOCKER\n";
                break;
            case 12:
                std::cout << "BFS\n";
                break;
            case 13:
                std::cout << "F2FS\n";
                break;
            case 14:
                std::cout << "ISO9660\n";
                break;
            case 15:
                std::cout << "UDF\n";
                break;
            case 16:
                std::cout << "Legacy HFS\n";
                break;
            case 17:
                std::cout << "ZFS\n";
                break;
            case 18:
                std::cout << "ReFS\n";
                break;
            case 19:
                std::cout << "EROFS\n";
                break;
            case 20:
                std::cout << "BCACHEFS\n";
                break;
            case 21:
                std::cout << "ZONEFS\n";
                break;
            default:
                std::cout << "UNKNOWN SO FAR\n";
                break;
        }
    }

    /*
        devicepath = argv[1];
        std::string filestr = argv[2];
        std::size_t found = filestr.find_last_of("/");
        std::string pathname = filestr.substr(0, found);
        std::string filename = filestr.substr(found+1);
        std::filesystem::path initpath = std::filesystem::canonical(pathname + "/");
        imagepath = initpath.string() + "/" + filename + ".wfi";
        logpath = imagepath + ".log";
        if(devicepath.empty())
        {
            ShowUsage(0);
            return 1;
        }
        if(imagepath.empty())
        {
            ShowUsage(0);
            return 1;
        }

        int64_t totalbytes = 0;
        int16_t sectorsize = 0;
        int64_t curpos = 0;
        int64_t errcnt = 0;
        int infile = open(devicepath.c_str(), O_RDONLY | O_NONBLOCK);
	FILE* fin = NULL;
	FILE* fout = NULL;
        FILE* filelog = NULL;
        filelog = fopen(logpath.c_str(), "w+");
        if(filelog == NULL)
        {
            printf("Error opening log file.\n");
            return 1;
        }

        if(infile >= 0)
        {
            ioctl(infile, BLKGETSIZE64, &totalbytes);
	    ioctl(infile, BLKSSZGET, &sectorsize);
            close(infile);
	    fin = fopen_orDie(devicepath.c_str(), "rb");
	    fout = fopen_orDie(imagepath.c_str(), "wb");

	    wfimd.skipframeheader = 0x184d2a5f;
            wfimd.skipframesize = 256;
	    wfimd.sectorsize = sectorsize;
            wfimd.reserved  = 0x0;
	    wfimd.totalbytes = totalbytes;

            time_t starttime = time(NULL);
            char dtbuf[35];
            fprintf(filelog, "wombatimager v0.1 zstd Compressed Raw Forensic Image started at: %s\n", GetDateTime(dtbuf));
            fprintf(filelog, "\nSource Device\n");
            fprintf(filelog, "-------------\n");

            // GET UDEV DEVICE PROPERTIES FOR LOG...
            // !!!!! THIS ONLY GET PARENT DEVICES, DOESN'T WORK FOR PARTITIONS OF PARENT DEVICES...
            struct udev* udev;
            struct udev_device* dev;
            struct udev_enumerate* enumerate;
            struct udev_list_entry* devices;
            struct udev_list_entry* devlistentry;
            udev = udev_new();
            enumerate = udev_enumerate_new(udev);
            udev_enumerate_add_match_subsystem(enumerate, "block");
            udev_enumerate_scan_devices(enumerate);
            devices = udev_enumerate_get_list_entry(enumerate);
            udev_list_entry_foreach(devlistentry, devices)
            {
                const char* path;
                const char* tmp;
                path = udev_list_entry_get_name(devlistentry);
                dev = udev_device_new_from_syspath(udev, path);
                if(strncmp(udev_device_get_devtype(dev), "partition", 9) != 0 && strncmp(udev_device_get_sysname(dev), "loop", 4) != 0)
                {
                    tmp = udev_device_get_devnode(dev);
                    if(strcmp(tmp, devicepath.c_str()) == 0)
                    {
                        fprintf(filelog, "Device:");
                        const char* devvendor = udev_device_get_property_value(dev, "ID_VENDOR");
                        if(devvendor != NULL)
                            fprintf(filelog, " %s", devvendor);
                        const char* devmodel = udev_device_get_property_value(dev, "ID_MODEL");
                        if(devmodel != NULL)
                            fprintf(filelog, " %s", devmodel);
                        const char* devname = udev_device_get_property_value(dev, "ID_NAME");
                        if(devname != NULL)
                            fprintf(filelog, " %s", devname);
                        fprintf(filelog, "\n");
                        tmp = udev_device_get_devnode(dev);
                        fprintf(filelog, "Device Path: %s\n", tmp);
                        tmp = udev_device_get_property_value(dev, "ID_SERIAL_SHORT");
                        fprintf(filelog, "Serial Number: %s\n", tmp);
                        fprintf(filelog, "Size: %u bytes\n", totalbytes);
                        fprintf(filelog, "Sector Size: %u bytes\n", sectorsize);
                    }
                }
                udev_device_unref(dev);
            }
            udev_enumerate_unref(enumerate);
            udev_unref(udev);
            
	    fseek(fin, 0, SEEK_SET);
	    fseek(fout, 0, SEEK_SET);

            // BLAKE3_OUT_LEN IS 32 BYTES LONG
            uint8_t srchash[BLAKE3_OUT_LEN];
            uint8_t wfihash[BLAKE3_OUT_LEN];

	    blake3_hasher srchasher;
	    blake3_hasher_init(&srchasher);

	    // USE ZSTD STREAM COMPRESSION
	    size_t bufinsize = ZSTD_CStreamInSize();
	    void* bufin = malloc_orDie(bufinsize);
	    size_t bufoutsize = ZSTD_CStreamOutSize();
	    void* bufout = malloc_orDie(bufoutsize);
	    size_t writecount = 0;

	    ZSTD_CCtx* cctx = ZSTD_createCCtx();
	    CHECK(cctx != NULL, "ZSTD_createCCtx() failed");

	    size_t toread = bufinsize;
	    for(;;)
	    {
		size_t read = fread_orDie(bufin, toread, fin);
		writecount = writecount + read;
		printf("Writing %llu of %llu bytes\r", writecount, totalbytes);
		fflush(stdout);

		blake3_hasher_update(&srchasher, bufin, read);

		int lastchunk = (read < toread);
		ZSTD_EndDirective mode = lastchunk ? ZSTD_e_end : ZSTD_e_continue;
		ZSTD_inBuffer input = { bufin, read, 0 };
		int finished;
		do
		{
		    ZSTD_outBuffer output = { bufout, bufoutsize, 0 };
		    size_t remaining = ZSTD_compressStream2(cctx, &output, &input, mode);
		    CHECK_ZSTD(remaining);
		    fwrite_orDie(bufout, output.pos, fout);
		    finished = lastchunk ? (remaining == 0) : (input.pos == input.size);
		} while (!finished);
		CHECK(input.pos == input.size, "Impossible, zstd only returns 0 when the input is completely consumed");

		if(lastchunk)
		    break;
	    }
	    ZSTD_freeCCtx(cctx);

	    blake3_hasher_finalize(&srchasher, srchash, BLAKE3_OUT_LEN);
            memcpy(wfimd.devhash, srchash, BLAKE3_OUT_LEN);
	    // NEED TO WRITE SKIPPABLE FRAME CONTENT HERE
	    fwrite_orDie(&wfimd, sizeof(struct wfi_metadata), fout);
	    
	    time_t endtime = time(NULL);
            fprintf(filelog, "Wrote %llu out of %llu bytes\n", curpos, totalbytes);
            fprintf(filelog, "%llu blocks replaced with zeroes\n", errcnt);
            fprintf(filelog, "Forensic Image: %s\n", imagepath.c_str());
            fprintf(filelog, "Forensic Image finished at: %s\n", GetDateTime(dtbuf));
            fprintf(filelog, "Forensic Image created in: %f seconds\n\n", difftime(endtime, starttime));
            printf("\nForensic Image Creation Finished\n");
            for(size_t i=0; i < BLAKE3_OUT_LEN; i++)
            {
                fprintf(filelog, "%02x", srchash[i]);
                printf("%02x", srchash[i]);
            }
            printf(" - BLAKE3 Source Device\n");
            fprintf(filelog, " - BLAKE3 Source Device\n");

	    fclose_orDie(fout);
	    fclose_orDie(fin);
	    free(bufin);
	    free(bufout);

	    if(verify == 1) // start verification
	    {
                fprintf(filelog, "Verification started at: %s\n", GetDateTime(dtbuf));
                printf("Verification Started\n");
                uint8_t forimghash[BLAKE3_OUT_LEN];
                blake3_hasher imghasher;
                blake3_hasher_init(&imghasher);
                
		fout = fopen_orDie(imagepath.c_str(), "rb");
		size_t bufinsize = ZSTD_DStreamInSize();
		void* bufin = malloc_orDie(bufinsize);
		size_t bufoutsize = ZSTD_DStreamOutSize();
		void* bufout = malloc_orDie(bufoutsize);

		ZSTD_DCtx* dctx = ZSTD_createDCtx();
		CHECK(dctx != NULL, "ZSTD_createDCtx() failed");

		size_t toread = bufinsize;
		size_t read;
		size_t lastret = 0;
		int isempty = 1;
		size_t readcount = 0;
		while( (read = fread_orDie(bufin, toread, fout)) )
		{
		    isempty = 0;
		    ZSTD_inBuffer input = { bufin, read, 0 };
		    while(input.pos < input.size)
		    {
			ZSTD_outBuffer output = { bufout, bufoutsize, 0 };
			size_t ret = ZSTD_decompressStream(dctx, &output, &input);
			CHECK_ZSTD(ret);
			blake3_hasher_update(&imghasher, bufout, output.pos);
			lastret = ret;
			readcount = readcount + output.pos;
			printf("Read %llu of %llu bytes\r", readcount, totalbytes);
			fflush(stdout);
		    }
		}
		printf("\nVerification Finished\n");

		if(isempty)
		{
		    printf("input is empty\n");
		    return 1;
		}

		if(lastret != 0)
		{
		    printf("EOF before end of stream: %zu\n", lastret);
		    exit(1);
		}
		ZSTD_freeDCtx(dctx);
		fclose_orDie(fout);
		free(bufin);
		free(bufout);

                blake3_hasher_finalize(&imghasher, forimghash, BLAKE3_OUT_LEN);

		for(size_t i=0; i < BLAKE3_OUT_LEN; i++)
		{
		    fprintf(filelog, "%02x", srchash[i]);
		    printf("%02x", srchash[i]);
		}
		printf(" - BLAKE3 Source Device\n");
		fprintf(filelog, " - BLAKE3 Source Device\n");

                for(size_t i=0; i < BLAKE3_OUT_LEN; i++)
                {
		    fprintf(filelog, "%02x", forimghash[i]);
                    printf("%02x", forimghash[i]);
                }
                printf(" - Forensic Image Hash\n");
                fprintf(filelog, " - Forensic Image Hash\n");
		printf("\n");
		if(memcmp(&srchash, &forimghash, BLAKE3_OUT_LEN) == 0)
		{
		    printf("Verification Successful\n");
		    fprintf(filelog, "Verification Successful\n");
		}
		else
		{
		    printf("Verification Failed\n");
		    fprintf(filelog, "Verification Failed\n");
		}
		printf("\n");
	    }
	}
	else
	{
	    printf("error opening device: %d %s\n", infile, errno);
	    return 1;
	}
	fclose(filelog);
    }
    else
    {
	ShowUsage(0);
	return 1;
    }
    */
    return 0;
}

// OLD WOMBATLOGICAL
/*
 *
void PopulateFile(QFileInfo* tmpfileinfo, bool blake3bool, bool catsigbool, QDataStream* out, QTextStream* logout)
{
    if(tmpfileinfo->isDir()) // its a directory, need to read its contents..
    {
        QDir tmpdir(tmpfileinfo->absoluteFilePath());
        if(tmpdir.isEmpty())
            qDebug() << "dir" << tmpfileinfo->fileName() << "is empty and will be skipped.";
        else
        {
            QFileInfoList infolist = tmpdir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs);
            for(int i=0; i < infolist.count(); i++)
            {
                QFileInfo tmpinfo = infolist.at(i);
                PopulateFile(&tmpinfo, blake3bool, catsigbool, out, logout);
            }
        }
    }
    else if(tmpfileinfo->isFile()) // its a file, so read the info i need...
    {
        // name << path << size << created << accessed << modified << status changed << b3 hash << category << signature
        // other info to gleam such as groupid, userid, permission, 
        *out << (qint64)0x776c69696e646578; // wombat logical image index entry
        *out << (QString)tmpfileinfo->fileName(); // FILENAME
        *out << (QString)tmpfileinfo->absolutePath(); // FULL PATH
        *out << (qint64)tmpfileinfo->size(); // FILE SIZE (8 bytes)
        *out << (qint64)tmpfileinfo->birthTime().toSecsSinceEpoch(); // CREATED (8 bytes)
        *out << (qint64)tmpfileinfo->lastRead().toSecsSinceEpoch(); // ACCESSED (8 bytes)
        *out << (qint64)tmpfileinfo->lastModified().toSecsSinceEpoch(); // MODIFIED (8 bytes)
        *out << (qint64)tmpfileinfo->metadataChangeTime().toSecsSinceEpoch(); // STATUS CHANGED (8 bytes)
        QFile tmpfile(tmpfileinfo->absoluteFilePath());
        if(!tmpfile.isOpen())
            tmpfile.open(QIODevice::ReadOnly);
        // Initialize Blake3 Hasher for block device and forensic image
        uint8_t sourcehash[BLAKE3_OUT_LEN];
        blake3_hasher blkhasher;
        blake3_hasher_init(&blkhasher);
        QString srchash = "";
        QString mimestr = "";
        while(!tmpfile.atEnd())
        {
            QByteArray tmparray = tmpfile.read(2048);
            if(blake3bool)
                blake3_hasher_update(&blkhasher, tmparray.data(), tmparray.count());
        }
        if(blake3bool)
        {
            blake3_hasher_finalize(&blkhasher, sourcehash, BLAKE3_OUT_LEN);
            for(size_t i=0; i < BLAKE3_OUT_LEN; i++)
            {
                srchash.append(QString("%1").arg(sourcehash[i], 2, 16, QChar('0')));
            }
        }
        *out << (QString)srchash; // BLAKE3 HASH FOR FILE CONTENTS OR EMPTY
        if(catsigbool)
        {
            if(tmpfileinfo->fileName().startsWith("$UPCASE_TABLE"))
                mimestr = "System File/Up-case Table";
            else if(tmpfileinfo->fileName().startsWith("$ALLOC_BITMAP"))
                mimestr = "System File/Allocation Bitmap";
            else if(tmpfileinfo->fileName().startsWith("$UpCase"))
                mimestr = "Windows System/System File";
            else if(tmpfileinfo->fileName().startsWith("$MFT") || tmpfileinfo->fileName().startsWith("$MFTMirr") || tmpfileinfo->fileName().startsWith("$LogFile") || tmpfileinfo->fileName().startsWith("$Volume") || tmpfileinfo->fileName().startsWith("$AttrDef") || tmpfileinfo->fileName().startsWith("$Bitmap") || tmpfileinfo->fileName().startsWith("$Boot") || tmpfileinfo->fileName().startsWith("$BadClus") || tmpfileinfo->fileName().startsWith("$Secure") || tmpfileinfo->fileName().startsWith("$Extend"))
                mimestr = "Windows System/System File";
            else
            {
                // NON-QT WAY USING LIBMAGIC
                tmpfile.seek(0);
                QByteArray sigbuf = tmpfile.read(2048);
                magic_t magical;
                const char* catsig;
                magical = magic_open(MAGIC_MIME_TYPE);
                magic_load(magical, NULL);
                catsig = magic_buffer(magical, sigbuf.data(), sigbuf.count());
                std::string catsigstr(catsig);
                mimestr = QString::fromStdString(catsigstr);
                magic_close(magical);
                for(int i=0; i < mimestr.count(); i++)
                {
                    if(i == 0 || mimestr.at(i-1) == ' ' || mimestr.at(i-1) == '-' || mimestr.at(i-1) == '/')
                        mimestr[i] = mimestr[i].toUpper();
                }
            }
            //else if(tmpfileinfo->fileName().startsWith("$INDEX_ROOT:") || tmpfileinfo->fileName().startsWith("$DATA:") || tmpfileinfo->fileName().startWith("$INDEX_ALLOCATION:"))
        }
        *out << (QString)mimestr; // CATEGORY/SIGNATURE STRING FOR FILE CONTENTS OR EMPTY
        *out << (qint8)5; // itemtype
        *out << (qint8)0; // deleted

        // ATTEMPT USING A SINGLE LZ4FRAME SIMILAR TO BLAKE3 HASHING
        tmpfile.seek(0);
        qint64 curpos = 0;
        size_t destsize = LZ4F_compressFrameBound(2048, NULL);
        char* dstbuf = new char[destsize];
        char* srcbuf = new char[2048];
        int dstbytes = 0;
        int compressedsize = 0;
        LZ4F_cctx* lz4cctx;
        LZ4F_errorCode_t errcode;
        errcode = LZ4F_createCompressionContext(&lz4cctx, LZ4F_getVersion());
        if(LZ4F_isError(errcode))
            qDebug() << "LZ4 Create Error:" << LZ4F_getErrorName(errcode);
        dstbytes = LZ4F_compressBegin(lz4cctx, dstbuf, destsize, NULL);
        compressedsize += dstbytes;
        if(LZ4F_isError(dstbytes))
            qDebug() << "LZ4 Begin Error:" << LZ4F_getErrorName(dstbytes);
        out->writeRawData(dstbuf, dstbytes);
        while(curpos < tmpfile.size())
        {
            int bytesread = tmpfile.read(srcbuf, 2048);
            dstbytes = LZ4F_compressUpdate(lz4cctx, dstbuf, destsize, srcbuf, bytesread, NULL);
            if(LZ4F_isError(dstbytes))
                qDebug() << "LZ4 Update Error:" << LZ4F_getErrorName(dstbytes);
            dstbytes = LZ4F_flush(lz4cctx, dstbuf, destsize, NULL);
            if(LZ4F_isError(dstbytes))
                qDebug() << "LZ4 Flush Error:" << LZ4F_getErrorName(dstbytes);
            compressedsize += dstbytes;
            out->writeRawData(dstbuf, dstbytes);
            curpos = curpos + bytesread;
            //printf("Wrote %llu of %llu bytes\r", curpos, totalbytes);
            //fflush(stdout);
        }
        dstbytes = LZ4F_compressEnd(lz4cctx, dstbuf, destsize, NULL);
        compressedsize += dstbytes;
        out->writeRawData(dstbuf, dstbytes);
        delete[] srcbuf;
        delete[] dstbuf;
        errcode = LZ4F_freeCompressionContext(lz4cctx);
        tmpfile.close();
        *logout << "Processed:" << tmpfileinfo->absoluteFilePath() << Qt::endl;
        qDebug() << "Processed: " << tmpfileinfo->fileName();
    }
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("wombatlogical");
    QCoreApplication::setApplicationVersion("0.1");
    QCommandLineParser parser;
    parser.setApplicationDescription("Create a wombat logical forensic image from files and directories");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("image", QCoreApplication::translate("main", "Desination logical forensic image file name w/o extension."));
    parser.addPositionalArgument("source", QCoreApplication::translate("main", "Multitple files and directories to copy to the forensic image."));

    // ADD OPTIONS TO HASH, CATEGORY/SIGNATURE ANALYSIS FOR IMPORTING INTO WOMBAT...
    
    QCommandLineOption blake3option(QStringList() << "b" << "compute-hash", QCoreApplication::translate("main", "Compute the Blake3 Hash for the file."));
    QCommandLineOption signatureoption(QStringList() << "s" << "cat-sig", QCoreApplication::translate("main", "Compute the category/signature for the file."));
    //QCommandLineOption compressionleveloption(QStringList() << "l" << "compress-level", QCoreApplication::translate("main", "Set compression level, default=3."), QCoreApplication::translate("main", "clevel"));
    QCommandLineOption casenumberoption(QStringList() << "c" << "case-number", QCoreApplication::translate("main", "List the case number."), QCoreApplication::translate("main", "casenum"));
    //QCommandLineOption evidencenumberoption(QStringList() << "e" << "evidence-number", QCoreApplication::translate("main", "List the evidence number."), QCoreApplication::translate("main", "evidnum"));
    QCommandLineOption examineroption(QStringList() << "x" << "examiner", QCoreApplication::translate("main", "Examiner creating forensic image."), QCoreApplication::translate("main", "examiner"));
    QCommandLineOption descriptionoption(QStringList() << "d" << "description", QCoreApplication::translate("main", "Enter description of evidence to be imaged."), QCoreApplication::translate("main", "desciption"));
    //parser.addOption(compressionleveloption);
    parser.addOption(blake3option);
    parser.addOption(signatureoption);
    parser.addOption(casenumberoption);
    //parser.addOption(evidencenumberoption);
    parser.addOption(examineroption);
    parser.addOption(descriptionoption);

    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if(args.count() <= 1)
    {
        qInfo() << "No image and/or source files/dirs provided.\n";
        parser.showHelp(1);
    }
    QString casenumber = parser.value(casenumberoption);
    //QString evidencenumber = parser.value(evidencenumberoption);
    QString examiner = parser.value(examineroption);
    QString description = parser.value(descriptionoption);
    bool blake3bool = parser.isSet(blake3option);
    bool catsigbool = parser.isSet(signatureoption);
    //QString clevel = parser.value(compressionleveloption);
    //qDebug() << "casenumber:" << casenumber << "evidencenumber:" << evidencenumber << "examiner:" << examiner << "descrption:" << description;
    QString imgfile = args.at(0) + ".wli";
    QString logfile = args.at(0) + ".log";
    QStringList filelist;
    filelist.clear();
    for(int i=1; i < args.count(); i++)
        filelist.append(args.at(i));

    // Initialize the datastream and header for the custom forensic image
    QFile wli(imgfile);
    if(!wli.isOpen())
        wli.open(QIODevice::WriteOnly);
    QDataStream out(&wli);
    out.setVersion(QDataStream::Qt_5_15);
    out << (quint64)0x776f6d6261746c69; // wombatli - wombat logical image signature (8 bytes)
    out << (quint8)0x01; // version 1 (1 byte)
    out << (QString)casenumber;
    //out << (QString)evidencenumber;
    out << (QString)examiner;
    out << (QString)description;

    // Initialize the log file
    QFile log(logfile);
    log.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream logout(&log);

    logout << "wombatlogical v0.1 Logical Forensic Imager started at: " << QDateTime::currentDateTime().toString("MM/dd/yyyy hh:mm:ss ap") << Qt::endl;

    // Initiate the Loop over all source files to write their info the logical image file...
    for(int i=0; i < filelist.count(); i++)
    {
        QFileInfo tmpstat(filelist.at(i));
        PopulateFile(&tmpstat, blake3bool, catsigbool, &out, &logout);
    }
    // WRITE THE INDEX STRING TO THE FILE FOR READING LATER...

    wli.close();
    // NEED TO REPOPEN THE WLI FILE AS READONLY AND THEN HASH THE CONTENTS TO SPIT OUT A HASH... TO VERIFY THE LOGICAL IMAGE
    // OF COURSE, IF I STICK THE HASH BACK INTO THE END OF THE FILE, I WOULD THEN HAVE TO READ THE ENTIRE FILE MINUS THE HASH VALUE TO VERIFY IT AT A LATER DATE WITH WOMBATVERIFY
    // HASHING THE LOGICAL IMAGE IN THIS WAY WOULDN'T BE COMPATIBLE WITH B3SUM, BUT THIS IS DIFFERENT AND DOESN'T NEED TO BE COMPATIBLE. WITH THAT IN MIND, I'LL HAVE TO FIGURE OUT WHAT TO PASS...
    // AND HOW TO VERIFY
    blake3_hasher logicalhasher;
    blake3_hasher_init(&logicalhasher);
    wli.open(QIODevice::ReadOnly);
    wli.seek(0);
    while(!wli.atEnd())
    {
        QByteArray tmparray = wli.read(65536);
        blake3_hasher_update(&logicalhasher, tmparray.data(), tmparray.count());
    }
    wli.close();
    uint8_t output[BLAKE3_OUT_LEN];
    blake3_hasher_finalize(&logicalhasher, output, BLAKE3_OUT_LEN);
    QString logicalhash = "";
    for(size_t i=0; i < BLAKE3_OUT_LEN; i++)
        logicalhash.append(QString("%1").arg(output[i], 2, 16, QChar('0')));
    wli.open(QIODevice::WriteOnly | QIODevice::Append);
    wli.seek(wli.size());
    QDataStream wout(&wli);
    wout << (QString)logicalhash;
    wli.close();

    logout << logicalhash << " - Logical Image Hash" << Qt::endl;
    // WHEN VERIFYING THE LOGICAL IMAGE, I NEED TO DO 128 FOR THE HASH TO COMPARE AND READ THE IMAGE FROM 0 UP TO 132 (128 FOR HASH + 4 FOR SIZE OF THE QSTRING);
    log.close();

    return 0;
}
*/

/*
 *void ParseDirectory(std::filesystem::path dirpath, std::vector<std::filesystem::path>* filelist, uint8_t isrelative)
{
    for(auto const& dir_entry : std::filesystem::recursive_directory_iterator(dirpath))
    {
	if(std::filesystem::is_regular_file(dir_entry))
        {
            if(isrelative)
                filelist->push_back(std::filesystem::relative(dir_entry, std::filesystem::current_path()));
            else
                filelist->push_back(dir_entry);
        }
    }
}

void HashFile(std::string filename, std::string whlfile)
{
    std::ifstream fin(filename.c_str());
    char tmpchar[65536];
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    while(fin)
    {
	fin.read(tmpchar, 65536);
	size_t cnt = fin.gcount();
	blake3_hasher_update(&hasher, tmpchar, cnt);
	if(!cnt)
	    break;
    }
    uint8_t output[BLAKE3_OUT_LEN];
    blake3_hasher_finalize(&hasher, output, BLAKE3_OUT_LEN);
    std::stringstream ss;
    for(int i=0; i < BLAKE3_OUT_LEN; i++)
        ss << std::hex << (int)output[i]; 
    std::string srcmd5 = ss.str();
    std::string whlstr = srcmd5 + "," + filename + "\n";
    FILE* whlptr = NULL;
    whlptr = fopen(whlfile.c_str(), "a");
    fwrite(whlstr.c_str(), strlen(whlstr.c_str()), 1, whlptr);
    fclose(whlptr);
}

void CompareFile(std::string filename, std::map<std::string, std::string>* knownhashes, int8_t matchbool, uint8_t isdisplay)
{
    std::ifstream fin(filename.c_str());
    char tmpchar[65536];
    blake3_hasher hasher;
    blake3_hasher_init(&hasher);
    while(fin)
    {
	fin.read(tmpchar, 65536);
	size_t cnt = fin.gcount();
	blake3_hasher_update(&hasher, tmpchar, cnt);
	if(!cnt)
	    break;
    }
    uint8_t output[BLAKE3_OUT_LEN];
    blake3_hasher_finalize(&hasher, output, BLAKE3_OUT_LEN);
    std::stringstream ss;
    for(int i=0; i < BLAKE3_OUT_LEN; i++)
        ss << std::hex << (int)output[i]; 
    std::string srcmd5 = ss.str();
    
    std::string matchstring = filename;
    uint8_t hashash = knownhashes->count(srcmd5);
    if(matchbool == 0 && hashash == 1)
    {
	matchstring += " matches";
        if(isdisplay == 1)
            matchstring += " " + knownhashes->at(srcmd5);
        matchstring += ".\n";
	std::cout << matchstring;
    }
    if(matchbool == 1 && hashash == 0)
    {
	matchstring += " does not match known files.\n";
	std::cout << matchstring;
    }
}

void ShowUsage(int outtype)
{
    if(outtype == 0)
    {
        printf("Generates a Wombat Hash List (whl) or compare files against a hash list.\n\n");
        printf("Usage :\n");
        printf("\twombathasher [OPTIONS] files\n\n");
        printf("Options :\n");
	printf("-c <new list>\t: Create new hash list.\n");
	printf("-a <existing list>\t: Append to an existing hash list.\n");
	printf("-r\t: Recurse sub-directories.\n");
	printf("-k <file>\t: Compare computed hashes to known list.\n");
	printf("-m\t: Matching mode. Requires -k.\n");
	printf("-n\t: Negative (Inverse) matching mode. Requires -k.\n");
	printf("-w\t: Display which known file was matched, requires -m.\n");
	printf("-l\t: Print relative paths for filenames.\n");
        printf("-v\t: Prints Version information\n");
        printf("-h\t: Prints help information\n\n");
	printf("Arguments:\n");
	printf("files\t: Files to hash for comparison or to add to a hash list.\n");
    }
    else if(outtype == 1)
    {
        printf("wombathasher v0.3\n");
	printf("License CC0-1.0: Creative Commons Zero v1.0 Universal\n");
        printf("This software is in the public domain\n");
        printf("There is NO WARRANTY, to the extent permitted by law.\n\n");
        printf("Written by Pasquale Rinaldi\n");
    }
};

int main(int argc, char* argv[])
{
    uint8_t isnew = 0;
    uint8_t isappend = 0;
    uint8_t isrecursive = 0;
    uint8_t isknown = 0;
    uint8_t ismatch = 0;
    uint8_t isnotmatch = 0;
    uint8_t isdisplay = 0;
    uint8_t isrelative = 0;

    std::string newwhlstr = "";
    std::string appendwhlstr = "";
    std::string knownwhlstr = "";
    std::filesystem::path newpath;
    std::filesystem::path appendpath;
    std::filesystem::path knownpath;

    std::vector<std::filesystem::path> filevector;
    filevector.clear();
    std::vector<std::filesystem::path> filelist;
    filelist.clear();
    std::map<std::string, std::string> knownhashes;
    knownhashes.clear();

    //unsigned int threadcount = std::thread::hardware_concurrency();
    //std::cout << threadcount << " concurrent threads are supported.\n";

    if(argc == 1 || (argc == 2 && strcmp(argv[1], "-h") == 0))
    {
	ShowUsage(0);
	return 1;
    }
    else if(argc == 2 && strcmp(argv[1], "-v") == 0)
    {
	ShowUsage(1);
	return 1;
    }
    else if(argc >= 3)
    {
        int i;
        while((i=getopt(argc, argv, "a:c:hk:lmnrvw")) != -1)
        {
            switch(i)
            {
                case 'a':
                    isappend = 1;
                    appendwhlstr = optarg;
                    break;
                case 'c':
                    isnew = 1;
                    newwhlstr = optarg;
                    break;
                case 'h':
                    ShowUsage(0);
                    return 1;
                    break;
                case 'k':
                    isknown = 1;
                    knownwhlstr = optarg;
                    break;
                case 'l':
                    isrelative = 1;
                    break;
                case 'm':
                    ismatch = 1;
                    break;
                case 'n':
                    isnotmatch = 1;
                    break;
                case 'r':
                    isrecursive = 1;
                    break;
                case 'v':
                    ShowUsage(1);
                    return 1;
                    break;
                case 'w':
                    isdisplay = 1;
                    break;
                default:
                    printf("unknown option: %s\n", optarg);
                    return 1;
                    break;
            }
        }
        for(int i=optind; i < argc; i++)
        {
            filevector.push_back(std::filesystem::canonical(argv[i]));
        }
	if(isnew)
        {
	    newpath = std::filesystem::absolute(newwhlstr);
        }
	if(isappend)
        {
	    appendpath = std::filesystem::absolute(appendwhlstr);
        }
	if(isknown)
        {
	    knownpath = std::filesystem::absolute(knownwhlstr);
        }
        // CHECK FOR INCOMPATIBLE OPTIONS
	if(isnew && isappend)
	{
	    printf("You cannot create a new and append to an existing wombat hash list (whl) file.\n");
	    return 1;
	}
	if(ismatch && !isknown || ismatch && isnotmatch || isnotmatch && !isknown)
	{
	    printf("(Not) Match requires a known whl file, cannot match/not match at the same time\n");
	    return 1;
	}
	if(isnew && std::filesystem::exists(newwhlstr))
	{
	    printf("Wombat Hash List (whl) file %s already exists. Cannot Create, try to append (-a)\n", newwhlstr.c_str());
	    return 1;
	}
	if(isappend && !std::filesystem::exists(appendwhlstr))
	{
	    printf("Wombat Hash List (whl) file %s does not exist. Cannot Append, try to create (-c)\n", appendwhlstr.c_str());
	    return 1;
	}
	if(isknown && !std::filesystem::exists(knownwhlstr))
	{
	    printf("Known wombat hash list (whl) file %s does not exist\n", knownwhlstr.c_str());
	    return 1;
	}

	for(int i=0; i < filevector.size(); i++)
	{
	    if(std::filesystem::is_regular_file(filevector.at(i)))
	    {
                if(isrelative)
                    filelist.push_back(std::filesystem::relative(filevector.at(i), std::filesystem::current_path()));
                else
                    filelist.push_back(filevector.at(i));
	    }
	    else if(std::filesystem::is_directory(filevector.at(i)))
	    {
		if(isrecursive)
		    ParseDirectory(filevector.at(i), &filelist, isrelative);
		else
		    printf("Directory %s skipped. Use -r to recurse directory\n", filevector.at(i).c_str());
	    }
	}
    }
    // GOT THE LIST OF FIlES (filelist), NOW I NEED TO HASH AND HANDLE ACCORDING TO OPTIONS 
    if(isnew || isappend)
    {
        std::string whlstr;
        if(isnew) // create a new whl file
        {
            whlstr = newpath.string();
            std::size_t found = whlstr.rfind(".whl");
            if(found == -1)
                whlstr += ".whl";
        }
        if(isappend) // append to existing whl file
            whlstr = appendpath.string();
	for(int i=0; i < filelist.size(); i++)
	{
	    std::thread tmp(HashFile, filelist.at(i).string(), whlstr);
	    tmp.join();
	}
    }
    if(isknown)
    {
        // READ KNOWN HASH LIST FILE INTO A MAP
	std::string matchstring = "";
        std::ifstream knownstream;
        knownstream.open(knownpath.string(), std::ios::in);
        std::string tmpfile;
        while(std::getline(knownstream, tmpfile))
	{
	    std::size_t found = tmpfile.find(",");
	    std::string tmpkey = tmpfile.substr(0, found);
	    std::string tmpval = tmpfile.substr(found+1);
	    knownhashes.insert(std::pair<std::string, std::string>(tmpkey, tmpval));
	    //std::cout << tmpkey << " | " << tmpval << "\n";
	}
        knownstream.close();
	int8_t matchbool = -1;
	if(ismatch)
	    matchbool = 0;
	if(isnotmatch)
	    matchbool = 1;

	for(int i=0; i < filelist.size(); i++)
	{
	    std::thread tmp(CompareFile, filelist.at(i).string(), &knownhashes, matchbool, isdisplay);
	    tmp.join();
	}
    }

    return 0;
}
*/
