#include "types.h"

typedef BYTE	DSTATUS;

typedef enum {
	RES_OK = 0,
	RES_ERROR,
	RES_NOTRDY,
	RES_PARERR
} DRESULT;

DSTATUS disk_initialize (void);
DRESULT disk_readp (BYTE* buff, DWORD sector, UINT offset, UINT count);
DRESULT disk_writep (const BYTE* buff, DWORD sc);