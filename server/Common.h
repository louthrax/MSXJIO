#ifndef Common_h
#define Common_h

typedef enum {
    eLogInfo,
    eLogWarning,
    eLogError,
    eLogConnected,
    eLogClient,             // text sent by the MSX (COMMAND_LOG)
    eLogRead,
    eLogWrite,
    eLogBDOS,
    eLogBDOSModify,         // BDOS function modifying the served directories
    eLogBDOSDetails
} tdLogType;

#define LOG_LINES                       5000    // lines kept in the log of the graphical server
#define REQUEST_TIMEOUT                 2000    // ms without data in a request: request abandoned at the next data

#if defined(Q_OS_ANDROID) || defined(Q_OS_MAC)
    #define APPLICATION_FONT_SIZE       15
    #define LOG_WIDGET_FONT_SIZE        12
    #define NAMES_LIST_WIDGET_FONT_SIZE 17
#else
    #define APPLICATION_FONT_SIZE       11
    #define LOG_WIDGET_FONT_SIZE        9
    #define NAMES_LIST_WIDGET_FONT_SIZE 13
#endif

#endif
