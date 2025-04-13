
#define FLAG_RX_CRC                 (1 << 0)
#define FLAG_TX_CRC                 (1 << 1)
#define FLAG_RETRY_TIMEOUT          (1 << 2)
#define FLAG_RETRY_CRC              (1 << 3)

#define COMMAND_DRIVE_REPORT_OK		      0
#define COMMAND_DRIVE_REPORT_BAD_RX_CRC       1
#define COMMAND_DRIVE_REPORT_BAD_TX_CRC       2
#define COMMAND_DRIVE_REPORT_BAD_ACKNOWLEDGE  3
#define COMMAND_DRIVE_REPORT_TIMEOUT          4
#define COMMAND_DRIVE_READ		      5
#define COMMAND_DRIVE_WRITE		      6
#define COMMAND_DRIVE_INFO		      7
