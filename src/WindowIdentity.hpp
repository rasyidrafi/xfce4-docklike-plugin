/*
 * Docklike Taskbar - A modern, minimalist taskbar for Xfce
 * Copyright (c) 2019-2024 The Xfce development team
 * gnu.org/licenses/gpl-3.0
 */

#ifndef WINDOW_IDENTITY_HPP
#define WINDOW_IDENTITY_HPP

#include <cstddef>
#include <cstring>

namespace WindowIdentity
{
	inline bool isChromiumBrowserId(const char* id)
	{
		const char* browserIds[] = {"google-chrome", "chromium", "chromium-browser"};
		for (const char* const* browserId = browserIds; browserId != browserIds + 3; ++browserId)
		{
			const char* left = id;
			const char* right = *browserId;
			while (*left != '\0' && *right != '\0')
			{
				char lower = (*left >= 'A' && *left <= 'Z') ? *left + ('a' - 'A') : *left;
				if (lower != *right)
					break;
				++left;
				++right;
			}
			if (*left == '\0' && *right == '\0')
				return true;
		}
		return false;
	}

	inline bool isCrxInstance(const char* id)
	{
		if (id == nullptr || std::strlen(id) != 36 || id[0] != 'c' || id[1] != 'r' || id[2] != 'x' || id[3] != '_')
			return false;

		for (std::size_t i = 4; i < 36; ++i)
			if (id[i] < 'a' || id[i] > 'p')
				return false;

		return true;
	}

	inline const char* preferredClassId(const char* const* classIds)
	{
		if (classIds == nullptr)
			return nullptr;

		bool isChromiumWindow = false;
		for (const char* const* id = classIds; *id != nullptr; ++id)
			if (isChromiumBrowserId(*id))
				isChromiumWindow = true;

		if (isChromiumWindow)
		{
			for (const char* const* id = classIds; *id != nullptr; ++id)
				if (isCrxInstance(*id))
					return *id;
		}

		for (const char* const* id = classIds; *id != nullptr; ++id)
			if ((*id)[0] != '\0')
				return *id;

		return nullptr;
	}
} // namespace WindowIdentity

#endif // WINDOW_IDENTITY_HPP
