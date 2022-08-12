#include "USB.h"
#include "CDCHandler.h"
#include "ExtFlash.h"
#include "FatTools.h"
#include "Samples.h"
#include "NoteHandler.h"
#include "ff.h"

uint32_t flashBuff[1024];
uint32_t dmaTestBuffer[128];

void CDCHandler::DataIn()
{

}

// As this is called from an interrupt assign the command to a variable so it can be handled in the main loop
void CDCHandler::DataOut()
{
	uint32_t maxLen = std::min((uint32_t)CDC_CMD_LEN - 1, outBuffCount);
	strncpy(comCmd, (char*)outBuff, CDC_CMD_LEN);
	comCmd[maxLen] = '\0';
	cmdPending = true;
}


void CDCHandler::ClassSetup(usbRequest& req)
{
	if (req.RequestType == DtoH_Class_Interface && req.Request == GetLineCoding) {
		SetupIn(req.Length, (uint8_t*)&lineCoding);
	}

	if (req.RequestType == HtoD_Class_Interface && req.Request == SetLineCoding) {
		// Prepare to receive line coding data in ClassSetupData
		usb->classPendingData = true;
		EndPointTransfer(Direction::out, 0, req.Length);
	}
}


void CDCHandler::ClassSetupData(usbRequest& req, const uint8_t* data)
{
	// ClassSetup passes instruction to set line coding - this is the data portion where the line coding is transferred
	if (req.RequestType == HtoD_Class_Interface && req.Request == SetLineCoding) {
		lineCoding = *(LineCoding*)data;
	}
}


int32_t CDCHandler::ParseInt(const std::string cmd, const char precedingChar, int low = 0, int high = 0) {
	int32_t val = -1;
	int8_t pos = cmd.find(precedingChar);		// locate position of character preceding
	if (pos >= 0 && std::strspn(cmd.substr(pos + 1).c_str(), "0123456789-") > 0) {
		val = stoi(cmd.substr(pos + 1));
	}
	if (high > low && (val > high || val < low)) {
		usb->SendString("Must be a value between " + std::to_string(low) + " and " + std::to_string(high) + "\r\n");
		return low - 1;
	}
	return val;
}


void CDCHandler::ProcessCommand()
{
	if (!cmdPending) {
		return;
	}

	std::string cmd = std::string(comCmd);
	if (cmd.compare("help\n") == 0) {
		usb->SendString("Mountjoy Punck\r\n"
				"\r\nSupported commands:\r\n"
				"info        -  Show diagnostic information\r\n"
				"resume      -  Resume I2S after debugging\r\n"
				"readreg     -  Print QSPI flash status registers\r\n"
				"printflash:A   Print 512 bytes of flash (A = decimal address)\r\n"
				"eraseflash  -  Erase all flash data\r\n"
				"dirdetails  -  Print detailed file list for root directory\r\n"
				"dir         -  Print list of all files and their directories\r\n"
				"flushcache  -  Flush any changed data in cache to flash\r\n"
				"cacheinfo   -  Show all bytes changed in header cache\r\n"
				"samplelist  -  Show details of all samples found in flash\r\n"
				"play:NN     -  Play the sample numbered NN\r\n"
				"clusterchain   List chain of clusters\r\n"
				"midimap     -  Display MIDI note mapping\r\n"
				"\r\n"

		);


	} else if (cmd.compare("midimap\n") == 0) {					// Display MIDI note mapping
		for (auto note : noteHandler.noteMapper) {
			switch (note.voice) {
			case NoteHandler::kick:
				printf("Kick");
				break;
			case NoteHandler::snare:
				printf("Snare");
				break;
			case NoteHandler::hatClosed:
				printf("Close HH");
				break;
			case NoteHandler::hatOpen:
				printf("Open HH");
				break;
			case NoteHandler::tomHigh:
				printf("High Tom");
				break;
			case NoteHandler::tomMedium:
				printf("Mid Tom");
				break;
			case NoteHandler::tomLow:
				printf("Low Tom");
				break;
			case NoteHandler::sampler1:
				printf("Sample 1");
				break;
			case NoteHandler::sampler2:
				printf("Sample 2");
				break;
			}
			printf(": %d, %d\r\n", note.midiLow, note.midiHigh);
		}


	} else if (cmd.compare("samplelist\n") == 0) {				// Prints sample list
		uint32_t pos = 0;

		printf("Num Name          Bytes    Rate Bits Channels Valid Address    Seconds\r\n");

		while (samples.sampleList[pos].name[0] != 0) {
			printf("%3lu %.11s %7lu %7lu %4u %8u %s     0x%08x %.3f\r\n",
					pos,
					samples.sampleList[pos].name,
					samples.sampleList[pos].size,
					samples.sampleList[pos].sampleRate,
					samples.sampleList[pos].byteDepth * 8,
					samples.sampleList[pos].channels,
					samples.sampleList[pos].valid ? "Y" : " ",
					(unsigned int)samples.sampleList[pos].startAddr,
					(float)samples.sampleList[pos].sampleCount / samples.sampleList[pos].sampleRate
					);
			++pos;
		}
		printf("\r\n");


	} else if (cmd.compare(0, 5, "play:") == 0) {				// Play sample
		int sn = ParseInt(cmd, ':', 0, 0xFFFFFF);
		printf("%s\r\n", samples.sampleList[sn].name);
		samples.Play(sn);


	} else if (cmd.compare("dir\n") == 0) {						// Get basic FAT directory list
		char workBuff[256];
		strcpy(workBuff, "/");

		fatTools.InvalidateFatFSCache();						// Ensure that the FAT FS cache is updated
		fatTools.PrintFiles(workBuff);


	} else if (cmd.compare("dirdetails\n") == 0) {				// Get detailed FAT directory info
		fatTools.PrintDirInfo();


	} else if (cmd.compare("clusterchain\n") == 0) {			// Print used clusters with links from FAT area
		printf("Cluster | Link\r\n");
		uint32_t cluster = 0;
		while (fatTools.clusterChain[cluster]) {
			printf("%7lu   0x%04x\r\n", cluster, fatTools.clusterChain[cluster]);
			++cluster;
		}


	} else if (cmd.compare("cacheinfo\n") == 0) {				// Basic counts of differences between cache and Flash
		uint32_t count = 0;
		uint8_t oldCache = 0, oldFlash = 0;
		bool skipDuplicates = false;

		for (uint32_t blk = 0; blk < (fatCacheSectors / fatEraseSectors); ++blk) {

			// Check if block is actually dirty or clean
			uint32_t dirtyBytes = 0, firstDirtyByte = 0, lastDirtyByte = 0;
			for (uint32_t byte = 0; byte < (fatEraseSectors * fatSectorSize); ++byte) {
				uint32_t offset = (blk * fatEraseSectors * fatSectorSize) + byte;
				if (fatTools.headerCache[offset] != flashAddress[offset]) {
					++dirtyBytes;
					if (firstDirtyByte == 0) {
						firstDirtyByte = offset;
					}
					lastDirtyByte = offset;
				}
			}

			bool blockDirty = (fatTools.dirtyCacheBlocks & (1 << blk));
			printf("Block %2lu: %s  Dirty bytes: %lu from %lu to %lu\r\n",
					blk, (blockDirty ? "dirty" : "     "), dirtyBytes, firstDirtyByte, lastDirtyByte);

		}

		// the write cache holds any blocks currently being written to to avoid multiple block erasing when writing large data
		if (fatTools.writeCacheDirty) {
			uint32_t dirtyBytes = 0, firstDirtyByte = 0, lastDirtyByte = 0;
			for (uint32_t byte = 0; byte < (fatEraseSectors * fatSectorSize); ++byte) {
				uint32_t offset = (fatTools.writeBlock * fatEraseSectors * fatSectorSize) + byte;
				if (fatTools.writeBlockCache[byte] != flashAddress[offset]) {
					++dirtyBytes;
					if (firstDirtyByte == 0) {
						firstDirtyByte = offset;
					}
					lastDirtyByte = offset;
				}
			}

			printf("Block %2lu: %s  Dirty bytes: %lu from %lu to %lu\r\n",
					fatTools.writeBlock, (fatTools.writeCacheDirty ? "dirty" : "     "), dirtyBytes, firstDirtyByte, lastDirtyByte);

		}


	} else if (cmd.compare("cachechanges\n") == 0) {			// List bytes that are different in cache to Flash
		uint32_t count = 0;
		uint8_t oldCache = 0, oldFlash = 0;
		bool skipDuplicates = false;

		for (uint32_t i = 0; i < (fatCacheSectors * fatSectorSize); ++i) {

			if (flashAddress[i] != fatTools.headerCache[i]) {					// Data has changed
				if (oldCache == fatTools.headerCache[i] && oldFlash == flashAddress[i] && i > 0) {
					if (!skipDuplicates) {
						printf("...\r\n");						// Print continuation mark
						skipDuplicates = true;
					}
				} else {
					printf("%5lu c: 0x%02x f: 0x%02x\r\n", i, fatTools.headerCache[i], flashAddress[i]);
				}

				oldCache = fatTools.headerCache[i];
				oldFlash = flashAddress[i];
				++count;
			} else {
				if (skipDuplicates) {
					printf("%5lu c: 0x%02x f: 0x%02x\r\n", i - 1, oldCache, oldFlash);
					skipDuplicates = false;
				}
			}

		}

		printf("Found %lu different bytes\r\n", count);



	} else if (cmd.compare(0, 11, "printflash:") == 0) {		// QSPI flash: print memory mapped data
		int address = ParseInt(cmd, ':', 0, 0xFFFFFF);
		if (address >= 0) {
			unsigned int* p = (unsigned int*)(0x90000000 + address);

			for (uint8_t a = 0; a < 128; a += 4) {
				printf("%6d: %#010x %#010x %#010x %#010x\r\n", (a * 4) + address, p[a], p[a + 1], p[a + 2], p[a + 3]);
			}
		}


	} else if (cmd.compare(0, 13, "printcluster:") == 0) {		// QSPI flash: print memory mapped data
		int cluster = ParseInt(cmd, ':', 0, 0xFFFFFF);
		if (cluster >= 0) {
			printf("Cluster %d:\r\n", cluster);

			unsigned int* p = (unsigned int*)(fatTools.GetClusterAddr(cluster));

			for (uint8_t a = 0; a < 128; a += 4) {
				printf("0x%08lx: %#010x %#010x %#010x %#010x\r\n", (a * 4) + (uint32_t)p, p[a], p[a + 1], p[a + 2], p[a + 3]);
			}
		}


	} else if (cmd.compare(0, 7, "setzero") == 0) {				// Set data at address to 0 [A = address; W = num words]
		int address = ParseInt(cmd, 'o', 0, 0xFFFFFF);
		if (address >= 0) {
			int words = ParseInt(cmd, ':');
			printf("Clearing %d words at %d ...\r\n", words, address);

			for (int a = 0; a < words; ++a) {
				flashBuff[a] = 0;
			}
			extFlash.WriteData(address, flashBuff, words);
			extFlash.MemoryMapped();
			printf("Finished\r\n");
		}


	} else if (cmd.compare("flushcache\n") == 0) {				// Flush FAT cache to Flash
		uint8_t sectors = fatTools.FlushCache();
		printf("%i blocks flushed\r\n", sectors);
		extFlash.MemoryMapped();


	} else if (cmd.compare("eraseflash\n") == 0) {				// Erase all flash memory
		extFlash.FullErase();
		usb->SendString("Flash erased\r\n");


	} else if (cmd.compare(0, 10, "erasesect:") == 0) {			// Erase sector of flash memory
		int address = ParseInt(cmd, ':', 0, 0xFFFFFF);
		if (address >= 0) {
			extFlash.BlockErase(address);
			usb->SendString("Sector erased\r\n");
		}
		extFlash.MemoryMapped();


	} else if (cmd.compare("readreg\n") == 0) {					// Read QSPI register
		usb->SendString("Status register 1: " + std::to_string(extFlash.ReadStatus(ExtFlash::readStatusReg1)) +
				"\r\nStatus register 2: " + std::to_string(extFlash.ReadStatus(ExtFlash::readStatusReg2)) +
				"\r\nStatus register 3: " + std::to_string(extFlash.ReadStatus(ExtFlash::readStatusReg3)) + "\r\n");
		extFlash.MemoryMapped();


	} else if (cmd.compare(0, 12, "writesector:") == 0) {		// Write 1 sector of test data: format writesector:S [S = sector]
		int sector = ParseInt(cmd, ':', 0, 0xFFFFFF);
		if (sector >= 0) {
			printf("Writing to %d ...\r\n", sector);

			for (uint32_t a = 0; a < fatSectorSize; ++a) {
				flashBuff[a] = a + 1;
			}
			fatTools.Write((uint8_t*)flashBuff, sector, 1);

			printf("Finished\r\n");
		}


	} else if (cmd.compare(0, 5, "write") == 0) {				// Write test pattern to flash writeA:W [A = address; W = num words]
		int address = ParseInt(cmd, 'e', 0, 0xFFFFFF);
		if (address >= 0) {
			int words = ParseInt(cmd, ':');
			printf("Writing %d words to %d ...\r\n", words, address);

			for (int a = 0; a < words; ++a) {
				flashBuff[a] = a + 1;
			}
			extFlash.WriteData(address, flashBuff, words);

			extFlash.MemoryMapped();
			printf("Finished\r\n");
		}


	} else if (cmd.compare(0, 5, "read:") == 0) {				// Read QSPI data (format read:A where A is address)
		int address = ParseInt(cmd, ':', 0, 0xFFFFFF);
		if (address >= 0) {
			printf("Data Read: %#010x\r\n", (unsigned int)extFlash.FastRead(address));
		}


	} else {
		usb->SendString("Unrecognised command: " + cmd + "Type 'help' for supported commands\r\n");
	}
	cmdPending = false;

}




float CDCHandler::ParseFloat(const std::string cmd, const char precedingChar, float low = 0.0, float high = 0.0) {
	float val = -1.0f;
	int8_t pos = cmd.find(precedingChar);		// locate position of character preceding
	if (pos >= 0 && std::strspn(cmd.substr(pos + 1).c_str(), "0123456789.") > 0) {
		val = stof(cmd.substr(pos + 1));
	}
	if (high > low && (val > high || val < low)) {
		usb->SendString("Must be a value between " + std::to_string(low) + " and " + std::to_string(high) + "\r\n");
		return low - 1.0f;
	}
	return val;
}
