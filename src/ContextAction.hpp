#pragma once
namespace ContextAction {
 enum Kind { Open, NewWindow, ShowWindow, ListWindows, Unavailable };
 inline Kind choose(bool launcher, bool supportsNew, unsigned count) {
  if (!count) return launcher ? Open : Unavailable;
  if (launcher && supportsNew) return NewWindow;
  return count == 1 ? ShowWindow : ListWindows;
 }
}
