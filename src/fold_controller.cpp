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

#include "fold_controller.h"

#include "ass_file.h"
#include "include/aegisub/context.h"
#include "format.h"
#include "subs_controller.h"

#include <algorithm>
#include <unordered_map>

#include <libaegisub/log.h>
#include <libaegisub/split.h>
#include <libaegisub/util.h>

const char *folds_key = "_aegi_folddata";

FoldController::FoldController(agi::Context *c)
: context(c)
, pre_commit_listener(c->ass->AddPreCommitListener(&FoldController::FixFoldsPreCommit, this))
{ }


bool FoldController::CanAddFold(AssDialogue& start, AssDialogue& end) {
	if (start.Fold.valid || end.Fold.valid) {
		return false;
	}
	int folddepth = 0;
	for (auto it = std::next(context->ass->Events.begin(), start.Row); it->Row < end.Row; it++) {
		if (it->Fold.valid) {
			folddepth += it->Fold.side ? -1 : 1;
		}
		if (folddepth < 0) {
			return false;
		}
	}
	return folddepth == 0;
}

void FoldController::RawAddFold(AssDialogue& start, AssDialogue& end, bool collapsed) {
	int id = ++max_fold_id;
	context->ass->SetExtradataValue(start, folds_key, agi::format("0;%d;%d", int(collapsed), id));
	context->ass->SetExtradataValue(end, folds_key, agi::format("1;%d;%d", int(collapsed), id));
}

void FoldController::UpdateLineExtradata(AssDialogue &line) {
	if (line.Fold.extraExists)
		context->ass->SetExtradataValue(line, folds_key, agi::format("%d;%d;%d", int(line.Fold.side), int(line.Fold.collapsed), int(line.Fold.id)));
	else
		context->ass->DeleteExtradataValue(line, folds_key);
}

void FoldController::InvalidateLineFold(AssDialogue &line) {
	line.Fold.valid = false;
	if (++line.Fold.invalidCount > 100) {
		line.Fold.extraExists = false;
		UpdateLineExtradata(line);
	}
}

void FoldController::AddFold(AssDialogue& start, AssDialogue& end, bool collapsed) {
	if (CanAddFold(start, end)) {
		RawAddFold(start, end, true);
		context->ass->Commit(_("add fold"), AssFile::COMMIT_FOLD);
	}
}

bool FoldController::DoForAllFolds(bool action(AssDialogue& line)) {
	for (AssDialogue& line : context->ass->Events) {
		if (line.Fold.valid) {
			bool result = action(line);
			UpdateLineExtradata(line);
			if (result)
				return true;
		}
	}
	return false;
}

void FoldController::FixFoldsPreCommit(int type, const AssDialogue *single_line) {
	if ((type & (AssFile::COMMIT_FOLD | AssFile::COMMIT_DIAG_ADDREM | AssFile::COMMIT_ORDER)) || type == AssFile::COMMIT_NEW) {
		UpdateFoldInfo();
	}
}

// For each line in lines, applies action() to the opening delimiter of the innermost fold containing this line.
// Returns true as soon as any action() call returned true.
//
// In general, this can leave the folds in an inconsistent state, so unless action() is read-only this should always
// be followed by a commit.
bool FoldController::DoForFoldsAt(std::vector<AssDialogue *> const& lines, bool action(AssDialogue& line)) {
	for (AssDialogue *line : lines) {
		if (line->Fold.parent != nullptr && !(line->Fold.valid && !line->Fold.side)) {
			line = line->Fold.parent;
		}
		if (!line->Fold.visited) {
			bool result = action(*line);
			UpdateLineExtradata(*line);
			if (result)
				return true;
		}
		line->Fold.visited = true;
	}
	return false;
}

void FoldController::UpdateFoldInfo() {
	ReadFromExtradata();
	FixFolds();
	LinkFolds();
}

void FoldController::ReadFromExtradata() {
	max_fold_id = 0;

	for (auto line = context->ass->Events.begin(); line != context->ass->Events.end(); line++) {
		line->Fold.extraExists = false;

		for (auto const& extra : context->ass->GetExtradata(line->ExtradataIds)) {
			if (extra.key == folds_key) {
				std::vector<std::string> fields;
				agi::Split(fields, extra.value, ';');
				if (fields.size() != 3)
					break;

				int side;
				int collapsed;
				if (!agi::util::try_parse(fields[0], &side)) break;
				if (!agi::util::try_parse(fields[1], &collapsed)) break;
				if (!agi::util::try_parse(fields[2], &line->Fold.id)) break;
				line->Fold.side = side;
				line->Fold.collapsed = collapsed;

				line->Fold.extraExists = true;
				max_fold_id = std::max(max_fold_id, line->Fold.id);
				break;
			}
		}
		line->Fold.valid = line->Fold.extraExists;
	}
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

	// Map iteratively applied to all id's.
	// Once some fold has been completely found, subsequent markers found with the same id will be mapped to this new id.
	std::unordered_map<int, int> idRemap;

	for (auto line = context->ass->Events.begin(); line != context->ass->Events.end(); line++) {
		if (line->Fold.extraExists) {
			bool needs_update = false;

			while (idRemap.count(line->Fold.id)) {
				LOG_D("fold") << "remapping " << line->Fold.id << " to " << idRemap[line->Fold.id];
				line->Fold.id = idRemap[line->Fold.id];
				needs_update = true;
			}

			if (completedFolds.count(line->Fold.id)) { 	// Duplicate entry - try to start a new one
				LOG_D("fold") << "creating remap: " << line->Fold.id << " to " << max_fold_id + 1;
				idRemap[line->Fold.id] = ++max_fold_id;
				line->Fold.id = idRemap[line->Fold.id];
				needs_update = true;
			}
			LOG_D("fold") << "done remapping";

			if (!line->Fold.side) {
				if (foldHeads.count(line->Fold.id)) { 	// Duplicate entry
					InvalidateLineFold(*line);
				} else {
					foldHeads[line->Fold.id] = &*line;
					foldStack.push_back(&*line);
				}
			} else {
				if (!foldHeads.count(line->Fold.id)) { 	// Non-matching ender
					// Deactivate it. Because we can, also push it to completedFolds:
					// If its counterpart appears further below, we can delete it right away.
					completedFolds[line->Fold.id] = true;
					InvalidateLineFold(*line);
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
								InvalidateLineFold(*foldStack[j]);
								foldStack.pop_back();
							}

							// Sync the found fold and pop the stack
							if (line->Fold.collapsed != foldStack[i]->Fold.collapsed) {
								line->Fold.collapsed = foldStack[i]->Fold.collapsed;
								needs_update = true;
							}
							foldStack.pop_back();

							found = true;
							break;
						}
					}
					if (!found) {
						completedFolds[line->Fold.id] = true;
						InvalidateLineFold(*line);
					}
				}
			}

			if (needs_update) {
				UpdateLineExtradata(*line);
			}
		}
	}

	// All remaining lines are invalid
	for (AssDialogue *line : foldStack) {
		line->Fold.valid = false;
		if (++line->Fold.invalidCount > 100) {
			line->Fold.extraExists = false;
			UpdateLineExtradata(*line);
		}
	}
}

void FoldController::LinkFolds() {
	std::vector<AssDialogue *> foldStack;
	AssDialogue *lastVisible = nullptr;

	maxdepth = 0;

	int visibleRow = 0;
	int highestFolded = 1;
	for (auto line = context->ass->Events.begin(); line != context->ass->Events.end(); line++) {
		line->Fold.parent = foldStack.empty() ? nullptr : foldStack.back();
		line->Fold.nextVisible = nullptr;
		line->Fold.visible = highestFolded > (int) foldStack.size();
		line->Fold.visited = false;
		line->Fold.visibleRow = visibleRow;

		if (line->Fold.visible) {
			if (lastVisible != nullptr) {
				lastVisible->Fold.nextVisible = &*line;
			}
			lastVisible = &*line;
			visibleRow++;
		}
		if (line->Fold.valid && !line->Fold.side) {
			foldStack.push_back(&*line);
			if (!line->Fold.collapsed && highestFolded == (int) foldStack.size()) {
				highestFolded++;
			}
			if ((int) foldStack.size() > maxdepth) {
				maxdepth = foldStack.size();
			}
		}
		if (line->Fold.valid && line->Fold.side) {
			line->Fold.counterpart = foldStack.back();
			(*foldStack.rbegin())->Fold.counterpart = &*line;

			if (highestFolded >= (int) foldStack.size()) {
				highestFolded = foldStack.size();
			}

			foldStack.pop_back();
		}
	}
}

int FoldController::GetMaxDepth() {
	return maxdepth;
}

bool FoldController::ActionHasFold(AssDialogue& line) { return line.Fold.valid; }

bool FoldController::ActionClearFold(AssDialogue& line) { line.Fold.extraExists = false; line.Fold.valid = false; return false; }

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
