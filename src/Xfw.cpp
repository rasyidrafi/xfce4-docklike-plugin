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

	GtkWidget* buildActionMenu(GroupWindow* groupWindow, Group* group)
	{
		auto menu = gtk_menu_new();
		auto append = [](GtkWidget* parent, const char* label) {
			auto item = gtk_menu_item_new_with_label(label);
			gtk_menu_shell_append(GTK_MENU_SHELL(parent), item);
			return item;
		};
		auto app = group->mAppInfo;
		auto more = groupWindow ? xfw_window_action_menu_new(groupWindow->mXfwWindow) : gtk_menu_new();
		if (!app->path.empty()) {
			auto primary = append(menu, app->supportsNewWindow() ? _("New Window  (Shift+Click)") : _("Open Application"));
			g_signal_connect(primary, "activate", G_CALLBACK(+[](GtkMenuItem*, AppInfo* info) { info->launchAnother(); }), app.get());
			for (auto actions = app->get_actions(); actions && *actions; ++actions) {
				if (g_str_equal(*actions, "new-window")) continue;
				auto label = g_desktop_app_info_get_action_name(app->gAppInfo.get(), *actions);
				auto item = append(menu, label);
				g_free(label);
				g_object_set_data_full(G_OBJECT(item), "action", g_strdup(*actions), g_free);
				g_signal_connect(item, "activate", G_CALLBACK(+[](GtkMenuItem* i, AppInfo* info) {
					info->launch_action(static_cast<const char*>(g_object_get_data(G_OBJECT(i), "action")));
				}), app.get());
			}
			auto pin = append(menu, group->mPinned ? _("Unpin from Taskbar") : _("Pin to Taskbar"));
			g_signal_connect(pin, "activate", G_CALLBACK(+[](GtkMenuItem*, Group* g) { Dock::setPinned(g, !g->mPinned); }), group);
			auto editor = g_find_program_in_path("exo-desktop-item-edit");
			if (editor) {
				g_free(editor);
				auto edit = append(more, _("Edit Launcher"));
				g_signal_connect(edit, "activate", G_CALLBACK(+[](GtkMenuItem*, AppInfo* info) { info->edit(); }), app.get());
			}
		}
		auto windowAction = [&](GtkWidget* parent, const char* label, GroupWindow* window, GCallback callback) {
			auto item = append(parent, label);
			g_object_set_data_full(G_OBJECT(item), "window", g_object_ref(window->mXfwWindow), g_object_unref);
			g_signal_connect(item, "activate", callback, nullptr);
		};
		int count = group->mWindowsCount;
		if (!groupWindow && count > 1) {
			auto windows = gtk_menu_new();
			auto label = g_strdup_printf(_("Windows (%d)"), count);
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(append(menu, label)), windows);
			g_free(label);
			group->mWindows.forEach([&](GroupWindow* w) {
				if (!w->getState(XFW_WINDOW_STATE_SKIP_TASKLIST)) windowAction(windows, xfw_window_get_name(w->mXfwWindow), w, G_CALLBACK(+[](GtkMenuItem* i, gpointer) {
					auto live = mGroupWindows.get(XFW_WINDOW(g_object_get_data(G_OBJECT(i), "window")));
					if (live) live->activate(gtk_get_current_event_time());
				}));
			});
		}
		auto children = gtk_container_get_children(GTK_CONTAINER(more));
		if (children) gtk_menu_item_set_submenu(GTK_MENU_ITEM(append(menu, _("More Options"))), more);
		else { g_object_ref_sink(more); g_object_unref(more); }
		g_list_free(children);
		if (groupWindow) windowAction(menu, _("Close This Window"), groupWindow, G_CALLBACK(+[](GtkMenuItem* i, gpointer) {
			auto live = mGroupWindows.get(XFW_WINDOW(g_object_get_data(G_OBJECT(i), "window")));
			if (live) close(live.get(), gtk_get_current_event_time());
		}));
		else if (count > 1) {
			auto label = g_strdup_printf(_("Close All %d Windows"), count);
			auto item = append(menu, label);
			g_free(label);
			g_signal_connect(item, "activate", G_CALLBACK(+[](GtkMenuItem*, Group* g) { g->closeAll(); }), group);
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
