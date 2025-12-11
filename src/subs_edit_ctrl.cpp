// Copyright (c) 2021, Qirui Wang
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

#include "subs_edit_ctrl.h"

#include "command/command.h"
#include "compat.h"
#include "options.h"
#include "include/aegisub/context.h"
#include "include/aegisub/spellchecker.h"
#include "text_selection_controller.h"
#include "selection_controller.h"
#include "thesaurus.h"
#include "utils.h"
#include "format.h"
#include "ass_dialogue.h"

#include <libaegisub/ass/dialogue_parser.h>
#include <libaegisub/character_count.h>
#include <libaegisub/make_unique.h>
#include <libaegisub/spellchecker.h>

#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/predicate.hpp>

// Maximum number of languages (locales)
#define LANGS_MAX 1000

/// Event ids
enum {
	EDIT_MENU_SPLIT_PRESERVE = (wxID_HIGHEST + 1) + 4000,
	EDIT_MENU_SPLIT_ESTIMATE,
	EDIT_MENU_SPLIT_VIDEO,
	EDIT_MENU_CUT,
	EDIT_MENU_COPY,
	EDIT_MENU_PASTE,
	EDIT_MENU_SELECT_ALL,
	EDIT_MENU_ADD_TO_DICT,
	EDIT_MENU_REMOVE_FROM_DICT,
	EDIT_MENU_SUGGESTION,
	EDIT_MENU_SUGGESTIONS,
	EDIT_MENU_THESAURUS = (wxID_HIGHEST + 1) + 5000,
	EDIT_MENU_THESAURUS_SUGS,
	EDIT_MENU_SPELL_LANGUAGE = (wxID_HIGHEST + 1) + 6000,
	EDIT_MENU_SPELL_LANGS,
	EDIT_MENU_THES_LANGUAGE = EDIT_MENU_SPELL_LANGUAGE + LANGS_MAX,
	EDIT_MENU_THES_LANGS,
	EDIT_MENU_RTL = (wxID_HIGHEST + 1) + 7000
};

SubsTextEditCtrl::SubsTextEditCtrl(wxWindow* parent, wxSize wsize, long style, agi::Context* context)
	: wxTextCtrl(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wsize, style | wxTE_MULTILINE)
	, thesaurus(agi::make_unique<Thesaurus>())
	, spellchecker(SpellCheckerFactory::GetSpellChecker())
	, context(context)
{
	SetStyles();

	using std::bind;

	Bind(wxEVT_CHAR_HOOK, &SubsTextEditCtrl::OnKeyDown, this);

	Bind(wxEVT_MENU, bind(&SubsTextEditCtrl::Cut, this), EDIT_MENU_CUT);
	Bind(wxEVT_MENU, bind(&SubsTextEditCtrl::Copy, this), EDIT_MENU_COPY);
	Bind(wxEVT_MENU, bind(&SubsTextEditCtrl::Paste, this), EDIT_MENU_PASTE);
	Bind(wxEVT_MENU, bind(&SubsTextEditCtrl::SelectAll, this), EDIT_MENU_SELECT_ALL);

	if (context) {
		Bind(wxEVT_MENU, bind(&cmd::call, "edit/line/split/preserve", context), EDIT_MENU_SPLIT_PRESERVE);
		Bind(wxEVT_MENU, bind(&cmd::call, "edit/line/split/estimate", context), EDIT_MENU_SPLIT_ESTIMATE);
		Bind(wxEVT_MENU, bind(&cmd::call, "edit/line/split/video", context), EDIT_MENU_SPLIT_VIDEO);
		Bind(wxEVT_CONTEXT_MENU, &SubsTextEditCtrl::OnContextMenu, this);
	}
	// Bind RTL toggle so native mode has an explicit toggle
	Bind(wxEVT_MENU, &SubsTextEditCtrl::OnToggleRTL, this, EDIT_MENU_RTL);
	// Bind text update for syntax highlighting
	Bind(wxEVT_TEXT, [this](wxCommandEvent&){ UpdateSyntaxHighlight(); });
	// Bind spell checker suggestion handlers
	Bind(wxEVT_MENU, bind(&SubsTextEditCtrl::OnUseSuggestion, this, std::placeholders::_1), EDIT_MENU_SUGGESTIONS, EDIT_MENU_SUGGESTIONS+LANGS_MAX);
	Bind(wxEVT_MENU, &SubsTextEditCtrl::OnAddToDict, this, EDIT_MENU_ADD_TO_DICT);
	Bind(wxEVT_MENU, &SubsTextEditCtrl::OnRemoveFromDict, this, EDIT_MENU_REMOVE_FROM_DICT);
	Bind(wxEVT_MENU, &SubsTextEditCtrl::OnSetSpellLang, this, EDIT_MENU_SPELL_LANGS, EDIT_MENU_SPELL_LANGS+LANGS_MAX);
	// Bind thesaurus suggestion handlers
	Bind(wxEVT_MENU, bind(&SubsTextEditCtrl::OnUseSuggestion, this, std::placeholders::_1), EDIT_MENU_THESAURUS_SUGS, EDIT_MENU_THESAURUS_SUGS+LANGS_MAX);
	Bind(wxEVT_MENU, &SubsTextEditCtrl::OnSetThesLanguage, this, EDIT_MENU_THES_LANGS, EDIT_MENU_THES_LANGS+LANGS_MAX);

	OPT_SUB("Subtitle/Edit Box/Font Face", &SubsTextEditCtrl::SetStyles, this);
	OPT_SUB("Subtitle/Edit Box/Font Size", &SubsTextEditCtrl::SetStyles, this);
	OPT_SUB("Colour/Subtitle/Background", &SubsTextEditCtrl::SetStyles, this);
	OPT_SUB("Colour/Subtitle/Syntax/Normal", &SubsTextEditCtrl::SetStyles, this);
}

SubsTextEditCtrl::~SubsTextEditCtrl() {
}

void SubsTextEditCtrl::OnKeyDown(wxKeyEvent& event) {
	// Handle Shift+Return for soft line breaks
	if (event.GetKeyCode() == WXK_RETURN && event.GetModifiers() == wxMOD_SHIFT) {
		long sel_start, sel_end;
		GetSelection(&sel_start, &sel_end);
		std::string linebreak = OPT_GET("Subtitle/Edit Box/Soft Line Break")->GetBool() ? "\\n" : "\\N";
		wxString data = GetRange(0, sel_start) + to_wx(linebreak) + GetRange(sel_end, GetLastPosition());
		SetValue(data);
		SetSelection(sel_start + 2, sel_start + 2);
		return;  // We handled it, don't skip
	}

	// For all other keys, let the native widget and OS handle them
	// This includes Ctrl+Shift+Right for word selection, etc.
	event.Skip();
}

void SubsTextEditCtrl::SetStyles() {
	wxFont font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
	font.SetEncoding(wxFONTENCODING_DEFAULT);
	wxString fontname = FontFace("Subtitle/Edit Box");
	if (!fontname.empty()) font.SetFaceName(fontname);
	font.SetPointSize(OPT_GET("Subtitle/Edit Box/Font Size")->GetInt());
	SetFont(font);

	SetBackgroundColour(to_wx(OPT_GET("Colour/Subtitle/Background")->GetColor()));
	SetForegroundColour(to_wx(OPT_GET("Colour/Subtitle/Syntax/Normal")->GetColor()));
}

void SubsTextEditCtrl::OnContextMenu(wxContextMenuEvent& event) {
	wxPoint pos = event.GetPosition();
	int activePos;
	if (pos == wxDefaultPosition)
		activePos = GetInsertionPoint();
	else {
		long from, to;
		GetSelection(&from, &to);
		activePos = to;
	}

	currentWordPos = GetBoundsOfWordAtPosition(activePos);
	wxString textValue = GetValue();
	if (currentWordPos.second > 0 && currentWordPos.first + currentWordPos.second <= (int)textValue.length()) {
		// Use wxString::Mid to get the substring by character indices, then convert to UTF-8
		wxString wxcur = textValue.Mid(currentWordPos.first, currentWordPos.second);
		currentWord = from_wx(wxcur);
	} else {
		currentWord.clear();
	}

	wxMenu menu;

	// Spell checker
	if (spellchecker)
		AddSpellCheckerEntries(menu);

	// Thesaurus
	AddThesaurusEntries(menu);

	// Standard actions
	menu.Append(EDIT_MENU_CUT, _("Cu&t"))->Enable(!GetStringSelection().IsEmpty());
	menu.Append(EDIT_MENU_COPY, _("&Copy"))->Enable(!GetStringSelection().IsEmpty());
	menu.Append(EDIT_MENU_PASTE, _("&Paste"))->Enable(CanPaste());
	menu.AppendSeparator();
	menu.Append(EDIT_MENU_SELECT_ALL, _("Select &All"));

	// Split
	if (context) {
		menu.AppendSeparator();
		menu.Append(EDIT_MENU_SPLIT_PRESERVE, _("Split at cursor (preserve times)"));
		menu.Append(EDIT_MENU_SPLIT_ESTIMATE, _("Split at cursor (estimate times)"));
		cmd::Command* split_video = cmd::get("edit/line/split/video");
		menu.Append(EDIT_MENU_SPLIT_VIDEO, split_video->StrMenu(context))->Enable(split_video->Validate(context));
	}

	// Add explicit RTL toggle fallback so native mode always has the option
	menu.AppendSeparator();
	menu.Append(EDIT_MENU_RTL, _("Right to left Reading order"));

	// Use PopupMenu to properly route menu events through the event system
	PopupMenu(&menu);
}

void SubsTextEditCtrl::Paste() {
	std::string data = GetClipboard();

	boost::replace_all(data, "\r\n", "\\N");
	boost::replace_all(data, "\n", "\\N");
	boost::replace_all(data, "\r", "\\N");

	long sel_start, sel_end;
	GetSelection(&sel_start, &sel_end);
	wxString data_first_half = GetRange(0, sel_start) + to_wx(data);
	wxString data_full = data_first_half + GetRange(sel_end, GetLastPosition());
	Freeze();
	SetValue(data_first_half);
	sel_start = GetLastPosition();
	SetValue(data_full);
	SetSelection(sel_start, sel_start);
	Thaw();
}

void SubsTextEditCtrl::SetTextTo(std::string const& text) {
	// Mirror STC behaviour: preserve insertion point, update selection controller
	SetEvtHandlerEnabled(false);
	Freeze();

	long insertion_point = GetInsertionPoint();

	// Get current value as std::string
	wxCharBuffer curbuf = GetValue().utf8_str();
	std::string cur = curbuf.data() ? std::string(curbuf.data(), curbuf.length()) : std::string();

	if (static_cast<size_t>(insertion_point) > cur.size())
		; // nothing to do, cur is up-to-date

	// Compute old character index (clamped)
	size_t clamp_pos = std::min<size_t>(cur.size(), static_cast<size_t>(std::max<long>(0, insertion_point)));
	size_t old_pos = agi::CharacterCount(cur.begin(), cur.begin() + clamp_pos, 0);

	if (context) {
		context->textSelectionController->SetSelection(0, 0);
		SetValue(to_wx(text));
		auto pos = agi::IndexOfCharacter(text, old_pos);
		context->textSelectionController->SetSelection(pos, pos);
	}
	else {
		SetSelection(0, 0);
		SetValue(to_wx(text));
		auto pos = agi::IndexOfCharacter(text, old_pos);
		SetSelection(pos, pos);
	}

	SetEvtHandlerEnabled(true);
	Thaw();
}

std::pair<int, int> SubsTextEditCtrl::GetBoundsOfWordAtPosition(int pos) {
	// Simple word boundary detection for native wxTextCtrl
	// Returns {start_pos, length} of word at position pos
	wxString text = GetValue();
	// Handle empty control or invalid position
	if (text.empty() || pos < 0 || pos > (int)text.length()) return {0, 0};

	// Find start of word
	int start = pos;
	while (start > 0 && wxIsalnum(text[start - 1])) {
		start--;
	}

	// Find end of word
	int end = pos;
	while (end < (int)text.length() && wxIsalnum(text[end])) {
		end++;
	}

	return {start, end - start};
}

// Placeholder implementations for menu items (simplified for wxTextCtrl)
void SubsTextEditCtrl::AddSpellCheckerEntries(wxMenu &menu) {
	if (currentWord.empty()) return;

	if (spellchecker->CanRemoveWord(currentWord))
		menu.Append(EDIT_MENU_REMOVE_FROM_DICT, fmt_tl("Remove \"%s\" from dictionary", currentWord));

	sugs = spellchecker->GetSuggestions(currentWord);
	if (spellchecker->CheckWord(currentWord)) {
		// Word is spelled correctly
		if (sugs.empty())
			menu.Append(EDIT_MENU_SUGGESTION,_("No spell checker suggestions"))->Enable(false);
		else {
			auto subMenu = new wxMenu;
			for (size_t i = 0; i < sugs.size(); ++i)
				subMenu->Append(EDIT_MENU_SUGGESTIONS+i, to_wx(sugs[i]));

			menu.Append(-1, fmt_tl("Spell checker suggestions for \"%s\"", currentWord), subMenu);
		}
	}
	else {
		// Word is misspelled - show suggestions directly
		if (sugs.empty())
			menu.Append(EDIT_MENU_SUGGESTION,_("No correction suggestions"))->Enable(false);
		else {
			for (size_t i = 0; i < sugs.size(); ++i)
				menu.Append(EDIT_MENU_SUGGESTIONS+i, to_wx(sugs[i]));
		}

		// Append "add word" option for misspelled words
		menu.Append(EDIT_MENU_ADD_TO_DICT, fmt_tl("Add \"%s\" to dictionary", currentWord))->Enable(spellchecker->CanAddWord(currentWord));
	}
	menu.Append(-1,_("Spell checker language"), GetLanguagesMenu(
		EDIT_MENU_SPELL_LANGS,
		to_wx(OPT_GET("Tool/Spell Checker/Language")->GetString()),
		to_wx(spellchecker->GetLanguageList())));
	menu.AppendSeparator();
}

void SubsTextEditCtrl::AddThesaurusEntries(wxMenu &menu) {
	if (currentWord.empty()) return;

	auto results = thesaurus->Lookup(currentWord);

	thesSugs.clear();

	if (results.size()) {
		auto thesMenu = new wxMenu;

		int curThesEntry = 0;
		for (auto const& result : results) {
			// Single word, insert directly
			if (result.second.empty()) {
				thesMenu->Append(EDIT_MENU_THESAURUS_SUGS+curThesEntry, to_wx(result.first));
				thesSugs.push_back(result.first);
				++curThesEntry;
			}
			// Multiple, create submenu
			else {
				auto subMenu = new wxMenu;
				for (auto const& sug : result.second) {
					subMenu->Append(EDIT_MENU_THESAURUS_SUGS+curThesEntry, to_wx(sug));
					thesSugs.push_back(sug);
					++curThesEntry;
				}

				thesMenu->Append(-1, to_wx(result.first), subMenu);
			}
		}

		menu.Append(-1, fmt_tl("Thesaurus suggestions for \"%s\"", currentWord), thesMenu);
	}
	else
		menu.Append(EDIT_MENU_THESAURUS,_("No thesaurus suggestions"))->Enable(false);

	// Append language list
	menu.Append(-1,_("Thesaurus language"), GetLanguagesMenu(
		EDIT_MENU_THES_LANGS,
		to_wx(OPT_GET("Tool/Thesaurus/Language")->GetString()),
		to_wx(thesaurus->GetLanguageList())));
	menu.AppendSeparator();
}

wxMenu *SubsTextEditCtrl::GetLanguagesMenu(int base_id, wxString const& curLang, wxArrayString const& langs) {
	auto languageMenu = new wxMenu;
	languageMenu->AppendRadioItem(base_id, _("Disable"))->Check(curLang.empty());

	for (size_t i = 0; i < langs.size(); ++i)
		languageMenu->AppendRadioItem(base_id + i + 1, LocalizedLanguageName(langs[i]))->Check(langs[i] == curLang);

	return languageMenu;
}

void SubsTextEditCtrl::OnUseSuggestion(wxCommandEvent &event) {
	std::string suggestion;
	int eventId = event.GetId();

	// Check if this is a spell checker suggestion
	if (eventId >= EDIT_MENU_SUGGESTIONS && eventId < EDIT_MENU_SUGGESTIONS + LANGS_MAX) {
		int sugIdx = eventId - EDIT_MENU_SUGGESTIONS;
		if (sugIdx >= 0 && (size_t)sugIdx < sugs.size())
			suggestion = sugs[sugIdx];
		else
			return;
	}
	// Or a thesaurus suggestion
	else if (eventId >= EDIT_MENU_THESAURUS_SUGS && eventId < EDIT_MENU_THESAURUS_SUGS + LANGS_MAX) {
		int sugIdx = eventId - EDIT_MENU_THESAURUS_SUGS;
		if (sugIdx >= 0 && (size_t)sugIdx < thesSugs.size())
			suggestion = thesSugs[sugIdx];
		else
			return;
	}
	else
		return;

	size_t pos;
	while ((pos = suggestion.rfind('(')) != std::string::npos) {
		// If there's only one suggestion for a word it'll be in the form "(noun) word",
		// so we need to trim the "(noun) " part
		if (pos == 0) {
			pos = suggestion.find(')');
			if (pos != std::string::npos) {
				if (pos + 1< suggestion.size() && suggestion[pos + 1] == ' ') ++pos;
				suggestion.erase(0, pos + 1);
			}
			break;
		}

		// Some replacements have notes about their usage after the word in the
		// form "word (generic term)" that we need to remove (plus the leading space)
		suggestion.resize(pos - 1);
	}

	long sel_start, sel_end;
	GetSelection(&sel_start, &sel_end);
	wxString beforeWord = GetRange(0, currentWordPos.first);
	wxString afterWord = GetRange(currentWordPos.first + currentWordPos.second, GetLastPosition());
	wxString newValue = beforeWord + to_wx(suggestion) + afterWord;

	SetValue(newValue);
	SetSelection(currentWordPos.first + suggestion.length(), currentWordPos.first + suggestion.length());
}

void SubsTextEditCtrl::OnSetThesLanguage(wxCommandEvent &event) {
	if (!thesaurus) return;

	std::vector<std::string> langs = thesaurus->GetLanguageList();

	int index = event.GetId() - EDIT_MENU_THES_LANGS - 1;
	std::string lang;
	if (index >= 0 && (size_t)index < langs.size())
		lang = langs[index];

	OPT_SET("Tool/Thesaurus/Language")->SetString(lang);
}

void SubsTextEditCtrl::OnSetSpellLang(wxCommandEvent &event) {
	if (!spellchecker) return;

	std::vector<std::string> langs = spellchecker->GetLanguageList();

	int index = event.GetId() - EDIT_MENU_SPELL_LANGS - 1;
	std::string lang;
	if (index >= 0 && (size_t)index < langs.size())
		lang = langs[index];

	OPT_SET("Tool/Spell Checker/Language")->SetString(lang);
}

void SubsTextEditCtrl::OnAddToDict(wxCommandEvent &event) {
	if (spellchecker)
		spellchecker->AddWord(currentWord);
}

void SubsTextEditCtrl::OnRemoveFromDict(wxCommandEvent &event) {
	if (spellchecker)
		spellchecker->RemoveWord(currentWord);
}

void SubsTextEditCtrl::UpdateSyntaxHighlight() {
	if (!OPT_GET("Subtitle/Highlight/Syntax")->GetBool()) {
		return;
	}

	wxString textValue = GetValue();
	std::string line_text = std::string(textValue.utf8_str().data());

	// Reset whole control to normal text color first (clears previous styling)
	wxTextAttr defaultAttr;
	defaultAttr.SetTextColour(to_wx(OPT_GET("Colour/Subtitle/Syntax/Normal")->GetColor()));
	SetStyle(0, GetLastPosition(), defaultAttr);

	if (line_text.empty()) return;

	// Tokenize the line for syntax analysis
	AssDialogue *diag = context ? context->selectionController->GetActiveLine() : nullptr;
	bool template_line = diag && diag->Comment && (boost::istarts_with(diag->Effect.get(), "template") || boost::istarts_with(diag->Effect.get(), "mixin"));

	auto tokenized_line = agi::ass::TokenizeDialogueBody(line_text, template_line);
	agi::ass::SplitWords(line_text, tokenized_line);

	// Apply syntax highlighting colors
	// Note: agi::ass::SyntaxHighlight returns ranges measured in bytes (UTF-8),
	// but wxTextCtrl::SetStyle expects character indices. Convert byte offsets
	// to character indices using agi::CharacterCount.
	size_t byte_pos = 0;
	for (auto const& style_range : agi::ass::SyntaxHighlight(line_text, tokenized_line, spellchecker.get())) {
		// Compute byte range
		size_t start_byte = byte_pos;
		size_t end_byte = byte_pos + style_range.length;

		// Clamp to valid range
		if (start_byte >= line_text.size()) break;
		end_byte = std::min(end_byte, line_text.size());

		// Convert to character indices
		size_t start_char = agi::CharacterCount(line_text.begin(), line_text.begin() + start_byte, 0);
		size_t end_char = agi::CharacterCount(line_text.begin(), line_text.begin() + end_byte, 0);
		size_t char_len = end_char - start_char;

		wxColour color;
		bool apply = true;

		// Map syntax style types to colors with defensive fallbacks
		if (style_range.type == agi::ass::SyntaxStyle::TAG) {
			auto opt = OPT_GET("Colour/Subtitle/Syntax/Tags");
			color = opt ? to_wx(opt->GetColor()) : wxColour(200, 200, 200);
		}
		else if (style_range.type == agi::ass::SyntaxStyle::OVERRIDE) {
			auto opt = OPT_GET("Colour/Subtitle/Syntax/Brackets");
			color = opt ? to_wx(opt->GetColor()) : wxColour(200, 200, 200);
		}
		else if (style_range.type == agi::ass::SyntaxStyle::PUNCTUATION) {
			auto opt = OPT_GET("Colour/Subtitle/Syntax/Slashes");
			color = opt ? to_wx(opt->GetColor()) : wxColour(200, 200, 200);
		}
		else if (style_range.type == agi::ass::SyntaxStyle::PARAMETER) {
			auto opt = OPT_GET("Colour/Subtitle/Syntax/Parameters");
			color = opt ? to_wx(opt->GetColor()) : wxColour(200, 200, 200);
		}
		else if (style_range.type == agi::ass::SyntaxStyle::ERROR) {
			auto opt = OPT_GET("Colour/Subtitle/Syntax/Error");
			color = opt ? to_wx(opt->GetColor()) : *wxRED;
		}
		else if (style_range.type == agi::ass::SyntaxStyle::SPELLING) {
			color = *wxRED;
		}
		else {
			apply = false;
		}

		if (apply && char_len > 0) {
			wxTextAttr attr;
			attr.SetTextColour(color);
			SetStyle((long)start_char, (long)(start_char + char_len), attr);
		}

		byte_pos = end_byte;
	}
}


void SubsTextEditCtrl::OnToggleRTL(wxCommandEvent &event) {
	wxLayoutDirection cur = GetLayoutDirection();
	wxLayoutDirection next = (cur == wxLayout_RightToLeft) ? wxLayout_LeftToRight : wxLayout_RightToLeft;
	SetLayoutDirection(next);
	// Also update caret/selection behavior by refreshing control
	Refresh();
}
