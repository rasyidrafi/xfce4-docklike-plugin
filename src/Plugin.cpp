/*
 * Docklike Taskbar - A modern, minimalist taskbar for Xfce
 * Copyright (c) 2019-2020 Nicolas Szabo <nszabo@vivaldi.net>
 * Copyright (c) 2020-2021 David Keogh <davidtkeogh@gmail.com>
 * gnu.org/licenses/gpl-3.0
 */

#include "Plugin.hpp"
#include "Helpers.hpp"
#ifdef ENABLE_X11
#include "Hotkeys.hpp"
#endif

namespace Plugin
{
	XfcePanelPlugin* mXfPlugin;
	GdkDevice* mPointer;
	GdkDisplay* mDisplay;

	static guint busOwner, registration;
	static GDBusNodeInfo* api;
	static constexpr const char* interfaceName = "org.xfce.Docklike.Pins1";
	static constexpr const char* objectPath = "/org/xfce/Docklike/Pins";

	static void handlePinCall(GDBusConnection*, const gchar*, const gchar*, const gchar*,
		const gchar* method, GVariant* args, GDBusMethodInvocation* invocation, gpointer)
	{
		const char* id;
		gboolean desired = FALSE;
		bool write = g_str_equal(method, "SetPinned");
		if (write) g_variant_get(args, "(&sb)", &id, &desired);
		else g_variant_get(args, "(&s)", &id);
		auto app = AppInfos::byDesktopId(id);
		if (!app)
		{
			g_dbus_method_invocation_return_error(invocation, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "Unknown desktop ID: %s", id);
			return;
		}
		auto pinned = Dock::mGroups.findIf([&app](const auto& entry) { return entry.first == app && entry.second->mPinned; });
		if (write && desired && !pinned) Dock::setPinned(Dock::prepareGroup(app), true);
		if (write && !desired && pinned) Dock::setPinned(pinned.get(), false);
		g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", write ? desired : pinned != nullptr));
	}

	static void exportPins(GDBusConnection* bus, const gchar*, gpointer)
	{
		static const GDBusInterfaceVTable handlers = {handlePinCall, nullptr, nullptr, {nullptr}};
		registration = g_dbus_connection_register_object(bus, objectPath, api->interfaces[0], &handlers, nullptr, nullptr, nullptr);
	}

	static void init(XfcePanelPlugin* xfPlugin)
	{
		xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, nullptr);

		mXfPlugin = xfPlugin;
		mDisplay = gdk_display_get_default();
		mPointer = gdk_seat_get_pointer(gdk_display_get_default_seat(mDisplay));

		Settings::init();
		AppInfos::init();
		Xfw::init();
		Dock::init();
		Theme::init();
		api = g_dbus_node_info_new_for_xml("<node><interface name='org.xfce.Docklike.Pins1'>"
			"<method name='IsPinned'><arg type='s' direction='in'/><arg type='b' direction='out'/></method>"
			"<method name='SetPinned'><arg type='s' direction='in'/><arg type='b' direction='in'/><arg type='b' direction='out'/></method>"
			"</interface></node>", nullptr);
		std::string busName = std::string(interfaceName) + ".Instance" + std::to_string(xfce_panel_plugin_get_unique_id(xfPlugin));
		busOwner = g_bus_own_name(G_BUS_TYPE_SESSION, busName.c_str(), G_BUS_NAME_OWNER_FLAGS_NONE, exportPins, nullptr, nullptr, nullptr, nullptr);
#ifdef ENABLE_X11
		if (GDK_IS_X11_DISPLAY(gdk_display_get_default()))
			Hotkeys::init();
#endif

		gtk_container_add(GTK_CONTAINER(mXfPlugin), Dock::mBox);
		xfce_panel_plugin_menu_show_configure(mXfPlugin);
		xfce_panel_plugin_menu_show_about(mXfPlugin);

		//--------------------------------------------------

		g_signal_connect(G_OBJECT(GTK_WIDGET(mXfPlugin)), "size-changed",
			G_CALLBACK(+[](XfcePanelPlugin* plugin, gint size) {
				Dock::onPanelResize(size);
				return true;
			}),
			nullptr);

		g_signal_connect(G_OBJECT(GTK_WIDGET(mXfPlugin)), "orientation-changed",
			G_CALLBACK(+[](XfcePanelPlugin* plugin, GtkOrientation orientation) {
				Dock::onPanelOrientationChange(orientation);
			}),
			nullptr);

		g_signal_connect(G_OBJECT(mXfPlugin), "configure-plugin",
			G_CALLBACK(+[](XfcePanelPlugin* plugin) {
				SettingsDialog::popup();
			}),
			nullptr);

		g_signal_connect(G_OBJECT(mXfPlugin), "about",
			G_CALLBACK(+[](XfcePanelPlugin* plugin) {
				Plugin::aboutDialog();
			}),
			nullptr);

		g_signal_connect(G_OBJECT(mXfPlugin), "remote-event",
			G_CALLBACK(+[](XfcePanelPlugin* plugin, gchar* name, GValue* value) -> gboolean {
				return remoteEvent(name, value);
			}),
			nullptr);

		g_signal_connect(G_OBJECT(mXfPlugin), "free-data",
			G_CALLBACK(+[](XfcePanelPlugin* plugin) {
				if (registration) {
					auto bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, nullptr);
					if (bus) { g_dbus_connection_unregister_object(bus, registration); g_object_unref(bus); }
					registration = 0;
				}
				g_bus_unown_name(busOwner);
				g_dbus_node_info_unref(api);
				Xfw::finalize();
				Dock::mGroups.clear();
				AppInfos::finalize();
				Settings::finalize();
			}),
			nullptr);
	}

	void aboutDialog()
	{
		const gchar* AUTHORS[] = {
			"Nicolas Szabo <nszabo@vivaldi.net>",
			"David Keogh <davidtkeogh@gmail.com>",
			nullptr};

		// TODO: Load these from a TRANSLATORS text file, so people can add themselves.
		const gchar* TRANSLATORS =
			"Yamada Hayao <hayao@fascode.net> : ja\n"
			"Dmitry K <dkabishchev@ya.ru> : ru\n"
			"Fábio Meneghetti <fabiom@riseup.net> : pt\n"
			"Mirko Brombin <send@mirko.pm> : it\n"
			"Adem Kürşat Uzun <ademkursatuzun@gmail.com> : tr\n"
			"Santiago Soler <santiago.r.soler@gmail.com> : es\n"
			"fredii: de\n"
			"Lucas Hadjilucas <puzzle@outlook.com> : el\n"
			"Jan Kazemier : nl\n"
			"Matthaiks : pl\n"
			"Faisal Rachmadin <frachmadin@gmail.com> : id\n";

		gtk_show_about_dialog(nullptr,
			"program-name", "Docklike Taskbar",
			"logo-icon-name", "preferences-system",
			"version", PACKAGE_VERSION,
			"copyright", "Copyright \302\251 2003-2024 The Xfce development team",
			"license-type", GTK_LICENSE_GPL_3_0,
			"authors", AUTHORS,
			"translator-credits", TRANSLATORS,
			nullptr);
	}

	gboolean remoteEvent(gchar* name, GValue* value)
	{
		if (g_strcmp0(name, "settings") == 0)
			SettingsDialog::popup();
		else if (g_strcmp0(name, "about") == 0)
			aboutDialog();
		else if (g_strcmp0(name, "pin") == 0 || g_strcmp0(name, "unpin") == 0)
		{
			auto app = AppInfos::byDesktopId(value && G_VALUE_HOLDS_STRING(value) ? g_value_get_string(value) : nullptr);
			if (!app)
				return FALSE;
			auto pinned = Dock::mGroups.findIf([&app](const auto& entry) {
				return entry.first == app && entry.second->mPinned;
			});
			bool pin = g_strcmp0(name, "pin") == 0;
			if (pin && !pinned)
				Dock::setPinned(Dock::prepareGroup(app), true);
			else if (!pin && pinned)
				Dock::setPinned(pinned.get(), false);
		}
		else
			return FALSE;
		return TRUE;
	}

} // namespace Plugin

//----------------------------------------------------------------------------------------------------------------------

extern "C" void construct(XfcePanelPlugin* xfPlugin) { Plugin::init(xfPlugin); }
