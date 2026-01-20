#pragma once

#include <libaegisub/fs.h>

#include <set>
#include <string>
#include <string_view>

namespace automation_trust {
std::set<std::string> ParseTrustedKeys(std::string_view raw);
std::string SerializeTrustedKeys(std::set<std::string> const& keys);

// Trust keys are prefixed to avoid collisions between file and directory entries.
std::string MakeProjectDirKey(agi::fs::path const& project_dir);
std::string MakeScriptFileKey(agi::fs::path const& script_path);
}

