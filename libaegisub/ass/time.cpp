// Copyright (c) 2014, Thomas Goyne <plorkyeran@aegisub.org>
//
// Permission to use, copy, modify, and distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
// ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
// ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//
// Aegisub Project http://www.aegisub.org/

#include <libaegisub/ass/time.h>
#include <libaegisub/ass/smpte.h>

#include <libaegisub/format.h>
#include <libaegisub/split.h>
#include <libaegisub/util.h>

#include <algorithm>

// classic VSFilter internally uses a signed 32-bit int to denote milliseconds.
// To avoid this limit to < 596h (-6 to avoid rounding up to 596h in centisecond precision)
static const int MAX_TIME = 596 * 60 * 60 * 1000 - 6;

static void decompose_time(int ms_time, int& h, int& m, int& s, int& ms) {
	h = ms_time / 3600000;
	ms_time -= h * 3600000;
	m = ms_time / 60000;
	ms_time -= m * 60000;
	s = ms_time / 1000;
	ms = ms_time - s * 1000;
}

namespace agi {
Time::Time(int time) : time(util::mid(0, time, MAX_TIME)) { }

Time::Time(std::string const& text) {
	int after_decimal = -1;
	int current = 0;
	for (char c : text) {
		if (c == ':') {
			time = time * 60 + current;
			current = 0;
		}
		else if (c == '.' || c == ',') {
			time = (time * 60 + current) * 1000;
			current = 0;
			after_decimal = 100;
		}
		else if (c < '0' || c > '9')
			continue;
		else if (after_decimal < 0) {
			current *= 10;
			current += c - '0';
		}
		else {
			time += (c - '0') * after_decimal;
			after_decimal /= 10;
		}
	}

	// Never saw a decimal, so convert now to ms
	if (after_decimal < 0)
		time = (time * 60 + current) * 1000;

	// Limit to the valid range
	time = util::mid(0, time, MAX_TIME);
}

std::string Time::GetAssFormatted(bool msPrecision) const {
	int ass_time = msPrecision ? time : int(*this);
	int h, m, s, ms;

	decompose_time(ass_time, h, m, s, ms);

	if (!msPrecision)
		return format("%d:%02d:%02d.%02d", h, m, s, ms / 10);
	else
		return format("%d:%02d:%02d.%03d", h, m, s, ms);
}

std::string Time::GetSrtFormatted() const {
	int h, m, s, ms;
	decompose_time(time, h, m, s, ms);
	return format("%02d:%02d:%02d,%03d", h, m, s, ms);
}

SmpteFormatter::SmpteFormatter(vfr::Framerate fps, char sep)
: fps(std::move(fps))
, sep(sep)
{
}

std::string SmpteFormatter::ToSMPTE(Time time) const {
	int h=0, m=0, s=0, f=0;
	fps.SmpteAtTime(time, &h, &m, &s, &f);
	return format("%02d%c%02d%c%02d%c%02d", h, sep, m, sep, s, sep, f);
}

Time SmpteFormatter::FromSMPTE(std::string const& str) const {
	std::vector<std::string> toks;
	Split(toks, str, sep);
	if (toks.size() != 4) return 0;

	int h, m, s, f;
	util::try_parse(toks[0], &h);
	util::try_parse(toks[1], &m);
	util::try_parse(toks[2], &s);
	util::try_parse(toks[3], &f);
	return fps.TimeAtSmpte(h, m, s, f);
}
}
