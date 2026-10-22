#include <QAudioSink>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QBuffer>
#include <QFile>
#include <QTimer>
#include <cmath>
#include <cstring>

#include "CasPlayer.h"

// MSX tape signal (MSX2 Technical Handbook, cassette interface): bit 0 = 1 cycle of 1200 Hz, bit 1 = 2 cycles of
// 2400 Hz, byte = start bit 0, 8 bits (bit 0 first), 2 stop bits 1. Each block of the .CAS file (after its 8 bytes
// header) is preceded by a silence and a header tone: long before a file header (10 bytes D0H, D3H or EAH), short
// before the data.
// Headers much shorter than on a tape (16000 and 4000 cycles at 1200 bauds, 6.7 s and 1.7 s): no place to find on a
// tape here. The BIOS (TAPION) needs a number of cycles whatever the speed: loaded by openMSX (VG-8020, VG-8235, 1200
// and 2400 bauds, 44.1 and 48 kHz) from 1500 cycles, not with 1200 or less.
#ifndef CAS_LONG_HEADER
#define CAS_LONG_HEADER         3000        // cycles of 2 x bauds Hz (1.25 s at 1200 bauds)
#define CAS_SHORT_HEADER        2500        // (1 s at 1200 bauds)
#define CAS_LONG_SILENCE        0.5         // seconds (also while the audio output starts)
#define CAS_SHORT_SILENCE       0.3
#endif
#define CAS_MAX_RATE            192000      // Hz (wave of 30 MB at 1200 bauds, stereo, 32 bits)
#define CAS_END_SILENCE         1.0         // the end is played before the audio output goes idle
#define CAS_AMPLITUDE           0.8f

static const char g_acCasHeader[8] = { '\x1F', '\xA6', '\xDE', '\xBA', '\xCC', '\x13', '\x7D', '\x74' };

/*
 =======================================================================================================================
    Square wave in the format of the audio output: frames of the 3 levels (silence, high, low) appended, durations in
    samples accumulated without rounding drift
 =======================================================================================================================
 */
class CasWave
{
public:
    CasWave(const QAudioFormat &_roFormat, int _iBauds, QByteArray &_racData) :
        m_dRate(_roFormat.sampleRate()),
        m_iBauds(_iBauds),
        m_racData(_racData)
    {
        m_aacFrames[0] = acFrame(_roFormat, 0.0f);
        m_aacFrames[1] = acFrame(_roFormat, CAS_AMPLITUDE);
        m_aacFrames[2] = acFrame(_roFormat, -CAS_AMPLITUDE);
    }

    void vSilence(double _dSeconds) { vLevel(0, _dSeconds * m_dRate); }
    void vCycle(double _dFrequency)
    {
        vLevel(1, m_dRate / (2 * _dFrequency));
        vLevel(2, m_dRate / (2 * _dFrequency));
    }
    void vHeader(int _iCycles) { for(int i = 0; i < _iCycles; i++) vCycle(2 * m_iBauds); }
    void vBit(bool _bOne)
    {
        if(_bOne)
        {
            vCycle(2 * m_iBauds);
            vCycle(2 * m_iBauds);
        }
        else
            vCycle(m_iBauds);
    }
    void vByte(quint8 _ucByte)
    {
        vBit(false);
        for(int i = 0; i < 8; i++) vBit((_ucByte >> i) & 1);
        vBit(true);
        vBit(true);
    }

private:
    static QByteArray acFrame(const QAudioFormat &_roFormat, float _fLevel)
    {
        /*~~~~~~~~~~~~~~*/
        QByteArray  acFrame;
        char        acSample[4];
        int         iSize = _roFormat.bytesPerSample();
        /*~~~~~~~~~~~~~~*/

        switch(_roFormat.sampleFormat())
        {
            case QAudioFormat::UInt8:
            {
                quint8 ucValue = 128 + qRound(_fLevel * 127);
                memcpy(acSample, &ucValue, 1);
            }
            break;

            case QAudioFormat::Int16:
            {
                qint16 iValue = qRound(_fLevel * 32767);
                memcpy(acSample, &iValue, 2);
            }
            break;

            case QAudioFormat::Int32:
            {
                qint32 iValue = qRound(_fLevel * 2147483647.0);
                memcpy(acSample, &iValue, 4);
            }
            break;

            default:
                memcpy(acSample, &_fLevel, 4);
                break;
        }
        for(int i = 0; i < _roFormat.channelCount(); i++) acFrame.append(acSample, iSize);
        return acFrame;
    }

    void vLevel(int _iLevel, double _dSamples)
    {
        m_dTime += _dSamples;

        qint64 iEnd = llround(m_dTime);

        for(; m_iSamples < iEnd; m_iSamples++) m_racData.append(m_aacFrames[_iLevel]);
    }

    double      m_dRate;
    int         m_iBauds;
    double      m_dTime = 0;
    qint64      m_iSamples = 0;
    QByteArray  m_aacFrames[3];
    QByteArray  &m_racData;
};

/*
 =======================================================================================================================
    Wave of a .CAS file in the given format. Output: false if no block is found
 =======================================================================================================================
 */
bool CasPlayer::bMakeWave(const QByteArray &_racCas, const QAudioFormat &_roFormat, int _iBauds, QByteArray &_racWave)
{
    /*~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QList<qsizetype> aiBlocks;
    /*~~~~~~~~~~~~~~~~~~~~~~~~~*/

    // blocks: after each header (at a multiple of 8 bytes), up to the next header
    for(qsizetype i = 0; i + 8 <= _racCas.size(); i += 8)
    {
        if(!memcmp(_racCas.constData() + i, g_acCasHeader, 8)) aiBlocks.append(i);
    }
    if(aiBlocks.isEmpty()) return false;
    aiBlocks.append(_racCas.size());

    _racWave.clear();

    CasWave oWave(_roFormat, _iBauds, _racWave);

    for(int iBlock = 0; iBlock + 1 < aiBlocks.size(); iBlock++)
    {
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
        QByteArray  acBlock = _racCas.mid(aiBlocks[iBlock] + 8, aiBlocks[iBlock + 1] - aiBlocks[iBlock] - 8);
        bool        bFileHeader = acBlock.size() >= 10;
        /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

        for(int i = 0; bFileHeader && (i < 10); i++)
        {
            quint8 ucByte = acBlock[i];

            bFileHeader = (ucByte == (quint8) acBlock[0]) && ((ucByte == 0xD0) || (ucByte == 0xD3) || (ucByte == 0xEA));
        }
        oWave.vSilence(bFileHeader ? CAS_LONG_SILENCE : CAS_SHORT_SILENCE);
        oWave.vHeader(bFileHeader ? CAS_LONG_HEADER : CAS_SHORT_HEADER);
        for(char cByte : acBlock) oWave.vByte(cByte);
    }
    oWave.vSilence(CAS_END_SILENCE);
    return true;
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
CasPlayer::CasPlayer(QObject *_poParent) :
    QObject(_poParent)
{
    m_poProgressTimer = new QTimer(this);
    m_poProgressTimer->setInterval(250);
    connect(m_poProgressTimer, &QTimer::timeout, this, [this]()
    {
        if(m_poSink) emit progress(qMax<qint64>(0, (m_iDurationUSecs - m_poSink->processedUSecs() + 999999) / 1000000));
    });
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
CasPlayer::~CasPlayer()
{
    vStop();
}

/*
 =======================================================================================================================
    Wave of the .CAS file made in the format preferred by the default audio output, then played. Output: false (and
    _rszError) if it cannot be played
 =======================================================================================================================
 */
bool CasPlayer::bPlay(const QString &_szPath, int _iBauds, QString &_rszError)
{
    vStop();

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QFile           oFile(_szPath);
    QAudioDevice    oDevice = QMediaDevices::defaultAudioOutput();
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    if(!oFile.open(QIODevice::ReadOnly))
    {
        _rszError = "cannot read " + _szPath;
        return false;
    }
    if(oDevice.isNull())
    {
        _rszError = "no audio output";
        return false;
    }

    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
    QAudioFormat    oFormat = oDevice.preferredFormat();
    /*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/

    // highest rate supported by the audio output (more regular edges), up to CAS_MAX_RATE (size of the wave in
    // memory), else the preferred rate, 44.1 kHz at least if possible: at 22.05 kHz, the edges of the 2400 Hz cycles
    // (4.6 samples per half cycle) are too irregular (not loaded by openMSX)
    for(int iRate : { qMin(oDevice.maximumSampleRate(), CAS_MAX_RATE), 192000, 96000, 48000, 44100 })
    {
        if(iRate > CAS_MAX_RATE) continue;
        if(oFormat.sampleRate() >= iRate) break;

        QAudioFormat oHigher = oFormat;

        oHigher.setSampleRate(iRate);
        if(oDevice.isFormatSupported(oHigher))
        {
            oFormat = oHigher;
            break;
        }
    }

    if(!bMakeWave(oFile.readAll(), oFormat, _iBauds, m_acWave))
    {
        _rszError = "not a .CAS file: " + _szPath;
        return false;
    }

    m_iDurationUSecs = oFormat.durationForBytes(m_acWave.size());
    m_szDeviceName = oDevice.description();
    m_iSampleRate = oFormat.sampleRate();

    m_poBuffer = new QBuffer(&m_acWave, this);
    m_poBuffer->open(QIODevice::ReadOnly);
    m_poSink = new QAudioSink(oDevice, oFormat, this);
    m_poSink->setVolume(1.0);
    connect(m_poSink, &QAudioSink::stateChanged, this, [this](QAudio::State _eState)
    {
        if(!m_poSink) return;
        if(_eState == QAudio::IdleState)
            vFinish(QString());     // all the wave played
        else if((_eState == QAudio::StoppedState) && (m_poSink->error() != QAudio::NoError))
            vFinish("audio output error");
    });
    m_poSink->start(m_poBuffer);
    if(m_poSink->error() != QAudio::NoError)
    {
        vStop();
        _rszError = "cannot open the audio output " + m_szDeviceName;
        return false;
    }
    m_poProgressTimer->start();
    emit progress((m_iDurationUSecs + 999999) / 1000000);
    return true;
}

/*
 =======================================================================================================================
    Stopped by the user: no finished signal
 =======================================================================================================================
 */
void CasPlayer::vStop()
{
    m_poProgressTimer->stop();
    if(m_poSink)
    {
        // deleted later: vStop can be called from a signal of the sink
        m_poSink->disconnect(this);
        m_poSink->stop();
        m_poSink->deleteLater();
        m_poSink = nullptr;
    }
    if(m_poBuffer)
    {
        m_poBuffer->deleteLater();
        m_poBuffer = nullptr;
    }
}

/*
 =======================================================================================================================
 =======================================================================================================================
 */
void CasPlayer::vFinish(const QString &_szError)
{
    vStop();
    emit finished(_szError);
}
