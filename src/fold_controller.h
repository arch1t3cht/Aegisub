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

#pragma once

#include <libaegisub/signal.h>
#include "ass_file.h"

#include <vector>

namespace agi { struct Context; }

extern const char *folds_key;

/// We allow hiding ass lines using cascading folds, each of which collapses a contiguous collection of dialogue lines into a single one.
/// A fold is described by inclusive start and end points of the contiguous set of dialogue line it extends over.
/// An existing fold can be active (collapsed) or inactive (existing, but not collapsed at the moment)
/// A fold may *strictly* contain other folds or be *strictly* contained in other folds, but it may not intersect another fold with
/// an intersection set not equal to one of the two folds.
/// Only one fold may be started or ended at any given line.

/// In order for folds to be preserved while adding or deleting lines and work nicely with operations like copy/paste,
/// they need to be stored as extradata. Furthermore, in order for the subtitle grid and fold management commands to efficiently
/// navigate the folds, we cache some information on the fold after each commit.
///
/// A fold descriptor for a line is an extradata field of the form <direction>;<collapsed>;<id>, where
///     direction is 0 if this line starts a fold, and 1 if the line ends one
///     collapsed is 1 if the fold is collapsed and 0 otherwise
///     id is a unique id pairing this fold with its counterpart

/// Part of the data for an AssDialogue object, describing folds starting or ending at this line.
class FoldInfo {
	// Cached, parsed versions of the contents of the extradata entry

	/// Whether there is some extradata entry on folds here
	bool extraExists = false;
	/// Whether a fold starts or ends at the line. The following three fields are only valid if this is true.
	bool valid = false;
	/// The id
	int id = 0;
	/// Whether the fold is currently collapsed
	bool collapsed = false;
	/// False if a fold is started here, true otherwise.
	bool side = false;


	// Used in DoForFoldsAt to ensure each line is visited only once
	bool visited = false;
	/// Whether the line is currently visible
	bool visible = true;

	/// If exists is true, this is a pointer to the other line with the given fold id
	AssDialogue *counterpart = nullptr;
	/// A pointer to the opener of the innermost fold containing the line, if one exists.
	/// If the line starts a fold, this points to the next bigger fold.
	AssDialogue *parent = nullptr;
	/// If this line is visible, this points to the next visible line, if one exists
	AssDialogue *nextVisible = nullptr;

	/// Increased when there's an extradata entry in here that turned out to be invalid.
	/// Once this hits some threshold, the extradata entry is deleted.
	/// We don't delete it immediately to allow cut/pasting fold delimiters around.
	int invalidCount = 0;

	/// The row number where this line would appear in the subtitle grid. That is, the ordinary
	/// Row value, but with hidden lines skipped.
	/// Out of all AssDialogue lines with the same visibleRow, only the one with the lowest Row is shown.
	int visibleRow = -1;

	friend class FoldController;

public:
	bool hasFold() const { return valid; }
	bool isFolded() const { return collapsed; }
	bool isEnd() const { return side; }

	// The following functions are only valid directly after a commit.
	// Their behaviour is undefined as soon as any uncommitted change is made to the Events.
	AssDialogue *getFoldOpener() const { return parent; }
	AssDialogue *getNextVisible() const { return nextVisible; }
	int getVisibleRow() const { return visibleRow; }
};

#include "ass_dialogue.h"

class FoldController {
	agi::Context *context;
	agi::signal::Connection pre_commit_listener;
	int maxdepth = 0;
	int max_fold_id = 0;

	bool CanAddFold(AssDialogue& start, AssDialogue& end);

	void RawAddFold(AssDialogue& start, AssDialogue& end, bool collapsed);

	bool DoForFoldsAt(std::vector<AssDialogue *> const& lines, bool action(AssDialogue& line));

	bool DoForAllFolds(bool action(AssDialogue& line));

	void FixFoldsPreCommit(int type, const AssDialogue *single_line);

	// These are used for the DoForAllFolds action and should not be used as ordinary getters/setters

	static bool ActionHasFold(AssDialogue& line);

	static bool ActionClearFold(AssDialogue& line);

	static bool ActionOpenFold(AssDialogue& line);

	static bool ActionCloseFold(AssDialogue& line);

	static bool ActionToggleFold(AssDialogue& line);

	/// Updates the line's extradata entry from the values in FoldInfo. Used after actions like toggling folds.
	void UpdateLineExtradata(AssDialogue& line);

	/// Sets valid = false and increases the invalidCounter, deleting the extradata if necessary
	void InvalidateLineFold(AssDialogue &line);

	/// After lines have been added or deleted, this ensures consistency again. Run with every relevant commit.
	/// Performs the three actions below in order.
	void UpdateFoldInfo();

	/// Parses the extradata of all lines and sets the respective lines in the FoldInfo.
	/// Also deduplicates extradata entries and mangles fold id's when necessary.
	void ReadFromExtradata();

	/// Ensures consistency by making sure every fold has two delimiters and folds are properly nested.
	/// Cleans up extradata entries if they've been invalid for long enough.
	void FixFolds();

	/// Once the fold base data is valid, sets up all the cached links in the FoldData.
	void LinkFolds();

public:
	FoldController(agi::Context *context);

	int GetMaxDepth();

	// All of the following functions are only valid directly after a commit.
	// Their behaviour is undefined as soon as any uncommitted change is made to the Events.

	/// @brief Add a new fold
	///
	/// The new fold must not intersect with any existing fold.
	///
	/// Calling this method should only cause a commit if the fold was
	/// successfully added.
	void AddFold(AssDialogue& start, AssDialogue& end, bool collapsed);

	void ClearAllFolds();

	void OpenAllFolds();

	void CloseAllFolds();

	bool HasFolds();

	/// @brief Remove the folds in which the given lines are contained, if they exist
	/// @param lines The lines whose folds should be removed
	void ClearFoldsAt(std::vector<AssDialogue *> const& lines);

	/// @brief Open the folds in which the given lines are contained, if they exist
	/// @param lines The lines whose folds should be opened
	void OpenFoldsAt(std::vector<AssDialogue *> const& lines);

	/// @brief Open or closes the folds in which the given lines are contained, if they exist
	/// @param lines The lines whose folds should be opened
	void ToggleFoldsAt(std::vector<AssDialogue *> const& lines);

	/// @brief Close the folds in which the given lines are contained, if they exist
	/// @param lines The lines whose folds should be closed
	void CloseFoldsAt(std::vector<AssDialogue *> const& lines);

	/// @brief Returns whether any of the given lines are contained in folds
	/// @param lines The lines
	bool AreFoldsAt(std::vector<AssDialogue *> const& lines);

};
