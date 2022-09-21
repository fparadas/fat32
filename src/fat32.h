#include <stdlib.h>
#include <stdio.h>
#include "types.h"

/* FATFS.flag */
#define	FA_OPENED	0x01
#define	FA_WPRT		0x02
#define	FA__WIP		0x40


/* FILINFO.fattrib */
#define	AM_RDO	0x01	/* Read only */
#define	AM_HID	0x02	/* Hidden */
#define	AM_SYS	0x04	/* System */
#define	AM_VOL	0x08	/* Volume label */
#define AM_LFN	0x0F	/* LFN entry */
#define AM_DIR	0x10	/* Directory */
#define AM_ARC	0x20	/* Archive */
#define AM_MASK	0x3F	/* Mask of defined bits */

typedef struct {
	BYTE	fs_type;	/* FAT sub type */
	BYTE	flag;		/* File status flags */
	BYTE	csize;		/* Number of sectors per cluster */
	BYTE	pad1;
	WORD	n_rootdir;	/* Number of root directory entries (0 on FAT32) */
	DWORD	n_fatent;	/* Number of FAT entries (= number of clusters + 2) */
	DWORD	fatbase;	/* FAT start sector */
	DWORD	dirbase;	/* Root directory start sector (Cluster# on FAT32) */
	DWORD	database;	/* Data start sector */
	DWORD	fptr;		/* File R/W pointer */
	DWORD	fsize;		/* File size */
	DWORD	org_clust;	/* File start cluster */
	DWORD	curr_clust;	/* File current cluster */
	DWORD	dsect;		/* File current data sector */
} FATFS;

typedef struct {
	WORD	index;		/* Current read/write index number */
	BYTE*	fn;			/* Pointer to the SFN (in/out) {file[8],ext[3],status[1]} */
	DWORD	sclust;		/* Table start cluster (0:Static table) */
	DWORD	clust;		/* Current cluster */
	DWORD	sect;		/* Current sector */
} DIR;

typedef struct {
	DWORD	fsize;		/* File size */
	WORD	fdate;		/* Last modified date */
	WORD	ftime;		/* Last modified time */
	BYTE	fattrib;	/* Attribute */
	char	fname[13];	/* File name */
} FILINFO;


int f_open (FIL* fp, const char* path, BYTE mode);				/* Open or create a file */
int f_close (FIL* fp);											/* Close an open file object */
int f_read (FIL* fp, void* buff, UINT btr, UINT* br);			/* Read data from the file */
int f_write (FIL* fp, const void* buff, UINT btw, UINT* bw);	/* Write data to the file */
int f_opendir (DIR* dp, const char* path);						/* Open a directory */
int f_closedir (DIR* dp);										/* Close an open directory */
int f_readdir (DIR* dp, FILINFO* fno);							/* Read a directory item */
int f_mkdir (const char* path);								/* Create a sub directory */
int f_unlink (const char* path);								/* Delete an existing file or directory */
int f_mount (FATFS* fs, const char* path, BYTE opt);			/* Mount/Unmount a logical drive */
int f_chmod (const char* path, BYTE attr, BYTE mask);			/* Change attribute of a file/dir */