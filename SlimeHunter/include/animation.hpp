#pragma once
// Animation.hpp - Quản lý hoạt ảnh lấy frame từ sprite sheet.
//
// Sprite sheet là MỘT ảnh lớn chứa nhiều frame xếp thành lưới:
//     hàng 0: idle   | frame0 | frame1 | frame2 | ...
//     hàng 1: walk   | frame0 | frame1 | ...
// Mỗi "đoạn animation" (clip) nằm trên một hàng. Animation chỉ tính xem frame nào đang hiển thị
// và trả về hình chữ nhật (IntRect) cắt frame đó; Sprite sẽ dùng IntRect này.

#include <SFML/Graphics.hpp>

struct AnimationClip {
    int row = 0;             // hàng nào trong sprite sheet
    int frameCount = 1;      // số frame
    float frameTime = 0.1f;  // mỗi frame hiển thị bao nhiêu giây
    bool loop = true;        // true: lặp lại; false: chạy một lần rồi dừng ở frame cuối
};

class Animation {
public:
    Animation() = default;
    explicit Animation(sf::Vector2i frameSize) : m_frameSize(frameSize) {}

    void setFrameSize(sf::Vector2i frameSize) { m_frameSize = frameSize; }

    // Phát một clip. Nếu clip này (cùng hàng) đang chạy thì KHÔNG làm gì, để animation không bị
    // khởi động lại ở mỗi frame của game loop (lỗi rất hay gặp khiến nhân vật đứng im ở frame 0).
    void play(const AnimationClip& clip);

    // Buộc chạy lại từ frame đầu (dùng khi bắt đầu một đòn tấn công mới).
    void restart();

    // Hệ số tốc độ (1 = bình thường, 1.5 = nhanh hơn 50%).
    void setSpeed(float speed) { m_speed = speed; }

    // Cộng dồn thời gian trôi qua (dt) rồi chuyển frame khi đủ thời gian.
    void update(float dt);

    sf::IntRect getFrameRect() const;
    int getFrameIndex() const { return m_frame; }
    int getCurrentRow() const { return m_clip.row; }
    bool isFinished() const { return m_finished; }

private:
    sf::Vector2i m_frameSize{32, 32};
    AnimationClip m_clip;
    int m_frame = 0;
    float m_timer = 0.f;
    float m_speed = 1.f;
    bool m_finished = false;
    bool m_hasClip = false;
};
