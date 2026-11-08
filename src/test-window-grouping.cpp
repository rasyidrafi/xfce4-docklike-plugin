#include "WindowGrouping.hpp"

int main()
{
	// A pinned slot belongs to a window only while it is represented. Filtered
	// windows release assignment, leaving an idle pin; hidden siblings do not
	// take the pin because they have no represented windows.
	const bool pinAfterFilteringOwner = WindowGrouping::isIdlePin(true, false, 0);
	const bool hiddenSiblingIsTarget = WindowGrouping::hasRepresentedWindow(0);
	if (WindowGrouping::shouldTransferPin(true, pinAfterFilteringOwner, hiddenSiblingIsTarget)) return 1;
	if (!WindowGrouping::isIdlePin(true, false, 0)) return 6;
	if (WindowGrouping::isIdlePin(true, false, 1)) return 7;

	// When a sibling becomes represented, the idle pin transfers to its button.
	const bool visibleSiblingIsTarget = WindowGrouping::hasRepresentedWindow(1);
	if (!WindowGrouping::shouldTransferPin(true, pinAfterFilteringOwner, visibleSiblingIsTarget)) return 2;

	// Pin transfer remains specific to ungrouped mode.
	if (WindowGrouping::shouldTransferPin(false, pinAfterFilteringOwner, visibleSiblingIsTarget)) return 3;
	if (WindowGrouping::isIdlePin(false, false, 0)) return 4;
	if (WindowGrouping::isIdlePin(true, true, 0)) return 5;
	if (WindowGrouping::isIdlePin(true, false, 1)) return 8;
	return 0;
}
