#include "AudioManager.hpp"

#include <cmath>
#include <iostream>
#include <random>

namespace {
const unsigned int kSampleRate = 22050;
const float kPi = 3.14159265f;

// Hàm hỗ trợ tạo SoundBuffer an toàn trong SFML 3
bool makeBuffer(sf::SoundBuffer& buffer, const std::vector<std::int16_t>& samples) {
    try {
        const std::vector<sf::SoundChannel> channels = {sf::SoundChannel::Mono};
        return buffer.loadFromSamples(samples.data(), samples.size(), 1, kSampleRate, channels);
    } catch (...) {
        return false;
    }
}
}  // namespace

AudioManager::AudioManager() {
    init();
}

void AudioManager::init() {
    if (m_initialized) return;
    try {
        generateProceduralSounds();
        m_initialized = true;
    } catch (const std::exception& e) {
        std::cerr << "[Audio] Khong khoi tao duoc thiet bi am thanh: " << e.what() << '\n';
    } catch (...) {
        std::cerr << "[Audio] Khong khoi tao duoc thiet bi am thanh.\n";
    }
}

void AudioManager::generateProceduralSounds() {
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> noiseDist(-1.f, 1.f);

    // 1) Slash (Tiếng vung gậy / gió rít)
    {
        const int count = static_cast<int>(kSampleRate * 0.12f);
        std::vector<std::int16_t> samples(count);
        for (int i = 0; i < count; ++i) {
            const float t = static_cast<float>(i) / count;
            const float freq = 650.f * (1.f - t) + 120.f;
            const float sine = std::sin(2.f * kPi * freq * (static_cast<float>(i) / kSampleRate));
            const float noise = noiseDist(rng) * 0.4f;
            const float env = std::sin(t * kPi);  // Envelope vòm
            samples[i] = static_cast<std::int16_t>((sine + noise) * env * 18000.f);
        }
        makeBuffer(m_bufSlash, samples);
    }

    // 2) Hit (Tiếng đánh trúng quái)
    {
        const int count = static_cast<int>(kSampleRate * 0.14f);
        std::vector<std::int16_t> samples(count);
        for (int i = 0; i < count; ++i) {
            const float t = static_cast<float>(i) / count;
            const float freq = 220.f * (1.f - t) + 40.f;
            const float sine = std::sin(2.f * kPi * freq * (static_cast<float>(i) / kSampleRate));
            const float env = (1.f - t) * (1.f - t);
            samples[i] = static_cast<std::int16_t>(sine * env * 24000.f);
        }
        makeBuffer(m_bufHit, samples);
    }

    // 3) Quái chết (Slime die)
    {
        const int count = static_cast<int>(kSampleRate * 0.22f);
        std::vector<std::int16_t> samples(count);
        for (int i = 0; i < count; ++i) {
            const float t = static_cast<float>(i) / count;
            const float freq = 340.f * (1.f - t * 0.7f);
            const float squish = std::sin(2.f * kPi * freq * (static_cast<float>(i) / kSampleRate)) +
                                 std::sin(2.f * kPi * (freq * 0.5f) * (static_cast<float>(i) / kSampleRate));
            const float env = std::sin(t * kPi * 0.9f) * (1.f - t);
            samples[i] = static_cast<std::int16_t>(squish * env * 16000.f);
        }
        makeBuffer(m_bufDie, samples);
    }

    // 4) Pickup (Nhặt đồ / exp - Arpeggio vui tai)
    {
        const int count = static_cast<int>(kSampleRate * 0.20f);
        std::vector<std::int16_t> samples(count);
        const float notes[] = {523.25f, 659.25f, 783.99f, 1046.50f};  // C5, E5, G5, C6
        const int step = count / 4;
        for (int i = 0; i < count; ++i) {
            const int noteIdx = std::min(i / step, 3);
            const float freq = notes[noteIdx];
            const float tLocal = static_cast<float>(i % step) / step;
            const float sine = std::sin(2.f * kPi * freq * (static_cast<float>(i) / kSampleRate));
            const float env = (1.f - tLocal);
            samples[i] = static_cast<std::int16_t>(sine * env * 20000.f);
        }
        makeBuffer(m_bufPickup, samples);
    }

    // 5) Level Up (Giai điệu chiến thắng)
    {
        const int count = static_cast<int>(kSampleRate * 0.42f);
        std::vector<std::int16_t> samples(count);
        const float notes[] = {440.f, 554.37f, 659.25f, 880.f};  // A4, C#5, E5, A5
        const int step = count / 4;
        for (int i = 0; i < count; ++i) {
            const int noteIdx = std::min(i / step, 3);
            const float freq = notes[noteIdx];
            const float sine = std::sin(2.f * kPi * freq * (static_cast<float>(i) / kSampleRate));
            const float tOverall = static_cast<float>(i) / count;
            const float env = 1.f - tOverall * 0.5f;
            samples[i] = static_cast<std::int16_t>(sine * env * 22000.f);
        }
        makeBuffer(m_bufLevelUp, samples);
    }

    // 6) Dash (Tiếng vèo gió)
    {
        const int count = static_cast<int>(kSampleRate * 0.18f);
        std::vector<std::int16_t> samples(count);
        for (int i = 0; i < count; ++i) {
            const float t = static_cast<float>(i) / count;
            const float noise = noiseDist(rng);
            const float env = std::sin(t * kPi);
            samples[i] = static_cast<std::int16_t>(noise * env * 21000.f);
        }
        makeBuffer(m_bufDash, samples);
    }

    // 7) Bomb (Tiếng nổ rền)
    {
        const int count = static_cast<int>(kSampleRate * 0.45f);
        std::vector<std::int16_t> samples(count);
        for (int i = 0; i < count; ++i) {
            const float t = static_cast<float>(i) / count;
            const float freq = 120.f * (1.f - t * 0.8f);
            const float bass = std::sin(2.f * kPi * freq * (static_cast<float>(i) / kSampleRate));
            const float noise = noiseDist(rng) * 0.7f;
            const float env = (1.f - t) * (1.f - t);
            samples[i] = static_cast<std::int16_t>((bass + noise) * env * 28000.f);
        }
        makeBuffer(m_bufBomb, samples);
    }

    // 8) Gate (Cổng mở)
    {
        const int count = static_cast<int>(kSampleRate * 0.55f);
        std::vector<std::int16_t> samples(count);
        for (int i = 0; i < count; ++i) {
            const float t = static_cast<float>(i) / count;
            const float f1 = 587.33f + std::sin(t * 30.f) * 20.f;  // D5 rung nhẹ
            const float f2 = 880.f;
            const float wave = std::sin(2.f * kPi * f1 * (static_cast<float>(i) / kSampleRate)) +
                               std::sin(2.f * kPi * f2 * (static_cast<float>(i) / kSampleRate)) * 0.5f;
            const float env = std::sin(t * kPi);
            samples[i] = static_cast<std::int16_t>(wave * env * 17000.f);
        }
        makeBuffer(m_bufGate, samples);
    }

    // 9) BGM Loop: Chiptune bassline 4 nhịp vui nhộn
    {
        const int count = static_cast<int>(kSampleRate * 3.2f);
        std::vector<std::int16_t> samples(count);
        const float bassline[] = {130.81f, 164.81f, 196.00f, 220.00f, 196.00f, 164.81f, 146.83f, 130.81f};
        const int noteSamples = count / 8;
        for (int i = 0; i < count; ++i) {
            const int idx = (i / noteSamples) % 8;
            const float freq = bassline[idx];
            const float tNote = static_cast<float>(i % noteSamples) / noteSamples;
            const float wave = std::sin(2.f * kPi * freq * (static_cast<float>(i) / kSampleRate));
            const float env = std::exp(-3.5f * tNote);
            samples[i] = static_cast<std::int16_t>(wave * env * 8500.f);
        }
        if (makeBuffer(m_bufBgm, samples)) {
            try {
                m_bgmSound = std::make_unique<sf::Sound>(m_bufBgm);
                m_bgmSound->setLooping(true);
                m_bgmSound->setVolume(30.f);
                m_bgmSound->play();
            } catch (...) {}
        }
    }
}

void AudioManager::update(float dt) {
    (void)dt;
    // Dọn các Sound đã phát xong
    while (!m_activeSounds.empty() && m_activeSounds.front().getStatus() == sf::Sound::Status::Stopped) {
        m_activeSounds.pop_front();
    }
}

void AudioManager::playSound(const sf::SoundBuffer& buffer, float pitch, float volume) {
    if (m_muted || !m_initialized) return;
    try {
        if (m_activeSounds.size() >= kMaxConcurrentSounds) {
            m_activeSounds.pop_front();
        }
        m_activeSounds.emplace_back(buffer);
        sf::Sound& snd = m_activeSounds.back();
        snd.setPitch(pitch);
        snd.setVolume(volume);
        snd.play();
    } catch (...) {}
}

void AudioManager::toggleMute() {
    m_muted = !m_muted;
    if (m_muted) {
        if (m_bgmSound) m_bgmSound->pause();
        for (auto& s : m_activeSounds) s.stop();
        m_activeSounds.clear();
    } else {
        if (m_bgmSound) m_bgmSound->play();
    }
}

void AudioManager::onGameEvent(GameEvent event, const void* data) {
    (void)data;
    switch (event) {
        case GameEvent::PlayerAttack:
            playSlash();
            break;
        case GameEvent::PlayerDamaged:
            playHit();
            break;
        case GameEvent::PlayerDashed:
            playDash();
            break;
        case GameEvent::PlayerLeveledUp:
            playLevelUp();
            break;
        case GameEvent::MonsterKilled:
            playMonsterDie();
            break;
        case GameEvent::ItemCollected:
            playPickup();
            break;
        case GameEvent::BombExploded:
            playBomb();
            break;
        case GameEvent::GateOpened:
            playGateOpen();
            break;
        default:
            break;
    }
}

void AudioManager::playSlash()      { playSound(m_bufSlash, 1.0f, 75.f); }
void AudioManager::playHit()        { playSound(m_bufHit, 1.0f, 85.f); }
void AudioManager::playMonsterDie() { playSound(m_bufDie, 1.0f, 70.f); }
void AudioManager::playPickup()     { playSound(m_bufPickup, 1.0f, 80.f); }
void AudioManager::playLevelUp()    { playSound(m_bufLevelUp, 1.0f, 90.f); }
void AudioManager::playDash()       { playSound(m_bufDash, 1.1f, 75.f); }
void AudioManager::playBomb()       { playSound(m_bufBomb, 1.0f, 100.f); }
void AudioManager::playGateOpen()   { playSound(m_bufGate, 1.0f, 95.f); }
