#pragma once
#include <memory>
#include <GLFW/glfw3.h>

// An interface for all editor widgets
class IWidgetController
{
protected:
    // Widgets start closed by default
    bool m_isOpen = false;
    virtual void onShow() {}

public:
    virtual ~IWidgetController() = default;

    // The main update loop for the widget's view
    virtual void update(std::shared_ptr<GLFWwindow> window) = 0;

    // Public methods to control visibility
    void show()
    {
        m_isOpen = true;
        onShow();
    }
    void hide() { m_isOpen = false; }
    bool isOpen() const { return m_isOpen; }
};
