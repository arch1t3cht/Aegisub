#include <main.h>

#include "string_codec.h"

TEST(lagi_string_codec, decode_valid_hex) {
	EXPECT_EQ("A", inline_string_decode("#41"));
	EXPECT_EQ("abcAdef", inline_string_decode("abc#41def"));
	EXPECT_EQ(std::string("\x00", 1), inline_string_decode("#00"));
	EXPECT_EQ(std::string("\x7F", 1), inline_string_decode("#7F"));
	EXPECT_EQ(std::string("\xFF", 1), inline_string_decode("#FF"));
}

TEST(lagi_string_codec, decode_malformed_passthrough) {
	EXPECT_EQ("#", inline_string_decode("#"));
	EXPECT_EQ("#A", inline_string_decode("#A"));
	EXPECT_EQ("#4", inline_string_decode("#4"));
	EXPECT_EQ("#GZ", inline_string_decode("#GZ"));
	EXPECT_EQ("abc#4", inline_string_decode("abc#4"));
}

TEST(lagi_string_codec, roundtrip_encode_decode) {
	std::string in = std::string("a\nb#c|d:e,") + std::string("\x01\x02\x7F", 3);
	EXPECT_EQ(in, inline_string_decode(inline_string_encode(in)));
}
