#include <GLFW/glfw3.h>

#include <vkpt/input.h>

VKPT_BEGIN

Input::Input(GLFWwindow *window)
{
    window_ = window;
}

Input::~Input()
{
    if(!show_)
        showCursor(true);
}

void Input::reset()
{
    this_frame_pressed_[0] = this_frame_pressed_[1] = this_frame_pressed_[2] = false;
    last_frame_pressed_[0] = last_frame_pressed_[1] = last_frame_pressed_[2] = false;

    x_ = y_ = 0;
    rel_x_ = rel_y_ = 0;

    lock_ = false;

    if(!show_)
        showCursor(true);

    last_frame_keys_.reset();
    this_frame_keys_.reset();
}

bool Input::isPressed(MouseButton button) const
{
    return this_frame_pressed_[int(button)];
}

bool Input::isDown(MouseButton button) const
{
    return !last_frame_pressed_[int(button)] &&
            this_frame_pressed_[int(button)];
}

bool Input::isUp(MouseButton button) const
{
    return last_frame_pressed_[int(button)] &&
          !this_frame_pressed_[int(button)];
}

float Input::getCursorX() const
{
    return x_;
}

float Input::getCursorY() const
{
    return y_;
}

Vec2f Input::getCursorXY() const
{
    return { x_, y_ };
}

float Input::getRelativeCursorX() const
{
    return rel_x_;
}

float Input::getRelativeCursorY() const
{
    return rel_y_;
}

Vec2f Input::getRelativeCursorXY() const
{
    return { rel_x_, rel_y_ };
}

void Input::lockCursor(bool lock)
{
    lock_ = lock;
}

bool Input::isCursorLocked() const
{
    return lock_;
}

void Input::showCursor(bool show)
{
    show_ = show;
    glfwSetInputMode(
        window_, GLFW_CURSOR, show ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
}

bool Input::isCursorVisible() const
{
    return show_;
}

bool Input::isPressed(KeyCode key) const
{
    return this_frame_keys_[key];
}

bool Input::isDown(KeyCode key) const
{
    return !last_frame_keys_[key] && this_frame_keys_[key];
}

bool Input::isUp(KeyCode key) const
{
    return last_frame_keys_[key] && !this_frame_keys_[key];
}

void Input::_update()
{
    double new_x_d, new_y_d;
    glfwGetCursorPos(window_, &new_x_d, &new_y_d);
    const float new_x = static_cast<float>(new_x_d);
    const float new_y = static_cast<float>(new_y_d);

    rel_x_ = new_x - x_;
    rel_y_ = new_y - y_;
    x_ = new_x;
    y_ = new_y;

    if(rel_x_ != 0 || rel_y_ != 0)
        sender_.send(CursorMove{ new_x, new_y, rel_x_, rel_y_ });

    if(lock_)
    {
        int width, height;
        glfwGetWindowSize(window_, &width, &height);
        const int lock_x = width / 2, lock_y = height / 2;

        glfwSetCursorPos(window_, new_x, new_y);
        x_ = static_cast<float>(lock_x);
        y_ = static_cast<float>(lock_y);
    }

    for(int i = 0; i < 3; ++i)
        last_frame_pressed_[i] = this_frame_pressed_[i];

    last_frame_keys_ = this_frame_keys_;
}

void Input::_triggerMouseButtonDown(MouseButton button)
{
    this_frame_pressed_[int(button)] = true;
    sender_.send(MouseButtonDown{ button });
}

void Input::_triggerMouseButtonUp(MouseButton button)
{
    this_frame_pressed_[int(button)] = false;
    sender_.send(MouseButtonUp{ button });
}

void Input::_triggerWheelScroll(int offset)
{
    sender_.send(WheelScroll{ offset });
}

void Input::_triggerKeyDown(KeyCode key)
{
    if(!isPressed(key))
    {
        this_frame_keys_[key] = true;
        sender_.send(KeyDown{ key });
    }
}

void Input::_triggerKeyUp(KeyCode key)
{
    if(isPressed(key))
    {
        this_frame_keys_[key] = false;
        sender_.send(KeyUp{ key });
    }
}

void Input::_triggerCharInput(uint32_t ch)
{
    sender_.send(CharInput{ ch });
}

VKPT_END
