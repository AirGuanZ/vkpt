#pragma once

#include <bitset>

#include <agz-utils/event.h>

#include <vkpt/common.h>

struct GLFWwindow;

VKPT_BEGIN

enum class MouseButton
{
    Left   = 0,
    Middle = 1,
    Right  = 2
};

using KeyCode = agz::event::keycode::keycode_t;

using namespace agz::event::keycode::keycode_constants;

struct MouseButtonDown { MouseButton button; };
struct MouseButtonUp   { MouseButton button; };
struct CursorMove      { float x, y, rel_x, rel_y; };
struct WheelScroll     { int offset; };

struct KeyDown   { KeyCode key; };
struct KeyUp     { KeyCode key; };
struct CharInput { uint32_t ch; };

using MouseButtonDownHandler = agz::event::functional_receiver_t<MouseButtonDown>;
using MouseButtonUpHandler   = agz::event::functional_receiver_t<MouseButtonUp>;
using CursorMoveHandler      = agz::event::functional_receiver_t<CursorMove>;
using WheelScrollHandler     = agz::event::functional_receiver_t<WheelScroll>;

using KeyDownHandler   = agz::event::functional_receiver_t<KeyDown>;
using KeyUpHandler     = agz::event::functional_receiver_t<KeyUp>;
using CharInputHandler = agz::event::functional_receiver_t<CharInput>;

class Input
{
    using EventSender = agz::event::sender_t<
        MouseButtonDown,
        MouseButtonUp,
        CursorMove,
        WheelScroll,
        KeyDown,
        KeyUp,
        CharInput>;

    EventSender sender_;

    GLFWwindow *window_ = nullptr;

    // mouse

    bool last_frame_pressed_[3] = { false, false, false };
    bool this_frame_pressed_[3] = { false, false, false };

    float x_ = 0, y_ = 0, rel_x_ = 0, rel_y_ = 0;

    bool lock_ = false;
    bool show_ = true;

    // keyboard

    std::bitset<KEY_MAX + 1> last_frame_keys_;
    std::bitset<KEY_MAX + 1> this_frame_keys_;

public:

    explicit Input(GLFWwindow *window);

    ~Input();

    void reset();

    bool isPressed(MouseButton button) const;
    bool isDown   (MouseButton button) const;
    bool isUp     (MouseButton button) const;

    float getCursorX()  const;
    float getCursorY()  const;
    Vec2f getCursorXY() const;

    float getRelativeCursorX()  const;
    float getRelativeCursorY()  const;
    Vec2f getRelativeCursorXY() const;

    void lockCursor(bool lock);
    bool isCursorLocked() const;

    void showCursor(bool show);
    bool isCursorVisible() const;

    bool isPressed(KeyCode key) const;
    bool isDown   (KeyCode key) const;
    bool isUp     (KeyCode key) const;

    AGZ_DECL_EVENT_SENDER_HANDLER(sender_, MouseButtonDown)
    AGZ_DECL_EVENT_SENDER_HANDLER(sender_, MouseButtonUp)
    AGZ_DECL_EVENT_SENDER_HANDLER(sender_, CursorMove)
    AGZ_DECL_EVENT_SENDER_HANDLER(sender_, WheelScroll)
    AGZ_DECL_EVENT_SENDER_HANDLER(sender_, KeyDown)
    AGZ_DECL_EVENT_SENDER_HANDLER(sender_, KeyUp)
    AGZ_DECL_EVENT_SENDER_HANDLER(sender_, CharInput)

    void _update();

    void _triggerMouseButtonDown(MouseButton button);

    void _triggerMouseButtonUp  (MouseButton button);

    void _triggerWheelScroll(int offset);

    void _triggerKeyDown(KeyCode key);

    void _triggerKeyUp(KeyCode key);

    void _triggerCharInput(uint32_t ch);
};

VKPT_END
