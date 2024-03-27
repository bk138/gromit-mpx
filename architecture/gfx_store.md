# Background and Rationale

`gromit` currently follows a "draw-and-forget" strategy, i.e. each
drawing operation, once completed, is drawn on a `cairo_surface_t`
(`GromitData.back_buffer`).

While this is very straightforward, it prevents the implementation of

-  dynamic content, e.g. items that fade after a certain time and
   disappear, or otherwise change through time

-  editing of past content (e.g. select and move or delete an old item)

Currently, a work-around exists in the form of an `aux_backbuffer`,
which stores the state prior to the drawing operation in
progress. This allows dynamic content during the ongoing drawing
operation, and is used in e.g. `LINE`, `SMOOTH`, `RECT`.
However, the drawing is split up over various callbacks (`on_press`,
`on_motion`, and `on_release`, which complicates maintenance).

Here, I propose to implement a "graphics object store" that contains
the elements that have been produced so far. By re-executing these
drawing operations (function `redraw`), the concent of `backbuffer`
could be re-created at any time.

# Implementation

A graphics object store is added to `GromitData`. This object store
simple is a `GList` of `GfxObject`. Each `GfxObject` has the
following fields:

    typedef struct {
        guint       id;                                 // unique id
        GfxType     type;                               // the type of object, e.g. Stroke, or Text
        gboolean    dynamic;                            // TRUE = content is dynamically updated
        gboolean    selected;                           // TRUE = object is currently selected
        guint32     capabilities;                       // bit mask with capabilities (see below)
        BoundingBox extent;                             // the objects rectangular extent
        bool        (*do)(GfxObject *, action, void *); // object-specific methods
        // ------------- object-specific fields -----------
        ...
    } GfxObject;

The object specific fields allow for an inheritance-like structure,
i.e. each specialized object can be downcast to the above `GfxObject`.

For a normal stroked path, the specific field could be:

    typedef struct {
        .... common fields ....
        //  ------------- object-specific fields -----------      
        GList     *coordinates;
        GdkRGBA    color;
        gfloat     arrowsize;
        ArrowType  arrowtype;
    } GfxObjectPath;
    
For a text object, one could have:

    typedef struct {
        .... common fields ....
        //  ------------- object-specific fields -----------      
        GdkRGBA  color;
        gchar   *text;
        gfloat   fontsize;
        gfloat   x, y;
    }  GfxObjectPath;
    
A path that fades away after a certain time could look like so:

    typedef struct {
        .... common fields ....
        //  ------------- object-specific fields -----------      
        GList     *coordinates;
        GdkRGBA    color;
        gfloat     arrowsize;
        ArrowTyype arrowtype;
        guint64    start_fading;  // timestamps in
        guint64    end_fading;    // microseconds
    } GfxObjectFadingPath;

## Capabilities

Each `gfx_object` has a bit mask indicating its "capabilities". These
capabilities imply that certain actions are implemented in the `do`
function.

<table>
<tr>
   <td> <b>Capability</b> </td> <td> <b>Description</b> </td>
</tr>
<tr>
   <td> draw              </td> <td> object can be drawn </td>
</tr>
<tr>
   <td> draw_transformed  </td> <td> object can be drawn in transformed state; <br>
                                     this allows for dynamic representation during <br>
                                     operations such as dragging </td>
</tr>
<tr>
   <td> move              </td> <td> object can be moved </td>
</tr>
<tr>
   <td> isotropic_scale   </td> <td> object can be scaled isotropically </td>
</tr>
<tr>
   <td> anisotropic_scale </td> <td> object can be scaled anisotropically,<BR>
                                     i.e. differently in X and Y direction<BR>
                                     isotropic_scale also must be set </td>
</tr>
<tr>
   <td> rotate            </td> <td> object can be rotated</td>
</tr>
<tr>
   <td> edit              </td> <td> object is editable (e.g. TEXT or parts of a path)<BR>
                                     this probably is hardest to implement, and<br>
                                     left for the future</td>
</tr>
</table>


### Use of capabilities with selections and during transformations

When multiple `GfxObject`s are selected, the capability bitmasks could
be `and`ed to obtain the greatest common denominator. For example,
objects may be movable, but not rotateable, and then the rotate handle
is omitted in the GUI. Similarly, isotropic-only scaling would leave
only the corner handles, whereas anisotropic scaling would also
display handles in the middle of the sides (see
[here](#selection-wireframe) for selection wireframe).

During transform operations, `draw_transformed` is called for each
object, with a 2D affine transformation matrix as argument.

At the end of the operation, `transform` is called with the final
transformation matrix, which then modifies the object in the object
store.

The 2D affine transformation functions are already implemented in
`coordlist_ops.c`.

## Actions

When calling `do`, the `action` argument selects the task to be performed.

<table>
<tr>
<td> draw             </td><td> draw object in "normal" state </td>
</tr>
<tr>
<td> draw_selected    </td><td> draw object in "selected" state </td>
</tr>
<tr>
<td> draw_transformed </td><td> draw object in intermediate "transformed" state; <br>
                                this allows to display the object dynamically, <br>
                                e.g. while it is being dragged </td>
</tr>
<tr>
<td> is_clicked       </td><td> detect whether a click at coordinate (x,y)  <br>
                                "hits" the object, for example to select it </td>
</tr>
<tr>
<td> transform        </td><td> transforms the object, modifying the context <br>
                                in the <tt>gfx_store</tt> </td>
</tr>
<tr>
<td> destroy          </td><td> destroys the object, deallocating memory </td>
</tr>
</table>
  

## Drawing of objects store

Whenever objects need to be re-drawn, the `redraw` function simply iterates
through the object store and calls `object->do(ptr, draw_action)` for
all objects.

To improve update speed, a cached `cairo_surface_t` (similar to
`backbuffer`) is maintained. This cache contains all drawing
operations up to but not including the first that is flagged as
`dynamic`.

In practice, most of the time only the last object would need to be
redrawn atop the cached image. This is no different from the current
code which often draws over `aux_backbuffer`.

# Migration to new drawing system

## First step

* **Object store**

  The object store is added to `GromitData`, together with some
  housekeeping functions.

* **Callbacks**

  Then, the callbacks are adapted to add data to the object store
  instead of `GromitData->coordlist`. Direct drawing is replaced by a
  subsequent call of `redraw`.

  - `on_press` adds a new item of the respective type to the object
    store, and "remembers" its `id` (for example, in the hash_table).

  - `on_motion` appends coordinates to the respective `GfxObject`, and
    calls `redraw`.

  - `on_release` finalizes the operation, for example by smoothing a
    path (SMOOTH tool), and then calling `redraw`.
       
  - for each tool, the specific drawing operation is implemented. The
    only drawing operation that is required at this first stage is the
    drawing of a simple stroke path.
      
* **Undo/redo**

  - "Undo" is implemented by simply moving the last `GfxObjects`
    from the object store to the head of a separate "redo" list,
    followed by a call to `redraw`.

  - "Redo" moves the first element from the "redo" list to the last
    position in the object store, followed by a call of `redraw`.

These are relatively minor changes. With these, the new architecture
already is functional.

## Second step

In a next step, a "select" tool is implemented. This requires that the
`GfxObject` implement `is_clicked` and `draw_selected`.

The select tool draws a wireframe around the selection, showing the
respective handles (see "capabilities").

Then, a "delete" function is implemented. It basically calls the
"destroy" method and removes the selected items from the object store,
and then calls `redraw`.

## Third step

Depending on the capabilities of the selected objects, suitable
transformations are offered. The simplest one is a "move" handle.

The items that implement "draw_transformed" are dynamically redrawn
during the "drag" operation. Upon releasing the "move" handle, the
tool calls the "transform" method, which modifies the coordinates of
the selecetd items in the object store.

Similarly, "scaling" and "rotation" operations are implemented by
adding extra handles to the selection wireframe. Everything else
remains the same.

# Selection wireframe

The selection wireframe could look like this:

        R                                                 R
      R                                                     R
    R   +---+                  +---+                  +---+   R
        | S |------------------| A |------------------| S |
        +---+                  +---+                  +---+
          |                                             |
          |                                             |
          |                                             |
          |                                             |
          |                                             |
          |                      ^                      |
        +---+                    |                    +---+
        | A |                <---M--->                | A |
        +---+                    |                    +---+
          |                      V                      |
          |                                             |
          |                                 BIN         |
          |                                 ICON        |
          |                                             |
          |                                             |
        +---+                  +---+                  +---+
        | S |------------------| A |------------------| S |
    R   +---+                  +---+                  +---+   R
      R                                                     R
        R                                                 R
       
| Handle | Function                                                                  |
|--------|---------------------------------------------------------------------------|
| BIN    | trash icon, deletes the selected items                                    |
| M      | move handle to drag the selection. SHIFT stays H or V                     |
| S      | isotropic scaling handle, opposite corner stays where it is               |
| A      | anisotropic scaling handle, opposite side stays where it is               |
| R      | rotate handle, opposite corner is center of rotation, SHIFT for 45° steps |
