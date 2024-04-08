# 2. implement graphics object store

Date: 2024-03-28

## Status

Proposed

## Context

`gromit-mpx` currently follows a "draw-and-forget" strategy, i.e. each
drawing operation, once completed, is drawn on a `cairo_surface_t`
(`GromitData.backbuffer`).

While this is very straightforward, it prevents the implementation of

-  **dynamic content**, e.g. items that fade after a certain time and
   disappear, or otherwise change through time

-  **editing** of past content (e.g. select and move or delete an old item)

Currently, a work-around exists in the form of an `aux_backbuffer`,
which stores the state prior to the drawing operation in
progress. This allows dynamic content during the ongoing drawing
operation, and is used in e.g. `LINE`, `SMOOTH`, `RECT`.
However, the drawing is split up over various callbacks (`on_press`,
`on_motion`, and `on_release`, which complicates code and maintenance).

Here, I propose to implement a **graphics object store** that contains
the elements that have been produced so far. By re-executing these
drawing operations, the widget is (re)painted.

## Implementation

A graphics object store (`objects`) is added to `GromitData`. This
object store simply is a `GList` of `GfxObject`. Each `GfxObject` has
the following fields:

    typedef struct {
        guint             id;                                 // unique id
        GfxType           type;                               // type of object, e.g. Stroke, or Text
        gboolean          is_dynamic;                         // content is dynamically updated
        GromitDeviceData *selected;                           // NULL, or which user selected the item
        guint32           capabilities;                       // bit mask with capabilities (see below)
        BoundingBox       extent;                             // the objects rectangular extent
        void            (*exec)(GfxObject *, action, void *); // object-specific methods
    } GfxObjectBase;

This basic set of fields is extended by object-specific fields, which
allows for an inheritance-like pattern, i.e. each specialized object
can be downcast to the above `GfxObjectBase`.

For a normal stroked path, the specific field could be:

    typedef struct {
        GfxObjectBase base;
        GList        *coordinates;
        GdkRGBA       color;
        gfloat        arrowsize;
        ArrowType     arrowtype;
    } GfxObjectPath;
    
For a text object, one could have:

    typedef struct {
        GfxObjectBase base;
        GdkRGBA       color;
        gchar        *text;
        gfloat        fontsize;
        gfloat        x, y;
    }  GfxObjectPath;
    
A path that fades away after a certain time could look like so:

    typedef struct {
        GfxObjectBase base;
        GList        *coordinates;
        GdkRGBA       color;
        gfloat        arrowsize;
        ArrowTyype    arrowtype;
        guint64       start_fading;  // timestamps in
        guint64       end_fading;    // microseconds
    } GfxObjectFadingPath;

### Capabilities

Each `gfx_object` contains a bit mask indicating its
"capabilities". These capabilities imply that certain actions are
implemented in the `do_action` function.

<table>
<tr>
   <td> <b>Capability</b> </td> <td> <b>Description</b> </td>
</tr>
<tr>
   <td> draw              </td> <td> object can be <b>drawn</b> <br>
                                     this would allow to add non-drawable "information" objects </td>
</tr>
<tr>
   <td> draw_transformed  </td> <td> object can be <b>drawn in transformed state</b>; <br>
                                     this allows for dynamic representation during operations such as dragging </td>
</tr>
<tr>
   <td> move              </td> <td> object can be <b>moved</b> </td>
</tr>
<tr>
   <td> isotropic_scale   </td> <td> object can be <b>scaled isotropically</b> </td>
</tr>
<tr>
   <td> anisotropic_scale </td> <td> object can be <b>scaled anisotropically</b>,<BR>
                                     i.e. differently in X and Y direction isotropic_scale also must be set </td>
</tr>
<tr>
   <td> rotate            </td> <td> object can be <b>rotated</b></td>
</tr>
<tr>
   <td> edit              </td> <td> object is <b>editable</b> (e.g. TEXT or parts of a path)<BR>
                                     this probably is hardest to implement, and left for the future</td>
</tr>
</table>


#### Use of capabilities with selections and during transformations

When multiple `GfxObject`s are selected, the capability bitmasks could
be `and`ed to obtain the greatest common denominator. For example,
objects may be movable, but not rotateable, and then the rotate handle
is omitted in the GUI. Similarly, isotropic-only scaling would leave
only the corner handles, whereas anisotropic scaling would also
display handles in the middle of the sides (see
[here](#selection-wireframe) for selection wireframe).

During transform operations, `draw_transformed` is called for each
object, with a 2D affine transformation matrix as argument.

If an object cannot be drawn dynamically in transformed state, for
example because this is computationally to costly, then the
transformation is just shown by selection wireframe until the
operation completed.

At the end of the operation, `transform` is called with the final
transformation matrix, which then modifies the object in the object
store.

The 2D affine transformation functions are already implemented in
`coordlist_ops.c`.

### Actions

When calling `exec`, the `action` argument selects the task to be performed.

<table>
<tr>
<td> draw             </td><td> draw object in "normal" state </td>
</tr>
<tr>
<td> draw_selected    </td><td> draw object in "selected" state </td>
</tr>
<tr>
<td> draw_transformed </td><td> draw object in intermediate "transformed" state; <br>
                                this allows to display the object dynamically, e.g. while it is being dragged </td>
</tr>
<tr>
<tr>
<td> update_bbox      </td><td> update the bounding box of the base object; <br>
                                trigger recalculation of the bounding box </td>
</tr>
<tr>
<td> get_is_clicked   </td><td> detect whether a click at coordinate (x,y) "hits" the object, for example to select it </td>
</tr>
<tr>
<td> transform        </td><td> transforms the object, modifying the context in the <tt>gfx_store</tt> </td>
</tr>
<tr>
<td> destroy          </td><td> destroys the object, deallocating memory </td>
</tr>
<tr>
<td> clone            </td><td> deep-copies the object </td>
</tr>
</table>
  

### Drawing of object store

Whenever objects need to be re-drawn, the `draw` function simply
iterates through the object store and calls `object->do_action(ptr,
draw_action)` for all objects. Objects with a bounding box not
intersecting the dirty region (`gdk_cairo_get_clip_rectangle()`) are
skipped.

### Undo/redo system

Each object that is added or transformed receives a new unique
`id`. This `id` is delivered by a "factory" function and is a simple
`guint32` that is incremented at each request.

Maintaining a per-user (i.e. per device) undo system is complicated by
the fact that an object can be "taken over" by another user by
manipulating it. This is necessary to enable a truly collaborative
workflow. At the same time, a user should only undo his/her own
operations.

To solve this conflict, I propose the following system:

- each device (i.e. user) contains an `GList` with `undo` and `redo` records.

- `GromitData` is extended with an undo object store (`undo_objects`).

- the steps necessary to revert a user's operation are recorded in the
  respective device's `undo` records.
  
- an undo operation executes the last undo operation according to the
  `undo` record of the device, and moves it to the `redo` record.
  
- a redo operation does the reverse, and moves the record back to the
  end of the `undo` records.
  
- any non-undo/redo operation clears the `redo` records.
  
The `undo` records contain the `id` of the object to move from
`objects` to `undo_objects`, and the `id` of the object to move from
`undo_objects` to `objects`. An `id` of zero indicates that there is
no such object.

- when a `GfxObject` is added, the `undo` record contains the
  instruction to move the `GfxObject` with the particular `id` from
  `object`to `undo_objects`.
  
- when a `GfxObject` is deleted, this object is moved from `objects`
  to `undo_objects`. The `undo` record consists of the instruction to
  add the `GfxObject` with the particular `id` back to `objects`.

- when a `GfxObject` is transformed, the object is deep-copied (action
  `clone`) is created and placed in `undo_objects`. The transformed
  object remains in `objects`but receives a new `id`. The `undo`
  record for the respective device contains the instruction to swap
  the old and new object (which are in the `undo_objects` and the
  `object_store`, respectively).
  
Handling of conflicts:

- Conflicts emerge when a user has "taken over" an object from another
  user. In this case, the objects referenced in the `undo` records no
  longer exist. In this case, this `undo` record is discarded and the
  next `undo` record processed.
  
  Example (here I use letters for the `id`):
  
  - user 1 adds objects A, B, and C:
   
```
      objects      -> A -> B -> C -> nil

      undo_objects -> nil

      dev1.undo    -> [A->undo] -> [B->undo] -> [C->undo]
      dev1.redo    -> nil
```

  - user 2 manipulates B, which becomes D:

```
      objects      -> A -> D -> C -> nil

      undo_objects -> B -> nil

      dev1.undo    -> [A->undo] -> [B->undo] -> [C->undo] -> nil
      dev1.redo    -> nil

      dev2.undo    -> [B->store, D->undo] -> nil 
      dev2.redo    -> nil
```

  - user 1 undos the last step:
  

```
      objects      -> A -> D -> nil

      undo_objects -> B -> C -> nil

      dev1.undo    -> [A->undo] -> [B->undo] -> nil
      dev1.redo    -> [C->store] -> nil

      dev2.undo    -> [B->store, D->undo] -> nil
      dev2.redo    -> nil
```  

  - user 1 undos one more step, which fails because B is no longer
    there; this step is discarded and the next step executed.
  
```
      objects      -> D -> nil

      undo_objects -> B -> C -> A -> nil

      dev1.undo    -> nil
      dev1.redo    -> [C->store] -> [A->store] ->  nil

      dev2.undo    -> [B->store, D->undo] -> nil 
      dev2.redo    -> nil
```
  
Notes:
  
- When adding back previously deleted objects from the `undo_objects`,
  they are placed at the very end of `objects`. This may change the
  Z-order of the objects. 
  
  **Question**: Should we add two more handles to the selection
  wireframe? One to move the selection forward, and one to move the
  selection backwards in Z-order?

- Orphans could occur in the undo store. One possibility would be to
  reference-count these objects and delete them when the last
  reference disappears from the undo/redo buffers, but that would
  require pointers or some related mechanism. The other, simpler,
  possibility which I prefer is to garbage-collect these once the
  `undo_objects` exceeds a maximum size.
  
- When `undo_objects` exceeds a maximum size, the oldest items could
  be dropped and the `undo`/`redo` records referencing these be deleted.

## Migration to new drawing system

### First step

* **Object store**

  The `objects` store is added to `GromitData`, together with some
  housekeeping functions.

* **Callbacks**

  Then, the callbacks are adapted to add data to the object store
  instead of `coordlist`. Direct drawing is replaced by a subsequent
  call of `draw`.

  - `on_press` adds a new item of the respective type to the object
    store, and "remembers" its `id` (for example, in the hash_table).

  - `on_motion` appends coordinates to the respective `GfxObject`, and
    calls `draw`.

  - `on_release` finalizes the operation, for example by smoothing a
    path (SMOOTH tool), and then calling `draw`.
       
  - for each tool, the specific drawing operation is implemented. The
    only drawing operation that is required at this first stage is the
    drawing of a simple stroke path.
      
These are relatively minor changes. With these, the new architecture
already is functional.

### Second step

The "undo/redo" functions are implemented.

### Thirs step

A "select" tool is implemented. This requires that the `GfxObject`
implement `get_is_clicked` and `draw_selected`.

The select tool draws a wireframe around the selection, showing the
respective handles (see "capabilities").

Then, a "delete" function is implemented. It basically calls the
"destroy" method and removes the selected items from the object store,
and then calls `draw`.

### Fourth step

Depending on the capabilities of the selected objects, suitable
transformations are offered. The simplest one is a "move" handle.

The items that implement "draw_transformed" are dynamically redrawn
during the "drag" operation. Upon releasing the "move" handle, the
tool calls the "transform" method, which modifies the coordinates of
the selecetd items in the object store.

Similarly, "scaling" and "rotation" operations are implemented by
adding extra handles to the selection wireframe. Everything else
remains the same.

## Selection wireframe

The selection wireframe could look like this:

        R                                                 R
      R                                                     R
    R   +---+                  +---+                  +---+   R
        | S |------------------| A |------------------| S |
        +---+                  +---+                  +---+
          |                                             |
          |                                ^    |       |
          |                                Z    Z       |
          |                                |    V       |
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
| Z      | move up/down selection in Z-order                                         |

