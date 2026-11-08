/*
 * Docklike Taskbar - A modern, minimalist taskbar for Xfce
 * Copyright (c) 2019-2024 The Xfce development team
 * gnu.org/licenses/gpl-3.0
 */

#ifndef WINDOW_GROUPING_HPP
#define WINDOW_GROUPING_HPP

namespace WindowGrouping
{
	inline bool isIdlePin(bool pinned, bool assignedWindow, unsigned int representedWindows)
	{
		return pinned && !assignedWindow && representedWindows == 0;
	}

	inline bool hasRepresentedWindow(unsigned int representedWindows)
	{
		return representedWindows > 0;
	}

	inline bool shouldTransferPin(bool ungrouped, bool idlePin, bool hasRepresentedTarget)
	{
		return ungrouped && idlePin && hasRepresentedTarget;
	}

} // namespace WindowGrouping

#endif // WINDOW_GROUPING_HPP
