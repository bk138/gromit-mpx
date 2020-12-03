# Gromit-MPX

[![Build Status](https://travis-ci.org/bk138/gromit-mpx.svg?branch=master)](https://travis-ci.org/bk138/gromit-mpx)
[![Help making this possible](https://img.shields.io/badge/liberapay-donate-yellow.png)](https://liberapay.com/bk138/donate)
[![Become a patron](https://img.shields.io/badge/patreon-donate-yellow.svg)](https://www.patreon.com/bk138)
[![Donate](https://img.shields.io/badge/paypal-donate-yellow.png)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=N7GSSPRPUSTPU&source=url)
[![Gitter](https://badges.gitter.im/gromit-mpx/community.svg)](https://gitter.im/gromit-mpx/community?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge)

Gromit-MPX is an on-screen annotation tool that works with any Unix
desktop environment under X11 as well as Wayland.

Its main use is for making presentations of some application.
Normally, you would have to move the mouse pointer around the point of
interest until hopefully everybody noticed it.  With Gromit-MPX, you
can draw everywhere onto the screen, highlighting some button or area.

Key features include:

  * **Desktop-independent**. Gromit-MPX works with GNOME, KDE, XFCE, ...
	under X11 as well as with a Wayland session.
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

    gromit-mpx -o <opacity>

Alternatively you can invoke Gromit-MPX with various arguments to
control an already running Gromit-MPX .

Usage:

    gromit-mpx --quit
        will cause the main Gromit-Mpx-MPX process to quit (or "-q")
    gromit-mpx --toggle
        will toggle the grabbing of the cursor (or "-t")
    gromit-mpx --visibility
        will toggle the visibility of the window (or "-v")
    gromit-mpx --clear
        will clear the screen (or "-c")
    gromit-mpx --undo
        will undo the last drawing stroke (or "-z")
    gromit-mpx --redo
        will redo the last undone drawing stroke (or "-y")

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
continuing to work normally with the first pair.

Alternatively, you can also use the graphical tool [gnome-device-
manager](https://github.com/bk138/gnome-device-manager) to arrange your
MPX setup.

### Configuration

Gromit-MPX is configurable via the file `gromit-mpx.cfg` in the
directory defined by `$XDG_CONFIG_HOME` (usually `~/.config`).  Here
you can specify which Device/Button/Modifier combination invokes which
tool.  See the copy of `gromit-mpx.cfg` distributed with this program
for an example.  An overview on the syntax:

    # Comments can be either # Shell-Style or
    /* C-Style. */

This entry defines the tool `red Pen`, a pen with size 7 and color red.
You can specify the color in X-Style: e.g. `#FF0033` or
colors from `rgb.txt`.

    "red Pen" = PEN (size=7 color="red");

The following Entries copy an existing configuration (in this case
`red Pen`) and modify the color.

    "blue Pen" = "red Pen" (color="blue");
    "yellow Pen" = "red Pen" (color="yellow");

If you want another minimum size instead of the default 1, add `minsize`
like this:

	"red Marker" = "red Pen" (minsize=14);

You can also draw lines that end in an arrow head. For this you
have to specify `arrowsize`. This is a factor relative to the width
of the line. For reasonable arrowheads start with 1.

    "blue Pen" = "blue Arrow" (arrowsize=2);

An `ERASER` is a tool that erases the drawings on screen.
The color parameter is not important.

    "Eraser" = ERASER (size = 75);

A `RECOLOR`-Tool changes the color of the drawing without changing
the shape. Try it out to see the effect.

    "green Marker" = RECOLOR (color = "Limegreen");

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

You can freely add command line arguments to the 'Exec' stanza, configuring
the autostarted instance to your needs.

## Building it

Gromit-MPX uses CMake as its build system. Thus, it's the usual:

    mkdir build
    cd build
    cmake ..
    make

from the root of the source tree.

## Potential Problems

XFCE per default grabs Ctrl-F1 to Ctrl-F12 (switch to workspace 1-12)
and Alt-F9 (minimize window) which renders Gromit-MPX's default hotkey
mapping unusable. Gromit-MPX detects XFCE and changes the default hotkeys
to Home and End. Those can can still be overridden by the user.

When there is no [compositing manager](https://en.wikipedia.org/wiki/Compositing_window_manager)
such as Mutter or KWin running, Gromit-MPX falls back to a legacy drawing mode. This may
drastically slow down your X-Server, especially when you draw very
thin lines. It makes heavy use of the shape extension, which is
quite expensive if you paint a complex pattern on screen. Especially
terminal-programs tend to scroll incredibly slow if something is
painted over their window.

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
