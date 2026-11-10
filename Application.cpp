#include "ApplicationInternal.hpp"

using namespace gm::application_detail;

Application::Application(ApplicationRunOptions options) : runOptions(std::move(options)), mainWindow(nullptr)
{
    try
    {
        if (runOptions.width <= 0 || runOptions.height <= 0 || runOptions.width > 16384 || runOptions.height > 16384)
            throw std::invalid_argument("Application viewport dimensions must be between 1 and 16384");
        Initialize();
        if (runOptions.initialized) runOptions.initialized(*this);
    }
    catch (const std::exception &e)
    {
        startupDiagnostics.record("startup.failed", e.what());
        Shutdown();
        throw;
    }
    catch (...)
    {
        startupDiagnostics.record("startup.failed", "unknown failure");
        Shutdown();
        throw;
    }
}

Application::~Application()
{
    Shutdown();
}
