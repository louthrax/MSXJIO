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

#ifdef Q_OS_ANDROID
    #define APPLICATION_FONT_SIZE       14
    #define LOG_WIDGET_FONT_SIZE        12
    #define NAMES_LIST_WIDGET_FONT_SIZE 14
#else
    #define APPLICATION_FONT_SIZE       11
    #define LOG_WIDGET_FONT_SIZE        10
    #define NAMES_LIST_WIDGET_FONT_SIZE 10
#endif

#endif
