#include "musicplayer.h"
#include "musicplaylist.h"
#include "musicsettingmanager.h"

MusicPlayer::MusicPlayer(QObject *parent)
    : QObject(parent)
{
    m_playlist = NULL;
    m_music = NULL;
    m_state = StoppedState;
    m_music = CreateZPlay();
    Q_ASSERT(m_music);
    m_equalizer = new MusicEqualizer(m_music);
    connect(&m_timer, SIGNAL(timeout()), SLOT(setTimeOut()));
    m_play3DMusic = false;
    m_posOnCircle = 0;
    m_currentVolumn = 100;
}

MusicPlayer::~MusicPlayer()
{
    delete m_equalizer;
    m_music->Close();
    m_music->Release();
}

MusicPlayer::State MusicPlayer::state() const
{
    return m_state;
}

MusicPlaylist *MusicPlayer::playlist() const
{
    return m_playlist;
}

qint64 MusicPlayer::duration() const
{
    TStreamInfo pInfo;
    m_music->GetStreamInfo(&pInfo);
    return pInfo.Length.hms.hour * 3600000 + pInfo.Length.hms.minute * 60000 +
           pInfo.Length.hms.second * 1000 + pInfo.Length.hms.millisecond;
}

qint64 MusicPlayer::position() const
{
    TStreamTime pos;
    m_music->GetPosition(&pos);
    return pos.hms.hour * 3600000 + pos.hms.minute * 60000 +
           pos.hms.second * 1000 + pos.hms.millisecond;
}

int MusicPlayer::volume() const
{
    uint vol;
    m_music->GetMasterVolume(&vol, &vol);
    return vol;
}

bool MusicPlayer::isMuted() const
{
    return (volume() == 0) ? true : false;
}

void MusicPlayer::setPlay3DMusicFlag(bool &flag)
{
    flag = m_play3DMusic;
    m_play3DMusic = !m_play3DMusic;
    m_music->EnableEcho(m_play3DMusic);
}

#ifdef Q_OS_WIN32
void MusicPlayer::setSpectrum(HWND wnd, int w, int h, int x, int y)
{
    if(!m_music)
    {
        return;
    }
    /// set graph type to AREA, left channel on top
    m_music->SetFFTGraphParam(gpGraphType, gtAreaLeftOnTop);
    /// set linear scale
    m_music->SetFFTGraphParam(gpHorizontalScale, gsLinear);
    m_music->DrawFFTGraphOnHWND(wnd, x, y, w, h);
}
#endif

void MusicPlayer::play()
{
    if(m_playlist->isEmpty())
    {
        return;
    }

    TStreamStatus status;
    m_music->GetStatus(&status);///Get the current state of play
    if(m_currentMedia == m_playlist->currentMediaString() &&
       status.fPause)
    {
        m_music->Resume();///When the pause time for recovery
        m_timer.start(1000);
        return;
    }

    m_music->Close();///For the release of the last memory
    m_currentMedia = m_playlist->currentMediaString();
    ///The current playback path
    if(m_music->OpenFileW(m_currentMedia.toStdWString().c_str(), sfAutodetect) == 0)
    {
        qDebug()<<"Error1."<<m_music->GetError();
        return;
    }

    m_timer.start(1000);
    ///Every second emits a signal change information
    emit positionChanged(0);
    emit durationChanged( duration() );
    m_state = PlayingState;
    if(m_music->Play() == 0)
    {
        qDebug()<<"Error2."<<m_music->GetError();
        return;
    }

    ////////////////////////////////////////////////
    ///Read the configuration settings for the sound
    int volumn = M_SETTING->value(MusicSettingManager::VolumeChoiced).toInt();
    if(volumn != -1)
    {
        setVolume(volumn);
    }
    ////////////////////////////////////////////////
}

void MusicPlayer::playNext()
{
    int index = m_playlist->currentIndex();
    m_playlist->setCurrentIndex((++index >= m_playlist->mediaCount()) ? 0 : index);
}

void MusicPlayer::playPrivious()
{
    int index = m_playlist->currentIndex();
    m_playlist->setCurrentIndex((--index < 0) ? 0 : index );
}

void MusicPlayer::pause()
{
    m_music->Pause();
    m_timer.stop();
    m_state = PausedState;
}

void MusicPlayer::stop()
{
    m_music->Stop();
    m_timer.stop();
    m_state = StoppedState;
}

void MusicPlayer::setPosition(qint64 position)
{
    TStreamTime pTime;
    pTime.ms = position;
    m_music->Seek(tfMillisecond, &pTime, smFromBeginning);
}

void MusicPlayer::setVolume(int volume)
{
    m_currentVolumn = volume;
    m_music->SetMasterVolume(m_currentVolumn, m_currentVolumn);
}

void MusicPlayer::setMuted(bool muted)
{
    muted ? m_music->SetMasterVolume(0, 0) :
            m_music->SetMasterVolume(m_currentVolumn, m_currentVolumn);
}

void MusicPlayer::setPlaylist(MusicPlaylist *playlist)
{
    m_playlist = playlist;
    connect(m_playlist, SIGNAL(removeCurrentMedia()),
                        SLOT(removeCurrentMedia()));
}

void MusicPlayer::setTimeOut()
{
    emit positionChanged( position() );

    if(m_play3DMusic)
    {   ///3D music settings
        m_posOnCircle += 0.5f;
        TEchoEffect effect;
        effect.nLeftDelay = 1000;
        effect.nLeftEchoVolume = 20;
        effect.nLeftSrcVolume = 100 * cosf(m_posOnCircle);
        effect.nRightDelay = 500;
        effect.nRightEchoVolume = 20;
        effect.nRightSrcVolume = 100 * sinf(m_posOnCircle * 0.5f);
        m_music->SetEchoParam(&effect, 1);
    }

    TStreamStatus status;
    m_music->GetStatus(&status);
    if(!status.fPlay && !status.fPause)
    {
        m_timer.stop();
        if(m_playlist->playbackMode() == MusicObject::MC_PlayOnce)
        {
            m_music->Stop();
            emit positionChanged(0);
            emit stateChanged();
            return;
        }
        m_playlist->setCurrentIndex();
        if(m_playlist->playbackMode() == MusicObject::MC_PlayOrder &&
           m_playlist->currentIndex() == -1)
        {
            m_music->Stop();
            emit positionChanged(0);
            emit stateChanged();
            return;
        }
        play();
    }
}

void MusicPlayer::removeCurrentMedia()
{
    if(m_music)
    {
        m_timer.stop();
        m_music->Close();
    }
}

void MusicPlayer::setEqEffect(const MIntList &hz)
{
    m_equalizer->setEqEffect(hz);
}

void MusicPlayer::setEnaleEffect(bool enable)
{
    m_equalizer->setEnaleEffect(enable);
}

void MusicPlayer::setEqInformation()
{
    m_equalizer->readEqInformation();
}

void MusicPlayer::resetEqEffect()
{
    m_equalizer->resetEqEffect();
}

void MusicPlayer::setSpEqEffect(MusicObject::SpecialEQ eq)
{
    if(m_state != PlayingState) return;
    switch(eq)
    {
        case MusicObject::EQ_EchoEffect:
                m_equalizer->setEchoEffect();break;
        case MusicObject::EQ_MixChannelEffect:
                m_equalizer->setMixChannelEffect();break;
        case MusicObject::EQ_ReverseEffect:
                m_equalizer->setReverseEffect();break;
        case MusicObject::EQ_SideCutEffect:
                m_equalizer->setSideCutEffect();break;
        case MusicObject::EQ_CenterCutEffect:
                m_equalizer->setCenterCutEffect();break;
        case MusicObject::EQ_RateUpEffect:
                m_equalizer->setRateUpEffect();break;
        case MusicObject::EQ_RateDownEffect:
                m_equalizer->setRateDownEffect();break;
        case MusicObject::EQ_PitchUpEffect:
                m_equalizer->setPitchUpEffect();break;
        case MusicObject::EQ_PitchDownEffect:
                m_equalizer->setPitchDownEffect();break;
        case MusicObject::EQ_TempoUpEffect:
                m_equalizer->setTempoUpEffect();break;
        case MusicObject::EQ_TempoDownEffect:
                m_equalizer->setTempoDownEffect();break;
        case MusicObject::EQ_FadeOutEffect:
                m_equalizer->setFadeOutEffect();break;
        case MusicObject::EQ_FadeInEffect:
                m_equalizer->setFadeInEffect();break;
    }
}
