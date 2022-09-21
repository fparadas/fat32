#include <stdlib.h>
#include <stdio.h>

struct BS_BPB {
	uint8_t BS_jmpBoot[3]; //1-3
	uint8_t BS_OEMName[8]; //4-11
	uint16_t BPB_BytsPerSec;//12-13
	uint8_t BPB_SecPerClus; //14
	uint16_t BPB_RsvdSecCnt; //15-16
	uint8_t BPB_NumFATs; //17
	uint16_t BPB_RootEntCnt; //18-19
	uint16_t BPB_TotSec16; //20-21
	uint8_t BPB_Media; //22
	uint16_t BPB_FATSz16; //23-24
	uint16_t BPB_SecPerTrk; //25-26
	uint16_t BPB_NumHeads; //27-28
	uint32_t BPB_HiddSec; //29-32
	uint32_t BPB_TotSec32; //33-36
	uint32_t BPB_FATSz32 ; //37-40
	uint16_t BPB_ExtFlags ; //41-42
	uint16_t BPB_FSVer ; //43-44
	uint32_t BPB_RootClus ;//45-48
	uint16_t BPB_FSInfo ; //49-50
	uint16_t BPB_BkBootSec ;//51-52
	uint8_t BPB_Reserved[12]; //53-64
	uint8_t BS_DrvNum ;//65
	uint8_t BS_Reserved1 ;//66
	uint8_t BS_BootSig ;//67
	uint32_t BS_VolID ;//68-71
	uint8_t BS_VolLab[11]; //72-82
	uint8_t BS_FilSysType[8]; //83-89


} __attribute__((packed));

struct DIR_ENTRY {
    uint8_t DIR_Name[11];
    uint8_t DIR_Attr;
    uint8_t DIR_NTRes;
    uint8_t DIR_CrtTimeTenth;
    uint16_t DIR_CrtTime;
    uint16_t DIR_CrtDate;
    uint16_t DIR_LstAccDate;
    uint8_t DIR_FstClusHI[2];
    uint16_t DIR_WrtTime;
    uint16_t DIR_WrtDate;
    uint8_t DIR_FstClusLO[2];
    uint32_t DIR_FileSize;
} __attribute__((packed));

// funções disponibilizadas para mexer em arquivos
int ffopen();
int ffclose();
int ffseek();
int ffread();
int ffwrite();
