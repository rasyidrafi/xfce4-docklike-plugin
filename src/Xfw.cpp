/*
 * Docklike Taskbar - A modern, minimalist taskbar for Xfce
 * Copyright (c) 2019-2020 Nicolas Szabo <nszabo@vivaldi.net>
 * Copyright (c) 2020-2021 David Keogh <davidtkeogh@gmail.com>
 * gnu.org/licenses/gpl-3.0
 */

#ifdef ENABLE_X11
#include <gdk/gdkx.h>
#endif

#include "Xfw.hpp"
#include "WindowIdentity.hpp"
#include "ContextAction.hpp"

namespace Xfw
{
	XfwScreen* mXfwScreen;
	XfwWorkspaceGroup* mXfwWorkspaceGroup;
	Store::KeyStore<XfwWindow*, std::shared_ptr<GroupWindow>> mGroupWindows;

	namespace // private:
	{
		std::string getGroupNameSys(XfwWindow* xfwWindow)
		{
			// Xfw method const char *
			const gchar* const* class_ids = xfw_window_get_class_ids(xfwWindow);
			const gchar* class_id = WindowIdentity::preferredClassId(class_ids);
			if (class_id != nullptr)
				return class_id;

			// proc/{pid}/cmdline method
			XfwApplicationInstance* instance = xfw_application_get_instance(xfw_window_get_application(xfwWindow), xfwWindow);
			char buffer[512];
			std::string path = "/proc/" + std::to_string(xfw_application_instance_get_pid(instance)) + "/cmdline";
			int fd = open(path.c_str(), O_RDONLY);

			if (fd >= 0)
			{
				int nbr = read(fd, buffer, 512);
				::close(fd);

				char* exe = g_path_get_basename(buffer);
				std::string exe_out = exe;
				g_free(exe);
				if (exe_out != "python") // ADDIT graphical interpreters here
					return exe_out;

				char* it = buffer;
				while (*it++)
					;

				if (it < buffer + nbr)
				{
					gchar* basename = g_path_get_basename(it);
					std::string basename_out = basename;
					g_free(basename);
					return basename_out;
				}
			}

			// fallback : return window's name
			return xfw_window_get_name(xfwWindow);
		}
	} // namespace

	// public:

	void init()
	{
		xfw_set_client_type(XFW_CLIENT_TYPE_PAGER);
		mXfwScreen = xfw_screen_get_default();

		g_signal_connect(G_OBJECT(mXfwScreen), "window-opened",
			G_CALLBACK(+[](XfwScreen* screen, XfwWindow* xfwWindow) {
				std::shared_ptr<GroupWindow> newWindow = std::make_shared<GroupWindow>(xfwWindow);
				mGroupWindows.pushSecond(xfwWindow, newWindow);
				newWindow->mGroup->updateStyle();

				if (Settings::showPreviews && newWindow->mGroup->mGroupMenu.mVisible)
					newWindow->mGroupMenuItem->mPreviewTimeout.start();
			}),
			nullptr);

		g_signal_connect(G_OBJECT(mXfwScreen), "window-closed",
			G_CALLBACK(+[](XfwScreen* screen, XfwWindow* xfwWindow) {
				std::shared_ptr<GroupWindow> groupWindow = mGroupWindows.pop(xfwWindow);
			}),
			nullptr);

		g_signal_connect(G_OBJECT(mXfwScreen), "active-window-changed",
			G_CALLBACK(+[](XfwScreen* screen, XfwWindow* previousActiveWindow) {
				XfwWindow* activeXfwWindow = getActiveWindow();
				if (activeXfwWindow != nullptr)
				{
					std::shared_ptr<GroupWindow> activeWindow = mGroupWindows.get(activeXfwWindow);
					Help::Gtk::cssClassAdd(GTK_WIDGET(activeWindow->mGroupMenuItem->mItem), "active_menu_item");
					gtk_widget_queue_draw(activeWindow->mGroup->mButton);
				}
				if (previousActiveWindow != nullptr)
				{
					std::shared_ptr<GroupWindow> prevWindow = mGroupWindows.get(previousActiveWindow);
					if (prevWindow)
					{
						Help::Gtk::cssClassRemove(GTK_WIDGET(prevWindow->mGroupMenuItem->mItem), "active_menu_item");
						gtk_widget_queue_draw(prevWindow->mGroup->mButton);
					}
				}
				setActiveWindow();
			}),
			nullptr);

		XfwWorkspaceManager* wpManager = xfw_screen_get_workspace_manager(Xfw::mXfwScreen);
		mXfwWorkspaceGroup = XFW_WORKSPACE_GROUP(xfw_workspace_manager_list_workspace_groups(wpManager)->data);
		g_signal_connect(G_OBJECT(mXfwWorkspaceGroup), "active-workspace-changed",
			G_CALLBACK(+[](XfwScreen* screen, XfwWindow* xfwWindow) {
				setVisibleGroups();
			}),
			nullptr);
	}

	void finalize()
	{
		mGroupWindows.clear();
		g_signal_handlers_disconnect_by_data(mXfwWorkspaceGroup, nullptr);
		g_signal_handlers_disconnect_by_data(mXfwScreen, nullptr);
		g_object_unref(mXfwScreen);
	}

	XfwWindow* getActiveWindow()
	{
		return xfw_screen_get_active_window(mXfwScreen);
	}

	std::string getGroupName(GroupWindow* groupWindow)
	{
		return Help::String::toLowercase(getGroupNameSys(groupWindow->mXfwWindow));
	}

	void activate(GroupWindow* groupWindow, guint32 timestamp)
	{
#ifdef ENABLE_X11
		if (!timestamp && GDK_IS_X11_DISPLAY(gdk_display_get_default()))
			timestamp = gdk_x11_get_server_time(gdk_get_default_root_window());
#endif

		XfwWorkspace* workspace = xfw_window_get_workspace(groupWindow->mXfwWindow);
		if (workspace != nullptr)
			xfw_workspace_activate(workspace, NULL);

		xfw_window_activate(groupWindow->mXfwWindow, NULL, timestamp, NULL);
	}

	void close(GroupWindow* groupWindow, guint32 timestamp)
	{
#ifdef ENABLE_X11
		if (!timestamp && GDK_IS_X11_DISPLAY(gdk_display_get_default()))
			timestamp = gdk_x11_get_server_time(gdk_get_default_root_window());
#endif

		xfw_window_close(groupWindow->mXfwWindow, timestamp, NULL);
	}

	void setActiveWindow()
	{
		XfwWindow* activeWindow = getActiveWindow();
		if (mGroupWindows.size() > 0)
			mGroupWindows.first()->onUnactivate();
		if (activeWindow != nullptr)
			mGroupWindows.moveToStart(activeWindow)->onActivate();
	}

	void setVisibleGroups()
	{
		for (GList* window_l = xfw_screen_get_windows(mXfwScreen);
			 window_l != nullptr;
			 window_l = window_l->next)
		{
			XfwWindow* xfwWindow = XFW_WINDOW(window_l->data);
			std::shared_ptr<GroupWindow> groupWindow = mGroupWindows.get(xfwWindow);

			groupWindow->leaveGroup();
			groupWindow->updateState();
		}
	}

	static std::list<GtkWidget*> actionMenus;
	void dismissActionMenus()
	{
		while (!actionMenus.empty()) {
			auto menu = actionMenus.front();
			actionMenus.pop_front();
			gtk_menu_popdown(GTK_MENU(menu));
		}
	}

	GtkWidget* buildActionMenu(GroupWindow* groupWindow, Group* group)
	{
		auto menu = gtk_menu_new();
		actionMenus.push_back(menu);
		g_object_weak_ref(G_OBJECT(menu), +[](gpointer, GObject* object) { actionMenus.remove(reinterpret_cast<GtkWidget*>(object)); }, nullptr);
		auto append = [](GtkWidget* parent, const char* label) {
			auto item = gtk_menu_item_new_with_label(label);
			gtk_menu_shell_append(GTK_MENU_SHELL(parent), item);
			return item;
		};
		using Callback = std::function<void()>;
		auto action = [&](GtkWidget* parent, const char* label, Callback fn) {
			auto item = append(parent, label);
			g_signal_connect_data(item, "activate", G_CALLBACK(+[](GtkMenuItem*, Callback* f) { (*f)(); }),
				new Callback(std::move(fn)), +[](gpointer data, GClosure*) { delete static_cast<Callback*>(data); }, GConnectFlags(0));
		};
		auto app = group->mAppInfo;
		using Window = std::shared_ptr<XfwWindow>;
		auto retain = [](XfwWindow* w) { return Window(XFW_WINDOW(g_object_ref(w)), g_object_unref); };
		std::vector<Window> targets;
		if (groupWindow) targets.push_back(retain(groupWindow->mXfwWindow));
		else group->mWindows.forEach([&](GroupWindow* w) {
			if (!w->getState(XFW_WINDOW_STATE_SKIP_TASKLIST)) targets.push_back(retain(w->mXfwWindow));
		});
		auto focus = [](Window w) { auto live = mGroupWindows.get(w.get()); if (live) live->activate(gtk_get_current_event_time()); };
		auto dismiss = [](Window w) { auto live = mGroupWindows.get(w.get()); if (live) close(live.get(), gtk_get_current_event_time()); };
		gtk_widget_set_sensitive(append(menu, app->name.c_str()), FALSE);
		if (groupWindow) {
			auto title = xfw_window_get_name(groupWindow->mXfwWindow);
			if (title && *title && app->name != title) gtk_widget_set_sensitive(append(menu, title), FALSE);
		}
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
		auto primary = ContextAction::choose(!app->path.empty(), app->supportsNewWindow(), targets.size());
		if (primary == ContextAction::Open) {
			auto label = g_strdup_printf(_("Open %s"), app->name.c_str());
			action(menu, label, [app] { app->launch(); }); g_free(label);
		} else if (primary == ContextAction::NewWindow) action(menu, _("New Window  (Shift+Click)"), [app] { app->launchAnother(); });
		else if (primary == ContextAction::ShowWindow) action(menu, _("Show Window"), [focus, targets] { focus(targets.front()); });
		if (targets.size() > 1) {
			auto windows = gtk_menu_new();
			auto label = g_strdup_printf(_("Windows (%u)"), unsigned(targets.size()));
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(append(menu, label)), windows); g_free(label);
			for (auto w : targets) action(windows, xfw_window_get_name(w.get()), [focus, w] { focus(w); });
		}
		auto more = groupWindow ? xfw_window_action_menu_new(groupWindow->mXfwWindow) : gtk_menu_new();
		if (!app->path.empty()) {
			for (auto actions = app->get_actions(); actions && *actions; ++actions) {
				std::string id = *actions;
				if (id == "new-window") continue;
				auto label = g_desktop_app_info_get_action_name(app->gAppInfo.get(), id.c_str());
				action(menu, label, [app, id] { app->launch_action(id.c_str()); }); g_free(label);
			}
			bool desired = !Dock::pinnedGroup(app);
			action(menu, desired ? _("Pin to Taskbar") : _("Unpin from Taskbar"), [app, desired] {
				Dock::setAppPinned(app, desired);
			});
			auto editor = g_find_program_in_path("exo-desktop-item-edit");
			if (editor) { g_free(editor); action(more, _("Edit Launcher"), [app] { app->edit(); }); }
		}
		auto children = gtk_container_get_children(GTK_CONTAINER(more));
		if (children) gtk_menu_item_set_submenu(GTK_MENU_ITEM(append(menu, _("More Options"))), more);
		else { g_object_ref_sink(more); g_object_unref(more); }
		g_list_free(children);
		if (targets.size() == 1) action(menu, _("Close This Window"), [dismiss, targets] { dismiss(targets.front()); });
		else if (targets.size() > 1) {
			auto label = g_strdup_printf(_("Close All %u Windows"), unsigned(targets.size()));
			action(menu, label, [dismiss, targets] { for (const auto& w : targets) dismiss(w); }); g_free(label);
		}
		gtk_widget_show_all(menu);
		return menu;
	}

	void switchToLastWindow(guint32 timestamp)
	{
		auto it = mGroupWindows.mList.begin();

		while (it != mGroupWindows.mList.end() && it->second->getState(XFW_WINDOW_STATE_SKIP_TASKLIST))
			++it; // skip dialogs
		if (it != mGroupWindows.mList.end())
			++it; // skip current window

		while (it != mGroupWindows.mList.end())
		{
			if (!it->second->getState(XFW_WINDOW_STATE_SKIP_TASKLIST))
			{
				it->second->activate(timestamp);
				return;
			}
			++it;
		}
	}
} // namespace Xfw
