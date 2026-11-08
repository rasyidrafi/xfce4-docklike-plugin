#include "WindowIdentity.hpp"

int main()
{
	const char* appInstance = "crx_abcdefghijklmnopabcdefghijklmnop";
	const char* browserClass = "Google-chrome";
	const char* badInstance = "crx_abcdefghijklmnopabcdefghijklmnopq";
	const char* badCharacters = "crx_abcdefghijklmnopabcdefghijklmnox";
	const char* const ids[] = {browserClass, appInstance, nullptr};
	const char* const emptyIds[] = {"", nullptr};

	if (!WindowIdentity::isCrxInstance(appInstance)) return 1;
	if (WindowIdentity::isCrxInstance("crx_")) return 2;
	if (WindowIdentity::isCrxInstance(badInstance)) return 3;
	if (WindowIdentity::isCrxInstance(badCharacters)) return 4;
	if (WindowIdentity::preferredClassId(ids) != appInstance) return 5;
	if (WindowIdentity::preferredClassId(emptyIds) != nullptr) return 6;
	if (WindowIdentity::preferredClassId(nullptr) != nullptr) return 7;

	const char* const ordinaryIds[] = {"", browserClass, nullptr};
	if (WindowIdentity::preferredClassId(ordinaryIds) != browserClass) return 8;
	const char* const unrelatedIds[] = {"ordinary", appInstance, "Firefox", nullptr};
	if (WindowIdentity::preferredClassId(unrelatedIds) != unrelatedIds[0]) return 9;
	const char* const mixedCaseBrowserIds[] = {appInstance, "Chromium-Browser", nullptr};
	if (WindowIdentity::preferredClassId(mixedCaseBrowserIds) != appInstance) return 10;
	return 0;
}
