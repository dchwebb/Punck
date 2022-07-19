#pragma once

#include "diskio.h"
#include <string>


/*
 Date Format:
 Bits 0–4: Day of month, valid value range 1-31 inclusive.
 Bits 5–8: Month of year, 1 = January, valid value range 1–12 inclusive.
 Bits 9–15: Count of years from 1980, valid value range 0–127 inclusive (1980–2107).

 Time Format. A FAT directory entry time stamp is a 16-bit field that has a granularity of 2 seconds. Here is the format (bit 0 is the LSB of the 16-bit word, bit 15 is the MSB of the 16-bit word).

 Bits 0–4: 2-second count, valid value range 0–29 inclusive (0 – 58 seconds).
 Bits 5–10: Minutes, valid value range 0–59 inclusive.
 Bits 11–15: Hours, valid value range 0–23 inclusive.
 */
struct FATFileInfo {
	char name[11];					// Short name: If name[0] == 0xE5 directory entry is free (0x00 also means free and rest of directory is free)
	uint8_t attr;					// READ_ONLY 0x01; HIDDEN 0x02; SYSTEM 0x04; VOLUME_ID 0x08; DIRECTORY 0x10; ARCHIVE 0x20; LONG_NAME 0xF
	uint8_t NTRes;					// Reserved for use by Windows NT
	uint8_t createTimeTenth;		// File creation time in count of tenths of a second
	uint16_t createTime;			// Time file was created
	uint16_t createDate;			// Date file was created
	uint16_t accessedDate;			// Last access date
	uint16_t firstClusterHigh;		// High word of first cluster number (always 0 for a FAT12 or FAT16 volume)
	uint16_t writeTime;				// Time of last write. Note that file creation is considered a write
	uint16_t writeDate;				// Date of last write
	uint16_t firstClusterLow;		// Low word of this entry’s first cluster number.
	uint32_t fileSize;				// File size in bytes
};

struct FATLongFilename {
	uint8_t order;					// Order in sequence of long dir entries associated with the short dir entry at the end of the long dir set. If masked with 0x40 (LAST_LONG_ENTRY), this indicates the entry is the last long dir entry in a set of long dir entries
	char name1[10];					// Characters 1-5 of the long-name sub-component in this dir entry
	uint8_t attr;					// Attribute - must be 0xF
	uint8_t Type;					// If zero, indicates a directory entry that is a sub-component of a long name
	uint8_t checksum;				// Checksum of name in the short dir entry at the end of the long dir set
	char name2[12];					// Characters 6-11 of the long-name sub-component in this dir entry
	uint16_t firstClusterLow;		// Must be ZERO
	char name3[4];					// Characters 12-13 of the long-name sub-component in this dir entry
};

// Class provides interface with FatFS library and some low level helpers not provided with the library
class FatTools
{
public:
	uint32_t cacheDirty = 0;		// Bit array containing dirty sectors in cache
	void InitFatFS();
	void InvalidateFATCache();
	void GetFileInfo();
	void PrintFiles (char* path);
	uint8_t FlushCache();
private:
	std::string GetFileName(FATFileInfo* lfn);
	std::string GetAttributes(FATFileInfo* fi);
	std::string FileDate(uint16_t date);
};


extern FatTools fatTools;



