#include "Animation.hpp"

void Animation::play(const AnimationClip& clip) {
    if (m_hasClip && m_clip.row == clip.row) return;  // Ä‘ang cháº¡y Ä‘Ãºng clip nÃ y rá»“i
    m_clip = clip;
    m_hasClip = true;
    restart();
}

void Animation::restart() {
    m_frame = 0;
    m_timer = 0.f;
    m_finished = false;
}

void Animation::update(float dt) {
    if (!m_hasClip || m_finished || m_clip.frameTime <= 0.f) return;

    m_timer += dt * m_speed;  // thá»i gian tÃ­ch lÅ©y, KHÃ”NG Ä‘áº¿m sá»‘ vÃ²ng láº·p game => khÃ´ng phá»¥ thuá»™c FPS
    while (m_timer >= m_clip.frameTime) {
        m_timer -= m_clip.frameTime;
        ++m_frame;
        if (m_frame >= m_clip.frameCount) {
            if (m_clip.loop) {
                m_frame = 0;
            } else {
                m_frame = m_clip.frameCount - 1;  // dá»«ng á»Ÿ frame cuá»‘i
                m_finished = true;
                m_timer = 0.f;
                break;
            }
        }
    }
}

sf::IntRect Animation::getFrameRect() const {
    return sf::IntRect({m_frame * m_frameSize.x, m_clip.row * m_frameSize.y}, m_frameSize);
}