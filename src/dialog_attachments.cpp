// Copyright (c) 2006, Rodrigo Braz Monteiro
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

#include "ass_attachment.h"
#include "ass_file.h"
#include "compat.h"
#include "help_button.h"
#include "libresrc/libresrc.h"
#include "options.h"
#include "utils.h"

#include <libaegisub/format.h>

#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/filedlg.h>
#include <wx/dirdlg.h>
#include <wx/listctrl.h>
#include <wx/sizer.h>

#include <set>

namespace {
struct DialogAttachments {
	wxDialog d;
	AssFile *ass;

	wxListView *listView;
	wxButton *extractButton;
	wxButton *deleteButton;

	void OnAttachFont(wxCommandEvent &event);
	void OnAttachGraphics(wxCommandEvent &event);
	void OnExtract(wxCommandEvent &event);
	void OnDelete(wxCommandEvent &event);
	void OnListClick(wxListEvent &event);

	void UpdateList();
	void AttachFile(wxFileDialog &diag, wxString const& commit_msg);

public:
	DialogAttachments(wxWindow *parent, AssFile *ass);
};

DialogAttachments::DialogAttachments(wxWindow *parent, AssFile *ass)
: d(parent, -1, _("Attachment List"))
, ass(ass)
{
	d.SetIcons(GETICONS(attach_button));

	listView = new wxListView(&d, -1, wxDefaultPosition, wxSize(500, 200));
	UpdateList();

	auto attachFont = new wxButton(&d, -1, _("Attach &Font"));
	auto attachGraphics = new wxButton(&d, -1, _("Attach &Graphics"));
	extractButton = new wxButton(&d, -1, _("E&xtract"));
	deleteButton = new wxButton(&d, -1, _("&Delete"));
	extractButton->Enable(false);
	deleteButton->Enable(false);

	auto buttonSizer = new wxBoxSizer(wxHORIZONTAL);
	buttonSizer->Add(attachFont, 1);
	buttonSizer->Add(attachGraphics, 1);
	buttonSizer->Add(extractButton, 1);
	buttonSizer->Add(deleteButton, 1);
	buttonSizer->Add(new HelpButton(&d, "Attachment Manager"), 1, wxLEFT, 5);
	buttonSizer->Add(new wxButton(&d, wxID_CANCEL, _("&Close")), 1);

	auto mainSizer = new wxBoxSizer(wxVERTICAL);
	mainSizer->Add(listView, 1, wxTOP | wxLEFT | wxRIGHT | wxEXPAND, 5);
	mainSizer->Add(buttonSizer, 0, wxALL | wxEXPAND, 5);
	d.SetSizerAndFit(mainSizer);
	d.CenterOnParent();

	attachFont->Bind(wxEVT_BUTTON, &DialogAttachments::OnAttachFont, this);
	attachGraphics->Bind(wxEVT_BUTTON, &DialogAttachments::OnAttachGraphics, this);
	extractButton->Bind(wxEVT_BUTTON, &DialogAttachments::OnExtract, this);
	deleteButton->Bind(wxEVT_BUTTON, &DialogAttachments::OnDelete, this);

	listView->Bind(wxEVT_LIST_ITEM_SELECTED, &DialogAttachments::OnListClick, this);
	listView->Bind(wxEVT_LIST_ITEM_DESELECTED, &DialogAttachments::OnListClick, this);
	listView->Bind(wxEVT_LIST_ITEM_FOCUSED, &DialogAttachments::OnListClick, this);
}

void DialogAttachments::UpdateList() {
	listView->ClearAll();

	listView->InsertColumn(0, _("Attachment name"), wxLIST_FORMAT_LEFT, 280);
	listView->InsertColumn(1, _("Size"), wxLIST_FORMAT_LEFT, 100);
	listView->InsertColumn(2, _("Group"), wxLIST_FORMAT_LEFT, 100);

	for (auto& attach : ass->Attachments) {
		int row = listView->GetItemCount();
		listView->InsertItem(row, to_wx(attach.GetFileName(true)));
		listView->SetItem(row, 1, PrettySize(attach.GetSize()));
		listView->SetItem(row, 2, to_wx(attach.GroupHeader()));
	}
}

void DialogAttachments::AttachFile(wxFileDialog &diag, wxString const& commit_msg) {
	if (diag.ShowModal() == wxID_CANCEL) return;

	wxArrayString paths;
	diag.GetPaths(paths);

	for (auto const& fn : paths)
		ass->InsertAttachment(agi::fs::path(fn.wx_str()));

	ass->Commit(commit_msg, AssFile::COMMIT_ATTACHMENT);

	UpdateList();
}

void DialogAttachments::OnAttachFont(wxCommandEvent &) {
	wxFileDialog diag(&d,
		_("Choose file to be attached"),
		to_wx(OPT_GET("Path/Fonts Collector Destination")->GetString()), "", _("Font Files") + " (*.ttf)|*.ttf",
		wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);

	AttachFile(diag, _("attach font file"));
}

void DialogAttachments::OnAttachGraphics(wxCommandEvent &) {
	wxFileDialog diag(&d,
		_("Choose file to be attached"),
		"", "",
		_("Graphic Files") + " (*.bmp, *.gif, *.jpg, *.ico, *.wmf)|*.bmp;*.gif;*.jpg;*.ico;*.wmf",
		wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);

	AttachFile(diag, _("attach graphics file"));
}

void DialogAttachments::OnExtract(wxCommandEvent &) {
	std::vector<long> selection;
	for (auto i = listView->GetFirstSelected(); i != -1; i = listView->GetNextSelected(i))
		selection.push_back(i);
	if (selection.empty()) return;

	const bool multi = selection.size() > 1;

	agi::fs::path base;
	if (multi) {
		base = wxDirSelector(_("Select the path to save the files to:"), to_wx(OPT_GET("Path/Fonts Collector Destination")->GetString())).utf8_str().data();
		if (base.empty()) return;
	}
	else {
		auto const& attach = ass->Attachments[selection.front()];
		auto const& original_name = attach.GetFileName();
		auto safe_default = agi::fs::SanitizeBasename(original_name);
		if (safe_default.empty())
			safe_default = "attachment" + agi::fs::path(original_name).extension().string();

		base = SaveFileSelector(
			_("Select the path to save the file to:"),
			"Path/Fonts Collector Destination",
			safe_default,
			"", from_wx(_("All Supported Formats") + " (*.bmp, *.gif, *.jpg, *.ico, *.ttf, *.wmf)|*.bmp;*.gif;*.jpg;*.ico;*.ttf;*.wmf|" +  _("Font Files") + " (*.ttf)|*.ttf|" + _("Graphic Files") + " (*.bmp, *.gif, *.jpg, *.ico, *.wmf)|*.bmp;*.gif;*.jpg;*.ico;*.wmf"),
			&d);
		if (base.empty()) return;
	}

	std::vector<agi::fs::path> out_paths;
	out_paths.reserve(selection.size());
	std::vector<std::pair<std::string, std::string>> renamed;

	if (multi) {
		std::set<std::string> used;
		auto make_unique = [&](agi::fs::path p) {
			auto stem = p.stem().string();
			auto ext = p.extension().string();
			for (int n = 1; agi::fs::Exists(p) || !used.emplace(p.string()).second; ++n) {
				p = p.parent_path() / agi::format("%s (%d)%s", stem, n, ext);
			}
			return p;
		};

		for (size_t n = 0; n < selection.size(); ++n) {
			auto const& attach = ass->Attachments[selection[n]];
			auto const& original_name = attach.GetFileName();

			auto safe = agi::fs::SanitizeBasename(original_name);
			if (safe.empty())
				safe = agi::format("attachment_%d%s", static_cast<int>(n + 1), agi::fs::path(original_name).extension().string());

			auto out = make_unique(base / safe);
			out_paths.push_back(out);

			auto final_name = out.filename().string();
			if (final_name != original_name)
				renamed.emplace_back(original_name, final_name);
		}

		if (!renamed.empty()) {
			wxString msg = _("Some attachment filenames were unsafe or conflicted with existing files, and will be changed:");
			msg += "\n\n";

			constexpr size_t max_lines = 25;
			for (size_t i = 0; i < renamed.size() && i < max_lines; ++i)
				msg += to_wx(agi::format("%s -> %s\n", renamed[i].first, renamed[i].second));
			if (renamed.size() > max_lines)
				msg += to_wx(agi::format("... (%d more)\n", static_cast<int>(renamed.size() - max_lines)));

			wxMessageDialog dlg(&d, msg, _("Unsafe filenames"), wxOK | wxCANCEL | wxICON_WARNING);
			dlg.SetOKLabel(_("Extract"));
			dlg.SetCancelLabel(_("Cancel"));
			if (dlg.ShowModal() != wxID_OK) return;
		}
	}

	for (size_t n = 0; n < selection.size(); ++n) {
		auto const& attach = ass->Attachments[selection[n]];
		attach.Extract(multi ? out_paths[n] : base);
	}
}

void DialogAttachments::OnDelete(wxCommandEvent &) {
	size_t removed = 0;
	for (auto i = listView->GetFirstSelected(); i != -1; i = listView->GetNextSelected(i))
		ass->Attachments.erase(ass->Attachments.begin() + i - removed++);

	ass->Commit(_("remove attachment"), AssFile::COMMIT_ATTACHMENT);

	UpdateList();
	extractButton->Enable(false);
	deleteButton->Enable(false);
}

void DialogAttachments::OnListClick(wxListEvent &) {
	bool hasSel = listView->GetFirstSelected() != -1;
	extractButton->Enable(hasSel);
	deleteButton->Enable(hasSel);
}
}

void ShowAttachmentsDialog(wxWindow *parent, AssFile *file) {
	DialogAttachments(parent, file).d.ShowModal();
}
