; Converted for SDCC/ASxxxx by rel2tab helper
	; Uses .equ/.db/.dw/.ds and .include syntax
; msx.inc
;
; MSX constants and definitions for DOS1 and DOS2
; ------------------------------------------------------------------------------

BOT16K	.equ	0x0C000			; lower of 16 KB RAM
BOT32K	.equ	0x08000			; lower of 32 KB RAM
BOOTSZ	.equ	256			; maximum size of MSX bootloader in bootsector
TMPSTK	.equ	BOT16K+BOOTSZ+256	; location of temporary stack
TBASE	.equ	0x00100			; start of MSX-DOS

; ------------------------------------------------------------------------------
; P0: BIOS or BDOS 2 kernel

WBOOT  	.equ     0x0000
KBDOS   .equ     0x0005
RDSLT   .equ     0x000C
WRSLT	.equ	0x0014
CALSLT  .equ     0x001C
ENASLT  .equ     0x0024
IDBYT0  .equ     0x002B
IDBYT2  .equ     0x002D
CALLF	.equ	0x0030
CHAR_16 .equ     0x0034
KEYINT	.equ	0x0038
SSLOT   .equ     0x003B
SSLOTE  .equ     0x004B
SSLOTL	.equ	0x0046
LDIRMV  .equ     0x0059
LDIRVM  .equ     0x005C
ALL_SEG	.equ	0x005C		; BDOS
FRE_SEG	.equ	0x005F		; BDOS
DBUF    .equ     0x0080
A0080	.equ	0x0080		; BDOS
A0083	.equ	0x0083		; BDOS
A0086	.equ	0x0086		; BODS
CHSNS   .equ     0x009C
CHGET   .equ     0x009F
CHPUT   .equ     0x00A2
LPTOUT  .equ     0x00A5
LPTSTT  .equ     0x00A8
BREAKX  .equ     0x00B7
CKCNTC  .equ     0x00BD
ERAFNK  .equ     0x00CC
TOTEXT  .equ     0x00D2
SNSMAT  .equ     0x0141
PHYDIO	.equ	0x0144
KILBUF  .equ     0x0156
CALBAS  .equ     0x0159
CHGCPU	.equ	0x0180		; TURBO R
GETCPU	.equ	0x0183		; TURBO R
DECSUB  .equ     0x268C
DECADD  .equ     0x269A
DECDIV  .equ     0x289F
VMOVE   .equ     0x2EF3
VMOVFM  .equ     0x2F08
VMOVMF  .equ     0x2F10
MAKINT  .equ     0x2F99
CONDS   .equ     0x3042
INT     .equ     0x30D1		; integer part of float
SGNMUL  .equ     0x325C
NULSTR  .equ     0x3FD6

; ------------------------------------------------------------------------------
; P1: disk rom or basic rom

A4010	.equ	0x04010		; DSKIO
A401F	.equ	0x0401F		; MTOFF
BASENT  .equ     0x04022		; Start DiskBASIC
A4043	.equ	0x04043		; EXTBIO handler DOS2 
A4049	.equ	0x04049		; H.TIMI handler
ERROR   .equ     0x0406F          ; BASIC error
READYR  .equ     0x0409B          ; restart BASIC
NTSTOP  .equ     0x04173          ; execute statement
LINKER  .equ     0x04253          ; recalculate linepointers
NEWSTT  .equ     0x04601          ; execution loop
CHRGTR  .equ     0x04666          ; CHRGTR
FLTLIN  .equ     0x046FF          ; convert to SNG
INTID2  .equ     0x04756          ; evaluate word operand and check for 0-32767 range
FINPRT  .equ     0x04AFF          ; output back to screen
FRMEQL  .equ     0x04C5F          ; evaluate =expression
FRMEVL  .equ     0x04C64          ; evaluate expression
DOCNVF  .equ     0x0517A          ; convert DAC to other type
GETIN2  .equ     0x0520F          ; evaluate integer operand
GTBYTC  .equ     0x0521B          ; evaluate next byte operand
GETBYT  .equ     0x0521C          ; evaluate byte operand
CONINT  .equ     0x0521F          ; convert to byte
JPFOUT  .equ     0x0537B          ; convert DAC to text, unformatted (JPFOUT is not an offical name)
GETUIN  .equ     0x0542F          ; evaluate address operand
GETUI   .equ     0x05432          ; convert address to integer
SCCPTR  .equ     0x054F7          ; convert pointers to linenumbers
GETYPR  .equ     0x05597          ; GETYPR
PTRGET  .equ     0x05EA4          ; get address of variable
STRINI  .equ     0x06627          ; allocate temp string
GETSPA  .equ     0x0668E          ; allocate string space
FRESTR  .equ     0x067D0          ; free temporary string
FILEVL  .equ     0x06A0E          ; evaluate filespecification
FILIDX  .equ     0x06A6D          ; get i/o channel pointer
OPNFIL  .equ     0x06AFA          ; open i/o channel
CLSFIL  .equ     0x06B24          ; close i/o channel
CLSALL  .equ     0x06C1C          ; close all i/o channels
NOSKCR  .equ     0x06E41          ; resume character putback routine
BSAVE   .equ     0x06E92          ; start of BSAVE routine
BLOAD   .equ     0x06EC6          ; start of BLOAD routine
BLDFIN  .equ     0x06EF4          ; finish BLOAD
ADRGET  .equ     0x06F0B          ; evaluate address operand (BLOAD/SAVE)
M6F0B   .equ     0x06F0B          ; evaluate address operand (BLOAD/SAVE) (DOS1)
PARDEV  .equ     0x06F15          ; skip strong cassette devicecheck
CRDONZ  .equ     0x07323          ; newline to OUTDO if not at start of line
CRDO    .equ     0x07328          ; newline to OUTDO
M739A   .equ     0x0739A          ; quit loading & start (headloop/executing)
M7D17   .equ     0x07D17          ; continue start of MSX-BASIC without executing BASIC programs in ROM
M7D2F   .equ     0x07D2F          ; address initialize BASIC screen
M7D31   .equ     0x07D31          ; BASIC initscreen (without INITXT & CNSDFG)
M7DE9   .equ     0x07DE9          ; start MSX-BASIC program in ROM
M7E14   .equ     0x07E14          ; start MSX-BASIC program in ROM (DOS1)


; ------------------------------------------------------------------------------
; P2: workarea DOS 2

ISB062	.equ	0x0B062
D_B064  .equ     0x0B064  	; --SL-
I_B066  .equ     0x0B066  	; ----I
ISB069  .equ     0x0B069  	; ----I
D_B06C  .equ     0x0B06C  	; --S-I
I_B0D0  .equ     0x0B0D0  	; ----I
I_B1CF  .equ     0x0B1CF  	; ----I
I_B1D0  .equ     0x0B1D0  	; ----I
ISB1D1  .equ     0x0B1D1  	; ----I
ISB1D2  .equ     0x0B1D2  	; ----I
I_B2D4  .equ     0x0B2D4  	; ----I
D_B400  .equ     0x0B400  	; --SL-
I_B6D4  .equ     0x0B6D4  	; ----I
ISB6F2  .equ     0x0B6F2  	; ----I
DSB6FA  .equ     0x0B6FA  	; --S--
DSB6FB  .equ     0x0B6FB  	; --S--
DSB6FD  .equ     0x0B6FD  	; ---L-
I_B8D4  .equ     0x0B8D4  	; ----I
I_B8F4  .equ     0x0B8F4  	; ----I
ISB901  .equ     0x0B901  	; ----I
I_B910  .equ     0x0B910  	; ----I
I_B91B  .equ     0x0B91B  	; ----I
I_B926  .equ     0x0B926  	; ----I
ISB931  .equ     0x0B931  	; ----I
ISB933  .equ     0x0B933  	; ----I
ISB973  .equ     0x0B973  	; ----I
I_B975  .equ     0x0B975  	; ----I
D_B976  .equ     0x0B976  	; --SL-
D_B977  .equ     0x0B977  	; --SL-
I_B99A  .equ     0x0B99A  	; ----I
I_B9DA  .equ     0x0B9DA  	; ----I
ISB9DB  .equ     0x0B9DB  	; ----I
DSB9E8  .equ     0x0B9E8  	; ---L-
D_B9EF  .equ     0x0B9EF  	; ---LI
DSB9F1  .equ     0x0B9F1  	; --S--
D_B9F3  .equ     0x0B9F3  	; ---L-
I_B9F4  .equ     0x0B9F4  	; ----I
D_B9F8  .equ     0x0B9F8  	; ---L-
I_B9FA  .equ     0x0B9FA  	; ----I
I_B9FF  .equ     0x0B9FF  	; ----I
D_BA07  .equ     0x0BA07  	; ---LI
DSBA08  .equ     0x0BA08  	; ---L-
D_BA09  .equ     0x0BA09  	; --S--
DSBA0B  .equ     0x0BA0B  	; ---L-
I_BA1A  .equ     0x0BA1A  	; ----I
I_BA23  .equ     0x0BA23  	; ----I
I_BA25  .equ     0x0BA25  	; ----I
D_BA33  .equ     0x0BA33  	; --SL-
I_BA35  .equ     0x0BA35  	; ----I
I_BA75  .equ     0x0BA75  	; ----I
D_BB76  .equ     0x0BB76  	; ---LI
D_BB77  .equ     0x0BB77  	; --SL-
D_BB78  .equ     0x0BB78  	; --SL-
D_BB7A  .equ     0x0BB7A  	; --SL-
D_BB7B  .equ     0x0BB7B  	; --SL-
D_BB7C  .equ     0x0BB7C  	; --SL-
D_BB7D  .equ     0x0BB7D  	; --SL-
D_BB7F  .equ     0x0BB7F  	; --SL-
D_BB80  .equ     0x0BB80  	; --SLI		IY variables base address (kernel uses: BB80 - BC00)
D_BB82  .equ     0x0BB82  	; --SL-
D_BB84  .equ     0x0BB84  	; --SL-
D_BB86  .equ     0x0BB86  	; --SLI
D_BB87  .equ     0x0BB87  	; --S--
DSBB89  .equ     0x0BB89  	; --S--
D_BB8A  .equ     0x0BB8A  	; --S-I
D_BB8B  .equ     0x0BB8B  	; --SL-
D_BB8D  .equ     0x0BB8D  	; --SLI
D_BB8E  .equ     0x0BB8E  	; --S--
DSBB8F  .equ     0x0BB8F  	; --S--
D_BB90  .equ     0x0BB90  	; --SL-
D_BB91  .equ     0x0BB91  	; --SL-
D_BB92  .equ     0x0BB92  	; ---LI
DSBB93  .equ     0x0BB93  	; --S--
D_BB95  .equ     0x0BB95  	; --SL-
D_BB96  .equ     0x0BB96  	; --SL-
D_BB98  .equ     0x0BB98  	; --SL-
DSBB99  .equ     0x0BB99  	; ---L-
D_BB9A  .equ     0x0BB9A  	; --SL-
D_BB9C  .equ     0x0BB9C  	; ---L-
D_BB9E  .equ     0x0BB9E  	; --SL-
DSBBA2  .equ     0x0BBA2  	; --S--
D_BBA3  .equ     0x0BBA3  	; --SL-
D_BBA5  .equ     0x0BBA5  	; --S-I
DSBBA6  .equ     0x0BBA6		; --S--
D_BBA7  .equ     0x0BBA7  	; --SL-
D_BBAA  .equ     0x0BBAA  	; --SL-
D_BBAB  .equ     0x0BBAB  	; --SL-
D_BBAD  .equ     0x0BBAD  	; --SL-
DSBBAF  .equ     0x0BBAF  	; --S--
D_BBB2  .equ     0x0BBB2  	; --S--
D_BBB4  .equ     0x0BBB4  	; --SL-
D_BBB6  .equ     0x0BBB6  	; --SL-
D_BBB8  .equ     0x0BBB8  	; --SL-
D_BBB9  .equ     0x0BBB9  	; --SL-
D_BBBB  .equ     0x0BBBB  	; --SL-
D_BBBC  .equ     0x0BBBC  	; --SL-
D_BBBE  .equ     0x0BBBE  	; --SL-
DSBBBF  .equ     0x0BBBF  	; --S--
D_BBC0  .equ     0x0BBC0  	; --SL-
D_BBC2  .equ     0x0BBC2  	; --SL-
D_BBC4  .equ     0x0BBC4  	; --SL-
I_BBC5  .equ     0x0BBC5  	; ----I
I_BBC6  .equ     0x0BBC6  	; ----I
I_BBD2  .equ     0x0BBD2  	; ----I
D_BBDE  .equ     0x0BBDE  	; --S-I
D_BBDF  .equ     0x0BBDF  	; ---L-
D_BBE0  .equ     0x0BBE0  	; --SL-
D_BBE1  .equ     0x0BBE1  	; --SL-
D_BBE2  .equ     0x0BBE2  	; --SL-
D_BBE4  .equ     0x0BBE4  	; --SL-
D_BBE6  .equ     0x0BBE6  	; --SL-
D_BBE8  .equ     0x0BBE8  	; --SL-
D_BBEA  .equ     0x0BBEA  	; --SL-
D_BBEB  .equ     0x0BBEB  	; --SL-
D_BBED  .equ     0x0BBED  	; --SL-
D_BBEE  .equ     0x0BBEE  	; --SLI
D_BBF0  .equ     0x0BBF0  	; --SL-
D_BBF2  .equ     0x0BBF2  	; --S-I
D_BBF4  .equ     0x0BBF4  	; --SL-
ISBBF5  .equ     0x0BBF5  	; ----I
D_BBF6  .equ     0x0BBF6  	; --SL-
D_BBF8  .equ     0x0BBF8  	; --SLI
I_BBFA  .equ     0x0BBFA  	; ----I
D_BBFB  .equ     0x0BBFB  	; ---LI
DSBBFD  .equ     0x0BBFD  	; --S--
D_BBFE  .equ     0x0BBFE  	; --SLI
ISBBFF  .equ     0x0BBFF
I_BC00	.equ	0x0BC00	        ; ----I		ramdisk boot sector
D_BE00  .equ     0x0BE00          ; --SLI		number of ramdisk segments
I_BE02  .equ     0x0BE02          ; ----I		ramdisk segment table

; ------------------------------------------------------------------------------
; P3: system variables, hooks, buffers and paging helper routines

YC000   .equ     0x0C000          ; bootsector transferaddress
CC206   .equ     0x0C206          ; debugger breakpoint handler

DATABA	.equ	0x0F1C9		; start of disk system static work area
SPRTBUF	.equ	0x0F1C9		; BDOS _STROUT
JUMPB   .equ     0x0F1D3          ; start of handlers jump table
RD_LDI	.equ	0x0F1D3		; transfer to/from page 1
P0_LDIR .equ     0x0F1D6          ; transfer with page 0
P0_CALL .equ     0x0F1D9          ; call main-bios
SFLUSH  .equ     0x0F1DC          ; print string via chput
GO_DRV  .equ     0x0F1DF          ; interslot call with prompt handler
JP_VEC	.equ	0x0F1E2		; start DOS1 style handler
SIRQ    .equ     0x0F1E5		; KEYINT handler
SRDSLT  .equ     0x0F1E8		; RDSLT handler
SWRSLT  .equ     0x0F1EB		; WRSLT handler
SCALSLT .equ     0x0F1EE		; CALSLT handler
SENASLT .equ     0x0F1F1		; ENASLT handler
SCALLF  .equ     0x0F1F4		; CALLF handler
PUT_BD  .equ     0x0F1F7          ; enable BDOS segments
PUT_US  .equ     0x0F1FA          ; enable DOS segments
P0_RAM	.equ	0x0F1FD		; enable DOS RAM on page 0
MAP_VE  .equ     0x0F200
RD_SEG	.equ	0x0F206		; RD_SEG handler
WR_SEG  .equ     0x0F209          ; WR_SEG handler
PUT_P0	.equ	0x0F218		; PUT_P0 handler
GET_P0	.equ	0x0F21B		; GET_P0 handler
PUT_P1	.equ	0x0F21E		; PUT_P1 handler
GET_P1	.equ	0x0F221		; GET_P1 handler
PUT_P2	.equ	0x0F224		; PUT_P2 handler
GET_P2	.equ	0x0F227		; GET_P2 handler
CARPOS  .equ     0x0F237          ; BDOS console output columnpos
STARTP  .equ     0x0F238          ; console columnpos at start of lineinput
INSERT  .equ     0x0F239          ; lineinput insert flag
CRLFPO  .equ     0x0F23A          ; lineinput secret message flag
PFLAG   .equ     0x0F23B          ; console output to printer flag
CUR_DRV	.equ	0x0F23C		; default drive
DIRTYD  .equ     0x0F23C          ; directory buffer dirty flag (DOS1)
DTA_AD  .equ     0x0F23D          ; default transfer address
DMAADD  .equ     0x0F23D          ; transferaddress (DOS1)
BUFSEC  .equ     0x0F23F          ; sectornumber in data buffer
BUFDRN  .equ     0x0F241          ; driveid of sector in data buffer
DIRTYB  .equ     0x0F242          ; data buffer dirty flag
BUFDRI  .equ     0x0F243          ; DPB pointer current operation
DIRBFI  .equ     0x0F245          ; sectornumber (offset) in directory buffer
DIRBFD  .equ     0x0F246          ; driveid of sector in directory buffer (DOS1)
CURDRV  .equ	0x0F247		; default driveid (DOS1)
DAY     .equ     0x0F248          ; current day (1..31) (DOS1)
MONTH   .equ     0x0F249          ; current month (1..12) (DOS1)
YEAR    .equ     0x0F24A          ; current year (offset to 1980) (DOS1)
DAYCNT  .equ     0x0F24C          ; current days since 1-1-1980 (DOS1)
WEEKDA  .equ     0x0F24E          ; current day of week (0=sunday) (DOS1)
HOOKBE  .equ     0x0F24F		; -C--I
H_PROM  .equ     0x0F24F          ; prompt for disk hook
H_BDOS  .equ     0x0F252
H_UNPA  .equ     0x0F252          ; get fat entry content hook (DOS1)
H_UP    .equ     0x0F255
H_DEVN  .equ     0x0F255          ; check if devicename hook (DOS1)
H_16CH  .equ     0x0F258
H_CONT  .equ     0x0F258          ; try next direntry hook (DOS1)
H_CHIN  .equ     0x0F25B
H_GETE  .equ     0x0F25B          ; get next direntry hook (DOS1)
H_CHOU  .equ     0x0F25E
H_NEXT  .equ     0x0F25E          ; next direntry hook (DOS1)
H_CHST  .equ     0x0F261
H_MOVN  .equ     0x0F261          ; validate FCB drive and filename hook (DOS1)
H_CHFL  .equ     0x0F264
H_DOOP  .equ     0x0F264          ; fcb open hook (DOS1)
H_LSTO  .equ     0x0F267
H_FATR  .equ     0x0F267          ; get latest FAT hook (DOS1)
H_LSTS  .equ     0x0F26A
H_GETI  .equ     0x0F26A          ; get pointer to DPB of current drive hook (DOS1)
H_FATW  .equ     0x0F26D          ; write FAT hook
H_DREA  .equ     0x0F270          ; read sector hook
H_FATA  .equ     0x0F273          ; diskerror hook
H_DIRW  .equ     0x0F276          ; write dirsector hook
H_DWRI  .equ     0x0F279          ; write sector hook
H_MUL1  .equ     0x0F27C          ; multiply hook
H_DIV3  .equ     0x0F27F          ; divide hook
H_FNDC  .equ     0x0F282          ; get absolute cluster hook
H_SKPC  .equ     0x0F285          ; get next absolute cluster hook
H_BUFR  .equ     0x0F288          ; partical sector read hook
H_BUFW  .equ     0x0F28B          ; partical sector write hook
H_ENUF  .equ     0x0F28E          ; start read recordoperation from disk hook
H_SETF  .equ     0x0F291          ; finish read recordoperation from disk hook
H_SETC  .equ	0x0F294          ; end read recordoperation from disk hook
H_WRTE  .equ     0x0F297          ; record operation error at start hook
H_NORN  .equ     0x0F29A          ; start write recordoperation to disk hook
H_WRTL  .equ     0x0F29D          ; finish write recordoperation to disk hook
H_OPTI  .equ     0x0F2A0          ; calculate sEQUencial sectors hook
H_FIGR  .equ     0x0F2A3          ; get sectornumber of cluster hook
H_ALLO  .equ     0x0F2A6          ; allocate FAT chain hook
H_RELB  .equ     0x0F2A9          ; release FAT chain hook
H_GETC  .equ     0x0F2AC          ; lineinput headloop hook
H_OUT   .equ     0x0F2AF          ; console output hook
H_DATE  .equ     0x0F2B2          ; get time and date for direntry hook
H_SETY  .equ     0x0F2B5          ; setup days in februari hook
TIM_RA  .equ     0x0F2B8  	; ---L-  number of VBLANK interrupts per 1/10 second
LASTEN  .equ     0x0F2B8          ; current direntry number (DOS1)
TIM_CO  .equ     0x0F2B9  	; ----I
NAME1   .equ     0x0F2B9          ; filename1 (DOS1)
RANDOM  .equ     0x0F2BA  	; --SL-  'random' number
ST_COU  .equ     0x0F2BD  	; --S--
CH_COU  .equ     0x0F2BE  	; --SL-
TIM_TI  .equ     0x0F2BF  	; --SL-
TIMI_S  .equ     0x0F2C0  	; J---I
ATTRIB  .equ     0x0F2C4          ; original DR byte FCB
MAP_TAB .equ     0x0F2C5          ; memory mapper table
NAME2   .equ     0x0F2C5          ; filename2 (rename) (DOS1)
P0_SEG  .equ     0x0F2C7          ; current segment page 0
P1_SEG  .equ     0x0F2C8          ; current segment page 1
P2_SEG  .equ     0x0F2C9          ; current segment page 2
P3_SEG  .equ     0x0F2CA          ; current segment page 3
P0_TPA  .equ     0x0F2CB          ; segment page 0 at BDOS entry
P1_TPA  .equ     0x0F2CC          ; segment page 1 at BDOS entry
P2_TPA  .equ     0x0F2CD          ; segment page 2 at BDOS entry
DATA_S  .equ     0x0F2CF		; BDOS data segment
CODE_S  .equ     0x0F2D0		; BDOS code segment
NAME3   .equ     0x0F2D0          ; temporary save for NAME1 and ATTRIB (rename) (DOS1)
SP_IRQ  .equ     0x0F2D1          ; temporary save stackpointer (KEYINT)
IRQ_ST  .equ     0x0F2D3          ; pointer to temporary stack (KEYINT)
FCALSA  .equ     0x0F2D5  	; J---I
SERR_M  .equ     0x0F2DA  	; --S--
SPROMPT .equ     0x0F2DC          ; --S--  pointer to prompt hander
CREATI  .equ     0x0F2DC          ; flag ignore fileattributes (DOS1)
SECCLU  .equ     0x0F2DD          ; current relative sector in cluster
SBDOS   .equ     0x0F2DE  	; --S--
DSKERR  .equ     0x0F2DE          ; result of recordoperation (DOS1)
TRANS   .equ     0x0F2DF          ; flag increase current relative sector in cluster (0 means not)
SS_TEMP .equ     0x0F2E0
PREREA  .equ     0x0F2E0          ; flag flake read (0 means real read) (DOS1)
RD_ADDR .equ     0x0F2E1  	; --SL-
THISDR  .equ     0x0F2E1          ; current driveid (DOS1)
NEXTAD  .equ     0x0F2E2          ; transferaddress for recordoperation
RD_SNU  .equ     0x0F2E3  	; --SL-
RECPOS  .equ     0x0F2E4          ; start record (32 bit) for recordoperation
RD_SCN  .equ     0x0F2E5  	; --SL-
IX_BDOS .equ     0x0F2E6          ; ---L-  temporary save IX register BDOS call
SP_BDOS .equ     0x0F2E8  	; --SL-
RECCNT  .equ     0x0F2E8          ; number of records for recordoperation (DOS1)
PS_BDOS .equ     0x0F2EA		; same as F2EB, but for primary slots
LASTPO  .equ     0x0F2EA          ; current relative cluster of file (DOS1)
SS_BDOS .equ     0x0F2EB		; Secundary slot status when BDOS function handler was executed
DSK_CHK .equ     0x0F2EC
CLUSNU  .equ     0x0F2EC          ; current cluster of file (DOS1)
SECPOS  .equ     0x0F2EE          ; start relative sector for recordoperation
VALSEC  .equ     0x0F2F0          ; relative cluster after fileend for write recordoperation
BLDCHK  .equ     0x0F2F1  	; --S--
BYTSEC  .equ     0x0F2F2          ; start offset in sector for recordoperation
BSVCHK  .equ     0x0F2F3  	; --S--
BYTPOS  .equ     0x0F2F4          ; start fileposition (32 bit) for recordoperation
BUF_1   .equ     0x0F2F5  	; --SL-
BUF_2   .equ     0x0F2F7  	; --SL-
BYTCT1  .equ     0x0F2F8          ; partical sector transfer at start
BUF_3   .equ     0x0F2F9  	; --SL-
BYTCT2  .equ     0x0F2FA          ; partical sector transfer at end
ERR_BUF .equ     0x0F2FB  	; --S--
SECCNT  .equ     0x0F2FC          ; number of complete sectors to transfer
BOOT_D  .equ     0x0F2FD  	; --SL-
ST_BDOS .equ     0x0F2FE          ; --SL- pointer to temporary stack (BDOS)
ENTFRE  .equ     0x0F2FE          ; first free direntry (0x0FF if none found) (DOS1)
READOP  .equ     0x0F2FF          ; flag diskoperation (0 if read, 1 if write)
KDSK_V  .equ     0x0F300  	; --S--
CONTPO  .equ     0x0F300          ; pointer to remaining lineinput from CON read record operation (DOS1)
KAB_VE  .equ     0x0F302  	; --S--
MAXCLS  .equ     0x0F302          ; temporary store for maximium cluster (DOS1)
CPMCAL  .equ     0x0F306  	; --S--
SRCHFC  .equ     0x0F307          ; saved pointer to FCB search first, used for search next
SRCHIX  .equ     0x0F309          ; saved pointer to DPB search first, used for search next
SRCHLO  .equ     0x0F30B          ; saved current direntry number search first/next (0xFF means invalid)
FCBEXT  .equ     0x0F30C          ; original EX byte FCB
RAWFLG  .equ     0x0F30D		; read after write (verify) flag
COUNTR  .equ     0x0F30E  	; --S--
KANJTA  .equ     0x0F30F  	; ----I  double byte header table
DOSVER 	.equ     0x0F313  	; --SL-
FUTURE  .equ     0x0F313          ; (DOS1)
P0_64K  .equ     0x0F314  	; ---L-  TPA segment table
P1_64K  .equ     0x0F315  	; ---L-
P2_64K  .equ     0x0F316  	; ---L-
P3_64K  .equ     0x0F317
DISKVE  .equ     0x0F323  	; --SLI
BREAKV  .equ     0x0F325  	; --SL-
AUXBOD  .equ     0x0F327  	; ----I
BDOSBO  .equ     0x0F331          ; BDOS hook (MSXHOOK style)
SDOS1   .equ     0x0F333  	; --S--
KEYVLD  .equ     0x0F336          ; flag saved input available (0 = none available)
KEYCHR  .equ     0x0F337          ; save input
TIMFLG  .equ     0x0F338  	; --S--
F_SPSA  .equ     0x0F339          ; saved stackpointer format routine
PATHNAM .equ     0x0F33B  	; --SL-
CURDAT  .equ	0x0F33B          ; days since 1-1-1980, used when no clockchip (DOS1)
RECSIZE .equ     0x0F33D  	; ---L-
RECSIZ  .equ     0x0F33D          ; recordsize GET/PUT recordoperations (DOS1)
TARGET  .equ     0x0F33F  	; --SL-
NOTFIR	.equ	0x0F340		; first start DiskBASIC flag
RAMAD0  .equ     0x0F341
RAMAD1  .equ     0x0F342
RAMAD2	.equ	0x0F343		; slotid DOS ram page 2
RAMAD3	.equ	0x0F344		; slotid DOS ram page 3
FILMAX  .equ     0x0F345          ; maximum number of diskbasic FCB's
DOSFLG	.equ	0x0F346		; MSXDOS started flag
SNUMDR	.equ	0x0F347		; number of drives
MASTER	.equ	0x0F348		; slotid disksystem ROM
HIMSAV  .equ     0x0F349  	; --SL-
DOSHIM  .equ     0x0F34B  	; --SL-
SSECBUF .equ	0x0F34D
SMAXSEC .equ     0x0F34F  	; --S--
SBUFFE  .equ     0x0F34F          ; pointer to datasectorbuffer (DOS1)
SDIRBUF .equ     0x0F351  	; --S--
SDIRBU  .equ     0x0F351		; pointer to directorysectorbuffer (DOS1)
RM_DPB  .equ     0x0F353  	; --S-I
FCBBAS  .equ     0x0F353          ; pointer to the diskbasic FCB's (DOS1)
SDPBLI  .equ     0x0F355  	; ----I
RSLREG  .equ     0x0F365  	; ----I
SDOSON	.equ	0x0F368		; enable disksystem ROM on page 1
SDOSOF	.equ	0x0F36B		; enable TPA RAM on page 2
XFER	.equ	0x0F36E		; transfer to/from TPA RAM on page 2
SAUXIN  .equ     0x0F371
SAUXOUT .equ     0x0F374		; 
SAUXOU  .equ     0x0F374          ; auxiliary output routine (DOS1)
GO_BIOS .equ     0x0F377  	; J----
BLDCHK1 .equ     0x0F377          ; routine diskbasic BLOAD (DOS1)
GO_BDOS .equ     0x0F37A  	; J----
BSVCHK1 .equ     0x0F37A          ; routine diskbasic BSAVE (DOS1)
BDOS  	.equ     0x0F37D  	; -C---
VARWRK	.equ	0x0F380		; Start of variable workarea
RDPRIM  .equ     0x0F380
WRPRIM  .equ     0x0F385
RETRTN	.equ	0x0F38B		; on this adres is a simple RET instruction
CLPRIM  .equ     0x0F38C
CLPRM1  .equ     0x0F398
LINLEN	.equ	0x0F3B0
CRTCNT  .equ     0x0F3B1
CLIKSW	.equ	0x0F3DB
CNSDFG	.equ	0x0F3DE
REPCNT  .equ     0x0F3F7          ; time interval until key-repeat
PUTPNT 	.equ	0x0F3F8 		; Address in the keyboard buffer where a character will be written
GETPNT 	.equ	0x0F3FA		; Address in the keyboard buffer where the next character is read
LPTPOS	.equ	0x0F415
PRTFLG	.equ	0x0F416
CURLIN	.equ	0x0F41C
KBUF    .equ     0x0F41F
YCONTP  .equ     KBUF+58         ; KBUF is reused for temporary buffer buffered input, size 128
YCONBF  .equ     KBUF+186        ; KBUF is reused for read CON device records, size 127+2 (DOS1)
BUF	.equ	0x0F55E
TMPBUF	.equ	BUF+10		; temporary buffer (DOS1)
TTYPOS	.equ	0x0F661
VALTYP	.equ	0x0F663
MEMSIZ	.equ	0x0F672
STKTOP	.equ	0x0F674
TXTTAB	.equ	0x0F676
TEMPPT	.equ	0x0F678
TEMPST  .equ     0x0F67A		; (DOS1)
DSCTMP	.equ	0x0F698
FRETOP	.equ	0x0F69B
AUTLIN	.equ	0x0F6AB		; used for biggest sectorsize during disksystem init
MAXSEC	.equ	0x0F6AB		; (DOS1)
SAVSTK	.equ	0x0F6B1
VARTAB	.equ	0x0F6C2
STREND	.equ	0x0F6C6
DAC	.equ	0x0F7F6
ARG	.equ	0x0F847
MAXFIL	.equ	0x0F85F
FILTAB	.equ	0x0F860
NULBUF	.equ	0x0F862
PTRFIL	.equ	0x0F864
FILNAM	.equ	0x0F866
NLONLY	.equ	0x0F87C
SAVEND	.equ	0x0F87D
HOKVLD	.equ	0x0FB20		; EXTBIO valid flag (b0)
DRVTBL	.equ	0x0FB21		; diskdriver table
HOOKSA	.equ	0x0FB29		; diskdriver interrupt table
BASROM	.equ	0x0FBB1
NEWKEY	.equ	0x0FBE5
BOTTOM	.equ	0x0FC48
HIMEM	.equ	0x0FC4A
INTFLG  .equ     0x0FC9B
JIFFY	.equ	0x0FC9E
ESCCNT  .equ     0x0FCA7
CAPST	.equ	0x0FCAB
KANAST	.equ	0x0FCAC
FLBMEM	.equ	0x0FCAE
RUNBNF	.equ	0x0FCBE
SAVENT	.equ	0x0FCBF
EXPTBL	.equ	0x0FCC1
SLTTBL	.equ	0x0FCC5
SLTATR  .equ     0x0FCC9
SLTWRK	.equ	0x0FD09
PROCNM	.equ	0x0FD89
DEVICE	.equ	0x0FD99		; used temp for diskdriver count
DISKID	.equ	0x0FD99
H_TIMI  .equ     0x0FD9F
H_DSKO	.equ	0x0FDEF
H_NAME	.equ	0x0FDF9
H_KILL	.equ	0x0FDFE
H_COPY	.equ	0x0FE08
H_DSKF	.equ	0x0FE12
H_DSKI	.equ	0x0FE17
H_LSET	.equ	0x0FE21
H_RSET	.equ	0x0FE26
H_FIEL	.equ	0x0FE2B
H_MKIS	.equ	0x0FE30
H_MKSS	.equ	0x0FE35
H_MKDS	.equ	0x0FE3A
H_CVI	.equ	0x0FE3F
H_CVS	.equ	0x0FE44
H_CVD	.equ	0x0FE49
H_GETP	.equ	0x0FE4E
H_NOFO	.equ	0x0FE58
H_NULO	.equ	0x0FE5D
H_NTFL	.equ	0x0FE62
H_BINS	.equ	0x0FE71
H_BINL	.equ	0x0FE76
H_FILE	.equ	0x0FE7B
H_DGET	.equ	0x0FE80
H_FILO	.equ	0x0FE85
H_INDS	.equ	0x0FE8A
H_LOC	.equ	0x0FE99
H_LOF	.equ	0x0FE9E
H_EOF	.equ	0x0FEA3
H_BAKU	.equ	0x0FEAD
H_PARD	.equ	0x0FEB2
H_NODE	.equ	0x0FEB7
H_POSD	.equ	0x0FEBC
H_RUNC	.equ	0x0FECB
H_CLEA	.equ	0x0FED0
H_LOPD	.equ	0x0FED5
H_STKE	.equ	0x0FEDA
H_ERRP	.equ	0x0FEFD
H_PHYD	.equ	0x0FFA7
H_FORM	.equ	0x0FFAC
EXTBIO  .equ     0x0FFCA
DISINT	.equ	0x0FFCF
ENAINT	.equ	0x0FFD4
DFFFF   .equ     0x0FFFF
YFFFF   .equ     0x0FFFF		; (DOS1)

; ------------------------------------------------------------------------------
; Byte constants

_NCOMP	.equ	0x0FF		; incompatible disk
_WRERR	.equ	0x0FE		; write error
_DISK	.equ	0x0FD		; disk error
_NRDY	.equ	0x0FC		; not ready
_VERFY	.equ	0x0FB		; verify error
_DATA	.equ	0x0FA		; data error
_RNF	.equ	0x0F9		; sector not found
_WPROT	.equ	0x0F8		; write protected disk
_UFORM	.equ	0x0F7		; unformatted disk
_NDOS	.equ	0x0F6		; not a DOS disk
_WDISK	.equ	0x0F5		; wrong disk
_WFILE	.equ	0x0F4		; wrong disk for file
_SEEK	.equ	0x0F3		; seek error
_IFAT	.equ	0x0F2		; bad file allocation table
_NOUPB	.equ	0x0F1		;
_IFORM	.equ	0x0F0		; cannot format this drive

_INTER	.equ	0x0DF		; internal error
_NORAM	.equ	0x0DE		; not enough memory
_IBDOS	.equ	0x0DC		; invalid BDOS call
_IDRV	.equ	0x0DB		; invalid drive
_IFNM	.equ	0x0DA		; invalid filename
_IPATH	.equ	0x0D9		; invalid pathname
_PLONG	.equ	0x0D8		; pathname too long
_NOFIL	.equ	0x0D7		; file not found
_NODIR	.equ	0x0D6		; directory not found
_DRFUL	.equ	0x0D5		; boot directory full
_DKFUL	.equ	0x0D4		; disk full
_DUPF	.equ	0x0D3		; duplicate filename
_DIRE	.equ	0x0D2		; invalid directory move
_FILRO	.equ	0x0D1		; read only file
_DIRNE	.equ	0x0D0		; directory not empty
_IATTR	.equ	0x0CF		; invalid attributes
_DOT	.equ	0x0CE		; invalid . or .. operation
_SYSX	.equ	0x0CD		; system file exists
_DIRX	.equ	0x0CC		; directory exists
_FILEX	.equ	0x0CB		; file exists
_FOPEN	.equ	0x0CA		; file is already in use
_OV64K	.equ	0x0C9		; cannot transfer above 64K
_FILE	.equ	0x0C8		; file allocation error
_EOF	.equ	0x0C7		; end of file
_ACCV	.equ	0x0C6		; file access violation
_IPROC	.equ	0x0C5		; invalid process id
_NHAND	.equ	0x0C4		; no spare file handles
_IHAND	.equ	0x0C3		; invalid file handle
_NOPEN	.equ	0x0C2		; file handle not open
_IDEV	.equ	0x0C1		; invalid device operation
_IENV	.equ	0x0C0		; invalid environment string
_ELONG	.equ	0x0BF		; environment string too long
_IDATE	.equ	0x0BE		; invalid date
_ITIME	.equ	0x0BD		; invalid time
_RAMDX	.equ	0x0BC		; ram disk already exists
_NRAMD	.equ	0x0BB		; ram disk does not exist
_HDEAD	.equ	0x0BA		; file handle has been deleted
_EOL	.equ	0x0B9		; end of line
_ISBFN	.equ	0x0B8		; invalid sub-function number

_STOP	.equ	0x09F
_CTRLC	.equ	0x09E
_ABORT	.equ	0x09D
_OUTERR .equ	0x09C
_INERR	.equ	0x09B

_IPARM	.equ	0x08B

; ------------------------------------------------------------------------------
; Structure information

; FCB structure

; +0	DR	drive			drive
; +1,8	F1-F8	file name		file name
; +9,3	T1-T3	filetype		filetype
; +12	EX	extent			extent (low byte)
; +13	S1	reserved		fileattribute
; +14	S2	reserved		extent high byte / record size low byte (block)
; +15	RC	record count in extent	record count in extent / record size high byte (block)
; +16,4	AL	allocation		Filesize
; +20,4	AL	allocation		volume-id
; +24	AL	allocation	
; +25	AL	allocation	
; +26,2	AL	allocation		start cluster
; +28,2	AL	allocation		current cluster
; +30,2	AL	allocation		current relative cluster
; +32	CR	record in extent	record in extent
; +33,3	R0-R2	random access record	random access record
; +36	R4	not used		random access record when record size <64


; FIB structure

; +0		fib indicator (0x0FF)
; +1,13		file name as an ASCIIZ string
; +14		file attributes byte
; +15,2		time of last modification
; +17,2		date of last modification
; +19,2		start cluster
; +21,4		file size
; +25		logical drive
; +26,4		disk serial (if on disk)
; +28,2		pointer to device jump table (if device)
; +30		device flags
; +32,2		pointer to drive table
; +34,2		current directory sector
; +36		current directory entry in sector
; +37,2		cluster number of parent directory
; +39,2		start cluster of file
; +41,2		current cluster of file
; +43,2		current relative cluster of file
; +45,4		current file position
; +49		open mode
; +50


; drive table structure

; +0		slot id interface
; +1		offset (relative to 0x4000) of jumptable of interface driver
; +2,2		pointer to Drive Parameter Block (DPB) of drive
; +4		flags
; +5		flags
; +6		drive id used within interface driver
; +7		
; +8		drivename
; +9		1/10 seconds+2 that disk cannot be changed (0 = init, 1 = error)
; +10		cluster mask
; +11		cluster shift
; +12,2		number of reserved sectors (bootarea)
; +14		number of FAT's
; +15		remainder of directory entries (no whole sector)
; +16		number of directorysectors (whole sectors)
; +17		number of sectors per FAT
; +18,2		first sector of rootdirectory
; +20,2		first sector of data area
; +22,2		number of clusters+1 on disk
; +24		dirty flag Bit 0 = 1, when on a DOS2 disk file(s) have been deleted
; +25,4		volume serial number (0x0FFFFFFFF if none)
; +29		media descriptor byte of disk
; +30,2		starting cluster of current directory (bit 15 = 1 means root)
; +32,64	current directory (ASCIIZ) without "drive:\" prefix


; device entry

; +0,2		pointer to jump table
; +2
; +8		device flags
; +9,11		device name
; +10,33


; directory entry

; +0,8		file name (main)
; +8,3		file name (extension)
; +11		entry attribute
; +12,10	not used
; +22,2		time
; +24,2		date
; +26,2		starting cluster
; +28,4		file size
