#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>


#define TF_MAX_PATH 256
#define TF_FILE_HANDLES 5

#define TF_FLAG_DIRTY 0x01
#define TF_FLAG_OPEN 0x02
#define TF_FLAG_SIZECHANGED 0x04
#define TF_FLAG_ROOT 0x08

#define TYPE_FAT12 0
#define TYPE_FAT16 1
#define TYPE_FAT32 2

#define TF_MARK_BAD_CLUSTER32 0x0ffffff7
#define TF_MARK_BAD_CLUSTER16 0xfff7
#define TF_MARK_EOC32 0x0ffffff8
#define TF_MARK_EOC16 0xfff8

#define LFN_ENTRY_CAPACITY 13       // bytes per LFN entry

#define TF_ATTR_DIRECTORY 0x10

#define LSN(CN, bpb) SSA + ((CN-2) * bpb->SectorsPerCluster)

#ifndef min
#define min(x,y)  (x<y)? x:y  
#define max(x,y)  (x>y)? x:y  
#endif

    
// Ultimately, once the filesystem is checked for consistency, you only need a few
// things to keep it up and running.  These are:
// 1) The type (fat16 or fat32, no fat12 support)
// 2) The number of sectors per cluster
// 3) Everything needed to compute indices into the FATs, which includes:
//    * Bytes per sector, which is fixed at 512
//    * The number of reserved sectors (pulled directly from the BPB)
// 4) The current sector in memory.  No sense reading it if it's already in memory!

typedef struct struct_tfinfo {
    // FILESYSTEM INFO PROPER
    uint8_t type; // 0 for FAT16, 1 for FAT32.  FAT12 NOT SUPPORTED
    uint8_t sectorsPerCluster;
    uint32_t firstDataSector;
    uint32_t totalSectors;
    uint16_t reservedSectors;
    // "LIVE" DATA
    uint32_t currentSector;
    uint8_t sectorFlags;
    uint32_t rootDirectorySize;
    uint8_t buffer[512];
} TFInfo;

/////////////////////////////////////////////////////////////////////////////////

typedef struct struct_TFFILE {
    uint32_t parentStartCluster;
    uint32_t startCluster;
    uint32_t currentClusterIdx;
    uint32_t currentCluster;
    short currentSector;
    short currentByte;
    uint32_t pos;
    uint8_t flags;
    uint8_t attributes;
    uint8_t mode;
    uint32_t size;
    uint8_t filename[TF_MAX_PATH];
} TFFile;

typedef struct struct_BPBFAT32_struct {
    uint32_t    FATSize;             // 4
    uint16_t    ExtFlags;              // 2
    uint16_t    FSVersion;             // 2
    uint32_t    RootCluster;           // 4
    uint16_t    FSInfo;                // 2
    uint16_t    BkBootSec;             // 2
    uint8_t     Reserved[12];          // 12
    uint8_t     BS_DriveNumber;            // 1
    uint8_t     BS_Reserved1;              // 1
    uint8_t     BS_BootSig;                // 1
    uint32_t    BS_VolumeID;               // 4
    uint8_t     BS_VolumeLabel[11];        // 11
    uint8_t     BS_FileSystemType[8];      // 8
} BPB32_struct;

typedef struct struct_BPB_struct {
    uint8_t     BS_JumpBoot[3];            // 3
    uint8_t     BS_OEMName[8];             // 8
    uint16_t    BytesPerSector;        // 2
    uint8_t     SectorsPerCluster;     // 1
    uint16_t    ReservedSectorCount;   // 2
    uint8_t     NumFATs;               // 1
    uint16_t    RootEntryCount;        // 2
    uint16_t    TotalSectors16;        // 2
    uint8_t     Media;                 // 1
    uint16_t    FATSize16;             // 2
    uint16_t    SectorsPerTrack;       // 2
    uint16_t    NumberOfHeads;         // 2
    uint32_t    HiddenSectors;         // 4
    uint32_t    TotalSectors32;        // 4
    BPB32_struct fat32;
} BPB_struct;

typedef struct struct_FatFile83 {
    uint8_t filename[8];
    uint8_t extension[3];
    uint8_t attributes;
    uint8_t reserved;
    uint8_t creationTimeMs;
    uint16_t creationTime;
    uint16_t creationDate;
    uint16_t lastAccessTime;
    uint16_t eaIndex;
    uint16_t modifiedTime;
    uint16_t modifiedDate;
    uint16_t firstCluster;
    uint32_t fileSize;
} FatFile83;

typedef struct struct_FatFileLFN {
    uint8_t sequence_number;
    uint16_t name1[5];      // 5 Chars of name (UTF 16???)
    uint8_t attributes;     // Always 0x0f
    uint8_t reserved;       // Always 0x00
    uint8_t checksum;       // Checksum of DOS Filename.  See Docs.
    uint16_t name2[6];      // 6 More chars of name (UTF-16)
    uint16_t firstCluster;  // Always 0x0000
    uint16_t name3[2];
} FatFileLFN;

typedef union struct_FatFileEntry {
    FatFile83 msdos;
    FatFileLFN lfn;
} FatFileEntry;

#define TF_MODE_READ 0x01
#define TF_MODE_WRITE 0x02
#define TF_MODE_APPEND 0x04
#define TF_MODE_OVERWRITE 0x08

#define TF_ATTR_READ_ONLY 0x01
#define TF_ATTR_HIDDEN 0x02
#define TF_ATTR_SYSTEM 0x04
#define TF_ATTR_VOLUME_LABEL 0x08
#define TF_ATTR_DIRECTORY 0x10
#define TF_ATTR_ARCHIVE 0x20
#define TF_ATTR_DEVICE 0x40 // Should never happen!
#define TF_ATTR_UNUSED 0x80


int read_sector(uint8_t *data, uint32_t blocknum);
int write_sector(uint8_t *data, uint32_t blocknum);
// New error codes
#define TF_ERR_NO_ERROR 0
#define TF_ERR_BAD_BOOT_SIGNATURE 1
#define TF_ERR_BAD_FS_TYPE 2

#define TF_ERR_INVALID_SEEK 1

// FS Types
#define TF_TYPE_FAT16 0
#define TF_TYPE_FAT32 1

// New backend functions
int tf_init(void);
int tf_fetch(uint32_t sector);
int tf_store(void);
uint32_t tf_get_fat_entry(uint32_t cluster);
int tf_set_fat_entry(uint32_t cluster, uint32_t value);
int tf_unsafe_fseek(TFFile *fp, int32_t base, long offset);
TFFile *tf_fnopen(uint8_t *filename, const uint8_t *mode, int n);
int tf_free_clusterchain(uint32_t cluster);
int tf_create(uint8_t *filename);
void tf_release_handle(TFFile *fp);
TFFile *tf_parent(uint8_t *filename, const uint8_t *mode, int mkParents);
int tf_shorten_filename(uint8_t *dest, uint8_t *src, uint8_t num);

// New frontend functions
int tf_init();
int tf_fflush(TFFile *fp);
int tf_fseek(TFFile *fp, int32_t base, long offset);
int tf_fclose(TFFile *fp);
int tf_fread(uint8_t *dest,  int size,  TFFile *fp);
int tf_find_file(TFFile *current_directory, uint8_t *name);
int tf_compare_filename(TFFile *fp, uint8_t *name);
uint32_t tf_first_sector(uint32_t cluster);
uint8_t *tf_walk(uint8_t *filename, TFFile *fp);
TFFile *tf_fopen(uint8_t *filename, const uint8_t *mode);
int tf_fwrite(uint8_t *src, int size, int count, TFFile *fp);
int tf_fputs(uint8_t *src, TFFile *fp);
int tf_mkdir(uint8_t *filename, int mkParents);
int tf_remove(uint8_t *filename);
void tf_print_open_handles(void);

uint32_t tf_find_free_cluster();
uint32_t tf_find_free_cluster_from(uint32_t c);

uint32_t tf_initializeMedia(uint32_t totalSectors);
uint32_t tf_initializeMediaNoBlock(uint32_t totalSectors, int start);

// hidden functions... IAR requires that all functions be declared
TFFile *tf_get_free_handle();
uint8_t upper(uint8_t c);

// New Datas
extern TFInfo tf_info;
extern TFFile tf_file;

uint32_t fat_size(BPB_struct *bpb);
int total_sectors(BPB_struct *bpb);
int root_dir_sectors(BPB_struct *bpb);
int cluster_count(BPB_struct *bpb);
int fat_type(BPB_struct *bpb);
int first_data_sector(BPB_struct *bpb);
int first_sector_of_cluster(BPB_struct *bpb, int N);
int data_sectors(BPB_struct *bpb);
int fat_sector_number(BPB_struct *bpb, int N);
int fat_entry_offset(BPB_struct *bpb, int N);
int fat_entry_for_cluster(BPB_struct *bpb, uint8_t *buffer, int N);

//int f_open (FIL* fp, const char* path, BYTE mode);				/* Open or create a file */
//int f_close (FIL* fp);											/* Close an open file object */
//int f_read (FIL* fp, void* buff, UINT btr, UINT* br);			/* Read data from the file */
//int f_write (FIL* fp, const void* buff, UINT btw, UINT* bw);	/* Write data to the file */
//int f_opendir (DIR* dp, const char* path);						/* Open a directory */
//int f_closedir (DIR* dp);										/* Close an open directory */
//int f_readdir (DIR* dp, FILINFO* fno);							/* Read a directory item */
//int f_mkdir (const char* path);								/* Create a sub directory */
//int f_unlink (const char* path);								/* Delete an existing file or directory */
//int f_link (const char* path);								/* Recover an deleted file or directory */
//int f_mount (FATFS* fs, const char* path, BYTE opt);			/* Mount/Unmount a logical drive */
//int f_chmod (const char* path, BYTE attr, BYTE mask);		/* Change attribute of a file/dir */
