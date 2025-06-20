// Copyright (c) 2022, arch1t3cht <arch1t3cht@gmail.com>
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

/// @file ffmpegsource_common.cpp
/// @brief Shared code for ffms video and audio providers
/// @ingroup video_input audio_input ffms
///

#ifdef WITH_BESTSOURCE
#include "bestsource_common.h"
#include "tracklist.h"

extern "C" {
#include <libavutil/avutil.h>
}

#include "format.h"
#include "options.h"
#include "utils.h"

#include <libaegisub/fs.h>
#include <libaegisub/path.h>

#include <boost/crc.hpp>
#include <boost/filesystem/path.hpp>

namespace provider_bs {

std::pair<TrackSelection, bool> SelectTrack(agi::fs::path const& filename, bool audio) {
	std::map<std::string, std::string> opts;
	BestTrackList tracklist(filename.string(), &opts);

	int n = tracklist.GetNumTracks();
	AVMediaType type = audio ? AVMEDIA_TYPE_AUDIO : AVMEDIA_TYPE_VIDEO;

	std::vector<int> TrackNumbers;
	wxArrayString Choices;
	bool has_audio = false;

	for (int i = 0; i < n; i++) {
		BestTrackList::TrackInfo info = tracklist.GetTrackInfo(i);
		has_audio = has_audio || (info.MediaType == AVMEDIA_TYPE_AUDIO);

		if (info.MediaType == type) {
			TrackNumbers.push_back(i);
			std::map<std::string, std::string> metadata = tracklist.GetTrackMetadata(i);
			wxString description = agi::wxformat(_("Track %02d: %s"), i, info.CodecString);

			if (metadata.count("language") > 0) {
				description += agi::wxformat(_(" - [%s]"), metadata["language"]);
			}

			if (metadata.count("title") > 0) {
				description += agi::wxformat(_(" - %s"), metadata["title"]);
			}

			Choices.Add(description);
		}
	}

	TrackSelection result;

	if (TrackNumbers.empty()) {
		result = TrackSelection::NoTracks;
	} else if (TrackNumbers.size() == 1) {
		result = static_cast<TrackSelection>(TrackNumbers[0]);
	} else {
		int Choice = wxGetSingleChoiceIndex(
			audio ? _("Multiple audio tracks detected, please choose the one you wish to load:") : _("Multiple video tracks detected, please choose the one you wish to load:"),
			audio ? _("Choose audio track") : _("Choose video track"),
			Choices);

		if (Choice >= 0)
			result = static_cast<TrackSelection>(TrackNumbers[Choice]) ;
		else
			result = TrackSelection::None;
	}

	return std::make_pair(result, has_audio);
}

std::string GetCacheFile(agi::fs::path const& filename) {
	boost::crc_32_type hash;
	hash.process_bytes(filename.string().c_str(), filename.string().size());

	auto result = config::path->Decode("?local/bsindex/" + filename.filename().string() + "_" + std::to_string(hash.checksum()) + "_" + std::to_string(agi::fs::ModifiedTime(filename)));
	agi::fs::CreateDirectory(result.parent_path());

	return result.string();
}

void CleanBSCache() {
	CleanCache(config::path->Decode("?local/bsindex/"),
		"*.bsindex",
		OPT_GET("Provider/BestSource/Cache/Size")->GetInt(),
		OPT_GET("Provider/BestSource/Cache/Files")->GetInt());

	// Delete old cache files: TODO remove this after a while
	CleanCache(config::path->Decode("?local/bsindex/"),
		"*.json", 0, 0);
}
}

#endif // WITH_BESTSOURCE
