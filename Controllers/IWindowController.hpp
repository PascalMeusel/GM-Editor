#pragma once

#include <memory>
#include <GLFW/glfw3.h>

#include "WindowTypes.hpp"

// An "interface" for all of our window views.
// It's an abstract class because it has a pure virtual function.
class IWindowController {
public:
    // Virtual destructor is important for base classes.
    virtual ~IWindowController() = default;

    // This is a "pure virtual function". Any class that inherits from
    // IWindowView MUST provide an implementation for this function.
    virtual void update(std::shared_ptr<GLFWwindow> window) = 0;


    virtual WindowTypes getWindowType() = 0;
    virtual void setWindowOpacity(bool showView) = 0;
    virtual bool getWindowActive() = 0;
};