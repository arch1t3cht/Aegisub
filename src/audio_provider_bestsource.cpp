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

/// @file audio_provider_bestsource.cpp
/// @brief BS-based audio provider
/// @ingroup audio_input bestsource
///

#ifdef WITH_BESTSOURCE
#include <libaegisub/audio/provider.h>

#include "bestsource_common.h"

#include "audiosource.h"

#include "compat.h"
#include "options.h"

#include <libaegisub/fs.h>
#include <libaegisub/make_unique.h>
#include <libaegisub/background_runner.h>
#include <libaegisub/log.h>

#include <map>

namespace {
class BSAudioProvider final : public agi::AudioProvider {
	std::map<std::string, std::string> bsopts;
	std::unique_ptr<BestAudioSource> bs;
	AudioProperties properties;

	void FillBuffer(void *Buf, int64_t Start, int64_t Count) const override;
public:
	BSAudioProvider(agi::fs::path const& filename, agi::BackgroundRunner *br);

	bool NeedsCache() const override { return OPT_GET("Provider/Audio/BestSource/Aegisub Cache")->GetBool(); }
};

/// @brief Constructor
/// @param filename The filename to open
BSAudioProvider::BSAudioProvider(agi::fs::path const& filename, agi::BackgroundRunner *br) try
: bsopts()
{
	provider_bs::CleanBSCache();
	auto track = provider_bs::SelectTrack(filename, true).first;

	if (track == provider_bs::TrackSelection::NoTracks)
		throw agi::AudioDataNotFound("no audio tracks found");
	else if (track == provider_bs::TrackSelection::None)
		throw agi::UserCancelException("audio loading cancelled by user");

	bool cancelled = false;
	br->Run([&](agi::ProgressSink *ps) {
		ps->SetTitle(from_wx(_("Indexing")));
		ps->SetMessage(from_wx(_("Indexing file... This will take a while!")));
		try {
			bs = agi::make_unique<BestAudioSource>(filename.string(), static_cast<int>(track), -1, false, 0, 1, provider_bs::GetCacheFile(filename), &bsopts, 0, [=](int Track, int64_t Current, int64_t Total) {
				ps->SetProgress(Current, Total);
				return !ps->IsCancelled();
			});
		} catch (BestSourceException const& err) {
			if (std::string(err.what()) == "Indexing canceled by user")
				cancelled = true;
			else
				throw err;
		}
	});
	if (cancelled)
		throw agi::UserCancelException("audio loading cancelled by user");

	bs->SetMaxCacheSize(OPT_GET("Provider/Audio/BestSource/Max Cache Size")->GetInt() << 20);
	properties = bs->GetAudioProperties();
	float_samples = properties.AF.Float;
	bytes_per_sample = properties.AF.BytesPerSample;
	sample_rate = properties.SampleRate;
	channels = properties.Channels;
	num_samples = properties.NumSamples;
	decoded_samples = OPT_GET("Provider/Audio/BestSource/Aegisub Cache")->GetBool() ? 0 : num_samples;
}
catch (BestSourceException const& err) {
	throw agi::AudioProviderError("Failed to create BestAudioSource");
}

void BSAudioProvider::FillBuffer(void *Buf, int64_t Start, int64_t Count) const {
	bs->GetPackedAudio(reinterpret_cast<uint8_t *>(Buf), Start, Count);
}

}

std::unique_ptr<agi::AudioProvider> CreateBSAudioProvider(agi::fs::path const& file, agi::BackgroundRunner *br) {
	return agi::make_unique<BSAudioProvider>(file, br);
}

#endif /* WITH_BESTSOURCE */

