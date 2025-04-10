
#define FLAGBITS_RX_CRC                 (1 << 4)
#define FLAGBITS_TX_CRC                 (1 << 5)
#define FLAGBITS_RETRY_TIMEOUT          (1 << 6)
#define FLAGBITS_RETRY_CRC              (1 << 7)

#define COMMAND_OK			0
#define COMMAND_READ			1
#define COMMAND_WRITE			2
#define COMMAND_INFO			3
#define COMMAND_REPORT_BAD_RX_CRC       4
#define COMMAND_REPORT_BAD_TX_CRC       5
#define COMMAND_REPORT_BAD_ACKNOWLEDGE  6
#define COMMAND_REPORT_TIMEOUT          7
