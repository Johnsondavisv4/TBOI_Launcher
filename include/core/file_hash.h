#pragma once

#include <filesystem>
#include <string>

namespace TBOI {

// Returns the lowercase SHA-256 digest, or an empty string on failure.
std::string CalculateFileSha256(const std::filesystem::path& filePath);

}
