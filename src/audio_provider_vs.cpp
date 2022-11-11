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

/// @file audio_provider_vs.cpp
/// @brief Vapoursynth-based audio provider
/// @ingroup audio_input
///

#ifdef WITH_VAPOURSYNTH
#include <libaegisub/audio/provider.h>

#include "audio_controller.h"
#include "options.h"
#include "utils.h"

#include <libaegisub/access.h>
#include <libaegisub/format.h>
#include <libaegisub/path.h>
#include <libaegisub/make_unique.h>

#include <mutex>

#include "vapoursynth_wrap.h"
#include "vapoursynth_common.h"
#include "VSScript4.h"

namespace {
class VapoursynthAudioProvider final : public agi::AudioProvider {
	VapourSynthWrapper vs;
	VSScript *script = nullptr;
	VSNode *node = nullptr;
	const VSAudioInfo *vi = nullptr;

	void FillBufferWithFrame(void *buf, int frame, int64_t start, int64_t count) const;
	void FillBuffer(void *buf, int64_t start, int64_t count) const override;
public:
	VapoursynthAudioProvider(agi::fs::path const& filename);
	~VapoursynthAudioProvider();

	bool NeedsCache() const override { return true; }
};

VapoursynthAudioProvider::VapoursynthAudioProvider(agi::fs::path const& filename) try {
	std::lock_guard<std::mutex> lock(vs.GetMutex());

	script = vs.GetScriptAPI()->createScript(nullptr);
	if (script == nullptr) {
		throw VapoursynthError("Error creating script API");
	}
	vs.GetScriptAPI()->evalSetWorkingDir(script, 1);
	if (OpenScriptOrVideo(vs.GetScriptAPI(), script, filename, OPT_GET("Provider/Audio/VapourSynth/Default Script")->GetString())) {
		std::string msg = agi::format("Error executing VapourSynth script: %s", vs.GetScriptAPI()->getError(script));
		vs.GetScriptAPI()->freeScript(script);
		throw VapoursynthError(msg);
	}
	node = vs.GetScriptAPI()->getOutputNode(script, 0);
	if (node == nullptr) {
		vs.GetScriptAPI()->freeScript(script);
		throw VapoursynthError("No output node set");
	}
	if (vs.GetAPI()->getNodeType(node) != mtAudio) {
		vs.GetAPI()->freeNode(node);
		vs.GetScriptAPI()->freeScript(script);
		throw VapoursynthError("Output node isn't an audio node");
	}
	vi = vs.GetAPI()->getAudioInfo(node);
	float_samples = vi->format.sampleType == stFloat;
	bytes_per_sample = vi->format.bytesPerSample;
	sample_rate = vi->sampleRate;
	channels = vi->format.numChannels;
	num_samples = vi->numSamples;
}
catch (VapoursynthError const& err) {
	// Unlike the video provider manager, the audio provider factory catches AudioProviderErrors and picks whichever source doesn't throw one.
	// So just rethrow the Error here with an extra label so the user will see the error message and know the audio wasn't loaded with VS
	throw VapoursynthError(agi::format("Vapoursynth error: %s", err.GetMessage()));
}

template<typename T>
static void PackChannels(const uint8_t **Src, void *Dst, size_t Length, size_t Channels) {
	T *D = reinterpret_cast<T *>(Dst);
	for (size_t c = 0; c < Channels; c++) {
		const T *S = reinterpret_cast<const T *>(Src[c]);
		for (size_t i = 0; i < Length; i++) {
			D[Channels * i + c] = S[i];
		}
	}
}

void VapoursynthAudioProvider::FillBufferWithFrame(void *buf, int n, int64_t start, int64_t count) const {
	char errorMsg[1024];
	const VSFrame *frame = vs.GetAPI()->getFrame(n, node, errorMsg, sizeof(errorMsg));
	if (frame == nullptr) {
		throw VapoursynthError(agi::format("Error getting frame: %s", errorMsg));
	}
	if (vs.GetAPI()->getFrameLength(frame) < count) {
		vs.GetAPI()->freeFrame(frame);
		throw VapoursynthError("Audio frame too short");
	}
	if (vs.GetAPI()->getAudioFrameFormat(frame)->numChannels != channels || vs.GetAPI()->getAudioFrameFormat(frame)->bytesPerSample != bytes_per_sample) {
		vs.GetAPI()->freeFrame(frame);
		throw VapoursynthError("Audio format is not constant");
	}

	std::vector<const uint8_t *> planes(channels);
	for (int c = 0; c < channels; c++) {
		planes[c] = vs.GetAPI()->getReadPtr(frame, c) + bytes_per_sample * start;
		if (planes[c] == nullptr) {
			vs.GetAPI()->freeFrame(frame);
			throw VapoursynthError("Failed to read audio channel");
		}
	}

	if (bytes_per_sample == 1)
		PackChannels<uint8_t>(planes.data(), buf, count, channels);
	else if (bytes_per_sample == 2)
		PackChannels<uint16_t>(planes.data(), buf, count, channels);
	else if (bytes_per_sample == 4)
		PackChannels<uint32_t>(planes.data(), buf, count, channels);
	else if (bytes_per_sample == 8)
		PackChannels<uint64_t>(planes.data(), buf, count, channels);

	vs.GetAPI()->freeFrame(frame);
}

void VapoursynthAudioProvider::FillBuffer(void *buf, int64_t start, int64_t count) const {
	int end = start + count; 	// exclusive
	int startframe = start / VS_AUDIO_FRAME_SAMPLES;
	int endframe = (end - 1) / VS_AUDIO_FRAME_SAMPLES;
	int offset = start - (VS_AUDIO_FRAME_SAMPLES * startframe);

	for (int frame = startframe; frame <= endframe; frame++) {
		int framestart = frame * VS_AUDIO_FRAME_SAMPLES;
		int frameend = (frame + 1) * VS_AUDIO_FRAME_SAMPLES;
		int fstart = framestart < start ? start - framestart : 0;
		int fcount = VS_AUDIO_FRAME_SAMPLES - fstart - (frameend > end ? frameend - end : 0);
		int bufstart = frame == startframe ? 0 : (frame - startframe) * VS_AUDIO_FRAME_SAMPLES - offset;
		FillBufferWithFrame(reinterpret_cast<uint8_t *>(buf) + channels * bytes_per_sample * bufstart, frame, fstart, fcount);
	}
}

VapoursynthAudioProvider::~VapoursynthAudioProvider() {
	if (node != nullptr) {
		vs.GetAPI()->freeNode(node);
	}
	if (script != nullptr) {
		vs.GetScriptAPI()->freeScript(script);
	}
}
}

std::unique_ptr<agi::AudioProvider> CreateVapoursynthAudioProvider(agi::fs::path const& file, agi::BackgroundRunner *) {
	agi::acs::CheckFileRead(file);
	return agi::make_unique<VapoursynthAudioProvider>(file);
}
#endif
