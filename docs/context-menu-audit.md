# Context menu audit — taskbar-integration (baseline 4214279)

The application identity, not whether a window is running, accounts for the original Chrome/Chamber discrepancy. Chrome advertises new-window; Chamber advertises Appearance Settings. Preserve the PWA and profile identity. Never promise a second window just because the desktop entry can be launched.

## Combined findings and resolution
- Label the application and, when different, the represented window. Idle launchers open the named app; running buttons request a native/known new window, focus the only target or list targets. Use the same policy for Shift+click and launch-on-middle-click.
- All ungrouped sibling buttons and the pin service query the application pin, not the clicked button. Nested launchers use the captured application rather than reconstructing a desktop ID from a basename.
- Close All acts on the pictured snapshot, not windows opened later. Focus/close re-check liveness. Callbacks own app metadata; model rebuild dismisses old menus (including preview menus).
- Use current skip-tasklist state on each event. On window removal repair the top index; Show Window focuses the represented survivor directly.
- When a sibling inherits a pin, preserve its position whichever direction the sibling came from.

## Verification
- make check: identity, grouping, context-action policy. git diff --check passed.
- Independent reviewer rechecked original lifecycle/order findings and two follow-up blockers (nested launchers, stale index): pass by inspection; tests rerun.
- Installed binary matched build. Existing pin configuration unchanged. Live menu of Chamber: Local showed Show Window and Appearance Settings (not a browser New Window).
- Remaining manual regression coverage: native New Window creates window 2; close one of two and use both shortcuts; nested launcher pin through menu + bus; reload entries with menu open; skip-tasklist toggle; pin transfer across another pinned app; new window arriving after Close All menu opened. Policy tests do not substitute for these interactions.
