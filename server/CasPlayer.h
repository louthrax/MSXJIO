#ifndef CasPlayer_h
#define CasPlayer_h

#include <QObject>
#include <QByteArray>
#include <QString>

class QAudioSink;
class QAudioFormat;
class QBuffer;
class QTimer;

#define CAS_BAUDS               1200        // default speed of the tape signal

/*
 =======================================================================================================================
    .CAS file played on the default audio output, as an MSX tape (FSK at 1200, 2400 or more bauds, as cas2wav and openMSX): to connect
    to the tape input of the MSX (BLOAD"CAS:",R)
 =======================================================================================================================
 */
class CasPlayer :
                  public QObject
{
    Q_OBJECT

public:
    explicit CasPlayer(QObject *_poParent = nullptr);
    ~CasPlayer();

    bool        bPlay(const QString &_szPath, int _iBauds, QString &_rszError);
    void        vStop();
    bool        bIsPlaying() const { return m_poSink != nullptr; }
    QString     szDeviceName() const { return m_szDeviceName; }
    int         iSampleRate() const { return m_iSampleRate; }

    static bool bMakeWave(const QByteArray &_racCas, const QAudioFormat &_roFormat, int _iBauds, QByteArray &_racWave);

signals:
    void        progress(int _iSecondsLeft);
    void        finished(const QString &_szError);     // empty if played to the end, or stopped

private:
    void        vFinish(const QString &_szError);

    QAudioSink  *m_poSink = nullptr;
    QBuffer     *m_poBuffer = nullptr;
    QTimer      *m_poProgressTimer = nullptr;
    QByteArray  m_acWave;
    qint64      m_iDurationUSecs = 0;
    QString     m_szDeviceName;
    int         m_iSampleRate = 0;
};

#endif
