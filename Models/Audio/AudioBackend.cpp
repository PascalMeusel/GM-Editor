#include "AudioBackend.hpp"

#include <sstream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#endif

namespace
{
#if defined(_WIN32)
class MciAudioCommandBackend : public AudioCommandBackend
{
public:
    const char *name() const override
    {
        return "Windows MCI";
    }

    bool sendCommand(const std::string &command, std::string &errorMessage) const override
    {
        const MCIERROR result = mciSendStringA(command.c_str(), nullptr, 0, nullptr);
        if (result == 0)
        {
            errorMessage.clear();
            return true;
        }

        return formatMciError(result, "Audio command failed: " + command, errorMessage, command);
    }

    bool queryCommand(const std::string &command, std::string &result, std::string &errorMessage) const override
    {
        char buffer[128] = {};
        const MCIERROR mciResult = mciSendStringA(command.c_str(), buffer, sizeof(buffer), nullptr);
        if (mciResult == 0)
        {
            result = buffer;
            errorMessage.clear();
            return true;
        }

        result.clear();
        return formatMciError(mciResult, "Audio query failed: " + command, errorMessage, command);
    }

private:
    static bool formatMciError(MCIERROR result, const std::string &fallback, std::string &errorMessage, const std::string &command)
    {
        char message[256] = {};
        if (mciGetErrorStringA(result, message, sizeof(message)))
        {
            std::ostringstream stream;
            stream << message << " (" << command << ")";
            errorMessage = stream.str();
        }
        else
        {
            errorMessage = fallback;
        }
        return false;
    }
};
#else
class NullAudioCommandBackend : public AudioCommandBackend
{
public:
    const char *name() const override
    {
        return "No Audio Backend";
    }

    bool sendCommand(const std::string &command, std::string &errorMessage) const override
    {
        (void)command;
        errorMessage = "Audio playback is not wired for this platform yet.";
        return false;
    }

    bool queryCommand(const std::string &command, std::string &result, std::string &errorMessage) const override
    {
        (void)command;
        result.clear();
        errorMessage = "Audio playback is not wired for this platform yet.";
        return false;
    }
};
#endif
}

std::unique_ptr<AudioCommandBackend> createAudioCommandBackend()
{
#if defined(_WIN32)
    return std::make_unique<MciAudioCommandBackend>();
#else
    return std::make_unique<NullAudioCommandBackend>();
#endif
}
