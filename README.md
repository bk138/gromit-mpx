# Gromit-MPX

[![Build Status](https://travis-ci.org/bk138/gromit-mpx.svg?branch=master)](https://travis-ci.org/bk138/gromit-mpx)

Gromit-MPX is a multi-pointer port of the original [Gromit annotation
tool](http://www.home.unix-ag.org/simon/gromit) by [Simon
Budig](mailto:simon@budig.de).

It enables graphical annotations with several pointers at once and is
A **lot** faster since it uses the XCOMPOSITE extension where
available.  Also, it does not inhibit Drag-and-Drop like the original
Gromit tool.

## What it is

Gromit-MPX (GRaphics Over MIscellaneous Things) is a small tool to
make annotations on the screen.

Its main use is for making presentations of some application.
Normally, you would have to move the mouse pointer around the point of
interest until hopefully everybody noticed it.  With Gromit-MPX, you
can draw everywhere onto the screen, highlighting some button or area.

Similar tools for MS-Windows include *DemoHelper* (GPLv2 also), or
proprietary tools like *ZoomIt* and *ScreenMarker*.  For Compiz, there
is also the *Annotate* plugin, and the much-flashier *Firepaint (paint
fire on screen)* plugin.

## How to use it

You can operate Gromit-MPX using its tray icon (if your desktop environment
provides a sys tray), but since you typically want to use the program you
are demonstrating and highlighting something is a short interruption of
your workflow, Gromit-MPX can be toggled on and off on the fly via a hotkey:

Per default, it grabs the `F9` and `F10` keys, so that no other application
can use them and they are available to Gromit-MPX only.  The available
commands are:

    F9:        toggle painting
    SHIFT-F9:  clear screen
    CTRL-F9:   toggle visibility
    ALT-F9:    quit Gromit-MPX
    F10:       undo last stroke
    SHIFT-F10: redo last undone stroke

You can specify the keys to grab via:

    gromit-mpx --key <keysym> --undo-key <keysym>

Specifying an empty string or `none` for the keysym will prevent gromit
from grabbing a key.

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
(see the output of `xinput --list`) this input-device uses this tool.
Additionally you can limit the Scope to specific combinations of
Mousebuttons (1,2,3,4,5 or Button1,...,Button5)
and Modifiers (`SHIFT`, `CONTROL`, `ALT`, `META`, while `ALT==META`).

    "Core Pointer" = "red Pen";
    "Core Pointer"[SHIFT] = "blue Pen";
    "Core Pointer"[CONTROL] = "yellow Pen";
    "Core Pointer"[2] = "green Marker";
    "Core Pointer"[Button3] = "Eraser";

The descision which tool to use follows a simple policy:

1. Buttons are more important than Modifiers
2. Low number Buttons are more important than higher ones
3. Modifiers: `SHIFT` > `CONTROL` > `ALT`/`META`.
4. Gromit-MPX tries partial matches:
      If you define `"Core Pointer"[]` and `"Core Pointer"[SHIFT, CONTROL]`
      and only `SHIFT` actually is pressed, Gromit-MPX will use the second
      definition if there is no `"Core Pointer"[SHIFT]` definition.
      The same logic holds for the buttons.
5. Slave device config takes precedence over master device config, which
   in turn takes precedence over the fallback default config.

## Building it

Gromit-MPX uses CMake as its build system. Thus, it's the usual:

    mkdir build
    cd build
    cmake ..
    make

from the root of the source tree.

## Potential Problems

When there is no compositing manager such as Compiz or xcompmgr
running, Gromit-MPX falls back to a legacy drawing mode. This may
drastically slow down your X-Server, especially when you draw very
thin lines. It makes heavy use of the shape extension, which is
quite expensive if you paint a complex pattern on screen. Especially
terminal-programs tend to scroll incredibly slow if something is
painted over their window.

Like the original Gromit, this program is distributed under the Gnu
General Public License.  See the file `COPYING` for details.  Thanks
to Simon for the groundwork done!

---[Christian Beier](mailto:dontmind@freeshell.org)
