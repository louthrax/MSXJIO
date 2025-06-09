#ifndef Common_h
#define Common_h

typedef enum {
    eLogInfo,
    eLogWarning,
    eLogError,
    eLogConnected,
    eLogRead,
    eLogWrite
} tdLogType;

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
