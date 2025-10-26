typedef enum
{
    DOS_PROGRAM_TERMINATE                 = 0x00,
    DOS_CONSOLE_INPUT                     = 0x01,
    DOS_CONSOLE_OUTPUT                    = 0x02,
    DOS_AUXILIARY_INPUT                   = 0x03,
    DOS_AUXILIARY_OUTPUT                  = 0x04,
    DOS_PRINTER_OUTPUT                    = 0x05,
    DOS_DIRECT_CONSOLE_IO                 = 0x06,
    DOS_DIRECT_CONSOLE_INPUT              = 0x07,
    DOS_CONSOLE_INPUT_WITHOUT_ECHO        = 0x08,
    DOS_STRING_OUTPUT                     = 0x09,
    DOS_BUFFERED_LINE_INPUT               = 0x0A,
    DOS_CONSOLE_STATUS                    = 0x0B,

    DOS_RETURN_VERSION_NUMBER             = 0x0C,
    DOS_DISK_RESET                        = 0x0D,
    DOS_SELECT_DISK                       = 0x0E,

    DOS_OPEN_FILE_FCB                     = 0x0F,
    DOS_CLOSE_FILE_FCB                    = 0x10,
    DOS_SEARCH_FOR_FIRST_ENTRY_FCB        = 0x11,
    DOS_SEARCH_FOR_NEXT_ENTRY_FCB         = 0x12,
    DOS_DELETE_FILE_FCB                   = 0x13,
    DOS_SEQUENTIAL_READ_FCB               = 0x14,
    DOS_SEQUENTIAL_WRITE_FCB              = 0x15,
    DOS_CREATE_FILE_FCB                   = 0x16,
    DOS_RENAME_FILE_FCB                   = 0x17,

    DOS_GET_LOGIN_VECTOR                  = 0x18,
    DOS_GET_CURRENT_DRIVE                 = 0x19,
    DOS_SET_DISK_TRANSFER_ADDRESS         = 0x1A,
    DOS_GET_ALLOCATION_INFORMATION        = 0x1B,

    DOS_RANDOM_READ_FCB                   = 0x21,
    DOS_RANDOM_WRITE_FCB                  = 0x22,
    DOS_GET_FILE_SIZE_FCB                 = 0x23,
    DOS_SET_RANDOM_RECORD_FCB             = 0x24,
    DOS_RANDOM_BLOCK_WRITE_FCB            = 0x26,
    DOS_RANDOM_BLOCK_READ_FCB             = 0x27,
    DOS_RANDOM_WRITE_ZERO_FILL_FCB        = 0x28,

    DOS_GET_DATE                          = 0x2A,
    DOS_SET_DATE                          = 0x2B,
    DOS_GET_TIME                          = 0x2C,
    DOS_SET_TIME                          = 0x2D,
    DOS_SET_RESET_VERIFY_FLAG             = 0x2E,

    DOS_ABSOLUTE_SECTOR_READ              = 0x2F,
    DOS_ABSOLUTE_SECTOR_WRITE             = 0x30,
    DOS_GET_DISK_PARAMETERS               = 0x31,

    DOS_FIND_FIRST_ENTRY                  = 0x40,
    DOS_FIND_NEXT_ENTRY                   = 0x41,
    DOS_FIND_NEW_ENTRY                    = 0x42,

    DOS_OPEN_FILE_HANDLE                  = 0x43,
    DOS_CREATE_FILE_HANDLE                = 0x44,
    DOS_CLOSE_FILE_HANDLE                 = 0x45,
    DOS_ENSURE_FILE_HANDLE                = 0x46,
    DOS_DUPLICATE_FILE_HANDLE             = 0x47,
    DOS_READ_FROM_FILE_HANDLE             = 0x48,
    DOS_WRITE_TO_FILE_HANDLE              = 0x49,
    DOS_MOVE_FILE_HANDLE_POINTER          = 0x4A,
    DOS_IO_CONTROL_FOR_DEVICES            = 0x4B,
    DOS_TEST_FILE_HANDLE                  = 0x4C,

    DOS_DELETE_FILE_OR_SUBDIRECTORY       = 0x4D,
    DOS_RENAME_FILE_OR_SUBDIRECTORY       = 0x4E,
    DOS_MOVE_FILE_OR_SUBDIRECTORY         = 0x4F,
    DOS_GET_SET_FILE_ATTRIBUTES           = 0x50,
    DOS_GET_SET_FILE_DATE_AND_TIME        = 0x51,

    DOS_DELETE_FILE_HANDLE                = 0x52,
    DOS_RENAME_FILE_HANDLE                = 0x53,
    DOS_MOVE_FILE_HANDLE                  = 0x54,
    DOS_GET_SET_FILE_HANDLE_ATTRIBUTES    = 0x55,
    DOS_GET_SET_FILE_HANDLE_DATE_AND_TIME = 0x56,

    DOS_GET_DISK_TRANSFER_ADDRESS         = 0x57,
    DOS_GET_VERIFY_FLAG_SETTING           = 0x58,
    DOS_GET_CURRENT_DIRECTORY             = 0x59,
    DOS_CHANGE_CURRENT_DIRECTORY          = 0x5A,
    DOS_PARSE_PATHNAME                    = 0x5B,
    DOS_PARSE_FILENAME                    = 0x5C,
    DOS_CHECK_CHARACTER                   = 0x5D,
    DOS_GET_WHOLE_PATH_STRING             = 0x5E,
    DOS_FLUSH_DISK_BUFFERS                = 0x5F,

    DOS_FORK_A_CHILD_PROCESS              = 0x60,
    DOS_REJOIN_PARENT_PROCESS             = 0x61,
    DOS_TERMINATE_WITH_ERROR_CODE         = 0x62,
    DOS_DEFINE_ABORT_ROUTINE              = 0x63,
    DOS_DEFINE_DISK_ERROR_HANDLER_ROUTINE = 0x64,
    DOS_GET_PREVIOUS_ERROR_CODE           = 0x65,
    DOS_EXPLAIN_ERROR_CODE                = 0x66,

    DOS_FORMAT_A_DISK                     = 0x67,
    DOS_CREATE_OR_DESTROY_RAMDISK         = 0x68,
    DOS_ALLOCATE_SECTOR_BUFFERS           = 0x69,
    DOS_LOGICAL_DRIVE_ASSIGNMENT          = 0x6A,

    DOS_GET_ENVIRONMENT_ITEM              = 0x6B,
    DOS_SET_ENVIRONMENT_ITEM              = 0x6C,
    DOS_FIND_ENVIRONMENT_ITEM             = 0x6D,

    DOS_GET_SET_DISK_CHECK_STATUS         = 0x6E,
    DOS_GET_MSX_DOS_VERSION_NUMBER        = 0x6F,
    DOS_GET_SET_REDIRECTION_STATUS        = 0x70
} tdFunction;

typedef enum MsxDos2Error {
    DOS_ERR_NCOMP  = 0xFF,
    DOS_ERR_WRERR  = 0xFE,
    DOS_ERR_DISK   = 0xFD,
    DOS_ERR_NRDY   = 0xFC,
    DOS_ERR_VERFY  = 0xFB,
    DOS_ERR_DATA   = 0xFA,
    DOS_ERR_RNF    = 0xF9,
    DOS_ERR_WPROT  = 0xF8,
    DOS_ERR_UFORM  = 0xF7,
    DOS_ERR_NDOS   = 0xF6,
    DOS_ERR_WDISK  = 0xF5,
    DOS_ERR_WFILE  = 0xF4,
    DOS_ERR_SEEK   = 0xF3,
    DOS_ERR_IFAT   = 0xF2,
    DOS_ERR_NOUPB  = 0xF1,
    DOS_ERR_IFORM  = 0xF0,
    DOS_ERR_INTER  = 0xDF,
    DOS_ERR_NORAM  = 0xDE,
    DOS_ERR_IBDOS  = 0xDC,
    DOS_ERR_IDRV   = 0xDB,
    DOS_ERR_IFNM   = 0xDA,
    DOS_ERR_IPATH  = 0xD9,
    DOS_ERR_PLONG  = 0xD8,
    DOS_ERR_NOFIL  = 0xD7,
    DOS_ERR_NODIR  = 0xD6,
    DOS_ERR_DRFUL  = 0xD5,
    DOS_ERR_DKFUL  = 0xD4,
    DOS_ERR_DUPF   = 0xD3,
    DOS_ERR_DIRE   = 0xD2,
    DOS_ERR_FILRO  = 0xD1,
    DOS_ERR_DIRNE  = 0xD0,
    DOS_ERR_IATTR  = 0xCF,
    DOS_ERR_DOT    = 0xCE,
    DOS_ERR_SYSX   = 0xCD,
    DOS_ERR_DIRX   = 0xCC,
    DOS_ERR_FILEX  = 0xCB,
    DOS_ERR_FOPEN  = 0xCA,
    DOS_ERR_OV64K  = 0xC9,
    DOS_ERR_FILE   = 0xC8,
    DOS_ERR_EOF    = 0xC7,
    DOS_ERR_ACCV   = 0xC6,
    DOS_ERR_IPROC  = 0xC5,
    DOS_ERR_NHAND  = 0xC4,
    DOS_ERR_IHAND  = 0xC3,
    DOS_ERR_NOPEN  = 0xC2,
    DOS_ERR_IDEV   = 0xC1,
    DOS_ERR_IENV   = 0xC0,
    DOS_ERR_ELONG  = 0xBF,
    DOS_ERR_IDATE  = 0xBE,
    DOS_ERR_ITIME  = 0xBD,
    DOS_ERR_RAMDX  = 0xBC,
    DOS_ERR_NRAMD  = 0xBB,
    DOS_ERR_HDEAD  = 0xBA,
    DOS_ERR_EOL    = 0xB9,
    DOS_ERR_ISBFN  = 0xB8,
    DOS_ERR_STOP   = 0x9F,
    DOS_ERR_CTRL_C = 0x9E,
    DOS_ERR_ABORT  = 0x9D,
    DOS_ERR_OUTERR = 0x9C,
    DOS_ERR_INERR  = 0x9B,
    DOS_ERR_BADCOM = 0x8F,
    DOS_ERR_BADCM  = 0x8E,
    DOS_ERR_BUFUL  = 0x8D,
    DOS_ERR_OKCMD  = 0x8C,
    DOS_ERR_IPARM  = 0x8B,
    DOS_ERR_INP    = 0x8A,
    DOS_ERR_NOPAR  = 0x89,
    DOS_ERR_IOPT   = 0x88,
    DOS_ERR_BADNO  = 0x87,
    DOS_ERR_NOHELP = 0x86,
    DOS_ERR_BADVER = 0x85,
    DOS_ERR_NOCAT  = 0x84,
    DOS_ERR_BADEST = 0x83,
    DOS_ERR_COPY   = 0x82,
    DOS_ERR_OVDEST = 0x81,
    DOS_ERR_OK     = 0x00
} MsxDos2Error;

enum {
	ATTRIBUTE_READ_ONLY   =   1,
	ATTRIBUTE_HIDDEN_FILE =   2,
	ATTRIBUTE_SYSTEM_FILE =   4,
	ATTRIBUTE_VOLUME_NAME =   8,
	ATTRIBUTE_DIRECTORY   =  16,
	ATTRIBUTE_ARCHIVE_BIT =  32,
	ATTRIBUTE_RESERVED    =  64,
	ATTRIBUTE_DEVICE_BIT  = 128
};

#ifdef QT_VERSION
typedef struct __attribute__((packed))
{
    unsigned char      m_ucFF;					    /*      0 - Always 0FFh */
    char               m_acFileName[13]; 			/*  1..13 - Filename as an ASCIIZ string */
    unsigned char      m_cAttributes;			    /*     14 - File attributes byte */
    unsigned short int m_uiLastModificationTime;	/* 15..16 - Time of last modification */
    unsigned short int m_uiLastModificationDate;	/* 17..18 - Date of last modification */
    unsigned short int m_uiStartCluster;			/* 19..20 - Start cluster */
    unsigned int       m_ulFileSize; 			    /* 21..24 - File size */
    unsigned char      m_ucDrive; 				    /*     25 - Logical drive */
    QFile *            m_poFile;
    char               m_acRegExp[13];
    unsigned char      m_ucResult;
}
tdFileInfoBlock;


typedef struct
{
    unsigned char m_ucDriverNumber;
    char          m_acFileName[8];
    char          m_acFileNameExtension[3];
    unsigned char m_ucExtentNumber;
    unsigned char m_ucFileAttributes;

    union
    {
        struct
        {
            unsigned char m_ucExtentHigh;
            unsigned char m_ucRecordCount;
        } cpm;

        struct
        {
            unsigned int m_uiRecordSize;
        } dos;
    } u;

    unsigned int  m_ulFileSize;
    unsigned int  m_ulVolumeID;
    unsigned char m_ucResult;
    unsigned char m_ucCurrentRecordWithinExtent;
    unsigned int  m_ulRandomRecordNumber;
}
tdFileControlBlock;

static_assert(sizeof(tdFileInfoBlock) == 48);

#else
typedef struct
{
    unsigned char      m_ucFF;					    /*      0 - Always 0FFh */
    char               m_acFileName[13]; 			/*  1..13 - Filename as an ASCIIZ string */
    unsigned char      m_cAttributes;			    /*     14 - File attributes byte */
    unsigned short int m_uiLastModificationTime;	/* 15..16 - Time of last modification */
    unsigned short int m_uiLastModificationDate;	/* 17..18 - Date of last modification */
    unsigned short int m_uiStartCluster;			/* 19..20 - Start cluster */
    unsigned long      m_ulFileSize; 			    /* 21..24 - File size */
    unsigned char      m_ucDrive; 				    /*     25 - Logical drive */
    unsigned char      m_acQtFile[8];
    char               m_acRegExp[13];
    unsigned char      m_ucResult;
}
tdFileInfoBlock;

typedef struct
{
    unsigned char m_ucDriverNumber;
    char          m_acFileName[8];
    char          m_acFileNameExtension[3];
    unsigned char m_ucExtentNumber;
    unsigned char m_ucFileAttributes;

    union
    {
        struct
        {
            unsigned char m_ucExtentHigh;
            unsigned char m_ucRecordCount;
        } cpm;

        struct
        {
            unsigned int m_uiRecordSize;
        } dos;
    } u;

    unsigned long m_ulFileSize;
    unsigned long m_ulVolumeID;
    unsigned char m_ucResult;
    unsigned char m_ucCurrentRecordWithinExtent;
    unsigned long m_ulRandomRecordNumber;
}
tdFileControlBlock;

#endif



/*

3.6 FILE CONTROL BLOCKS

It is not anticipated that specially written MSX-DOS 2
transient programs or MSX-DOS 1 or CP/M programs which are modified for MSX-DOS
2 will use the CP/M-compatible FCB functions, but the format of the FCBs used
for these functions is given here for reference. This format is, of course, very
similar to the FCBs used by CP/M and MSX-DOS 1 but the use of some of the fields
within the FCB are different (though generally compatible).

A basic FCB is 33 bytes long. This type of FCB can be used for file management
operations (delete, rename etc.) and also for sequential reading and writing.
The random read and write functions use an extra 3 bytes on the end of the FCB
to store a random record number. The MSX-DOS 1 compatible block read and write
functions also use this additional three (or in some cases four) bytes - see the
Function Specification for details.

The layout of an FCB is given below. A general description of each of the fields
is included here. The individual function description given in the Function
Specification details of how the fields are used for each function where this is
not obvious.

00h Drive number 1...8. 0 => default drive. Must be set up in all FCBs used,
never modified by MSX-DOS function calls (except "Open File" if APPEND was
used).

01h...08h Filename, left justified with trailing blanks. Can contain "?"
characters if ambiguous filename is allowed (see Function Specification). When
doing comparisons case will be ignored. When creating new files, name will be
uppercased.

09h...0Bh Filename extension. Identical to filename. Note that bit-7 of the
filename extension characters are NOT interpreted as flags as they are in CP/M.

0Ch Extent number (low byte). Must be set (usually to zero) by the transient
program before open or create. It is used and updated by sequential read and
write, and also set by random read and write. This is compatible with CP/M and
MSX-DOS 1.

0Dh File attributes. Set by "open", "create" or "find".

0Eh Extent number (high byte) for CP/M functions. Zeroed by open and create. For
sequential read and write it is used and updated as an extension to the extent
number to allow larger files to be accessed. Although this is different from
CP/M it does not interfere with CP/Ms use of FCBs and is the same as MSX-DOS 1.

Record size (low byte) for MSX-DOS 1 compatible block functions. Must be set to
the required record size before using the block read or write functions.

0Fh Record count for CP/M functions. Set up by open and create and modified when
necessary by sequential and random reads and writes. This is the same as CP/M
and MSX-DOS 1.

Record size (high byte) for MSX-DOS 1 compatible block functions. Must be set to
the required record size before using the block read and write functions.

10h...13h File size in bytes, lowest byte first. File size is exact, not rounded
up to 128 bytes. This field is set up by open and create and updated when the
file is extended by write operations. Should not be modified by the transient
program as it is written back to disk by a close function call. This is the same
as MSX-DOS 1 but different from CP/M which stores allocation information here.

14h...17h Volume-id. This is a four byte number identifying the particular disk
which this FCB is accessing. It is set up by open and create and is checked on
read, write and close calls. Should not be modified by the program. Note that
this is different from MSX-DOS 1 which stores the date and time of last update
here, and from CP/M which stores allocation information.

18h...1Fh Internal information. These bytes contain information to enable the
file to be located on the disk. Should not be modified at all by the transient
program. The internal information kept here is similar but not identical to that
kept by MSX-DOS 1 and totally different from CP/M.

20h Current record within extent (0...127). Must be set (normally to zero) by
the transient program before first sequential read or write. Used and modified
by sequential read and write. Also set up by random read and write. This is
compatible with CP/M and MSX-DOS 1.

21h...24h Random record number, low byte first. This field is optional, it is
only required if random or block reads or writes are used. It must be set up
before doing these operations and is updated by block read and write but not by
random read or write. Also set up by the "set random record" function.

For the block operations, which are in MSX-DOS 1 but not in CP/M, all four bytes
are used if the record size is less than 64 bytes, and only the first three
bytes are used if the record size is 64 bytes or more. For random read and write
only the first three bytes are used (implied record size is 128 bytes). This is
compatible with CP/M and with MSX-DOS 1.

*/