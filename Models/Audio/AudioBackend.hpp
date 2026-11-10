#pragma once

#include <memory>
#include <string>

class AudioCommandBackend
{
public:
    virtual ~AudioCommandBackend() = default;
    virtual const char *name() const = 0;
    virtual bool sendCommand(const std::string &command, std::string &errorMessage) const = 0;
    virtual bool queryCommand(const std::string &command, std::string &result, std::string &errorMessage) const = 0;
};

std::unique_ptr<AudioCommandBackend> createAudioCommandBackend();
