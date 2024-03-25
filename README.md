# Gromit-MPX

[![CI](https://github.com/bk138/gromit-mpx/actions/workflows/ci.yml/badge.svg)](https://github.com/bk138/gromit-mpx/actions/workflows/ci.yml)
[![Help making this possible](https://img.shields.io/badge/liberapay-donate-yellow.png)](https://liberapay.com/bk138/donate)
[![Become a patron](https://img.shields.io/badge/patreon-donate-yellow.svg)](https://www.patreon.com/bk138)
[![Donate](https://img.shields.io/badge/paypal-donate-yellow.png)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=N7GSSPRPUSTPU&source=url)
[![Gitter](https://badges.gitter.im/gromit-mpx/community.svg)](https://gitter.im/gromit-mpx/community?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)

[<img src="https://flathub.org/assets/badges/flathub-badge-i-en.png" width="190px" />](https://flathub.org/apps/details/net.christianbeier.Gromit-MPX)

Gromit-MPX is an on-screen annotation tool that works with any Unix
desktop environment under X11 as well as Wayland.

Its main use is for making presentations of some application.
Normally, you would have to move the mouse pointer around the point of
interest until hopefully everybody noticed it.  With Gromit-MPX, you
can draw everywhere onto the screen, highlighting some button or area.

<img src="https://github.com/bk138/gromit-mpx/blob/master/data/on-gnome-wayland.png" width="75%" />

Key features include:

  * **Desktop-independent**. Gromit-MPX works with GNOME, KDE, XFCE, ...
	under X11 (every desktop environment) as well as with a Wayland session
    using XWayland (most desktop environments).
  * **Hotkey-based**. The fundamental philosophy is that Gromit-MPX does not
    get into your way of doing things by sticking some UI widget on your
	desktop, potentially obscuring more important contents. It *does*
	provide a UI, but only in form of a tray icon.
  * **Configurable**. While Gromit-MPX comes with a default config, you are
	free to change key bindings as well as drawing tool configuration.
  * **Multi-Pointer**. Under X11, it enables graphical annotations with
	several pointers at once or a dedicated annotation device setup where
	you can use a second pair of input devices to annotate while
	simultaneously continuing to work normally with the first pair.
  * **Fast**. Where available, Gromit-MPX makes use of Compositing. This
	should be the case on any contemporary desktop environment making use
	of the XCOMPOSITE extension under X11 and with every Wayland-based session.

The name stands for (*GR*aphics *O*ver *MI*scellaneous *T*hings -
*M*ulti-*P*ointer-E*X*tension). It is a multi-pointer port of the original
[Gromit annotation tool](http://www.home.unix-ag.org/simon/gromit) by [Simon
Budig](mailto:simon@budig.de).

If you already used Gromit-MPX, you probably want to read [NEWS](NEWS.md).
The roadmap for future developments can be found [here](https://github.com/bk138/gromit-mpx/milestones?state=open).

## How to use it

You can operate Gromit-MPX using its tray icon (if your desktop environment
provides a sys tray), but since you typically want to use the program you
are demonstrating and highlighting something is a short interruption of
your workflow, Gromit-MPX can be toggled on and off on the fly via a hotkey:

Per default, it grabs the `F9` and `F8` keys, so that no other application
can use them and they are available to Gromit-MPX only.  The available
commands are:

    F9:        toggle painting
    SHIFT-F9:  clear screen
    CTRL-F9:   toggle visibility
    ALT-F9:    quit Gromit-MPX
    F8:       undo last stroke
    SHIFT-F8: redo last undone stroke

You can specify the keys to grab as hotkeys via:

    gromit-mpx --key <keysym> --undo-key <keysym>

Note that you can only specify single keysyms, not key combos. Specifying
an empty string or `none` for the keysym will prevent Gromit-MPX from
grabbing a key.

You can specify the opacity simply via:

    gromit-mpx -o <opacity as real value in [0,1]>

As opacity is not a tool but a canvas property, it is not configured via
`gromit-mpx.cfg` but remembered over restarts.

Alternatively you can invoke Gromit-MPX with various arguments to
control an already running Gromit-MPX .

Usage:

    gromit-mpx --quit
        will cause the main Gromit-Mpx-MPX process to quit (or "-q")
    gromit-mpx --toggle <device-number>
        will toggle the grabbing of all cursors (or "-t"). Optionally,
        it toggles for the given internal device number (not XInput ID)
        only.
    gromit-mpx --visibility
        will toggle the visibility of the window (or "-v")
    gromit-mpx --clear
        will clear the screen (or "-c")
    gromit-mpx --undo
        will undo the last drawing stroke (or "-z")
    gromit-mpx --redo
        will redo the last undone drawing stroke (or "-y")
    gromit-mpx --line <startX> <startY> <endX> <endY> <color> <thickness>
        will draw a straight line with characteristics specified by the arguments (or "-l")
        eg: gromit-mpx -l 200 200 400 400 '#C4A7E7' 6	

If activated Gromit-MPX prevents you from using other programs with the
mouse. You can press the button and paint on the screen. Key presses
(except the `F9`-Key, see above) will still reach the currently active
window but it may be difficult to change the window-focus without mouse...
The next `gromit-mpx --toggle` will deactivate Gromit-MPX and you can
use your programs as usual - only the painted regions will be obscured.

Gromit-MPX is pressure sensitive, if you are using properly configured
XInput-Devices you can draw lines with varying width.  It is
possible to erase something with the other end of the (Wacom) pen.

Undo/redo commands are cumulative. For example, sending two undo commands
will undo the last two strokes. The maximum undo/redo depth is 4 strokes.

### Setting up multi-pointer

As its name implies, Gromit-MPX relies on Multi-Pointer-X functionality
provided by the XInput2 extension.

You can create a simple MPX setup via the `xinput` utility:

    xinput --create-master two # create a second input focus (master)
    xinput --list # see your master and slave devices and ids
    xinput --reattach <slave-id> <master-id-of-'two'> # reattach slave

If you attach a keyboard slave device to the newly created second master,
its hotkey will activate annotation mode for the associated pointer only.
This way, can use a second pair of input devices to annotate while
continuing to work normally with the first pair. If you don't have a
second keyboard at hand, you can also toggle a specific pointer via
`--toggle <device-number>`, where device-number is Gromit-MPX's internal
device number, not the XInput ID.

Alternatively, you can also use the graphical tool [input-device-
manager](https://github.com/bk138/input-device-manager) to arrange your
MPX setup.

### Configuration

Gromit-MPX is configurable via the file `gromit-mpx.cfg` in the
directory defined by `$XDG_CONFIG_HOME` (usually `~/.config` or
`~/.var/app/net.christianbeier.Gromit-MPX/config/` if you installed
the Flatpak). Here you can specify which Device/Button/Modifier
combination invokes which tool. See the copy of `gromit-mpx.cfg`
distributed with this program for an example. An overview on the syntax:

    # Comments can be either # Shell-Style or
    /* C-Style. */

The `PEN`-tool is for freehand drawing.

![PEN tool](data/tool-pen.webp)

The following entry defines the tool `red Pen`, a pen with size 7 and color red.
You can specify an RGB color in X-Style: e.g. `#FF0033`, specify an
RGBA color like so: `rgba(0, 0, 255, 0.6)` or use color names from
[`rgb.txt`](https://en.wikipedia.org/wiki/X11_color_names).

    "red Pen" = PEN (size=7 color="red");

The following Entries copy an existing configuration (in this case
`red Pen`) and modify the color.

    "blue Pen" = "red Pen" (color="blue");
    "yellow Pen" = "red Pen" (color="yellow");

If you want another minimum size instead of the default 1, add `minsize`
like this:

	"red Marker" = "red Pen" (minsize=14);

You can set a maximum size as well:

	"red Marker" = "red Pen" (maxsize=20);

Both `minsize` and `maxsize` can be combined to define a tool that's
not allowed to change size:

	"red fixed Marker" = "red Pen" (minsize=10 maxsize=10);

You can also draw lines that start and/or end in an arrow head. For
this you have to specify `arrowsize` and optionally `arrowtype`.
`arrowsize` is a factor relative to the width of the line. For
reasonable arrowheads start with 1.
`arrowtype` can take `start`, `end` or `double` and defaults to `end`
when `arrowsize` is specified and no `arrowtype` given. 

    "blue Pen" = "blue Arrow" (arrowsize=2);

An `ERASER` is a tool that erases the drawings on screen.
The color parameter is not important.

    "Eraser" = ERASER (size = 75);

A `RECOLOR`-Tool changes the color of the drawing without changing
the shape. Try it out to see the effect.

    "green Marker" = RECOLOR (color = "Limegreen");

A `LINE`-tool draws straight lines.

![LINE tool](data/tool-line.webp)

    "green Line" = LINE (color = "green");

A `RECT`-tool draws rectangles.

![RECT tool](data/tool-rect.webp)

    "red Rectangle" = RECT (color = "red");

A `SMOOTH`-tool that behaves like `PEN` except that it produces smoothed curves.
The degree of smoothing can be specified using `simplify=N`. `N` can
be imagined as approximate pixel range around the resulting line 
within which intermediate points are "simplified away". Closed paths
can be drawn using the `snap=N` option where `N` indicates the maximum 
distance between start and end point within which these "snap" together. 

![SMOOTH tool](data/tool-smooth.webp)

    "smoothed line" = SMOOTH (color = "red" simplify=10 snap=30);

A `ORTHOGONAL`-tool that behaves like `SMOOTH` except that it produces
straight line segments that automatically snap to perfectly horizontal
and vertical direction when their direction deviated by a maximum of
`maxangle` degrees. Transitions between straight segments are drawn as
arcs with a certain `radius`, if these segments exceed a length of
`minlen`.

![ORTHOGONAL tool](data/tool-orthogonal.webp)

    "ortho line" = ORTHOGONAL (color="red" size=5 simplify=15 radius=20 minlen=50 snap=40);

If you define a tool with the same name as an input-device
(see the output of `xinput --list`) this input-device uses this tool:

	"ELAN Touchscreen Pen (0)" = "red Pen";
	"ELAN Touchscreen Eraser (0)" = "Eraser";

Additionally you can limit the Scope to specific combinations of
Mousebuttons (1,2,3,4,5 or Button1,...,Button5)
and Modifiers (`SHIFT`, `CONTROL`, `ALT`, `META`, while `ALT==META`).

    "Core Pointer" = "red Pen";
    "Core Pointer"[SHIFT] = "blue Pen";
    "Core Pointer"[CONTROL] = "yellow Pen";
    "Core Pointer"[2] = "green Marker";
    "Core Pointer"[Button3] = "Eraser";

If you want to limit drawing to a specific input device, define a
tool with the same name as an input device like above and disable
drawing for all others by assigning a zero-width tool like this:

	"no Pen" = PEN (size=0);
	"default" = "no Pen";


The decision which tool to use follows a simple policy:

1. Buttons are more important than Modifiers
2. High number Buttons are more important than lower ones
3. Modifiers: `SHIFT` > `CONTROL` > `ALT`/`META`.
4. Gromit-MPX tries partial matches:
      If you define `"Core Pointer"[]` and `"Core Pointer"[SHIFT, CONTROL]`
      and only `SHIFT` actually is pressed, Gromit-MPX will use the second
      definition if there is no `"Core Pointer"[SHIFT]` definition.
      The same logic holds for the buttons.
5. Slave device config takes precedence over master device config, which
   in turn takes precedence over the fallback default config.
   
For versions > 1.3, you can also change the [hotkeys from the config](data/gromit-mpx.cfg#L5)
file by setting the respective `HOTKEY` and/or `UNDOKEY` values.

### Autostart

If you want to have Gromit-MPX autostarted for your desktop session, the
safest way to do so is via the XDG autostart facility:

Simply create a file `~/.config/autostart/gromit-mpx.desktop` with the
following contents:

```
[Desktop Entry]
Type=Application
Exec=gromit-mpx
```

If you have the Flatpak installed, the last line needs to start with
`Exec=flatpak run net.christianbeier.Gromit-MPX`. You can freely add
command line arguments to the 'Exec' stanza, configuring the autostarted
instance to your needs.

## Building it

Gromit-MPX uses CMake as its build system. Thus, it's the usual:

    mkdir build
    cd build
    cmake ..
    make
    make install

from the root of the source tree.

### Release Checklist

* [ ] Update NEWS.
* [ ] Increment version in CMakeLists.txt
* [ ] Update Copyright year in About dialog.
* [ ] Update authors in About dialog and AUTHORS file.
* [ ] Update AppStream.
* [ ] Update ChangeLog.
* [ ] Fill out GitHub release.
* [ ] Make Flathub release.
* [ ] Announce on social.

## Potential Problems

* XFCE per default grabs Ctrl-F1 to Ctrl-F12 (switch to workspace 1-12)
  and Alt-F9 (minimize window) which renders Gromit-MPX's default hotkey
  mapping unusable. Gromit-MPX detects XFCE and changes the default hotkeys
  to Home and End. Those can can still be overridden by the user. In case
  you're using XFCE 4.14 or newer, chances are that all 'special' keys are
  grabbed by XFCE itself, which means you'll have to modify XFCE's keybindings
  (Settings->Window Manager->Keyboard) manually in order to 'make room' for
  Gromit-MPX's ones.

* When there is no [compositing manager](https://en.wikipedia.org/wiki/Compositing_window_manager)
  such as Mutter or KWin running, Gromit-MPX falls back to a legacy drawing mode. This may
  drastically slow down your X-Server, especially when you draw very
  thin lines. It makes heavy use of the shape extension, which is
  quite expensive if you paint a complex pattern on screen. Especially
  terminal-programs tend to scroll incredibly slow if something is
  painted over their window.

* If Gromit-MPX under Wayland complains about "cannot open display", make
  sure you have XWayland runnning or its autostart configured. Gromit-MPX
  needs XWayland when running in a Wayland session.

* In case you encounter any other kind of problem, please check if it's in
  the [list of known bugs](https://github.com/bk138/gromit-mpx/issues?q=is%3Aissue+is%3Aopen+label%3Abug).
  You can help by reporting a new one or adding info to an existing one.

## Similar Tools

In the Unix-world, similar but different tools are *Ardesia*, *Pylote*
and *Draw On You Screen*.

Similar tools for MS-Windows include *DemoHelper* (GPLv2 also), or
proprietary tools like *ZoomIt*, *ScreenMarker* or *EpicPen*.

## License

Like the original Gromit, this program is distributed under the Gnu
General Public License.  See the file `COPYING` for details.  Thanks
to Simon for the groundwork done!

---[Christian Beier](mailto:gromit-mpx@christianbeier.net)
