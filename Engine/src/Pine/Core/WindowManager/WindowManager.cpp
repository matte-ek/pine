#include "WindowManager.hpp"
#include "Pine/Core/Math/Math.hpp"

#include <GLFW/glfw3.h>
#include <stdexcept>

// Should be added to places where we assume m_Window isn't nullptr.
#define WINDOW_CHECK if (m_Window == nullptr) throw std::runtime_error("m_Window is nullptr");

namespace
{

    GLFWmonitor* m_WindowMonitor;

    GLFWwindow* m_Window;

    std::string m_WindowTitle;
    Pine::Vector2i m_WindowPosition;
    Pine::Vector2i m_WindowSize;

    Pine::WindowManager::ScreenType m_CurrentScreenType;

    void OnWindowSizeChanged(GLFWwindow* window, int width, int height)
    {
        if (window != m_Window)
            return;

        m_WindowSize = Pine::Vector2i(width, height);
    }

    void OnWindowPositionChanged(GLFWwindow* window, int x, int y)
    {
        if (window != m_Window)
            return;

        m_WindowPosition = Pine::Vector2i(x, y);
    }


}

bool Pine::WindowManager::Internal::CreateWindow(Vector2i position, Vector2i requestedSize,
                                                 const std::string &title, ScreenType type)
{
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

   // glfwWindowHint(GLFW_SAMPLES, 16);

    const auto targetMonitor = glfwGetPrimaryMonitor();
    auto size = requestedSize;

    if (type == ScreenType::FullscreenWindowed)
    {
        const GLFWvidmode* videoMode = glfwGetVideoMode(targetMonitor);

        glfwWindowHint(GLFW_RED_BITS, videoMode->redBits);
        glfwWindowHint(GLFW_GREEN_BITS, videoMode->greenBits);
        glfwWindowHint(GLFW_BLUE_BITS, videoMode->blueBits);
        glfwWindowHint(GLFW_REFRESH_RATE, videoMode->refreshRate);

        size = Vector2i(videoMode->width, videoMode->height);
    }

    m_Window = glfwCreateWindow(size.x, size.y, title.c_str(), type != ScreenType::Default ? targetMonitor : nullptr, nullptr);

    if (!m_Window)
    {
        return false;
    }

    glfwMakeContextCurrent(m_Window);

    // Enable vertical sync by default
    glfwSwapInterval(1);

    m_WindowTitle = title;
    m_WindowMonitor = targetMonitor;
    m_WindowSize = size;
    m_CurrentScreenType = type;

    SetWindowPosition(position);

    glfwSetWindowPosCallback(m_Window, OnWindowPositionChanged);
    glfwSetWindowSizeCallback(m_Window, OnWindowSizeChanged);

    return true;
}

void Pine::WindowManager::Internal::DestroyWindow()
{
    glfwDestroyWindow(m_Window);
}

bool Pine::WindowManager::IsWindowCreated()
{
    return m_Window != nullptr;
}

bool Pine::WindowManager::IsWindowOpen()
{
    return !glfwWindowShouldClose(m_Window);
}

void Pine::WindowManager::SetWindowPosition(Vector2i targetPosition)
{
    WINDOW_CHECK

    auto position = targetPosition;

    // If both x and y is set to -1, we should center the window.
    if (position.x == -1 && position.y == -1)
    {
        const GLFWvidmode* videoMode = glfwGetVideoMode(m_WindowMonitor);
        const auto currentWindowSize = GetWindowSize();

        position.x = videoMode->width / 2 - currentWindowSize.x / 2;
        position.y = videoMode->height / 2 - currentWindowSize.y / 2;
    }

    glfwSetWindowPos(m_Window, position.x, position.y);

    m_WindowPosition = targetPosition;
}

void Pine::WindowManager::SetWindowSize(Vector2i size)
{
    WINDOW_CHECK

    glfwSetWindowSize(m_Window, size.x, size.y);
}

void Pine::WindowManager::SetWindowTitle(const std::string &title)
{
    WINDOW_CHECK

    glfwSetWindowTitle(m_Window, title.c_str());

    m_WindowTitle = title;
}

void Pine::WindowManager::SetWindowScreenType(ScreenType screenType)
{
    WINDOW_CHECK

    if (screenType == m_CurrentScreenType)
    {
        return;
    }

    // We cannot currently restore between fullscreen at this point, as the window will have to
    // be destroyed and recreated. Might support it in the future.
    if (m_CurrentScreenType == ScreenType::Fullscreen)
    {
        return;
    }

    const auto targetMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* videoMode = glfwGetVideoMode(m_WindowMonitor);

    if (screenType == ScreenType::FullscreenWindowed)
    {
        glfwSetWindowMonitor(m_Window, targetMonitor, 0, 0, videoMode->width, videoMode->height, videoMode->refreshRate);

        return;
    }

    // Maybe we should remember the old size at this point
    glfwSetWindowMonitor(m_Window, nullptr, 100, 100, 1280, 720, videoMode->refreshRate);

    // I think centering the window here is a good default position
    SetWindowPosition(Vector2i(-1));
}

void Pine::WindowManager::SetWindowVisible(bool visible)
{
    WINDOW_CHECK

    visible ? glfwShowWindow(m_Window) : glfwHideWindow(m_Window);
}

Pine::Vector2i Pine::WindowManager::GetWindowPosition()
{
    WINDOW_CHECK

    return m_WindowPosition;
}

Pine::Vector2i Pine::WindowManager::GetWindowSize()
{
    WINDOW_CHECK

    return m_WindowSize;
}

bool Pine::WindowManager::GetWindowVisible()
{
    WINDOW_CHECK

    return glfwGetWindowAttrib(m_Window, GLFW_VISIBLE) == GLFW_TRUE;
}

const std::string &Pine::WindowManager::GetWindowTitle()
{
    return m_WindowTitle;
}

Pine::WindowManager::ScreenType Pine::WindowManager::GetWindowScreenType()
{
    return m_CurrentScreenType;
}

void* Pine::WindowManager::GetWindowHandlePointer()
{
    return nullptr;
}

void* Pine::WindowManager::GetWindowPointer()
{
    return m_Window;
}
