/*=======================================================================
*
*   Filename:    Layout.h
*   Purpose:     Lane picking for danmaku placement.
*
*   Extracted from Running.cpp so the sparse-screen fix (HANDOVER 6-45)
*   can be unit-tested without the renderer / player service locator.
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
=========================================================================*/

#pragma once

namespace Layout
{
	// Choose the least crowded lane among `count` candidates.
	//
	// thick[i] counts the on-screen danmaku that would collide with lane i.
	// Ties used to be broken by std::min_element, which always returns the
	// FIRST minimum. locate() enumerates lanes top-down (y = 0, 10, 20...),
	// and sparse danmaku never collide with each other, so thick stayed
	// all-zero and every danmaku landed in the top few lanes: the middle of
	// the screen had nothing to hover-freeze (HANDOVER 6-45).
	//
	// Equal thick means equal collision cost, so rotating the tie-break is
	// free: start at `cursor`, take the first lane at the minimum cost, then
	// advance the cursor. With a single least-crowded lane the result is
	// identical to the old behaviour, so dense scenes are unaffected.
	// `cursor` belongs to the caller, which must only touch it under its
	// write lock (Process::run holds it while placing a batch).
	inline int chooseLane(const int *thick, int count, int &cursor)
	{
		if (count <= 0) {
			return 0;
		}
		int best = thick[0];
		for (int i = 1; i < count; ++i) {
			if (thick[i] < best) {
				best = thick[i];
			}
		}
		int start = cursor % count;
		if (start < 0) {
			start += count;
		}
		for (int k = 0; k < count; ++k) {
			int idx = (start + k) % count;
			if (thick[idx] == best) {
				cursor = (idx + 1) % count;
				return idx;
			}
		}
		return 0; // unreachable: `best` came from one of the lanes
	}
}
