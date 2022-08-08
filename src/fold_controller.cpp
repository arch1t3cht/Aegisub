// Copyright (c) 2022, arch1t3cht <arch1t3cht@gmail.com>>
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

#include "fold_controller.h"

#include "ass_file.h"
#include "include/aegisub/context.h"
#include "format.h"
#include "subs_controller.h"

#include <algorithm>
#include <unordered_map>

#include <libaegisub/log.h>

static int next_fold_id = 0;

FoldController::FoldController(agi::Context *c)
: context(c)
, pre_commit_listener(c->ass->AddPreCommitListener(&FoldController::FixFoldsPreCommit, this))
{ }


bool FoldController::CanAddFold(AssDialogue& start, AssDialogue& end) {
	if (start.Fold.exists || end.Fold.exists) {
		return false;
	}
	int folddepth = 0;
	for (auto it = std::next(context->ass->Events.begin(), start.Row); it->Row < end.Row; it++) {
		if (it->Fold.exists) {
			folddepth += it->Fold.side ? -1 : 1;
		}
		if (folddepth < 0) {
			return false;
		}
	}
	return folddepth == 0;
}

void FoldController::RawAddFold(AssDialogue& start, AssDialogue& end, bool collapsed) {
	int id = next_fold_id++;

	start.Fold.exists = true;
	start.Fold.collapsed = collapsed;
	start.Fold.id = id;
	start.Fold.side = false;

	end.Fold.exists = true;
	end.Fold.collapsed = collapsed;
	end.Fold.id = id;
	end.Fold.side = true;
}

void FoldController::AddFold(AssDialogue& start, AssDialogue& end, bool collapsed) {
	if (CanAddFold(start, end)) {
		RawAddFold(start, end, true);
		context->ass->Commit(_("add fold"), AssFile::COMMIT_FOLD);
	}
}

bool FoldController::DoForAllFolds(bool action(AssDialogue& line)) {
	for (AssDialogue& line : context->ass->Events) {
		if (line.Fold.exists) {
			if (action(line)) {
				return true;
			}
		}
	}
	return false;
}

void FoldController::FixFoldsPreCommit(int type, const AssDialogue *single_line) {
	if ((type & (AssFile::COMMIT_FOLD | AssFile::COMMIT_DIAG_ADDREM | AssFile::COMMIT_ORDER)) || type == AssFile::COMMIT_NEW) {
		if (type == AssFile::COMMIT_NEW && context->subsController->IsUndoStackEmpty()) {
			// This might be the biggest hack in all of this. We want to hook into the FileOpen signal to
			// read and apply the folds from the project data, but if we do it naively, this will only happen
			// after the first commit has been pushed to the undo stack. Thus, if a user uses Ctrl+Z after opening
			// a file, all folds will be cleared.
			// Instead, we hook into the first commit which is made after loading a file, right after the undo stack was cleared.
			DoForAllFolds(FoldController::ActionClearFold);
			MakeFoldsFromFile();
		}
		FixFolds();
	}
}

void FoldController::MakeFoldsFromFile() {
	if (context->ass->Properties.folds.empty()) {
		return;
	}

	int numlines = context->ass->Events.size();
	for (LineFold fold : context->ass->Properties.folds) {
		if (fold.start >= 0 && fold.start < fold.end && fold.end <= numlines) {
			auto opener = std::next(context->ass->Events.begin(), fold.start);
			RawAddFold(*opener, *std::next(opener, fold.end - fold.start), fold.collapsed);
		}
	}
}


// For each line in lines, applies action() to the opening delimiter of the innermost fold containing this line.
// Returns true as soon as any action() call returned true.
//
// In general, this can leave the folds in an inconsistent state, so unless action() is read-only this should always
// be followed by a commit.
bool FoldController::DoForFoldsAt(std::vector<AssDialogue *> const& lines, bool action(AssDialogue& line)) {
	for (AssDialogue *line : lines) {
		if (line->Fold.parent != nullptr && !(line->Fold.exists && !line->Fold.side)) {
			line = line->Fold.parent;
		}
		if (!line->Fold.visited && action(*line)) {
			return true;
		}
		line->Fold.visited = true;
	}
	return false;
}

void FoldController::FixFolds() {
	// Stack of which folds we've desended into so far
	std::vector<AssDialogue *> foldStack;

	// ID's for which we've found starters
	std::unordered_map<int, AssDialogue*> foldHeads;

	// ID's for which we've either found a valid starter and ender,
	// or determined that the respective fold is invalid. All further
	// fold data with this ID is skipped and deleted.
	std::unordered_map<int, bool> completedFolds;

	for (auto line = context->ass->Events.begin(); line != context->ass->Events.end(); line++) {
		if (line->Fold.exists) {
			if (completedFolds.count(line->Fold.id)) { 	// Duplicate entry
				line->Fold.exists = false;
				continue;
			}
			if (!line->Fold.side) {
				if (foldHeads.count(line->Fold.id)) { 	// Duplicate entry
					line->Fold.exists = false;
				} else {
					foldHeads[line->Fold.id] = &*line;
					foldStack.push_back(&*line);
				}
			} else {
				if (!foldHeads.count(line->Fold.id)) { 	// Non-matching ender
					// Deactivate it. Because we can, also push it to completedFolds:
					// If its counterpart appears further below, we can delete it right away.
					completedFolds[line->Fold.id] = true;
					line->Fold.exists = false;
				} else {
					// We found a fold. Now we need to see if the stack matches.
					// We scan our stack for the counterpart of the fold.
					// If one exists, we assume all starters above it are invalid.
					// If none exists, we assume this ender is invalid.
					// If none of these assumptions are true, the folds are probably
					// broken beyond repair.

					completedFolds[line->Fold.id] = true;
					bool found = false;
					for (int i = foldStack.size() - 1; i >= 0; i--) {
						if (foldStack[i]->Fold.id == line->Fold.id) {
							// Erase all folds further inward
							for (int j = foldStack.size() - 1; j > i; j--) {
								completedFolds[foldStack[j]->Fold.id] = true;
								foldStack[j]->Fold.exists = false;
								foldStack.pop_back();
							}

							// Sync the found fold and pop the stack
							line->Fold.collapsed = foldStack[i]->Fold.collapsed;
							foldStack.pop_back();

							found = true;
							break;
						}
					}
					if (!found) {
						completedFolds[line->Fold.id] = true;
						line->Fold.exists = false;
					}
				}
			}
		}
	}

	// All remaining lines are invalid
	for (AssDialogue *line : foldStack) {
		line->Fold.exists = false;
	}

	LinkFolds();
}

void FoldController::LinkFolds() {
	std::vector<AssDialogue *> foldStack;
	AssDialogue *lastVisible = nullptr;
	context->ass->Properties.folds.clear();

	maxdepth = 0;

	int visibleRow = 0;
	int highestFolded = 1;
	for (auto line = context->ass->Events.begin(); line != context->ass->Events.end(); line++) {
		line->Fold.parent = foldStack.empty() ? nullptr : foldStack.back();
		line->Fold.nextVisible = nullptr;
		line->Fold.visible = highestFolded > foldStack.size();
		line->Fold.visited = false;
		line->Fold.visibleRow = visibleRow;
		
		if (line->Fold.visible) {
			if (lastVisible != nullptr) {
				lastVisible->Fold.nextVisible = &*line;
			}
			lastVisible = &*line;
			visibleRow++;
		}
		if (line->Fold.exists && !line->Fold.side) {
			foldStack.push_back(&*line);
			if (!line->Fold.collapsed && highestFolded == foldStack.size()) {
				highestFolded++;
			}
			if (foldStack.size() > maxdepth) {
				maxdepth = foldStack.size();
			}
		}
		if (line->Fold.exists && line->Fold.side) {
			context->ass->Properties.folds.push_back(LineFold {
				foldStack.back()->Row,
				line->Row,
				line->Fold.collapsed,
			});

			line->Fold.counterpart = foldStack.back();
			(*foldStack.rbegin())->Fold.counterpart = &*line;

			if (highestFolded >= foldStack.size()) {
				highestFolded = foldStack.size();
			}

			foldStack.pop_back();
		}
	}
}

int FoldController::GetMaxDepth() {
	return maxdepth;
}

bool FoldController::ActionHasFold(AssDialogue& line) { return line.Fold.exists; }

bool FoldController::ActionClearFold(AssDialogue& line) { line.Fold.exists = false; return false; }

bool FoldController::ActionOpenFold(AssDialogue& line) { line.Fold.collapsed = false; return false; }

bool FoldController::ActionCloseFold(AssDialogue& line) { line.Fold.collapsed = true; return false; }

bool FoldController::ActionToggleFold(AssDialogue& line) { line.Fold.collapsed = !line.Fold.collapsed; return false; }


void FoldController::ClearAllFolds() {
	FoldController::DoForAllFolds(FoldController::ActionClearFold);
	context->ass->Commit(_("clear all folds"), AssFile::COMMIT_FOLD);
}

void FoldController::OpenAllFolds() {
	FoldController::DoForAllFolds(FoldController::ActionOpenFold);
	context->ass->Commit(_("open all folds"), AssFile::COMMIT_FOLD);
}

void FoldController::CloseAllFolds() {
	FoldController::DoForAllFolds(FoldController::ActionCloseFold);
	context->ass->Commit(_("close all folds"), AssFile::COMMIT_FOLD);
}

bool FoldController::HasFolds() {
	return FoldController::DoForAllFolds(FoldController::ActionHasFold);
}

void FoldController::ClearFoldsAt(std::vector<AssDialogue *> const& lines) {
	FoldController::DoForFoldsAt(lines, FoldController::ActionClearFold);
	context->ass->Commit(_("clear folds"), AssFile::COMMIT_FOLD);
}

void FoldController::OpenFoldsAt(std::vector<AssDialogue *> const& lines) {
	FoldController::DoForFoldsAt(lines, FoldController::ActionOpenFold);
	context->ass->Commit(_("open folds"), AssFile::COMMIT_FOLD);
}

void FoldController::CloseFoldsAt(std::vector<AssDialogue *> const& lines) {
	FoldController::DoForFoldsAt(lines, FoldController::ActionCloseFold);
	context->ass->Commit(_("close folds"), AssFile::COMMIT_FOLD);
}

void FoldController::ToggleFoldsAt(std::vector<AssDialogue *> const& lines) {
	FoldController::DoForFoldsAt(lines, FoldController::ActionToggleFold);
	context->ass->Commit(_("toggle folds"), AssFile::COMMIT_FOLD);
}

bool FoldController::AreFoldsAt(std::vector<AssDialogue *> const& lines) {
	return FoldController::DoForFoldsAt(lines, FoldController::ActionHasFold);
}
