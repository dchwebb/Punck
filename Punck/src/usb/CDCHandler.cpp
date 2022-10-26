#include "USB.h"
#include "CDCHandler.h"
#include "ExtFlash.h"
#include "FatTools.h"
#include "Samples.h"
#include "VoiceManager.h"
#include "ff.h"

uint32_t flashBuff[1024];

void CDCHandler::DataIn()
{
	if (inBuffSize > 0 && inBuffSize % USB::ep_maxPacket == 0) {
		inBuffSize = 0;
		EndPointTransfer(Direction::in, inEP, 0);				// Fixes issue transmitting an exact multiple of max packet size (n x 64)
	}
}


// As this is called from an interrupt assign the command to a variable so it can be handled in the main loop
void CDCHandler::DataOut()
{
	// Check if sufficient space in command buffer
	const uint32_t newCharCnt = std::min(outBuffCount, maxCmdLen - 1 - buffPos);

	strncpy(&comCmd[buffPos], (char*)outBuff, newCharCnt);
	buffPos += newCharCnt;

	// Check if cr has been sent yet
	if (comCmd[buffPos - 1] == 13 || comCmd[buffPos - 1] == 10 || buffPos == maxCmdLen - 1) {
		comCmd[buffPos - 1] = '\0';
		cmdPending = true;
		buffPos = 0;
	}
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


int32_t CDCHandler::ParseInt(const std::string_view cmd, const char precedingChar, const int32_t low = 0, const int32_t high = 0) {
	int32_t val = -1;
	const int8_t pos = cmd.find(precedingChar);		// locate position of character preceding
	if (pos >= 0 && std::strspn(&cmd[pos + 1], "0123456789-") > 0) {
		val = std::stoi(&cmd[pos + 1]);
	}
	if (high > low && (val > high || val < low)) {
		printf("Must be a value between %ld and %ld\r\n", low, high);
		return low - 1;
	}
	return val;
}


void CDCHandler::ProcessCommand()
{
	if (!cmdPending) {
		return;
	}

	std::string_view cmd {comCmd};

	if (cmd.compare("help") == 0) {
		usb->SendString("Mountjoy Punck\r\n"
				"\r\nSupported commands:\r\n"
				"resume      -  Resume I2S after debugging\r\n"
				"readreg     -  Print QSPI flash status registers\r\n"
				"flashid     -  Print flash manufacturer and device IDs\r\n"
				"printflash:A   Print 512 bytes of flash (A = decimal address)\r\n"
				"printcluster:A Print 2048 bytes of cluster address A (>=2)\r\n"
				"eraseflash  -  Erase all flash data\r\n"
				"dirdetails  -  Print detailed file list for root directory\r\n"
				"dir         -  Print list of all files and their directories\r\n"
				"flushcache  -  Flush any changed data in cache to flash\r\n"
				"cacheinfo   -  Show all bytes changed in header cache\r\n"
				"samplelist  -  Show details of all samples found in flash\r\n"
				"play:NN     -  Play the sample numbered NN\r\n"
				"clusterchain   List chain of clusters\r\n"
				"midimap     -  Display MIDI note mapping\r\n"
				".."

		);


#if (USB_DEBUG)
	} else if (cmd.compare("usbdebug") == 0) {					// Activate USB debugging
		extern volatile bool debugStart;
		debugStart = true;
#endif

	} else if (cmd.compare("midimap") == 0) {					// Display MIDI note mapping
		for (auto note : voiceManager.noteMapper) {
			switch (note.voice) {
			case VoiceManager::kick:
				printf("Kick    ");
				break;
			case VoiceManager::snare:
				printf("Snare   ");
				break;
			case VoiceManager::hihat:
				printf("Close HH");
				break;
			case VoiceManager::samplerA:
				printf("Sample 1");
				break;
			case VoiceManager::samplerB:
				printf("Sample 2");
				break;
			}
			printf(" : %3d, %3d\r\n", note.midiLow, note.midiHigh);
		}

	} else if (cmd.compare("readreg") == 0) {					// Read QSPI register
		usb->SendString("Status register 1: " + std::to_string(extFlash.ReadStatus(ExtFlash::readStatusReg1)) +
				"\r\nStatus register 2: " + std::to_string(extFlash.ReadStatus(ExtFlash::readStatusReg2)) +
				"\r\nStatus register 3: " + std::to_string(extFlash.ReadStatus(ExtFlash::readStatusReg3)) + "\r\n");
		extFlash.MemoryMapped();


	} else if (cmd.compare("flashid") == 0) {					// Get manufacturer and device ID
		const uint16_t flashID = extFlash.GetID();
		printf("Manufacturer ID: %#04x; Device ID: %#04x\r\n", flashID & 0xFF, flashID >> 8);
		extFlash.MemoryMapped();


	} else if (cmd.compare("samplelist") == 0) {				// Prints sample list
		uint32_t pos = 0;

		printf("Num Name          Bytes    Rate Bits Channels Valid Address    Seconds\r\n");

		while (voiceManager.samples.sampleList[pos].name[0] != 0) {
			printf("%3lu %.11s %7lu %7lu %4u %8u %s     0x%08x %.3f\r\n",
					pos,
					voiceManager.samples.sampleList[pos].name,
					voiceManager.samples.sampleList[pos].size,
					voiceManager.samples.sampleList[pos].sampleRate,
					voiceManager.samples.sampleList[pos].byteDepth * 8,
					voiceManager.samples.sampleList[pos].channels,
					voiceManager.samples.sampleList[pos].valid ? "Y" : " ",
					(unsigned int)voiceManager.samples.sampleList[pos].startAddr,
					(float)voiceManager.samples.sampleList[pos].sampleCount / voiceManager.samples.sampleList[pos].sampleRate
					);
			++pos;
		}
		printf("\r\n");


	} else if (cmd.compare(0, 5, "play:") == 0) {				// Play sample
		const int32_t sn = ParseInt(cmd, ':', 0, 0xFFFFFF);
		printf("%s\r\n", voiceManager.samples.sampleList[sn].name);
		voiceManager.samples.Play(voiceManager.samples.SamplePlayer::playerA, sn);


	} else if (cmd.compare("dirdetails") == 0) {				// Get detailed FAT directory info
		fatTools.PrintDirInfo();


	} else if (cmd.compare(0, 5, "read:") == 0) {				// Read QSPI data (format read:A where A is address)
		const int32_t address = ParseInt(cmd, ':', 0, 0xFFFFFF);
		if (address >= 0) {
			printf("Data Read: %#010lx\r\n", extFlash.FastRead(address));
		}




	//--------------------------------------------------------------------------------------------------
	// All flash commands that rely on memory mapped data to follow this guard
	} else if (extFlash.flashCorrupt) {
		printf("** Flash Corrupt **\r\n");





	} else if (cmd.compare("dir") == 0) {						// Get basic FAT directory list
		if (fatTools.noFileSystem) {
			printf("** No file System **\r\n");
		} else {
			char workBuff[256];
			strcpy(workBuff, "/");

			fatTools.InvalidateFatFSCache();					// Ensure that the FAT FS cache is updated
			fatTools.PrintFiles(workBuff);
		}



	} else if (cmd.compare("clusterchain") == 0) {			// Print used clusters with links from FAT area
		printf("Cluster | Link\r\n");
		uint32_t cluster = 0;
		while (fatTools.clusterChain[cluster]) {
			printf("%7lu   0x%04x\r\n", cluster, fatTools.clusterChain[cluster]);
			++cluster;
		}


	} else if (cmd.compare("cacheinfo") == 0) {				// Basic counts of differences between cache and Flash
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

			const bool blockDirty = (fatTools.dirtyCacheBlocks & (1 << blk));
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


	} else if (cmd.compare("cachechanges") == 0) {			// List bytes that are different in cache to Flash
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


	} else if (cmd.compare(0, 11, "printflash:") == 0) {		// QSPI flash: print 512 byres of memory mapped data
		const int32_t address = ParseInt(cmd, ':', 0, 0xFFFFFF);
		if (address >= 0) {
			const uint32_t* p = (uint32_t*)(0x90000000 + address);

			for (uint32_t a = 0; a < 128; a += 4) {
				printf("%6ld: %#010lx %#010lx %#010lx %#010lx\r\n", (a * 4) + address, p[a], p[a + 1], p[a + 2], p[a + 3]);
			}
		}


	} else if (cmd.compare(0, 13, "printcluster:") == 0) {		// QSPI flash: print memory mapped data
		const int32_t cluster = ParseInt(cmd, ':', 2, 0xFFFFFF);
		if (cluster >= 2) {
			printf("Cluster %ld:\r\n", cluster);

			const uint32_t* p = (uint32_t*)(fatTools.GetClusterAddr(cluster));

			for (uint32_t a = 0; a < 512; a += 4) {
				printf("0x%08lx: %#010lx %#010lx %#010lx %#010lx\r\n", (a * 4) + (uint32_t)p, p[a], p[a + 1], p[a + 2], p[a + 3]);
			}
		}


	} else if (cmd.compare(0, 7, "setzero") == 0) {				// Set data at address to 0 [A = address; W = num words]
		const int32_t address = ParseInt(cmd, 'o', 0, 0xFFFFFF);
		if (address >= 0) {
			const uint32_t words = ParseInt(cmd, ':', 0, 0xFFFFFF);
			printf("Clearing %ld words at %ld ...\r\n", words, address);

			for (uint32_t a = 0; a < words; ++a) {
				flashBuff[a] = 0;
			}
			extFlash.WriteData(address, flashBuff, words);
			extFlash.MemoryMapped();
			printf("Finished\r\n");
		}


	} else if (cmd.compare("flushcache") == 0) {				// Flush FAT cache to Flash
		const uint8_t sectors = fatTools.FlushCache();
		printf("%i blocks flushed\r\n", sectors);
		extFlash.MemoryMapped();


	} else if (cmd.compare("eraseflash") == 0) {				// Erase all flash memory
		extFlash.FullErase();
		usb->SendString("Flash erased\r\n");


	} else if (cmd.compare(0, 10, "erasesect:") == 0) {			// Erase sector of flash memory
		int32_t address = ParseInt(cmd, ':', 0, 0xFFFFFF);
		if (address >= 0) {
			extFlash.BlockErase(address);
			usb->SendString("Sector erased\r\n");
		}
		extFlash.MemoryMapped();


	} else if (cmd.compare(0, 12, "writesector:") == 0) {		// Write 1 sector of test data: format writesector:S [S = sector]
		const int32_t sector = ParseInt(cmd, ':', 0, 0xFFFFFF);
		if (sector >= 0) {
			printf("Writing to %ld ...\r\n", sector);

			for (uint32_t a = 0; a < fatSectorSize; ++a) {
				flashBuff[a] = a + 1;
			}
			fatTools.Write((uint8_t*)flashBuff, sector, 1);

			printf("Finished\r\n");
		}


	} else if (cmd.compare(0, 5, "write") == 0) {				// Write test pattern to flash writeA:W [A = address; W = num words]
		const int32_t address = ParseInt(cmd, 'e', 0, 0xFFFFFF);
		if (address >= 0) {
			const uint32_t words = ParseInt(cmd, ':');
			printf("Writing %ld words to %ld ...\r\n", words, address);

			for (uint32_t a = 0; a < words; ++a) {
				flashBuff[a] = a + 1;
			}
			extFlash.WriteData(address, flashBuff, words);

			extFlash.MemoryMapped();
			printf("Finished\r\n");

		}



	} else {
		//usb->SendString("Unrecognised command: " + cmd + "\r\nType 'help' for supported commands\r\n");
		printf("Unrecognised command: %s\r\nType 'help' for supported commands\r\n", cmd.data());
	}
	cmdPending = false;

}




float CDCHandler::ParseFloat(const std::string_view cmd, const char precedingChar, const float low = 0.0f, const float high = 0.0f) {
	float val = -1.0f;
	const int8_t pos = cmd.find(precedingChar);		// locate position of character preceding
	if (pos >= 0 && std::strspn(&cmd[pos + 1], "0123456789.") > 0) {
		val = std::stof(&cmd[pos + 1]);
	}
	if (high > low && (val > high || val < low)) {
		printf("Must be a value between %f and %f\r\n", low, high);
		return low - 1.0f;
	}
	return val;
}
