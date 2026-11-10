#pragma once
#include "Models/Campaign/Campaign.hpp"
#include <map>
#include <string>

// Private, uncommitted text. Never part of campaign transfer packages.
namespace TextDraftRecovery
{
using Fields = std::map<std::string, std::string>;
struct ReadResult { bool success = false, found = false; Fields fields; std::string error, rawBytes; bool rawAvailable = false; };
// Opt-in complete bytes remain available for copying malformed checkpoints.
// Unsafe, oversized or incomplete reads never return an alleged complete copy.
ReadResult read(const Campaign &, const std::string &key, bool retainRawBytes = false);
// Empty error means success. A failed write retains the previous checkpoint.
std::string save(const Campaign &, const std::string &key, const Fields &);
std::string remove(const Campaign &, const std::string &key);
}
