/*
 * Copyright 2022, Raghav Sharma, raghavself28@gmail.com
 * Copyright 2020, Shubham Bhagat, shubhambhagat111@yahoo.com
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include "Extent.h"

#include "VerifyHeader.h"


Extent::Extent(Inode* inode)
	:
	fInode(inode),
	fOffset(0)
{
}


Extent::~Extent()
{
}


void
Extent::FillMapEntry(void* pointerToMap)
{
	uint64 firstHalf = *((uint64*)pointerToMap);
	uint64 secondHalf = *((uint64*)pointerToMap + 1);
		//dividing the 128 bits into 2 parts.
	firstHalf = B_BENDIAN_TO_HOST_INT64(firstHalf);
	secondHalf = B_BENDIAN_TO_HOST_INT64(secondHalf);
	fMap->br_state = (firstHalf >> 63);
	fMap->br_startoff = (firstHalf & MASK(63)) >> 9;
	fMap->br_startblock = ((firstHalf & MASK(9)) << 43) | (secondHalf >> 21);
	fMap->br_blockcount = (secondHalf & MASK(21));
	TRACE("Extent::Init: startoff:(%" B_PRIu64 "), startblock:(%" B_PRIu64 "),"
		"blockcount:(%" B_PRIu64 "),state:(%" B_PRIu8 ")\n", fMap->br_startoff, fMap->br_startblock,
		fMap->br_blockcount, fMap->br_state);
}


status_t
Extent::FillBlockBuffer()
{
	if (fMap->br_state != 0)
		return B_BAD_VALUE;

	int len = fInode->DirBlockSize();
	fBlockBuffer = new(std::nothrow) char[len];
	if (fBlockBuffer == NULL)
		return B_NO_MEMORY;

	xfs_daddr_t readPos =
		fInode->FileSystemBlockToAddr(fMap->br_startblock);

	if (read_pos(fInode->GetVolume()->Device(), readPos, fBlockBuffer, len)
		!= len) {
		ERROR("Extent::FillBlockBuffer(): IO Error");
		return B_IO_ERROR;
	}

	return B_OK;
}


status_t
Extent::Init()
{
	fMap = new(std::nothrow) ExtentMapEntry;
	if (fMap == NULL)
		return B_NO_MEMORY;

	ASSERT(IsBlockType() == true);
	void* pointerToMap = DIR_DFORK_PTR(fInode->Buffer(), fInode->CoreInodeSize());
	FillMapEntry(pointerToMap);
	ASSERT(fMap->br_blockcount == 1);
		//TODO: This is always true for block directories
		//If we use this implementation for leaf directories, this is not
		//always true
	status_t status = FillBlockBuffer();
	if (status != B_OK)
		return status;

	ExtentDataHeader* header = CreateDataHeader(fInode, fBlockBuffer);
	if (header == NULL)
		return B_NO_MEMORY;
	if (!VerifyHeader<ExtentDataHeader>(header, fBlockBuffer, fInode, 0, fMap, XFS_BLOCK)) {
		status = B_BAD_VALUE;
		ERROR("Extent:Init(): Bad Block!\n");
	}

	delete header;
	return status;
}


ExtentBlockTail*
Extent::BlockTail()
{
	return (ExtentBlockTail*)
		(fBlockBuffer + fInode->DirBlockSize() - sizeof(ExtentBlockTail));
}


uint32
Extent::GetOffsetFromAddress(uint32 address)
{
	address = address * 8;
		// block offset in eight bytes, hence multiple with 8
	return address & (fInode->DirBlockSize() - 1);
}


ExtentLeafEntry*
Extent::BlockFirstLeaf(ExtentBlockTail* tail)
{
	return (ExtentLeafEntry*)tail - B_BENDIAN_TO_HOST_INT32(tail->count);
}


bool
Extent::IsBlockType()
{
	bool status = true;
	if (fInode->BlockCount() != 1)
		status = false;
	if (fInode->Size() != fInode->DirBlockSize())
		status = false;
	void* pointerToMap = DIR_DFORK_PTR(fInode->Buffer(), fInode->CoreInodeSize());
	xfs_fileoff_t startoff = (*((uint64*)pointerToMap) & MASK(63)) >> 9;
	if (startoff != 0)
		status = false;
	return status;
}


int
Extent::EntrySize(int len) const
{
	int entrySize = sizeof(xfs_ino_t) + sizeof(uint8) + len + sizeof(uint16);
			// uint16 is for the tag
	if (fInode->HasFileTypeField())
		entrySize += sizeof(uint8);

	return (entrySize + 7) & -8;
			// rounding off to closest multiple of 8
}


status_t
Extent::GetNext(char* name, size_t* length, xfs_ino_t* ino)
{
	TRACE("Extend::GetNext\n");

	void* entry; // This could be unused entry so we should check

	entry = (void*)(fBlockBuffer + SizeOfDataHeader(fInode));

	int numberOfEntries = B_BENDIAN_TO_HOST_INT32(BlockTail()->count);
	int numberOfStaleEntries = B_BENDIAN_TO_HOST_INT32(BlockTail()->stale);

	// We don't read stale entries.
	numberOfEntries -= numberOfStaleEntries;
	TRACE("numberOfEntries:(%" B_PRId32 ")\n", numberOfEntries);
	uint16 currentOffset = (char*)entry - fBlockBuffer;

	for (int i = 0; i < numberOfEntries; i++) {
		ExtentUnusedEntry* unusedEntry = (ExtentUnusedEntry*)entry;

		if (B_BENDIAN_TO_HOST_INT16(unusedEntry->freetag) == DIR2_FREE_TAG) {
			TRACE("Unused entry found\n");
			currentOffset += B_BENDIAN_TO_HOST_INT16(unusedEntry->length);
			entry = (void*)
				((char*)entry + B_BENDIAN_TO_HOST_INT16(unusedEntry->length));
			i--;
			continue;
		}
		ExtentDataEntry* dataEntry = (ExtentDataEntry*) entry;

		if (fOffset >= currentOffset) {
			entry = (void*)((char*)entry + EntrySize(dataEntry->namelen));
			currentOffset += EntrySize(dataEntry->namelen);
			continue;
		}

		if ((size_t)(dataEntry->namelen) >= *length)
				return B_BUFFER_OVERFLOW;

		fOffset = currentOffset;
		memcpy(name, dataEntry->name, dataEntry->namelen);
		name[dataEntry->namelen] = '\0';
		*length = dataEntry->namelen + 1;
		*ino = B_BENDIAN_TO_HOST_INT64(dataEntry->inumber);

		TRACE("Entry found. Name: (%s), Length: (%" B_PRIuSIZE "),ino: (%" B_PRIu64 ")\n", name,
			*length, *ino);
		return B_OK;
	}

	return B_ENTRY_NOT_FOUND;
}


status_t
Extent::Lookup(const char* name, size_t length, xfs_ino_t* ino)
{
	TRACE("Extent: Lookup\n");
	TRACE("Name: %s\n", name);
	uint32 hashValueOfRequest = hashfunction(name, length);
	TRACE("Hashval:(%" B_PRIu32 ")\n", hashValueOfRequest);
	ExtentBlockTail* blockTail = BlockTail();
	ExtentLeafEntry* leafEntry = BlockFirstLeaf(blockTail);

	int numberOfLeafEntries = B_BENDIAN_TO_HOST_INT32(blockTail->count);
	int left = 0;
	int mid;
	int right = numberOfLeafEntries - 1;

	/*
	* Trying to find the lowerbound of hashValueOfRequest
	* This is slightly different from bsearch(), as we want the first
	* instance of hashValueOfRequest and not any instance.
	*/
	while (left < right) {
		mid = (left+right)/2;
		uint32 hashval = B_BENDIAN_TO_HOST_INT32(leafEntry[mid].hashval);
		if (hashval >= hashValueOfRequest) {
			right = mid;
			continue;
		}
		if (hashval < hashValueOfRequest) {
			left = mid+1;
		}
	}
	TRACE("left:(%" B_PRId32 "), right:(%" B_PRId32 ")\n", left, right);

	while (B_BENDIAN_TO_HOST_INT32(leafEntry[left].hashval)
			== hashValueOfRequest) {

		uint32 address = B_BENDIAN_TO_HOST_INT32(leafEntry[left].address);
		if (address == 0) {
			left++;
			continue;
		}

		uint32 offset = GetOffsetFromAddress(address);
		TRACE("offset:(%" B_PRIu32 ")\n", offset);
		ExtentDataEntry* entry = (ExtentDataEntry*)(fBlockBuffer + offset);

		int retVal = strncmp(name, (char*)entry->name, entry->namelen);
		if (retVal == 0) {
			*ino = B_BENDIAN_TO_HOST_INT64(entry->inumber);
			TRACE("ino:(%" B_PRIu64 ")\n", *ino);
			return B_OK;
		}
		left++;
	}

	return B_ENTRY_NOT_FOUND;
}


ExtentDataHeader::~ExtentDataHeader()
{
}


/*
	First see which type of directory we reading then
	return magic number as per Inode Version.
*/
uint32
ExtentDataHeader::ExpectedMagic(int8 WhichDirectory, Inode* inode)
{
	if (WhichDirectory == XFS_BLOCK) {
		if (inode->Version() == 1 || inode->Version() == 2)
			return DIR2_BLOCK_HEADER_MAGIC;
		else
			return DIR3_BLOCK_HEADER_MAGIC;
	} else {
		if (inode->Version() == 1 || inode->Version() == 2)
			return V4_DATA_HEADER_MAGIC;
		else
			return V5_DATA_HEADER_MAGIC;
	}
}


uint32
ExtentDataHeader::CRCOffset()
{
	return XFS_EXTENT_CRC_OFF - XFS_EXTENT_V5_VPTR_OFF;
}


void
ExtentDataHeaderV4::SwapEndian()
{
	magic	=	B_BENDIAN_TO_HOST_INT32(magic);
}


ExtentDataHeaderV4::ExtentDataHeaderV4(const char* buffer)
{
	uint32 offset = 0;

	magic = *(uint32*)(buffer + offset);
	offset += sizeof(uint32);

	memcpy(bestfree, buffer + offset, XFS_DIR2_DATA_FD_COUNT * sizeof(FreeRegion));

	SwapEndian();
}


ExtentDataHeaderV4::~ExtentDataHeaderV4()
{
}


uint32
ExtentDataHeaderV4::Magic()
{
	return magic;
}


uint64
ExtentDataHeaderV4::Blockno()
{
	return B_BAD_VALUE;
}


uint64
ExtentDataHeaderV4::Lsn()
{
	return B_BAD_VALUE;
}


uint64
ExtentDataHeaderV4::Owner()
{
	return B_BAD_VALUE;
}


uuid_t*
ExtentDataHeaderV4::Uuid()
{
	return NULL;
}


void
ExtentDataHeaderV5::SwapEndian()
{
	magic	=	B_BENDIAN_TO_HOST_INT32(magic);
	blkno	=	B_BENDIAN_TO_HOST_INT64(blkno);
	lsn		=	B_BENDIAN_TO_HOST_INT64(lsn);
	owner	=	B_BENDIAN_TO_HOST_INT64(owner);
	pad		=	B_BENDIAN_TO_HOST_INT32(pad);
}


ExtentDataHeaderV5::ExtentDataHeaderV5(const char* buffer)
{
	uint32 offset = 0;

	magic = *(uint32*)(buffer + offset);
	offset += sizeof(uint32);

	crc = *(uint32*)(buffer + offset);
	offset += sizeof(uint32);

	blkno = *(uint64*)(buffer + offset);
	offset += sizeof(uint64);

	lsn = *(uint64*)(buffer + offset);
	offset += sizeof(uint64);

	memcpy(&uuid, buffer + offset, sizeof(uuid_t));
	offset += sizeof(uuid_t);

	owner = *(uint64*)(buffer + offset);
	offset += sizeof(uint64);

	memcpy(bestfree, buffer + offset, XFS_DIR2_DATA_FD_COUNT * sizeof(FreeRegion));
	offset += XFS_DIR2_DATA_FD_COUNT * sizeof(FreeRegion);

	pad = *(uint32*)(buffer + offset);

	SwapEndian();
}


ExtentDataHeaderV5::~ExtentDataHeaderV5()
{
}


uint32
ExtentDataHeaderV5::Magic()
{
	return magic;
}


uint64
ExtentDataHeaderV5::Blockno()
{
	return blkno;
}


uint64
ExtentDataHeaderV5::Lsn()
{
	return lsn;
}


uint64
ExtentDataHeaderV5::Owner()
{
	return owner;
}


uuid_t*
ExtentDataHeaderV5::Uuid()
{
	return &uuid;
}


//Function to get V4 or V5 data header instance
ExtentDataHeader*
CreateDataHeader(Inode* inode, const char* buffer)
{
	if (inode->Version() == 1 || inode->Version() == 2) {
		ExtentDataHeaderV4* header = new (std::nothrow) ExtentDataHeaderV4(buffer);
		return header;
	} else {
		ExtentDataHeaderV5* header = new (std::nothrow) ExtentDataHeaderV5(buffer);
		return header;
	}
}


/*
	This Function returns Actual size of data header
	in all forms of directory.
	Never use sizeof() operator because we now have
	vtable as well and it will give wrong results
*/
uint32
SizeOfDataHeader(Inode* inode)
{
	if (inode->Version() == 1 || inode->Version() == 2)
		return sizeof(ExtentDataHeaderV4) - XFS_EXTENT_V4_VPTR_OFF;
	else
		return sizeof(ExtentDataHeaderV5) - XFS_EXTENT_V5_VPTR_OFF;
}
