
#define FLAG_RX_CRC                 (1 << 0)
#define FLAG_TX_CRC                 (1 << 1)
#define FLAG_RETRY_TIMEOUT          (1 << 2)
#define FLAG_RETRY_CRC              (1 << 3)

#define COMMAND_DRIVE_REPORT_OK		       0
#define COMMAND_DRIVE_REPORT_WRITE_PROTECTED   0+1
#define COMMAND_DRIVE_REPORT_DRIVE_NOT_READY   2+1
#define COMMAND_DRIVE_REPORT_CRC_ERROR         4+1
#define COMMAND_DRIVE_REPORT_WRITE_FAULT       10+1

#define COMMAND_DRIVE_READ		      16
#define COMMAND_DRIVE_WRITE		      17
#define COMMAND_DRIVE_INFO		      18

#define DRIVE_ACKNOWLEDGE_WRITE_OK            0x1111
#define DRIVE_ACKNOWLEDGE_WRITE_FAILED        0x2222
#define DRIVE_ACKNOWLEDGE_WRITE_PROTECTED     0x3333
