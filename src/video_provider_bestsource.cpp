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

/// @file video_provider_bestsource.cpp
/// @brief BestSource-based video provider
/// @ingroup video_input bestsource
///

#ifdef WITH_BESTSOURCE
#include "include/aegisub/video_provider.h"

#include "videosource.h"
#include "audiosource.h"
#include "BSRational.h"

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <libswscale/swscale.h>
}

#include "bestsource_common.h"
#include "options.h"
#include "compat.h"
#include "video_frame.h"
namespace agi { class BackgroundRunner; }

#include <libaegisub/fs.h>
#include <libaegisub/path.h>
#include <libaegisub/make_unique.h>
#include <libaegisub/background_runner.h>
#include <libaegisub/log.h>
#include <libaegisub/format.h>

namespace {

/// @class BSVideoProvider
/// @brief Implements video loading through BestSource.
class BSVideoProvider final : public VideoProvider {
	std::map<std::string, std::string> bsopts;
	BestVideoSource bs;
	VideoProperties properties;

	std::vector<int> Keyframes;
	agi::vfr::Framerate Timecodes;
	std::string colorspace;
	bool has_audio = false;

public:
	BSVideoProvider(agi::fs::path const& filename, std::string const& colormatrix, agi::BackgroundRunner *br);

	void GetFrame(int n, VideoFrame &out) override;

	void SetColorSpace(std::string const& matrix) override { } 	// TODO Follow Aegisub's colorspace forcing?

	int GetFrameCount() const override { return properties.NumFrames; };

	int GetWidth() const override { return properties.Width; };
	int GetHeight() const override { return properties.Height; };
	double GetDAR() const override { return ((double) properties.Width * properties.SAR.Num) / (properties.Height * properties.SAR.Den); };

	agi::vfr::Framerate GetFPS() const override { return Timecodes; };
	std::string GetColorSpace() const override { return colorspace; };
	std::string GetRealColorSpace() const override { return colorspace; };
	std::vector<int> GetKeyFrames() const override { return Keyframes; };
	std::string GetDecoderName() const override { return "BestSource"; };
	bool WantsCaching() const override { return false; };
	bool HasAudio() const override { return has_audio; };
};

// Match the logic from the ffms2 provider, but directly use libavutil's constants and don't abort when encountering an unknown color space
std::string colormatrix_description(const AVFrame *frame) {
	// Assuming TV for unspecified
	std::string str = frame->color_range == AVCOL_RANGE_JPEG ? "PC" : "TV";

	switch (frame->colorspace) {
		case AVCOL_SPC_BT709:
			return str + ".709";
		case AVCOL_SPC_FCC:
			return str + ".FCC";
		case AVCOL_SPC_BT470BG:
		case AVCOL_SPC_SMPTE170M:
			return str + ".601";
		case AVCOL_SPC_SMPTE240M:
			return str + ".240M";
		default:
			return "None";
	}
}

BSVideoProvider::BSVideoProvider(agi::fs::path const& filename, std::string const& colormatrix, agi::BackgroundRunner *br) try
: bsopts()
, bs(filename.string(), "", -1, false, OPT_GET("Provider/Video/BestSource/Threads")->GetInt(), GetBSCacheFile(filename), &bsopts)
{
	bs.SetMaxCacheSize(OPT_GET("Provider/Video/BestSource/Max Cache Size")->GetInt() << 20);
	bs.SetSeekPreRoll(OPT_GET("Provider/Video/BestSource/Seek Preroll")->GetInt());
	try {
		BestAudioSource dummysource(filename.string(), -1, 0, "");
		has_audio = true;
	} catch (AudioException const& err) {
		has_audio = false;
	}

	br->Run([&](agi::ProgressSink *ps) {
		ps->SetTitle(from_wx(_("Indexing")));
		ps->SetMessage(from_wx(_("Creating cache... This can take a while!")));
		ps->SetIndeterminate();
		if (bs.GetExactDuration()) {
			LOG_D("provider/video/bestsource") << "File cached and has exact samples.";
		}
	});
	properties = bs.GetVideoProperties();

	br->Run([&](agi::ProgressSink *ps) {
		ps->SetTitle(from_wx(_("Scanning")));
		ps->SetMessage(from_wx(_("Reading timecodes and frame/sample data")));

		std::vector<int> TimecodesVector;
		for (int n = 0; n < properties.NumFrames; n++) {
			if (ps->IsCancelled()) {
				return;
			}
			std::unique_ptr<BestVideoFrame> frame(bs.GetFrame(n));
			if (frame == nullptr) {
				throw VideoOpenError("Couldn't read frame!");
			}
// https://github.com/FFmpeg/FFmpeg/blob/n6.1/doc/APIchanges#L144-L148
#if (LIBAVUTIL_VERSION_MAJOR == 58 && LIBAVUTIL_VERSION_MINOR >= 7) || LIBAVUTIL_VERSION_MAJOR >= 59
			if (frame->GetAVFrame()->flags & AV_FRAME_FLAG_KEY) {
#else
			if (frame->GetAVFrame()->key_frame) {
#endif
				Keyframes.push_back(n);
			}

			TimecodesVector.push_back(1000 * frame->Pts * properties.TimeBase.Num / properties.TimeBase.Den);
			ps->SetProgress(n, properties.NumFrames);
		}

		if (TimecodesVector.size() < 2 || TimecodesVector.front() == TimecodesVector.back()) {
			Timecodes = (double) properties.FPS.Num / properties.FPS.Den;
		} else {
			Timecodes = agi::vfr::Framerate(TimecodesVector);
		}
	});

	BSCleanCache();

	// Decode the first frame to get the color space
	std::unique_ptr<BestVideoFrame> frame(bs.GetFrame(0));
	colorspace = colormatrix_description(frame->GetAVFrame());
}
catch (VideoException const& err) {
	throw VideoOpenError(agi::format("Failed to create BestVideoSource: %s",  + err.what()));
}

void BSVideoProvider::GetFrame(int n, VideoFrame &out) {
	std::unique_ptr<BestVideoFrame> bsframe(bs.GetFrame(n));
	if (bsframe == nullptr) {
		throw VideoDecodeError("Couldn't read frame!");
	}
	const AVFrame *frame = bsframe->GetAVFrame();

	SwsContext *context = sws_getContext(
			frame->width, frame->height, (AVPixelFormat) frame->format,  	// TODO figure out aegi's color space forcing.
			frame->width, frame->height, AV_PIX_FMT_BGR0,
			SWS_BICUBIC, nullptr, nullptr, nullptr);

	if (context == nullptr) {
		throw VideoDecodeError("Couldn't convert frame!");
	}

	int range = frame->color_range == AVCOL_RANGE_JPEG;
	const int *coefficients = sws_getCoefficients(frame->colorspace == AVCOL_SPC_UNSPECIFIED ? AVCOL_SPC_BT709 : frame->colorspace);

    sws_setColorspaceDetails(context,
        coefficients, range,
        coefficients, range,
        0, 1 << 16, 1 << 16);

	out.data.resize(frame->width * frame->height * 4);
	uint8_t *data[1] = {&out.data[0]};
	int stride[1] = {frame->width * 4};
	sws_scale(context, frame->data, frame->linesize, 0, frame->height, data, stride);

	out.width = frame->width;
	out.height = frame->height;
	out.pitch = stride[0];
	out.flipped = false; 		// TODO figure out flipped

	sws_freeContext(context);
}

}

std::unique_ptr<VideoProvider> CreateBSVideoProvider(agi::fs::path const& path, std::string const& colormatrix, agi::BackgroundRunner *br) {
	return agi::make_unique<BSVideoProvider>(path, colormatrix, br);
}

#endif /* WITH_BESTSOURCE */
