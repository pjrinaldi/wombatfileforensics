/*
 * Copyright 2020, Shubham Bhagat, shubhambhagat111@yahoo.com
 * All rights reserved. Distributed under the terms of the MIT License.
 */


#include "BPlusTree.h"

#include "VerifyHeader.h"


TreeDirectory::TreeDirectory(Inode* inode)
	:
	fInode(inode),
	fInitStatus(B_OK),
	fRoot(NULL),
	fExtents(NULL),
	fCountOfFilledExtents(0),
	fSingleDirBlock(NULL),
	fOffsetOfSingleDirBlock(-1),
	fCurMapIndex(0),
	fOffset(0),
	fCurBlockNumber(0)
{
	fRoot = new(std::nothrow) BlockInDataFork;
	if (fRoot == NULL) {
		fInitStatus = B_NO_MEMORY;
		return;
	}

	fSingleDirBlock = new(std::nothrow) char[fInode->DirBlockSize()];
	if (fSingleDirBlock == NULL) {
		fInitStatus = B_NO_MEMORY;
		return;
	}

	memcpy((void*)fRoot,
		DIR_DFORK_PTR(fInode->Buffer(), fInode->CoreInodeSize()), sizeof(BlockInDataFork));

	for (int i = 0; i < MAX_TREE_DEPTH; i++) {
		fPathForLeaves[i].blockData = NULL;
		fPathForData[i].blockData = NULL;
	}
}


TreeDirectory::~TreeDirectory()
{
	delete fRoot;
	delete[] fExtents;
	delete[] fSingleDirBlock;
}


status_t
TreeDirectory::InitCheck()
{
	return fInitStatus;
}


uint32
TreeDirectory::BlockLen()
{
	return fInode->SizeOfLongBlock();
}


size_t
TreeDirectory::KeySize()
{
	return XFS_KEY_SIZE;
}


size_t
TreeDirectory::PtrSize()
{
	return XFS_PTR_SIZE;
}


bool
TreeDirectory::VerifyBlockHeader(LongBlock* header, char* buffer)
{
	TRACE("VerifyBlockHeader\n");

	if (header->Magic() != XFS_BMAP_MAGIC
		&& header->Magic() != XFS_BMAP_CRC_MAGIC) {
		ERROR("Bad magic number");
		return false;
	}

	if (fInode->Version() == 1 || fInode->Version() == 2)
		return true;

	if (!xfs_verify_cksum(buffer, fInode->DirBlockSize(),
			XFS_LBLOCK_CRC_OFF)) {
		ERROR("Block is corrupted");
		return false;
	}

	if (!fInode->GetVolume()->UuidEquals(header->Uuid())) {
		ERROR("UUID is incorrect");
		return false;
	}

	if (fInode->ID() != header->Owner()) {
		ERROR("Wrong Block owner");
		return false;
	}

	return true;
}


size_t
TreeDirectory::MaxRecordsPossibleRoot()
{
	size_t lengthOfDataFork = 0;
	if (fInode->ForkOffset() != 0)
		lengthOfDataFork = fInode->ForkOffset() << 3;
	if (fInode->ForkOffset() == 0) {
		lengthOfDataFork = fInode->GetVolume()->InodeSize()
			- fInode->CoreInodeSize();
	}

	lengthOfDataFork -= sizeof(BlockInDataFork);
	return lengthOfDataFork / (KeySize() + PtrSize());
}


size_t
TreeDirectory::GetPtrOffsetIntoRoot(int pos)
{
	size_t maxRecords = MaxRecordsPossibleRoot();
	return sizeof(BlockInDataFork) + maxRecords * KeySize()
		+ (pos - 1) * PtrSize();
}


size_t
TreeDirectory::MaxRecordsPossibleNode()
{
	size_t availableSpace = fInode->GetVolume()->BlockSize() - BlockLen();
	return availableSpace / (KeySize() + PtrSize());
}


size_t
TreeDirectory::GetPtrOffsetIntoNode(int pos)
{
	size_t maxRecords = MaxRecordsPossibleNode();
	return BlockLen() + maxRecords * KeySize() + (pos - 1) * PtrSize();
}


TreePointer*
TreeDirectory::GetPtrFromRoot(int pos)
{
	return (TreePointer*)
		((char*)DIR_DFORK_PTR(fInode->Buffer(), fInode->CoreInodeSize())
			+ GetPtrOffsetIntoRoot(pos));
}


TreePointer*
TreeDirectory::GetPtrFromNode(int pos, void* buffer)
{
	size_t offsetIntoNode = GetPtrOffsetIntoNode(pos);
	return (TreePointer*)((char*)buffer + offsetIntoNode);
}


TreeKey*
TreeDirectory::GetKeyFromNode(int pos, void* buffer)
{
	return (TreeKey*)
		((char*)buffer + BlockLen() + (pos - 1) * KeySize());
}


TreeKey*
TreeDirectory::GetKeyFromRoot(int pos)
{
	off_t offset = (pos - 1) * KeySize();
	char* base = (char*)DIR_DFORK_PTR(fInode->Buffer(), fInode->CoreInodeSize())
		+ sizeof(BlockInDataFork);
	return (TreeKey*) (base + offset);
}


status_t
TreeDirectory::SearchOffsetInTreeNode(uint32 offset,
	TreePointer** pointer, int pathIndex)
{
	// This is O(MaxRecords). Next is to implement this using upper bound
	// binary search and get O(log(MaxRecords))
	*pointer = NULL;
	TreeKey* offsetKey = NULL;
	size_t maxRecords = MaxRecordsPossibleNode();
	for (int i = maxRecords - 1; i >= 0; i--) {
		offsetKey
			= GetKeyFromNode(i + 1, (void*)fPathForLeaves[pathIndex].blockData);
		if (B_BENDIAN_TO_HOST_INT64(*offsetKey) <= offset) {
			*pointer = GetPtrFromNode(i + 1, (void*)
				fPathForLeaves[pathIndex].blockData);

			break;
		}
	}

	if (offsetKey == NULL || *pointer == NULL)
		return B_BAD_VALUE;

	return B_OK;
}


status_t
TreeDirectory::SearchAndFillPath(uint32 offset, int type)
{
	TRACE("SearchAndFillPath:\n");
	PathNode* path;
	if (type == DATA)
		path = fPathForData;
	else if (type == LEAF)
		path = fPathForLeaves;
	else
		return B_BAD_VALUE;

	TreePointer* ptrToNode = NULL;
	TreeKey* offsetKey = NULL;
	// Go down the root of the tree first
	for (int i = fRoot->NumRecords() - 1; i >= 0; i--) {
		offsetKey = GetKeyFromRoot(i + 1);
		if (B_BENDIAN_TO_HOST_INT64(*offsetKey) <= offset) {
			ptrToNode = GetPtrFromRoot(i + 1);
			break;
		}
	}
	if (ptrToNode == NULL || offsetKey == NULL) {
		//Corrupt tree
		return B_BAD_VALUE;
	}

	// We now have gone down the root and save path if not saved.
	int level = fRoot->Levels() - 1;
	Volume* volume = fInode->GetVolume();
	status_t status;
	for (int i = 0; i < MAX_TREE_DEPTH && level >= 0; i++, level--) {
		uint64 requiredBlock = B_BENDIAN_TO_HOST_INT64(*ptrToNode);
		TRACE("requiredBlock:(%" B_PRIu64 ")\n", requiredBlock);
		if (path[i].blockNumber == requiredBlock) {
			// This block already has what we need
			if (path[i].type == 2)
				break;
			status = SearchOffsetInTreeNode(offset, &ptrToNode, i);
			if (status != B_OK)
				return status;
			continue;
		}
		// We do not have the block we need
		ssize_t len;
		if (level == 0) {
			// The size of buffer should be the directory block size
			len = fInode->DirBlockSize();
			TRACE("path node type:(%" B_PRId32 ")\n", path[i].type);
			if (path[i].type != 2) {
				// Size is not directory block size.
				delete path[i].blockData;
				path[i].type = 0;
				path[i].blockData = new(std::nothrow) char[len];
				if (path[i].blockData == NULL)
					return B_NO_MEMORY;
				path[i].type = 2;
			}
			uint64 readPos = fInode->FileSystemBlockToAddr(requiredBlock);
			if (read_pos(volume->Device(), readPos, path[i].blockData, len)
				!= len) {
				ERROR("FillPath::FillBlockBuffer(): IO Error");
				return B_IO_ERROR;
			}
			path[i].blockNumber = requiredBlock;
			break;
		}
		// The size of buffer should be the block size
		len = volume->BlockSize();
		if (path[i].type != 1) {
			delete path[i].blockData;
			path[i].type = 0;
			path[i].blockData = new(std::nothrow) char[len];
			if (path[i].blockData == NULL)
				return B_NO_MEMORY;
			path[i].type = 1;
		}
		uint64 readPos = fInode->FileSystemBlockToAddr(requiredBlock);
		if (read_pos(volume->Device(), readPos, path[i].blockData, len)
			!= len) {
			ERROR("FillPath::FillBlockBuffer(): IO Error");
			return B_IO_ERROR;
		}
		path[i].blockNumber = requiredBlock;
		status = SearchOffsetInTreeNode(offset, &ptrToNode, i);
		if (status != B_OK)
			return status;
	}

	return B_OK;
}


status_t
TreeDirectory::GetAllExtents()
{
	xfs_extnum_t noOfExtents = fInode->DataExtentsCount();

	ExtentMapUnwrap* extentsWrapped
		= new(std::nothrow) ExtentMapUnwrap[noOfExtents];
	if (extentsWrapped == NULL)
		return B_NO_MEMORY;

	ArrayDeleter<ExtentMapUnwrap> extentsWrappedDeleter(extentsWrapped);

	size_t maxRecords = MaxRecordsPossibleRoot();
	TRACE("Maxrecords: (%" B_PRIuSIZE ")\n", maxRecords);

	Volume* volume = fInode->GetVolume();
	ssize_t len = volume->BlockSize();

	uint16 levelsInTree = fRoot->Levels();
	status_t status = fInode->GetNodefromTree(levelsInTree, volume, len,
		fInode->DirBlockSize(), fSingleDirBlock);
	if (status != B_OK)
		return status;

	// We should be at the left most leaf node.
	// This could be a multilevel node type directory
	uint64 fileSystemBlockNo;
	while (1) {
		// Run till you have leaf blocks to checkout
		char* leafBuffer = fSingleDirBlock;
		if (!VerifyBlockHeader((LongBlock*)leafBuffer, leafBuffer)) {
			TRACE("Invalid Long Block");
			return B_BAD_VALUE;
		}

		uint32 offset = fInode->SizeOfLongBlock();
		int numRecs = ((LongBlock*)leafBuffer)->NumRecs();

		for (int i = 0; i < numRecs; i++) {
			extentsWrapped[fCountOfFilledExtents].first =
				*(uint64*)(leafBuffer + offset);
			extentsWrapped[fCountOfFilledExtents].second =
				*(uint64*)(leafBuffer + offset + sizeof(uint64));
			offset += sizeof(ExtentMapUnwrap);
			fCountOfFilledExtents++;
		}

		fileSystemBlockNo = ((LongBlock*)leafBuffer)->Right();
		TRACE("Next leaf is at: (%" B_PRIu64 ")\n", fileSystemBlockNo);
		if (fileSystemBlockNo == (uint64) -1)
			break;
		uint64 readPos = fInode->FileSystemBlockToAddr(fileSystemBlockNo);
		if (read_pos(volume->Device(), readPos, fSingleDirBlock, len)
				!= len) {
				ERROR("Extent::FillBlockBuffer(): IO Error");
				return B_IO_ERROR;
		}
	}
	TRACE("Total covered: (%" B_PRIu32 ")\n", fCountOfFilledExtents);

	status = UnWrapExtents(extentsWrapped);

	return status;
}


status_t
TreeDirectory::FillBuffer(char* blockBuffer, int howManyBlocksFurther,
	ExtentMapEntry* targetMap)
{
	TRACE("FILLBUFFER\n");
	ExtentMapEntry map;
	if (targetMap == NULL)
		map = fExtents[fCurMapIndex];
	if (targetMap != NULL)
		map = *targetMap;

	if (map.br_state != 0)
		return B_BAD_VALUE;

	ssize_t len = fInode->DirBlockSize();
	if (blockBuffer == NULL) {
		blockBuffer = new(std::nothrow) char[len];
		if (blockBuffer == NULL)
			return B_NO_MEMORY;
	}

	xfs_daddr_t readPos = fInode->FileSystemBlockToAddr(
		map.br_startblock + howManyBlocksFurther);

	if (read_pos(fInode->GetVolume()->Device(), readPos, blockBuffer, len)
		!= len) {
		ERROR("TreeDirectory::FillBlockBuffer(): IO Error");
		return B_IO_ERROR;
	}

	if (targetMap == NULL) {
		fSingleDirBlock = blockBuffer;
		ExtentDataHeader* header = CreateDataHeader(fInode, fSingleDirBlock);
		if (header == NULL)
			return B_NO_MEMORY;
		if (!VerifyHeader<ExtentDataHeader>(header, fSingleDirBlock, fInode,
				howManyBlocksFurther, &map, XFS_BTREE)) {
			ERROR("Invalid Data block");
			delete header;
			return B_BAD_VALUE;
		}
		delete header;
	}
	if (targetMap != NULL) {
		fSingleDirBlock = blockBuffer;
		/*
			This could be leaf or node block perform check for both
			based on magic number found.
		*/
		ExtentLeafHeader* leaf = CreateLeafHeader(fInode, fSingleDirBlock);
		if (leaf == NULL)
			return B_NO_MEMORY;

		if ((leaf->Magic() == XFS_DIR2_LEAFN_MAGIC
				|| leaf->Magic() == XFS_DIR3_LEAFN_MAGIC)
			&& !VerifyHeader<ExtentLeafHeader>(leaf, fSingleDirBlock, fInode,
				howManyBlocksFurther, &map, XFS_BTREE)) {
			TRACE("Leaf block invalid");
			delete leaf;
			return B_BAD_VALUE;
		}
		delete leaf;
		leaf = NULL;

		NodeHeader* node = CreateNodeHeader(fInode, fSingleDirBlock);
		if (node == NULL)
			return B_NO_MEMORY;

		if ((node->Magic() == XFS_DA_NODE_MAGIC
				|| node->Magic() == XFS_DA3_NODE_MAGIC)
			&& !VerifyHeader<NodeHeader>(node, fSingleDirBlock, fInode,
				howManyBlocksFurther, &map, XFS_BTREE)) {
			TRACE("Node block invalid");
			delete node;
			return B_BAD_VALUE;
		}
		delete node;
	}
	return B_OK;
}


int
TreeDirectory::EntrySize(int len) const
{
	int entrySize = sizeof(xfs_ino_t) + sizeof(uint8) + len + sizeof(uint16);
		// uint16 is for the tag
	if (fInode->HasFileTypeField())
		entrySize += sizeof(uint8);

	return ROUNDUP(entrySize, 8);
		// rounding off to next greatest multiple of 8
}


/*
 * Throw in the desired block number and get the index of it
 */
status_t
TreeDirectory::SearchMapInAllExtent(uint64 blockNo, uint32& mapIndex)
{
	ExtentMapEntry map;
	for (uint32 i = 0; i < fCountOfFilledExtents; i++) {
		map = fExtents[i];
		if (map.br_startoff <= blockNo
			&& (blockNo <= map.br_startoff + map.br_blockcount - 1)) {
			// Map found
			mapIndex = i;
			return B_OK;
		}
	}

	return B_ENTRY_NOT_FOUND;
}


status_t
TreeDirectory::GetNext(char* name, size_t* length, xfs_ino_t* ino)
{
	TRACE("TreeDirectory::GetNext\n");
	status_t status;
	if (fExtents == NULL) {
		status = GetAllExtents();
		if (status != B_OK)
			return status;
	}
	status = FillBuffer(fSingleDirBlock, 0);
	if (status != B_OK)
		return status;

	Volume* volume = fInode->GetVolume();

	void* entry; // This could be unused entry so we should check
	entry = (void*)(fSingleDirBlock + SizeOfDataHeader(fInode));

	uint32 blockNoFromAddress = BLOCKNO_FROM_ADDRESS(fOffset, volume);
	if (fOffset != 0 && blockNoFromAddress == fCurBlockNumber) {
		entry = (void*)(fSingleDirBlock
			+ BLOCKOFFSET_FROM_ADDRESS(fOffset, fInode));
		// This gets us a little faster to the next entry
	}

	uint32 curDirectorySize = fInode->Size();
	ExtentMapEntry& map = fExtents[fCurMapIndex];
	while (fOffset != curDirectorySize) {
		blockNoFromAddress = BLOCKNO_FROM_ADDRESS(fOffset, volume);

		TRACE("fOffset:(%" B_PRIu64 "), blockNoFromAddress:(%" B_PRIu32 ")\n",
			fOffset, blockNoFromAddress);
		if (fCurBlockNumber != blockNoFromAddress
			&& blockNoFromAddress > map.br_startoff
			&& blockNoFromAddress
				<= map.br_startoff + map.br_blockcount - 1) {
			// When the block is mapped in the same data
			// map entry but is not the first block
			status = FillBuffer(fSingleDirBlock,
				blockNoFromAddress - map.br_startoff);
			if (status != B_OK)
				return status;
			entry = (void*)(fSingleDirBlock + SizeOfDataHeader(fInode));
			fOffset = fOffset + SizeOfDataHeader(fInode);
			fCurBlockNumber = blockNoFromAddress;
		} else if (fCurBlockNumber != blockNoFromAddress) {
			// When the block isn't mapped in the current data map entry
			uint32 curMapIndex;
			status = SearchMapInAllExtent(blockNoFromAddress, curMapIndex);
			if (status != B_OK)
				return status;
			fCurMapIndex = curMapIndex;
			map = fExtents[fCurMapIndex];
			status = FillBuffer(fSingleDirBlock,
				blockNoFromAddress - map.br_startoff);
			if (status != B_OK)
				return status;
			entry = (void*)(fSingleDirBlock + SizeOfDataHeader(fInode));
			fOffset = fOffset + SizeOfDataHeader(fInode);
			fCurBlockNumber = blockNoFromAddress;
		}

		ExtentUnusedEntry* unusedEntry = (ExtentUnusedEntry*)entry;

		if (B_BENDIAN_TO_HOST_INT16(unusedEntry->freetag) == DIR2_FREE_TAG) {
			TRACE("Unused entry found\n");
			fOffset = fOffset + B_BENDIAN_TO_HOST_INT16(unusedEntry->length);
			entry = (void*)
				((char*)entry + B_BENDIAN_TO_HOST_INT16(unusedEntry->length));
			continue;
		}
		ExtentDataEntry* dataEntry = (ExtentDataEntry*) entry;

		uint16 currentOffset = (char*)dataEntry - fSingleDirBlock;
		TRACE("GetNext: fOffset:(%" B_PRIu64 "), currentOffset:(%" B_PRIu16 ")\n",
			BLOCKOFFSET_FROM_ADDRESS(fOffset, fInode), currentOffset);

		if (BLOCKOFFSET_FROM_ADDRESS(fOffset, fInode) > currentOffset) {
			entry = (void*)((char*)entry + EntrySize(dataEntry->namelen));
			continue;
		}

		if ((size_t)(dataEntry->namelen) >= *length)
			return B_BUFFER_OVERFLOW;

		fOffset = fOffset + EntrySize(dataEntry->namelen);
		memcpy(name, dataEntry->name, dataEntry->namelen);
		name[dataEntry->namelen] = '\0';
		*length = dataEntry->namelen + 1;
		*ino = B_BENDIAN_TO_HOST_INT64(dataEntry->inumber);

		TRACE("Entry found. Name: (%s), Length: (%" B_PRIuSIZE "), ino: (%" B_PRIu64 ")\n", name,
			*length, *ino);
		return B_OK;
	}

	return B_ENTRY_NOT_FOUND;
}


status_t
TreeDirectory::UnWrapExtents(ExtentMapUnwrap* extentsWrapped)
{
	fExtents = new(std::nothrow) ExtentMapEntry[fCountOfFilledExtents];
	if (fExtents == NULL)
		return B_NO_MEMORY;
	uint64 first, second;

	for (uint32 i = 0; i < fCountOfFilledExtents; i++) {
		first = B_BENDIAN_TO_HOST_INT64(extentsWrapped[i].first);
		second = B_BENDIAN_TO_HOST_INT64(extentsWrapped[i].second);
		fExtents[i].br_state = first >> 63;
		fExtents[i].br_startoff = (first & MASK(63)) >> 9;
		fExtents[i].br_startblock = ((first & MASK(9)) << 43) | (second >> 21);
		fExtents[i].br_blockcount = second & MASK(21);
	}

	return B_OK;
}


void
TreeDirectory::FillMapEntry(int num, ExtentMapEntry** fMap,
	int type, int pathIndex)
{
	void* pointerToMap;
	if (type == DATA) {
		char* base = fPathForData[pathIndex].blockData + BlockLen();
		off_t offset = num * EXTENT_SIZE;
		pointerToMap = (void*)(base + offset);
	} else {
		char* base = fPathForLeaves[pathIndex].blockData + BlockLen();
		off_t offset = num * EXTENT_SIZE;
		pointerToMap = (void*)(base + offset);
	}
	uint64 firstHalf = *((uint64*)pointerToMap);
	uint64 secondHalf = *((uint64*)pointerToMap + 1);
		//dividing the 128 bits into 2 parts.

	firstHalf = B_BENDIAN_TO_HOST_INT64(firstHalf);
	secondHalf = B_BENDIAN_TO_HOST_INT64(secondHalf);
	(*fMap)->br_state = firstHalf >> 63;
	(*fMap)->br_startoff = (firstHalf & MASK(63)) >> 9;
	(*fMap)->br_startblock = ((firstHalf & MASK(9)) << 43) | (secondHalf >> 21);
	(*fMap)->br_blockcount = secondHalf & MASK(21);
	TRACE("FillMapEntry: startoff:(%" B_PRIu64 "), startblock:(%" B_PRIu64 "),"
		"blockcount:(%" B_PRIu64 "),state:(%" B_PRIu8 ")\n", (*fMap)->br_startoff,
		(*fMap)->br_startblock,(*fMap)->br_blockcount, (*fMap)->br_state);
}


void
TreeDirectory::SearchForMapInDirectoryBlock(uint64 blockNo,
	int entries, ExtentMapEntry** map, int type, int pathIndex)
{
	TRACE("SearchForMapInDirectoryBlock: blockNo:(%" B_PRIu64 ")\n", blockNo);
	for (int i = 0; i < entries; i++) {
		FillMapEntry(i, map, type, pathIndex);
		if ((*map)->br_startoff <= blockNo
			&& (blockNo <= (*map)->br_startoff + (*map)->br_blockcount - 1)) {
			// Map found
			return;
		}
	}
	// Map wasn't found. Some kind of corruption. This is checked by caller.
	*map = NULL;
}


uint32
TreeDirectory::SearchForHashInNodeBlock(uint32 hashVal)
{
	NodeHeader* header = CreateNodeHeader(fInode, fSingleDirBlock);
	if (header == NULL)
		return B_NO_MEMORY;
	NodeEntry* entry = (NodeEntry*)(fSingleDirBlock + SizeOfNodeHeader(fInode));
	int count = header->Count();
	delete header;

	for (int i = 0; i < count; i++) {
		if (hashVal <= B_BENDIAN_TO_HOST_INT32(entry[i].hashval))
			return B_BENDIAN_TO_HOST_INT32(entry[i].before);
	}
	return 0;
}


status_t
TreeDirectory::Lookup(const char* name, size_t length, xfs_ino_t* ino)
{
	TRACE("TreeDirectory: Lookup\n");
	TRACE("Name: %s\n", name);
	uint32 hashValueOfRequest = hashfunction(name, length);
	TRACE("Hashval:(%" B_PRIu32 ")\n", hashValueOfRequest);

	Volume* volume = fInode->GetVolume();
	status_t status;

	ExtentMapEntry* targetMap = new(std::nothrow) ExtentMapEntry;
	if (targetMap == NULL)
		return B_NO_MEMORY;
	int pathIndex = -1;
	uint32 rightOffset = LEAF_STARTOFFSET(volume->BlockLog());

	// In node directories, the "node blocks" had a single level
	// Here we could have multiple levels. With each iteration of
	// the loop we go a level lower.
	while (rightOffset != fOffsetOfSingleDirBlock && 1) {
		status = SearchAndFillPath(rightOffset, LEAF);
		if (status != B_OK)
			return status;

		// The path should now have the Tree Leaf at appropriate level
		// Find the directory block in the path
		for (int i = 0; i < MAX_TREE_DEPTH; i++) {
			if (fPathForLeaves[i].type == 2) {
				pathIndex = i;
				break;
			}
		}
		if (pathIndex == -1) {
			// corrupt tree
			return B_BAD_VALUE;
		}

		// Get the node block from directory block
		// If level is non-zero, reiterate with new "rightOffset"
		// Else, we are at leaf block, then break
		LongBlock* curDirBlock
			= (LongBlock*)fPathForLeaves[pathIndex].blockData;

		if (!VerifyBlockHeader(curDirBlock, fPathForLeaves[pathIndex].blockData)) {
			TRACE("Invalid Long Block");
			return B_BAD_VALUE;
		}

		SearchForMapInDirectoryBlock(rightOffset, curDirBlock->NumRecs(),
			&targetMap, LEAF, pathIndex);
		if (targetMap == NULL)
			return B_BAD_VALUE;

		FillBuffer(fSingleDirBlock, rightOffset - targetMap->br_startoff,
			targetMap);
		fOffsetOfSingleDirBlock = rightOffset;
		ExtentLeafHeader* dirBlock = CreateLeafHeader(fInode, fSingleDirBlock);
		if (dirBlock == NULL)
			return B_NO_MEMORY;
		if (dirBlock->Magic() == XFS_DIR2_LEAFN_MAGIC
			|| dirBlock->Magic() == XFS_DIR3_LEAFN_MAGIC) {
			// Got the potential leaf. Break.
			delete dirBlock;
			break;
		}
		if (dirBlock->Magic() == XFS_DA_NODE_MAGIC
			|| dirBlock->Magic() == XFS_DA3_NODE_MAGIC) {
			rightOffset = SearchForHashInNodeBlock(hashValueOfRequest);
			if (rightOffset == 0)
				return B_ENTRY_NOT_FOUND;
			delete dirBlock;
			continue;
		}
	}
	// We now have the leaf block that might contain the entry we need.
	// Else go to the right subling if it might contain it. Else break.
	while (1) {
		ExtentLeafHeader* leafHeader
			= CreateLeafHeader(fInode, fSingleDirBlock);
		if (leafHeader == NULL)
			return B_NO_MEMORY;
		ExtentLeafEntry* leafEntry
			= (ExtentLeafEntry*)(fSingleDirBlock + SizeOfLeafHeader(fInode));

		int numberOfLeafEntries = leafHeader->Count();
		TRACE("numberOfLeafEntries:(%" B_PRId32 ")\n", numberOfLeafEntries);
		int left = 0;
		int mid;
		int right = numberOfLeafEntries - 1;

		// Trying to find the lowerbound of hashValueOfRequest
		// This is slightly different from bsearch(), as we want the first
		// instance of hashValueOfRequest and not any instance.
		while (left < right) {
			mid = (left + right) / 2;
			uint32 hashval = B_BENDIAN_TO_HOST_INT32(leafEntry[mid].hashval);
			if (hashval >= hashValueOfRequest) {
				right = mid;
				continue;
			}
			if (hashval < hashValueOfRequest) {
				left = mid + 1;
			}
		}
		TRACE("left:(%" B_PRId32 "), right:(%" B_PRId32 ")\n", left, right);
		uint32 nextLeaf = leafHeader->Forw();
		uint32 lastHashVal = B_BENDIAN_TO_HOST_INT32(
			leafEntry[numberOfLeafEntries - 1].hashval);

		while (B_BENDIAN_TO_HOST_INT32(leafEntry[left].hashval)
				== hashValueOfRequest) {
			uint32 address = B_BENDIAN_TO_HOST_INT32(leafEntry[left].address);
			if (address == 0) {
				left++;
				continue;
			}

			uint32 dataBlockNumber = BLOCKNO_FROM_ADDRESS(address * 8, volume);
			uint32 offset = BLOCKOFFSET_FROM_ADDRESS(address * 8, fInode);

			TRACE("BlockNumber:(%" B_PRIu32 "), offset:(%" B_PRIu32 ")\n", dataBlockNumber, offset);

			status = SearchAndFillPath(dataBlockNumber, DATA);
			int pathIndex = -1;
			for (int i = 0; i < MAX_TREE_DEPTH; i++) {
				if (fPathForData[i].type == 2) {
					pathIndex = i;
					break;
				}
			}
			if (pathIndex == -1)
				return B_BAD_VALUE;

			LongBlock* curDirBlock
				= (LongBlock*)fPathForData[pathIndex].blockData;

			SearchForMapInDirectoryBlock(dataBlockNumber,
				curDirBlock->NumRecs(), &targetMap, DATA, pathIndex);
			if (targetMap == NULL)
				return B_BAD_VALUE;

			FillBuffer(fSingleDirBlock,
				dataBlockNumber - targetMap->br_startoff, targetMap);
			fOffsetOfSingleDirBlock = dataBlockNumber;

			TRACE("offset:(%" B_PRIu32 ")\n", offset);
			ExtentDataEntry* entry
				= (ExtentDataEntry*)(fSingleDirBlock + offset);

			int retVal = strncmp(name, (char*)entry->name, entry->namelen);
			if (retVal == 0) {
				*ino = B_BENDIAN_TO_HOST_INT64(entry->inumber);
				TRACE("ino:(%" B_PRIu64 ")\n", *ino);
				return B_OK;
			}
			left++;
		}
		if (lastHashVal == hashValueOfRequest && nextLeaf != (uint32) -1) {
			// Go to forward neighbor. We might find an entry there.
			status = SearchAndFillPath(nextLeaf, LEAF);
			if (status != B_OK)
				return status;

			pathIndex = -1;
			for (int i = 0; i < MAX_TREE_DEPTH; i++) {
				if (fPathForLeaves[i].type == 2) {
					pathIndex = i;
					break;
				}
			}
			if (pathIndex == -1)
				return B_BAD_VALUE;

			LongBlock* curDirBlock
				= (LongBlock*)fPathForLeaves[pathIndex].blockData;

			SearchForMapInDirectoryBlock(nextLeaf, curDirBlock->NumRecs(),
				&targetMap, LEAF, pathIndex);
			if (targetMap == NULL)
				return B_BAD_VALUE;

			FillBuffer(fSingleDirBlock,
				nextLeaf - targetMap->br_startoff, targetMap);
			fOffsetOfSingleDirBlock = nextLeaf;

			continue;
		} else {
			break;
		}
		delete leafHeader;
	}
	return B_ENTRY_NOT_FOUND;
}