// Copyright (c) 2019, Charlie Jiang
// Copyright (c) 2022, sepro
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of the Aegisub Group nor the names of its contributors
//     may be used to endorse or promote products derived from this software
//     without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// Aegisub Project http://www.aegisub.org/

#include "ass_dialogue.h"
#include "ass_file.h"
#include "compat.h"
#include "dialog_manager.h"
#include "dialog_progress.h"
#include "format.h"
#include "include/aegisub/context.h"
#include "video_frame.h"
#include "libresrc/libresrc.h"
#include "options.h"
#include "project.h"
#include "selection_controller.h"
#include "video_controller.h"
#include "async_video_provider.h"
#include "colour_button.h"
#include "image_position_picker.h"

#include <cmath>

#include <libaegisub/ass/time.h>
#include <libaegisub/vfr.h>

#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/textctrl.h>
#if BOOST_VERSION >= 106900
#include <boost/gil.hpp>
#else
#include <boost/gil/gil_all.hpp>
#endif

namespace {
	struct QueuedFrame {
		int x, y;
		AssDialogue *line;
	};

	class DialogAlignToVideo final : public wxDialog {
		agi::Context* context;
		AsyncVideoProvider* provider;

		std::vector<QueuedFrame> queued_frames;

		ImagePositionPicker* preview_frame;
		wxTextCtrl* textbox_max_backward;
		wxTextCtrl* textbox_max_forward;
		wxTextCtrl* textbox_tolerance;

		agi::signal::Connection active_line_changed;

		bool check_exists(int pos, int x, int y, int* lrud, double* orig, unsigned char tolerance);
		void process_frame(QueuedFrame);
		void process(wxEvent&);
		void display_current_line();
	public:
		DialogAlignToVideo(agi::Context* context);
		~DialogAlignToVideo();
	};

	DialogAlignToVideo::DialogAlignToVideo(agi::Context* context)
		: wxDialog(context->parent, -1, _("Align subtitle to video by key point"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxMAXIMIZE_BOX | wxRESIZE_BORDER)
		, context(context), provider(context->project->VideoProvider()),
		active_line_changed(context->selectionController->AddActiveLineListener(&DialogAlignToVideo::display_current_line, this))
	{
		auto add_with_label = [&](wxSizer * sizer, wxString const& label, wxWindow * ctrl) {
			sizer->Add(new wxStaticText(this, -1, label), 0, wxLEFT | wxRIGHT | wxCENTER, 3);
			sizer->Add(ctrl, 1, wxLEFT);
		};

		auto tolerance = OPT_GET("Tool/Align to Video/Tolerance")->GetInt();
		auto max_backward = OPT_GET("Tool/Align to Video/Max Backward")->GetInt();
		auto max_forward = OPT_GET("Tool/Align to Video/Max Forward")->GetInt();
		auto maximized = OPT_GET("Tool/Align to Video/Maximized")->GetBool();

		AssDialogue *current_line = context->selectionController->GetActiveLine();
		int current_n_frame = context->videoController->FrameAtTime(current_line->Start, agi::vfr::Time::START);
		VideoFrame current_frame = *context->project->VideoProvider()->GetFrame(current_n_frame, 0, true);
		wxImage preview_image = GetImage(current_frame);
		queued_frames = {};

		preview_frame = new ImagePositionPicker(this, preview_image, [&](int x, int y, unsigned char r, unsigned char g, unsigned char b) -> void {
			AssDialogue *line = DialogAlignToVideo::context->selectionController->GetActiveLine();
			QueuedFrame frame = { x, y, line };
			queued_frames.push_back(frame);
			DialogAlignToVideo::context->selectionController->NextLine();
		});


		textbox_max_backward = new wxTextCtrl(this, -1, wxString::Format(wxT("%i"), int(max_backward)));
		textbox_max_backward->SetToolTip(_("Maximum number of frames to track backwards"));
		textbox_max_forward = new wxTextCtrl(this, -1, wxString::Format(wxT("%i"), int(max_forward)));
		textbox_max_forward->SetToolTip(_("Maximum number of frames to track forwards"));
		textbox_tolerance = new wxTextCtrl(this, -1, wxString::Format(wxT("%i"), int(tolerance)));
		textbox_tolerance->SetToolTip(_("Max tolerance of the color"));

		wxFlexGridSizer* right_sizer = new wxFlexGridSizer(4, 2, 5, 5);
		add_with_label(right_sizer, _("Max Backwards"), textbox_max_backward);
		add_with_label(right_sizer, _("Max Forwards"), textbox_max_forward);
		add_with_label(right_sizer, _("Tolerance"), textbox_tolerance);
		right_sizer->AddGrowableCol(1, 1);

		wxSizer* main_sizer = new wxBoxSizer(wxHORIZONTAL);

		main_sizer->Add(preview_frame, 1, (wxALL & ~wxRIGHT) | wxEXPAND, 5);
		main_sizer->Add(right_sizer, 0, wxALIGN_LEFT, 5);

		wxSizer* dialog_sizer = new wxBoxSizer(wxVERTICAL);
		dialog_sizer->Add(main_sizer, wxSizerFlags(1).Border(wxALL & ~wxBOTTOM).Expand());
		dialog_sizer->Add(CreateButtonSizer(wxOK | wxCANCEL), wxSizerFlags().Right().Border());
		SetSizerAndFit(dialog_sizer);
		SetSize(1024, 700);
		CenterOnParent();

		Bind(wxEVT_BUTTON, &DialogAlignToVideo::process, this, wxID_OK);
		SetIcon(GETICON(button_align_16));
		if (maximized)
			wxDialog::Maximize(true);
	}

	DialogAlignToVideo::~DialogAlignToVideo()
	{
		long l_backward, l_forward, lt;
		if (!textbox_max_backward->GetValue().ToLong(&l_backward) || !textbox_max_forward->GetValue().ToLong(&l_forward) || !textbox_tolerance->GetValue().ToLong(&lt))
		{
			return;
		}
		if (lt < 0 || lt > 255)
		{
			return;
		}

		OPT_SET("Tool/Align to Video/Tolerance")->SetInt(lt);
		OPT_SET("Tool/Align to Video/Max Backward")->SetInt(l_backward);
		OPT_SET("Tool/Align to Video/Max Forward")->SetInt(l_forward);
	}

	void rgb2lab(unsigned char r, unsigned char g, unsigned char b, double* lab)
	{
		double X = (0.412453 * r + 0.357580 * g + 0.180423 * b) / 255.0;
		double Y = (0.212671 * r + 0.715160 * g + 0.072169 * b) / 255.0;
		double Z = (0.019334 * r + 0.119193 * g + 0.950227 * b) / 255.0;
		double xr = X / 0.950456, yr = Y / 1.000, zr = Z / 1.088854;

		if (yr > 0.008856) {
			lab[0] = 116.0 * pow(yr, 1.0 / 3.0) - 16.0;
		}
		else {
			lab[0] = 903.3 * yr;
		}

		double fxr, fyr, fzr;
		if (xr > 0.008856)
			fxr = pow(xr, 1.0 / 3.0);
		else
			fxr = 7.787 * xr + 16.0 / 116.0;

		if (yr > 0.008856)
			fyr = pow(yr, 1.0 / 3.0);
		else
			fyr = 7.787 * yr + 16.0 / 116.0;

		if (zr > 0.008856)
			fzr = pow(zr, 1.0 / 3.0);
		else
			fzr = 7.787 * zr + 16.0 / 116.0;

		lab[1] = 500.0 * (fxr - fyr);
		lab[2] = 200.0 * (fyr - fzr);
	}

	template<typename T>
	bool check_point(boost::gil::pixel<unsigned char, T> & pixel, double orig[3], unsigned char tolerance)
	{
		double lab[3];
		// in pixel: B,G,R
		rgb2lab(pixel[2], pixel[1], pixel[0], lab);
		auto diff = sqrt(pow(lab[0] - orig[0], 2) + pow(lab[1] - orig[1], 2) + pow(lab[2] - orig[2], 2));
		return diff <= tolerance;
	}

	template<typename T>
	bool calculate_point(boost::gil::image_view<T> view, int x, int y, double orig[3], unsigned char tolerance, int* ret)
	{
		auto origin = *view.at(x, y);
		if (!check_point(origin, orig, tolerance))
			return false;
		auto w = view.width();
		auto h = view.height();
		int l = x, r = x, u = y, d = y;
		for (int i = x + 1; i < w; i++)
		{
			auto p = *view.at(i, y);
			if (!check_point(p, orig, tolerance))
			{
				r = i;
				break;
			}
		}

		for (int i = x - 1; i >= 0; i--)
		{
			auto p = *view.at(i, y);
			if (!check_point(p, orig, tolerance))
			{
				l = i;
				break;
			}
		}

		for (int i = y + 1; i < h; i++)
		{
			auto p = *view.at(x, i);
			if (!check_point(p, orig, tolerance))
			{
				d = i;
				break;
			}
		}

		for (int i = y - 1; i >= 0; i--)
		{
			auto p = *view.at(x, i);
			if (!check_point(p, orig, tolerance))
			{
				u = i;
				break;
			}
		}
		ret[0] = l;
		ret[1] = r;
		ret[2] = u;
		ret[3] = d;
		return true;
	}

	void DialogAlignToVideo::process(wxEvent &)
	{
		auto totalTime = queued_frames.size();
		if (totalTime == 0) {
			Close();
			return;
		}
		DialogProgress *progress = new DialogProgress(context->parent, _("Aligning Subtitle to Video"), agi::format(_("Processing frame %d of %d"), 1, totalTime));
		progress->Run([&](agi::ProgressSink *ps) {
			ps->SetProgress(0, totalTime);
			int i = 0;
			while (!queued_frames.empty() && !ps->IsCancelled()) {
				ps->SetProgress(i, totalTime);
				ps->SetMessage(agi::format(_("Processing frame %d of %d"), i+1, totalTime));
				process_frame(queued_frames.back());
				queued_frames.pop_back();
				i++;
			}
		});
		context->ass->Commit(_("Align to video by key point"), AssFile::COMMIT_DIAG_TIME);
		if (queued_frames.size() == 0)
			Close();
	}

	void DialogAlignToVideo::display_current_line(){
		AssDialogue *current_line = context->selectionController->GetActiveLine();

		if (current_line == nullptr)
			return;

		int current_n_frame = context->videoController->FrameAtTime(current_line->Start, agi::vfr::Time::START);
		VideoFrame current_frame = *context->project->VideoProvider()->GetFrame(current_n_frame, 0, true);
		wxImage preview_image = GetImage(current_frame);
		preview_frame->changeImage(preview_image);
	}

	void DialogAlignToVideo::process_frame(QueuedFrame queued_frame){
		int n_frames = provider->GetFrameCount();
		AssDialogue *line = queued_frame.line;
		int x = queued_frame.x;
		int y = queued_frame.y;

		long l_backward, l_forward, lt;
		if (!textbox_max_backward->GetValue().ToLong(&l_backward) || !textbox_max_forward->GetValue().ToLong(&l_forward) || !textbox_tolerance->GetValue().ToLong(&lt))
		{
			wxMessageBox(_("Bad max backwards, max forward or tolerance value!"));
			return;
		}
		if (lt < 0 || lt > 255)
		{
			wxMessageBox(_("Bad tolerance value! Require: 0 <= tolerance <= 255"));
			return;
		}
		int backward = int(l_backward);
		int forward = int(l_forward);
		unsigned char tolerance = (unsigned char)(lt);

		int pos = context->videoController->FrameAtTime(line->Start, agi::vfr::Time::START);
		auto frame = provider->GetFrame(pos, -1, true);
		auto view = interleaved_view(frame->width, frame->height, reinterpret_cast<boost::gil::bgra8_pixel_t*>(frame->data.data()), frame->pitch);
		if (frame->flipped)
			y = frame->height - y;

		auto base_color = *view.at(x, y);
		double lab[3];
		rgb2lab(base_color[2], base_color[1], base_color[0], lab);

		// Ensure selected color and position match
		if(!check_point(*view.at(x,y), lab, tolerance))
		{
			wxMessageBox(_("Selected position and color are not within tolerance!"));
			return;
		}

		int lrud[4];
		calculate_point(view, x, y, lab, tolerance, lrud);

		// find forward
#define CHECK_EXISTS_POS check_exists(pos, x, y, lrud, lab, tolerance)
		int offset = 0;
		do {
			pos -= 2;
			offset += 2;
		} while (pos >= 0 && (offset <= backward || backward == 0) && CHECK_EXISTS_POS);
		pos++;
		pos = std::max(0, pos);
		auto left = CHECK_EXISTS_POS ? pos : pos + 1;

		// find backward
		offset = 0;
		do {
			pos += 2;
			offset += 2;
		} while (pos < n_frames && (offset <= forward || forward == 0) && CHECK_EXISTS_POS);
		pos--;
		pos = std::min(pos, n_frames - 1);
		auto right = CHECK_EXISTS_POS ? pos : pos - 1;

		auto timecode = context->project->Timecodes();
		line->Start = timecode.TimeAtFrame(left, agi::vfr::Time::START);
		line->End = timecode.TimeAtFrame(right, agi::vfr::Time::END); // exclusive
	}

	bool DialogAlignToVideo::check_exists(int pos, int x, int y, int* lrud, double* orig, unsigned char tolerance)
	{
		auto frame = provider->GetFrame(pos, -1, true);
		auto view = interleaved_view(frame->width, frame->height, reinterpret_cast<boost::gil::bgra8_pixel_t*>(frame->data.data()), frame->pitch);
		if (frame->flipped)
			y = frame->height - y;
		int actual[4];
		if (!calculate_point(view, x, y, orig, tolerance, actual)) return false;
		int dl = abs(actual[0] - lrud[0]);
		int dr = abs(actual[1] - lrud[1]);
		int du = abs(actual[2] - lrud[2]);
		int dd = abs(actual[3] - lrud[3]);

		return dl <= 5 && dr <= 5 && du <= 5 && dd <= 5;
	}

}


void ShowAlignToVideoDialog(agi::Context * c)
{
	c->dialog->Show<DialogAlignToVideo>(c);
}
