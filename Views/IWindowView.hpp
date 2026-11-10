#pragma once

#include <memory>
#include <GLFW/glfw3.h>

// An "interface" for all of our window views.
// It's an abstract class because it has a pure virtual function.
class IWindowView {
public:
    // Virtual destructor is important for base classes.
    virtual ~IWindowView() = default;

    // This is a "pure virtual function". Any class that inherits from
    // IWindowView MUST provide an implementation for this function.
    virtual void update(std::shared_ptr<GLFWwindow> window) = 0;

    // A flag to easily control the window's visibility from anywhere.
    bool isVisible = true; 
};