/*
 * Docklike Taskbar - A modern, minimalist taskbar for Xfce
 * Copyright (c) 2019-2020 Nicolas Szabo <nszabo@vivaldi.net>
 * Copyright (c) 2020-2021 David Keogh <davidtkeogh@gmail.com>
 * gnu.org/licenses/gpl-3.0
 */

#include "Dock.hpp"
#include "WindowGrouping.hpp"
#ifdef ENABLE_X11
#include "Hotkeys.hpp"
#endif

#include <algorithm>

namespace Dock
{
	GtkWidget* mBox;
	Store::KeyStore<std::shared_ptr<AppInfo>, std::shared_ptr<Group>> mGroups;
	bool mDrawingGroups = false;

	int mPanelSize;
	int mIconSize;

	void init()
	{
		mBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_widget_set_name(mBox, "docklike-plugin");

		if (Settings::dockSize)
			gtk_widget_set_size_request(mBox, Settings::dockSize, -1);

		gtk_widget_show(mBox);
		drawGroups();
	}

	Group* prepareGroup(std::shared_ptr<AppInfo> appInfo)
	{
		std::shared_ptr<Group> group = mGroups.get(appInfo);

		if (!group)
		{
			group = std::make_shared<Group>(appInfo, false);
			mGroups.push(appInfo, group);
			gtk_container_add(GTK_CONTAINER(mBox), group->mButton);
		}

		return group.get();
	}

	Group* prepareWindowGroup(std::shared_ptr<AppInfo> appInfo, GroupWindow* window)
	{
		Group* group = mGroups.findIf([&appInfo, window](std::pair<std::shared_ptr<AppInfo>, std::shared_ptr<Group>> entry) -> bool {
			return entry.first == appInfo && entry.second->mAssignedWindow == window;
		}).get();
		if (group != nullptr)
			return group;

		// In ungrouped mode, consume an idle pin to represent the first window.
		// Further windows get their own group/button.
		group = mGroups.findIf([&appInfo](std::pair<std::shared_ptr<AppInfo>, std::shared_ptr<Group>> entry) -> bool {
			return entry.first == appInfo && entry.second->mPinned && entry.second->mAssignedWindow == nullptr;
		}).get();
		if (group != nullptr)
		{
			group->mAssignedWindow = window;
			return group;
		}

		// Reuse an empty window slot where possible; no two live windows share it.
		group = mGroups.findIf([&appInfo](std::pair<std::shared_ptr<AppInfo>, std::shared_ptr<Group>> entry) -> bool {
			return entry.first == appInfo && !entry.second->mPinned && entry.second->mAssignedWindow == nullptr;
		}).get();
		if (group != nullptr)
		{
			group->mAssignedWindow = window;
			return group;
		}

		std::shared_ptr<Group> newGroup = std::make_shared<Group>(appInfo, false);
		mGroups.push(appInfo, newGroup);
		gtk_container_add(GTK_CONTAINER(mBox), newGroup->mButton);
		newGroup->mAssignedWindow = window;
		return newGroup.get();
	}

	void releaseWindowGroup(Group* group, GroupWindow* window)
	{
		if (group->mAssignedWindow == window)
			group->mAssignedWindow = nullptr;
	}

	void setPinned(Group* group, bool pinned)
	{
		if (Settings::ungroupWindows && pinned)
		{
			mGroups.forEach([group](std::pair<std::shared_ptr<AppInfo>, std::shared_ptr<Group>> entry) -> void {
				if (entry.first == group->mAppInfo && entry.second.get() != group && entry.second->mPinned)
				{
					entry.second->mPinned = false;
					entry.second->updateStyle();
				}
			});
		}

		group->mPinned = pinned;
		group->updateStyle();
		savePinned();
	}

	void transferPin(Group* group)
	{
		if (mDrawingGroups || !Settings::ungroupWindows)
			return;

		Group* pin = mGroups.findIf([group](std::pair<std::shared_ptr<AppInfo>, std::shared_ptr<Group>> entry) -> bool {
			return entry.first == group->mAppInfo && WindowGrouping::isIdlePin(entry.second->mPinned,
				entry.second->mAssignedWindow != nullptr, entry.second->mWindowsCount);
		}).get();
		if (pin == nullptr)
			return;

		Group* next = mGroups.findIf([group, pin](std::pair<std::shared_ptr<AppInfo>, std::shared_ptr<Group>> entry) -> bool {
			return entry.first == group->mAppInfo && entry.second.get() != pin &&
				WindowGrouping::hasRepresentedWindow(entry.second->mWindowsCount);
		}).get();

		if (!WindowGrouping::isIdlePin(pin->mPinned, pin->mAssignedWindow != nullptr, pin->mWindowsCount))
			return;

		if (WindowGrouping::shouldTransferPin(Settings::ungroupWindows,
			WindowGrouping::isIdlePin(pin->mPinned, pin->mAssignedWindow != nullptr, pin->mWindowsCount), next != nullptr))
		{
			int pinPosition = Help::Gtk::getChildPosition(GTK_CONTAINER(mBox), pin->mButton);
			int nextPosition = Help::Gtk::getChildPosition(GTK_CONTAINER(mBox), next->mButton);
			pin->mPinned = false;
			pin->updateStyle();
			next->mPinned = true;
			if (pinPosition < nextPosition)
				gtk_box_reorder_child(GTK_BOX(mBox), next->mButton, pinPosition);
			next->updateStyle();
			savePinned();
		}
	}

	void moveButton(Group* moving, Group* dest)
	{
		int startpos = Help::Gtk::getChildPosition(GTK_CONTAINER(mBox), moving->mButton);
		int destpos = Help::Gtk::getChildPosition(GTK_CONTAINER(mBox), dest->mButton);

		if (startpos == destpos)
			return;

		gtk_box_reorder_child(GTK_BOX(mBox), moving->mButton, destpos);

		savePinned();
	}

	void savePinned()
	{
		std::list<std::string> pinnedList;
		GList* children = gtk_container_get_children(GTK_CONTAINER(mBox));

		for (GList* child = children; child != nullptr; child = child->next)
		{
			GtkWidget* widget = (GtkWidget*)child->data;
			Group* group = (Group*)g_object_get_data(G_OBJECT(widget), "group");

			if (group->mPinned && g_file_test(group->mAppInfo->path.c_str(), G_FILE_TEST_IS_REGULAR) &&
				std::find(pinnedList.begin(), pinnedList.end(), group->mAppInfo->id) == pinnedList.end())
				pinnedList.push_back(group->mAppInfo->id);
		}

		Settings::pinnedAppList.set(pinnedList);
		g_list_free(children);
	}

	void drawGroups()
	{
		mDrawingGroups = true;
		// Destroy old window objects while their associated groups still exist.
		Xfw::mGroupWindows.clear();
		// Remove old groups
		mGroups.clear();
		mDrawingGroups = false;

		// Add pinned groups
		std::list<std::string> pinnedApps = Settings::pinnedAppList;
		std::list<std::string>::iterator it = pinnedApps.begin();

		while (it != pinnedApps.end())
		{
			std::shared_ptr<AppInfo> appInfo = AppInfos::search(Help::String::toLowercase(*it));
			bool alreadyPinned = mGroups.findIf([&appInfo](std::pair<std::shared_ptr<AppInfo>, std::shared_ptr<Group>> entry) -> bool {
				return entry.first == appInfo && entry.second->mPinned;
			}) != nullptr;
			if (!alreadyPinned)
			{
				std::shared_ptr<Group> group = std::make_shared<Group>(appInfo, true);
				mGroups.push(appInfo, group);
				gtk_container_add(GTK_CONTAINER(mBox), group->mButton);
			}
			++it;
		}

		// Add open windows
		for (GList* window_l = xfw_screen_get_windows(Xfw::mXfwScreen);
			 window_l != nullptr;
			 window_l = window_l->next)
		{
			XfwWindow* xfwWindow = XFW_WINDOW(window_l->data);
			std::shared_ptr<GroupWindow> groupWindow = Xfw::mGroupWindows.get(xfwWindow);

			if (!groupWindow)
				groupWindow = std::make_shared<GroupWindow>(xfwWindow);
			else
				gtk_container_add(GTK_CONTAINER(mBox), groupWindow->mGroup->mButton);

			Xfw::mGroupWindows.push(xfwWindow, groupWindow);
			groupWindow->updateState();
		}

		gtk_widget_queue_draw(mBox);
	}

	void hoverSupered(bool on)
	{
#ifdef ENABLE_X11
		if (GDK_IS_X11_DISPLAY(gdk_display_get_default()))
		{
			int grabbedKeys = Hotkeys::mGrabbedKeys;
			GList* children = gtk_container_get_children(GTK_CONTAINER(mBox));

			for (GList* child = children; child != nullptr && grabbedKeys; child = child->next)
			{
				GtkWidget* widget = (GtkWidget*)child->data;

				if (!gtk_widget_get_visible(widget))
					continue;

				--grabbedKeys;
			}

			g_list_free(children);
		}
#endif
	}

	void activateGroup(int nb, guint32 timestamp)
	{
		int i = 0;
		GList* children = gtk_container_get_children(GTK_CONTAINER(mBox));

		for (GList* child = children; child != nullptr; child = child->next)
		{
			GtkWidget* widget = (GtkWidget*)child->data;

			if (gtk_widget_get_visible(widget))
			{
				if (i == nb)
				{
					Group* group = (Group*)g_object_get_data(G_OBJECT(widget), "group");

					if (group->mActive)
						group->scrollWindows(timestamp, GDK_SCROLL_DOWN);
					else if (group->mWindowsCount > 0)
						group->activate(timestamp);
					else
						group->mAppInfo->launch();

					return;
				}
				else
					++i;
			}
		}

		g_list_free(children);
	}

	void onPanelResize(int size)
	{
		if (size != -1)
			mPanelSize = size;

		gtk_box_set_spacing(GTK_BOX(mBox), mPanelSize / 10);

		if (Settings::forceIconSize)
			mIconSize = Settings::iconSize;
		else
		{
			if (mPanelSize <= 20)
				mIconSize = mPanelSize - 6;
			else if (mPanelSize <= 28)
				mIconSize = 16;
			else if (mPanelSize <= 38)
				mIconSize = 24;
			else if (mPanelSize <= 41)
				mIconSize = 32;
			else
				mIconSize = mPanelSize * 0.8;
		}

		mGroups.forEach([](std::pair<std::shared_ptr<AppInfo>, std::shared_ptr<Group>> g) -> void { g.second->resize(); });
	}

	void onPanelOrientationChange(GtkOrientation orientation)
	{
		gtk_orientable_set_orientation(GTK_ORIENTABLE(mBox), orientation);

		if (Settings::dockSize)
		{
			if (orientation == GTK_ORIENTATION_HORIZONTAL)
				gtk_widget_set_size_request(mBox, Settings::dockSize, -1);
			else
				gtk_widget_set_size_request(mBox, -1, Settings::dockSize);
		}
	}
} // namespace Dock
