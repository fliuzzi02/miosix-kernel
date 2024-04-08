/*----------------------------------------------------------------------------/
/  FatFs - Generic FAT Filesystem module  R0.15                               /
/-----------------------------------------------------------------------------/
/
/ Copyright (C) 2022, ChaN, all right reserved.
/
/ FatFs module is an open source software. Redistribution and use of FatFs in
/ source and binary forms, with or without modification, are permitted provided
/ that the following condition is met:

/ 1. Redistributions of source code must retain the above copyright notice,
/    this condition and the following disclaimer.
/
/ This software is provided by the copyright holder and contributors "AS IS"
/ and any warranties related to this software are DISCLAIMED.
/ The copyright owner or contributors be NOT LIABLE for any damages caused
/ by use of this software.
/
/----------------------------------------------------------------------------*/

/*
 * This version of FatFs has been modified to adapt it to the requirements of
 * Miosix:
 * - C++: moved from C to C++ to allow calling other Miosix code
 * - utf8: the original FatFs API supported only utf16 for file names, but the
 *   Miosix filesystem API has to be utf8 (aka, according with the
 *   "utf8 everywhere mainfesto", doesn't want to deal with that crap).
 *   For efficiency reasons the unicode conversion is done inside the FatFs code
 * - removal of global variables: to allow to create an arbitrary number of
 *   independent Fat32 filesystems
 * - unixification: removal of the dos-like drive numbering scheme and
 *   addition of an inode field in the FILINFO struct
 */

#ifndef _FATFS
#define _FATFS	80286	/* Revision ID */

#include <filesystem/file.h>
#include "config/miosix_settings.h"

#include "integer.h"	/* Basic integer types */
#include "ffconf.h"		/* FatFs configuration options */

#if _FATFS != _FFCONF
#error Wrong configuration file (ffconf.h).
#endif

#ifdef WITH_FILESYSTEM



/* Definitions of volume management */

#if _MULTI_PARTITION		/* Multiple partition configuration */
typedef struct {
	BYTE pd;	/* Physical drive number */
	BYTE pt;	/* Partition: 0:Auto detect, 1-4:Forced partition) */
} PARTITION;
extern PARTITION VolToPart[];	/* Volume - Partition resolution table */
#define LD2PD(vol) (VolToPart[vol].pd)	/* Get physical drive number */
#define LD2PT(vol) (VolToPart[vol].pt)	/* Get partition index */

#else							/* Single partition configuration */
#define LD2PD(vol) (BYTE)(vol)	/* Each logical drive is bound to the same physical drive number */
#define LD2PT(vol) 0			/* Find first valid partition or in SFD */

#endif

	/* Type of file size and LBA variables */

#if FF_FS_EXFAT
#if FF_INTDEF != 2
#error exFAT feature wants C99 or later
#endif
	typedef QWORD FSIZE_t;
#if FF_LBA64
	typedef QWORD LBA_t;
#else
	typedef DWORD LBA_t;
#endif
#else
#if FF_LBA64
#error exFAT needs to be enabled when enable 64-bit LBA
#endif
typedef DWORD FSIZE_t;
typedef DWORD LBA_t;
#endif


/* Type of path name strings on FatFs API */

#if _LFN_UNICODE			/* Unicode string */
#if !_USE_LFN
#error _LFN_UNICODE must be 0 in non-LFN cfg.
#endif
#ifndef _INC_TCHAR
typedef WCHAR TCHAR;
#define _T(x) L ## x
#define _TEXT(x) L ## x
#endif

#else						/* ANSI/OEM string */
#ifndef _INC_TCHAR
typedef char TCHAR;
#define _T(x) x
#define _TEXT(x) x
#endif

#endif

/* File access control feature */
#ifdef _FS_LOCK
#if _FS_READONLY
#error _FS_LOCK must be 0 at read-only cfg.
#endif
struct FATFS; //Forward decl

typedef struct {
	FATFS *fs;				/* Object ID 1, volume (NULL:blank entry) */
	DWORD clu;				/* Object ID 2, directory */
	WORD idx;				/* Object ID 3, directory index */
	WORD ctr;				/* Object open counter, 0:none, 0x01..0xFF:read mode open count, 0x100:write mode */
} FILESEM;
#endif


/* File system object structure (FATFS) */

struct FATFS
{
	BYTE fs_type;			/* Filesystem type (0:not mounted) */
	//BYTE pdrv;				/* Volume hosting physical drive */
	miosix::intrusive_ref_ptr<miosix::FileBase> pdrv; /* Physical drive device */
	//BYTE ldrv;				/* Logical drive number (used only when _FS_REENTRANT) */
	miosix::intrusive_ref_ptr<miosix::FileBase> ldrv; /* Logical drive number (used only when _FS_REENTRANT) */
	BYTE n_fats;			/* Number of FATs (1 or 2) */
	BYTE wflag;				/* win[] status (b0:dirty) */
	BYTE fsi_flag;			/* FSINFO status (b7:disabled, b0:dirty) */
	WORD id;				/* Volume mount ID */
	WORD n_rootdir; 		/* Number of root directory entries (FAT12/16) */
	WORD csize;				/* Cluster size [sectors] */
#if _MAX_SS != _MIN_SS
	WORD ssize; /* Sector size (512, 1024, 2048 or 4096) */
#endif
#if _USE_LFN
	WCHAR *lfnbuf; /* LFN working buffer */
#endif
#if _FS_EXFAT
	BYTE *dirbuf; /* Directory entry block scratchpad buffer for exFAT */
#endif
#if !_FS_READONLY
	DWORD last_clust; /* Last allocated cluster */
	DWORD free_clust; /* Number of free clusters */
#endif
#if _FS_RPATH
	DWORD cdir; /* Current directory start cluster (0:root) */
#if _FS_EXFAT
	DWORD cdc_scl;	/* Containing directory start cluster (invalid when cdir is 0) */
	DWORD cdc_size; /* b31-b8:Size of containing directory, b7-b0: Chain status */
	DWORD cdc_ofs;	/* Offset in the containing directory (invalid when cdir is 0) */
#endif
#endif
	DWORD n_fatent; /* Number of FAT entries (number of clusters + 2) */
	DWORD fsize;	/* Number of sectors per FAT */
	LBA_t volbase;	/* Volume base sector */
	LBA_t fatbase;	/* FAT base sector */
	LBA_t dirbase;	/* Root directory base sector (FAT12/16) or cluster (FAT32/exFAT) */
	LBA_t database; /* Data base sector */
	LBA_t winsect;	/* Current sector appearing in the win[] */
	// BYTE win[_MAX_SS]; /* Disk access window for Directory, FAT (and file data at tiny cfg) */
	BYTE win[_MAX_SS] __attribute__((aligned(4))); /* Disk access window for Directory, FAT (and file data at tiny cfg) */
#if _FS_EXFAT
	LBA_t bitbase; /* Allocation bitmap base sector */
#endif

#if _USE_LFN == 1
	WCHAR LfnBuf[_MAX_LFN + 1];
#endif
#ifdef _FS_LOCK
    FILESEM	Files[miosix::FATFS_MAX_OPEN_FILES];/* Open object lock semaphores */
#endif
	miosix::intrusive_ref_ptr<miosix::FileBase> drv; /* drive device */
};

/* Object ID and allocation information (FFOBJID) */

typedef struct
{
	FATFS *fs;		/* Pointer to the hosting volume of this object */
	WORD id;		/* Hosting volume's mount ID */
	BYTE attr;		/* Object attribute */
	BYTE stat;		/* Object chain status (b1-0: =0:not contiguous, =2:contiguous, =3:fragmented in this session, b2:sub-directory stretched) */
	DWORD sclust;	/* Object data start cluster (0:no cluster or root directory) */
	DWORD objsize; 	/* Object size (valid when sclust != 0) */
#if _FS_EXFAT
	DWORD n_cont; 	/* Size of first fragment - 1 (valid when stat == 3) */
	DWORD n_frag; 	/* Size of last fragment needs to be written to FAT (valid when not zero) */
	DWORD c_scl;  	/* Containing directory start cluster (valid when sclust != 0) */
	DWORD c_size; 	/* b31-b8:Size of containing directory, b7-b0: Chain status (valid when c_scl != 0) */
	DWORD c_ofs;  	/* Offset in the containing directory (valid when file object and sclust != 0) */
#endif
#if _FS_LOCK
	UINT lockid; /* File lock ID origin from 1 (index of file semaphore table Files[]) */
#endif
} FFOBJID;



/* File object structure (FIL) */

typedef struct {
	//FATFS*	fs;			// Now, FATFS*fs is wrapped by the FFOBJID obj object identifier
	WORD	id;				/* Owner file system mount ID (**do not change order**) */
	BYTE	flag;			/* File status flags */
	BYTE	err;			/* Abort flag (error code) */
	FSIZE_t fptr;			/* File read/write pointer (Zeroed on file open) */
	DWORD	fsize;			/* File size */
	//DWORD	sclust;			// Now included in te FFOBJID
	DWORD	clust;			/* Current cluster of fpter (invalid when fptr is 0) */
	LBA_t	dsect;			/* Current data sector of fpter */
	FFOBJID obj; 			/* Object identifier (must be the 1st member to detect invalid object pointer) */
	DWORD 	sect;	 		/* Sector number appearing in buf[] (0:invalid) */
#if !_FS_READONLY
	LBA_t 	dir_sect;		/* Sector containing the directory entry (not used at exFAT) */
	BYTE*	dir_ptr;		/* Pointer to the directory entry in the window (not used at exFAT) */
#endif
#if _USE_FASTSEEK
	DWORD*	cltbl;			/* Pointer to the cluster link map table (Nulled on file open, set by application) */
#endif
#ifdef _FS_LOCK
	UINT	lockid;			/* File lock ID (index of file semaphore table Files[]) */
#endif
#if !_FS_TINY
	BYTE	buf[_MAX_SS];	/* File private data read/write window */
#endif
} FIL;



/* Directory object structure (DIR) */

typedef struct {
	FFOBJID	obj;			/* Object identifier */
	DWORD	dptr;			/* Current read/write offset */
	WORD	index;			/* Current read/write index number */
	DWORD	sclust;			/* Table start cluster (0:Root dir) */
	DWORD	clust;			/* Current cluster */
	LBA_t	sect;			/* Current sector (0:Read operation has terminated) */
	BYTE*	dir;			/* Pointer to the directory item in the win[] */
	BYTE	fn[12];			/* SFN (in/out) {body[8],ext[3],status[1]} */
#if _USE_LFN
	DWORD blk_ofs; /* Offset of current entry block being processed (0xFFFFFFFF:Invalid) */
#endif
#if _USE_FIND
	const TCHAR *pat; /* Pointer to the name matching pattern */
#endif
#ifdef _FS_LOCK
	UINT	lockid;			/* File lock ID (index of file semaphore table Files[]) */
#endif
#if _USE_LFN
	WCHAR*	lfn;			/* Pointer to the LFN working buffer */
	WORD	lfn_idx;		/* Last matched LFN index number (0xFFFF:No LFN) */
#endif
} DIR_;



/* File information structure (FILINFO) */

typedef struct {
	DWORD	fsize;			/* File size */
	WORD	fdate;			/* Last modified date */
	WORD	ftime;			/* Last modified time */
	BYTE	fattrib;		/* File attribute */
	TCHAR	fname[13];		/* Short file name (8.3 format) */
#if _USE_LFN
	/*TCHAR*/ char *lfname;		 /* Pointer to the LFN buffer */
	TCHAR 	altname[_SFN_BUF + 1]; /* Alternative file name */
	UINT	lfsize;				 /* Size of LFN buffer in TCHAR */
#else
	TCHAR fname[12 + 1];	 /* File name */
#endif
	unsigned int inode; //By TFT: support inodes
} FILINFO;


/* Format parameter structure (MKFS_PARM) */

typedef struct
{
	BYTE fmt;	   /* Format option (FM_FAT, FM_FAT32, FM_EXFAT and FM_SFD) */
	BYTE n_fat;	   /* Number of FATs */
	UINT align;	   /* Data area alignment (sector) */
	UINT n_root;   /* Number of root directory entries */
	DWORD au_size; /* Cluster size (byte) */
} MKFS_PARM;


/* File function return code (FRESULT) */

typedef enum {
	FR_OK = 0,				/* (0) Succeeded */
	FR_DISK_ERR,			/* (1) A hard error occurred in the low level disk I/O layer */
	FR_INT_ERR,				/* (2) Assertion failed */
	FR_NOT_READY,			/* (3) The physical drive cannot work */
	FR_NO_FILE,				/* (4) Could not find the file */
	FR_NO_PATH,				/* (5) Could not find the path */
	FR_INVALID_NAME,		/* (6) The path name format is invalid */
	FR_DENIED,				/* (7) Access denied due to prohibited access or directory full */
	FR_EXIST,				/* (8) Access denied due to prohibited access */
	FR_INVALID_OBJECT,		/* (9) The file/directory object is invalid */
	FR_WRITE_PROTECTED,		/* (10) The physical drive is write protected */
	FR_INVALID_DRIVE,		/* (11) The logical drive number is invalid */
	FR_NOT_ENABLED,			/* (12) The volume has no work area */
	FR_NO_FILESYSTEM,		/* (13) There is no valid FAT volume */
	FR_MKFS_ABORTED,		/* (14) The f_mkfs() aborted due to any problem */
	FR_TIMEOUT,				/* (15) Could not get a grant to access the volume within defined period */
	FR_LOCKED,				/* (16) The operation is rejected according to the file sharing policy */
	FR_NOT_ENOUGH_CORE,		/* (17) LFN working buffer could not be allocated */
	FR_TOO_MANY_OPEN_FILES, /* (18) Number of open files > _FS_LOCK */
	FR_INVALID_PARAMETER	/* (19) Given parameter is invalid */
} FRESULT;



/*--------------------------------------------------------------*/
/* FatFs Module Application Interface                           */
/*--------------------------------------------------------------*/


FRESULT f_open (FATFS *fs, FIL* fp, const /*TCHAR*/char* path, BYTE mode);					 /* Open or create a file */
FRESULT f_close (FIL* fp);																	 /* Close an open file object */
FRESULT f_read (FIL* fp, void* buff, UINT btr, UINT* br);									 /* Read data from a file */
FRESULT f_write (FIL* fp, const void* buff, UINT btw, UINT* bw);							 /* Write data to a file */
FRESULT f_forward (FIL* fp, UINT(*func)(const BYTE*,UINT), UINT btf, UINT* bf);				 /* Forward data to the stream */
FRESULT f_lseek (FIL* fp, DWORD ofs);														 /* Move file pointer of a file object */
FRESULT f_truncate (FIL* fp);																 /* Truncate file */
FRESULT f_sync (FIL* fp);																	 /* Flush cached data of a writing file */
FRESULT f_opendir (FATFS* fs, DIR_* dp, const /*TCHAR*/char* path);						 	/* Open a directory */
FRESULT f_closedir (DIR_* dp);																 /* Close an open directory */
FRESULT f_readdir (DIR_* dp, FILINFO* fno);													 /* Read a directory item */
FRESULT f_mkdir (FATFS* fs, const /*TCHAR*/ char* path);									 /* Create a sub directory */
FRESULT f_unlink (FATFS* fs, const /*TCHAR*/ char* path);									 /* Delete an existing file or directory */
FRESULT f_rename (FATFS* fs, const /*TCHAR*/ char* path_old, const /*TCHAR*/ char* path_new);/* Rename/Move a file or directory */
FRESULT f_stat (FATFS* fs, const /*TCHAR*/ char* path, FILINFO* fno);						 /* Get file status */
FRESULT f_chmod (FATFS* fs, const /*TCHAR*/ char* path, BYTE value, BYTE mask);				 /* Change attribute of the file/dir */
FRESULT f_utime (FATFS* fs, const /*TCHAR*/ char* path, const FILINFO* fno);				 /* Change times-tamp of the file/dir */
FRESULT f_chdir (FATFS* fs, const TCHAR* path);												 /* Change current directory */
FRESULT f_chdrive (const TCHAR* path);														 /* Change current drive */
FRESULT f_getcwd( FATFS* fs, TCHAR *buff, UINT len);										 /* Get current directory */
FRESULT f_getfree (FATFS* fs, /*const TCHAR* path,*/ DWORD* nclst /*, FATFS** fatfs*/);		 /* Get number of free clusters on the drive */
FRESULT f_getlabel (FATFS* fs, const TCHAR* path, TCHAR* label, DWORD* sn);					 /* Get volume label */
FRESULT f_setlabel (FATFS* fs, const TCHAR* label);											 /* Set volume label */
FRESULT f_mount (FATFS* fs, /*const TCHAR* path,*/ BYTE opt, bool umount);					 /* Mount/Unmount a logical drive */
FRESULT f_mkfs (const TCHAR* path, BYTE sfd, UINT au);										 /* Create a file system on the volume */
FRESULT f_fdisk (BYTE pdrv, const DWORD szt[], void* work);									 /* Divide a physical drive into some partitions */
int f_putc (TCHAR c, FIL* fp);																 /* Put a character to the file */
int f_puts (const TCHAR* str, FIL* cp);														 /* Put a string to the file */
int f_printf (FIL *fp, const TCHAR *str, ...);												 /* Put a formatted string to the file */
TCHAR *f_gets (TCHAR* buff, int len, FIL* fp);												 /* Get a string from the file */

/* Some API functions are implemented as macro */

#define f_eof(fp) ((int)((fp)->fptr == (fp)->obj.objsize))
#define f_error(fp) ((fp)->err)
#define f_tell(fp) ((fp)->fptr)
#define f_size(fp) ((fp)->obj.objsize)
#define f_rewind(fp) f_lseek((fp), 0)
#define f_rewinddir(dp) f_readdir((dp), 0)
#define f_rmdir(path) f_unlink(path)
#define f_unmount(path) f_mount(0, path, 0)
#ifndef EOF
#define EOF (-1)
#endif




/*--------------------------------------------------------------*/
/* Additional Functions                                         */
/*--------------------------------------------------------------*/

/* RTC function (provided by user) */
#if !_FS_READONLY && !_FS_NORTC
DWORD get_fattime (void); /* Get current time */
#endif

/* LFN support functions (defined in ffunicode.c) */
/* Unicode support functions */
#if _USE_LFN							/* Unicode - OEM code conversion */
WCHAR ff_convert (WCHAR chr, UINT dir);	/* OEM-Unicode bidirectional conversion */
WCHAR ff_wtoupper (WCHAR chr);			/* Unicode upper-case conversion */
#if _USE_LFN == 3						/* Memory functions */
void* ff_memalloc (UINT msize);			/* Allocate memory block */
void ff_memfree (void* mblock);			/* Free memory block */
#endif
#endif

/* Sync functions */
#if _FS_REENTRANT
int ff_cre_syncobj (BYTE vol, _SYNC_t* sobj);	/* Create a sync object */
int ff_req_grant (_SYNC_t sobj);				/* Lock sync object */
void ff_rel_grant (_SYNC_t sobj);				/* Unlock sync object */
int ff_del_syncobj (_SYNC_t sobj);				/* Delete a sync object */
#endif



/* O/S dependent functions (samples available in ffsystem.c) */

#if _USE_LFN == 3			 /* Dynamic memory allocation */
void *_memalloc(UINT msize); /* Allocate memory block */
void _memfree(void *mblock); /* Free memory block */
#endif
#if _FS_REENTRANT			 /* Sync functions */
int _mutex_create(int vol);	 /* Create a sync object */
void _mutex_delete(int vol); /* Delete a sync object */
int _mutex_take(int vol);	 /* Lock sync object */
void _mutex_give(int vol);	 /* Unlock sync object */
#endif


/*--------------------------------------------------------------*/
/* Flags and Offset Address                                     */
/*--------------------------------------------------------------*/

/* File access mode and open method flags (3rd argument of f_open) */
#define FA_READ				0x01
#define FA_OPEN_EXISTING	0x00

#if !_FS_READONLY
#define FA_WRITE			0x02
#define FA_CREATE_NEW		0x04
#define FA_CREATE_ALWAYS	0x08
#define FA_OPEN_ALWAYS		0x10
#define FA_OPEN_APPEND		0x30
#define FA__WRITTEN			0x20
#define FA__DIRTY			0x40
#endif


/* Fast seek controls (2nd argument of f_lseek) */
#define CREATE_LINKMAP ((FSIZE_t)0 - 1)

/* Format options (2nd argument of f_mkfs) */
#define FM_FAT 0x01
#define FM_FAT32 0x02
#define FM_EXFAT 0x04
#define FM_ANY 0x07
#define FM_SFD 0x08

/* Filesystem type (FATFS.fs_type) */
#define FS_FAT12	1
#define FS_FAT16	2
#define FS_FAT32	3
#define FS_EXFAT 	4


/* File attribute bits for directory entry (FILINFO.fattrib) */

#define	AM_RDO	0x01	/* Read only */
#define	AM_HID	0x02	/* Hidden */
#define	AM_SYS	0x04	/* System */
#define	AM_VOL	0x08	/* Volume label */
#define AM_LFN	0x0F	/* LFN entry */
#define AM_DIR	0x10	/* Directory */
#define AM_ARC	0x20	/* Archive */
#define AM_MASK	0x3F	/* Mask of defined bits */


/* Fast seek feature */
#define CREATE_LINKMAP	0xFFFFFFFF



/*--------------------------------*/
/* Multi-byte word access macros  */

#if _WORD_ACCESS == 1	/* Enable word access to the FAT structure */
#define	LD_WORD(ptr)		(WORD)(*(WORD*)(BYTE*)(ptr))
#define	LD_DWORD(ptr)		(DWORD)(*(DWORD*)(BYTE*)(ptr))
#define	ST_WORD(ptr,val)	*(WORD*)(BYTE*)(ptr)=(WORD)(val)
#define	ST_DWORD(ptr,val)	*(DWORD*)(BYTE*)(ptr)=(DWORD)(val)
#else					/* Use byte-by-byte access to the FAT structure */
#define	LD_WORD(ptr)		(WORD)(((WORD)*((BYTE*)(ptr)+1)<<8)|(WORD)*(BYTE*)(ptr))
#define	LD_DWORD(ptr)		(DWORD)(((DWORD)*((BYTE*)(ptr)+3)<<24)|((DWORD)*((BYTE*)(ptr)+2)<<16)|((WORD)*((BYTE*)(ptr)+1)<<8)|*(BYTE*)(ptr))
#define	ST_WORD(ptr,val)	*(BYTE*)(ptr)=(BYTE)(val); *((BYTE*)(ptr)+1)=(BYTE)((WORD)(val)>>8)
#define	ST_DWORD(ptr,val)	*(BYTE*)(ptr)=(BYTE)(val); *((BYTE*)(ptr)+1)=(BYTE)((WORD)(val)>>8); *((BYTE*)(ptr)+2)=(BYTE)((DWORD)(val)>>16); *((BYTE*)(ptr)+3)=(BYTE)((DWORD)(val)>>24)
#endif

#endif //WITH_FILESYSTEM
#endif /* _FATFS */