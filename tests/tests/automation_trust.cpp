#include <main.h>

#include "automation_trust.h"

TEST(lagi_automation_trust, parse_and_serialize) {
	auto keys = automation_trust::ParseTrustedKeys("dir:/a|file:/b||  file:/c  ");
	EXPECT_EQ(3u, keys.size());
	EXPECT_TRUE(keys.count("dir:/a"));
	EXPECT_TRUE(keys.count("file:/b"));
	EXPECT_TRUE(keys.count("file:/c"));

	auto serialized = automation_trust::SerializeTrustedKeys(keys);
	auto reparsed = automation_trust::ParseTrustedKeys(serialized);
	EXPECT_EQ(keys, reparsed);
}

TEST(lagi_automation_trust, key_prefixes) {
	auto dir_key = automation_trust::MakeProjectDirKey("foo/bar");
	EXPECT_TRUE(dir_key.rfind("dir:", 0) == 0);
	EXPECT_NE(std::string::npos, dir_key.find("foo/bar"));

	auto file_key = automation_trust::MakeScriptFileKey("foo/bar.lua");
	EXPECT_TRUE(file_key.rfind("file:", 0) == 0);
	EXPECT_NE(std::string::npos, file_key.find("foo/bar.lua"));
}

