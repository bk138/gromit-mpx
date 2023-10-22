# Gromit-MPX 1.5.1 - 2023-10-22

## 🛠  Fixes

* Fixed intro dialog always showing up on new installations.

# Gromit-MPX 1.5.0 - 2023-10-11

## 🌅 New Features

* Added a maxsize config parameter.
* Added translations into German, Spanish, Italian, Farsi and Hebrew.
* Added error dialog that's shown on encountering invalid config.
* Added a faint pink background indicating debug mode.
* Added functionality to draw lines via command.

## 🛠  Fixes

* Removed AppStream `<em>` tags which seem unsupported by Flathub.
* Gromit-MPX now does a proper shutdown on SIGINT (Ctrl-C) and SIGTERM (kill).

# Gromit-MPX 1.4.3 - 2022-09-25

## 🛠  Fixes

* Fixed operation with hotkeys set to none.
* Fixed hotkey binding on Ubuntu 22/GNOME.

# Gromit-MPX 1.4.2 - 2022-02-12

## 🛠  Fixes

* Fixed Gromit-MPX blocking mouse clicks to other apps on Wayland in certain conditions.
* Fixed toggling from GNOME systray with gnome-shell-extension-appindicator under X11.

# Gromit-MPX 1.4.1 - 2022-02-03

## 🛠  Fixes

* Fixed hotkey setting from config file for XFCE.
* Fixed erase tool not working properly when not using a composited display.
* Fixed tray icon not showing up in Ubuntu when using the Flatpak.
* Fixed tray icon not showing on KDE when using the Flatpak.
* Fixed Wayland hotkey setting not working on GNOME when using the Flatpak.

# Gromit-MPX 1.4 - 2020-12-03
   * Added display of activation status to tray menu.
   * Added Flatpak packaging.
   * Added support options to tray menu.
   * Added commandline switch for getting version.
   * Added detection of `GDK_CORE_DEVICE_EVENTS`.
   * Fixed activation via tray menu on KDE.
   * Fixed running on Wayland.
   * Fixed non-working default hotkeys on XFCE.
   * Documented autostart, hotkey setting via config file, similar tools and XFCE pecularities. 


# Gromit-MPX 1.3.1 - 2020-08-01
   * Documented donation options.
   * Added ability to configure the Hot key and Undo key in gromit-mpx.cfg.
   * Added lotsa special-case config documentation.
   * Added GUI error displays in case one of the hotkeys can't be grabbed.


# Gromit-MPX 1.3 - 2018-12-28
   * Gromit-MPX now shows the hotkeys bound to some functionality directly in the menu.
   * Replaced the help menu with an introduction to Gromit-MPX that shows on startup.
   * Reworked slave device handling so that per-slave configuration is now done The Right
     Way™.
   * Build system switched to CMake, removed Autotools.


# Gromit-MPX 1.2 - 2016-11-16
   * Added undo/redo functionality thanks to Nathan Whitehead.
   * Added opacity-set functionality thanks to Lukas Hermann.
   * Added a tray icon.
   * Added an About and a Help dialog.
   * Added CMake build system support thanks to Russel Winder.
   * Replaced GTK3 deprecated functions with newer counterparts.


# Gromit-MPX 1.1 - 2013-08-19
   * Added reasonably complete support for slave devices thanks to Monty Montgomery.
   * Tweaked build system, packaging, icon thanks to Barak A. Pearlmutter.
   * Fixed some crashes.


# Gromit-MPX 1.0 - 2011-02-10
   * Concurrent multi-user operation using GTK3 with Multi-Pointer-X.
   * Drawn annotations do not intercept mouse clicks.
   * Makes use of the Composite extension where available to allow for
     really fast drawing.
