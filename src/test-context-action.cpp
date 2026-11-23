#include "ContextAction.hpp"
#include <cassert>
int main() {
 using namespace ContextAction;
 assert(choose(true, true, 0) == Open);
 assert(choose(true, false, 0) == Open);
 assert(choose(true, true, 1) == NewWindow);
 assert(choose(true, true, 2) == NewWindow);
 assert(choose(true, false, 1) == ShowWindow);
 assert(choose(true, false, 2) == ListWindows);
 assert(choose(false, true, 1) == ShowWindow);
 assert(choose(false, false, 0) == Unavailable);
}
