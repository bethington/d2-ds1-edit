=========================================================
   WIN_DS1EDIT.EXE : a WYSIWYG Diablo II DS1 (map) Editor

                Version : 30 October 2010
=========================================================

From "Microsoft Visual C++ 2010 Redistributable Package (x86)\README.txt" :

   This version of win_ds1edit.exe was compiled with Microsoft Visual C++ 2010, Express Edition.
   If the msvcr100.dll is not already installed on your system, you have to install it prior to be using the editor.

   You can simply use the vcredist_x86.exe provided here, or you can use the original installer at : http://www.microsoft.com/download/en/details.aspx?id=5555



various parameters can be changed in "Ds1edit.ini"
==================================================
(speed of scrolls, speed of mouse, paths to mpq, default gamma correction ...)
In case you delete this ini, the exe make a new default one

  +--------------------------------------------------------------------------------+
  | The first time you're launching win_ds1edit.exe, if your Diablo II directory   |
  | is not c:\program files\diablo II\ then you HAVE to edit the file ds1edit.ini  |
  |                                                                                |
  | For a first good help on how to use the editor, don't hesitate to check the    |
  | "On-line Documentation" link (see below), screenshots are there to help you,   |
  | as long as some basic "tutorials"                                              |
  +--------------------------------------------------------------------------------+



Download page :
   http://paul.siramy.free.fr/_divers/ds1/dl_ds1edit.html

On-line Documentation :
   http://paul.siramy.free.fr/_divers/ds1/doc/index.html

Advanced Tutorial :
   http://paul.siramy.free.fr/_divers/ds1/doc/tut01/index.html




Map editing : how to start
   http://d2mods.com/forum/viewtopic.php?t=29549

If trouble, try to check here :
   http://d2mods.com/forum/viewtopic.php?t=34449
   http://d2mods.com/forum/viewtopic.php?t=270

Main link (for history of releases) :
   http://d2mods.com/forum/viewtopic.php?t=44

General help can be found at :
   http://d2mods.com/forum/viewtopic.php?t=724#5563



GUI Loader (window application that make it easy for anyone to use the editor)
This GUI Loader was made by Mark Nevill ('DarthDevilous') :
   http://d2mods.com/forum/viewtopic.php?t=21281
   http://d2mods.com/forum/dload.php?action=file&file_id=1043
   http://paul.siramy.free.fr/_divers/ds1/ds1edit_loader.zip







Command Line Syntax, and Options
================================


Syntaxe :

   win_ds1edit <file> [options]

<file>
   This parameter is mandatory, and it MUST be the 1st parameter.
   It is either a DS1 or a INI file.

   If you want to open 1 and only 1 map, simply use the DS1 filename.
      Ex : win_ds1edit docktown3.ds1 20

   The <file> parameter can contain a path, either relative or absolute. It can also contain a drive letter.
   If the <file> parameter contain at least 1 space, it MUST be enclosed with quotes ( " ) characters.
      Ex : win_ds1edit "..\my mod\act 3\town\docktown3.ds1" 20

   If you want to open between 2 and 100 DS1, create a text file, with 1 to 100 rows.
   Each rows inside the INI MUST use this syntaxe :
      <LvlType_ID> <LvlPrest_DEF> <file>
   All fields are mandatory.  Check "lava_liste.ini" for a good example.
   Note : in a INI, <LvlPrest_DEF> can still be -1 for automatic detection, and <file> with spaces
          don't necessary have to be enclosed with quotes.


options are :
   * -debug
   * -no_vis_debug 
   * -force_dt1 <DT1 file 01> [<DT1 file 02> ... <DT1 file 32>] 
   * -resize <width> <height>
   * -no_check_act
   * -force_pal <act>
   * <LvlType_ID>
   * <LvlPrest_DEF>

ALL options can be used with a DS1 or a INI file.
If -force_dt1 is     present, you must NOT place <LvlType_ID> & <LvlPrest_DEF>.
If -force_dt1 is NOT present, you MUST     place at least <LvlType_ID> (<LvlPrest_DEF> is optional).
<LvlPrest_DEF> is optional in the command line, but not in the INI. If not present in the command line it'll be consider as -1. 
Each option can be placed in any order in the command line, so these 3 command line are valid and works exactly the same way with this map :

   win_ds1edit duriel.ds1 17 -debug -no_vis_debug  
   win_ds1edit duriel.ds1 -debug 17 -no_vis_debug 481 
   win_ds1edit duriel.ds1 17 -no_vis_debug -debug -1

   The first  numerical value is considered to be <LvlType_ID>, so it's 17 here.
   The second numerical value (if present) is considered to be <LvlPrest_DEF>, and if not present it'll be considered as -1 by default.


-debug
   You shouldn't have to use this option. If used it creates a "Debug" directory, and inside it creates some debug files about TXT files loaded by the editor.


-no_vis_debug
   If the parameter is present, the editor don't show the usual debuging infos on Vis Tiles that have graphics.
   It's only useful if you want to take a screenshot of a map, to get ride of the debuging Vis info that are drawn on top of stairs graphics.
      Ex : win_ds1edit WThrone.ds1 34 1086 -no_vis_debug 


-force_dt1
   This parameter makes the editor use your own DT1 files, instead of using LvlTypes.txt and LvlPrest.txt.
   For some few and special maps that are not in LvlPrest.txt you don't have the choice and you MUST use this parameter.

   DT1 files must be extracted from MPQ files prior to be used with the editor with this parameter.

   Note : Forcing DT1 files makes the <LvlType_ID> and <LvlPrest_DEF> useless.
          If you use the -force_dt1 parameter, the <LvlType_ID> and <LvlPrest_DEF> parameters must NOT be present in the command line. 

   As with the <file> parameter, EACH <DT1 file> parameter can use a path, spaces, driver letter... and must be enclosed with quotes if there's at least 1 space.  


-resize
   Makes the DS1, (warning : or ALL DS1 in a INI !!) to be these dimensions, in number of tiles.
   It's normally used to increased the size of a DS1, but it can also be used to decrease it.
   Of course, if you decrease the dimensions, Tiles and/or Objects will be lost.

      Ex : win_ds1edit gravey.ds1 2 108 -resize 50 100

   It will open the Graveyard, and resize it to be 50 * 100 tiles, instead of its original 25 * 33 dimensions.
   Note : in Levels.txt you'll have to put 49 and 99 for its dimensions (both -1 compared to DS1 number of tiles).


-no_check_act
   Makes the editor accept using a DS1 (or all DS1 in a INI) that have an internal Act value different than the Act of the LvlTypes it's using.


-force_pal
   This parameter must be folowed by a numerical value, from 1 to 5.
   It can be useful whith some act 5 lava DS1 that are using act 4 tiles and therefore act 4 palette colors.
   It's usually used in conjonction with the -no_check_act parameter.
      Ex : win_ds1edit lava_liste.ini -no_check_act -force_pal 4


<LvlType_ID>
   Indicates which row of LvlTypes.txt to use with a given DS1.
   Always mandatory, either directly in the command line or in a INI file. 

<LvlPrest_DEF>
   Indicates which row of LvlPrest.txt to use with a given DS1. It is optional in the command line (in this case it'll be
   considered to be -1), but it is mandatory in a INI. Note : in a INI you can use again that -1 value.

   Note : -1 is a special value, which makes the editor try to find itself the good row of LvlPrest to use for a given DS1, based on its filename. 
   Ex :

      win_ds1edit duriel.ds1 17 481
      win_ds1edit duriel.ds1 17 -1
      win_ds1edit duriel.ds1 17

      it'll works the same, because duriel.ds1 is present only 1 time in LvlPrest.txt, so no problem to find its row.


      win_ds1edit tri_town4.ds1 11
      win_ds1edit tri_town4.ds1 11 -1

      it won't works, because tri_town4.ds1 is present 2 times in LvlPrest, so the editor can't decide which row is the correct one.




Main window
===========

ESC            = quit

TAB            = change current Edit Mode (Tile / Objects / Paths)

F1, F2         = toggle floor layers

F3             = toggle Animations layer
                 (3 modes : none, freeze, animate)
                 Walls can hide objects so you may miss some of them, but in
                 the Object Editing Mode this problem can't occur.

F4             = toggle Object infos layer
                 (4 modes : none, "Type,Id", "Animation Speed", "Description in Obj.txt")

R              = Refresh the obj.txt (it'll read it again), and therefore update the
                 animations. Useful while editing the obj.txt to see immediately
                 the modification results in the editor. You edit the obj.txt in MS-Excel,
                 save it, swap back to the editor, press 'R' and your change is there,
                 without having to quit and re-launch the editor.

F5, F6, F7, F8 = toggle wall layers

F9             = toggle Special Tiles drawing priority
                 (2 modes : draw like walls, draw on top of all gfx like roofs)
                 It allows seeing Special Tiles hidden by Walls and Roofs.

F10            = toggle npc's paths layer

F11            = toggle Shadow layer
                 4 states :
                     * inactive
                     * normal (opaque)
                     * white (easy to view)
                     * transparent (DEFAULT)
                 This layer is draw after all the floors layers, and before
                 all the walls layers

SPACE          = toggle walkable info layer, 3 states :
                    * inactive (default)
                    * simple infos (walkable & jump infos only)
                    * complete infos. In this state a little help window
                         appear at the bottom / right corner, it's toggleable by
                         the T key.

G              = toggle the Tile Grid, 3 states :
                    * inactive (default)
                    * over floors but under walls
                    * over floors and walls

SHIFT + G      = same, but change Tile Grid states in reverse order

SHIFT + F1 to F2, SHIFT + F5 to F8, SHIFT + F11
   view only this layer

CTRL + F1 to F2, CTRL + F5 to F8, CTRL + F11
   view all floor & wall layers except this one

Backspace      = reset all layers states to their default value

1, 2, 3, 4, 5, 6, 7, 8, 9, 0 (up of your keyboard - NOT the keypad)
   change the current ds1 you're editing. Check at the end of this file,
   at the "Multiple ds1 loading" section.

CTRL + 1, ..., CTRL + 9, CTRL + 0 (up of your keyboard - NOT the keypad)
   Change the current SET (group) of ds1. When more than 10 ds1 are loaded
   (via a .ini) the editor split them into 10 different sets, from set 1 (CTRL + 1)
   to set 10 (CTRL + 0). In each set you have the usual 10 slots of ds1.

   For instance, to copy tiles from DS1 #22 to DS1 #84 you'll do :

      * CTRL + 3 (enter SET 3, it contains DS1 #21 to #30)
      * 2 (activate DS1 2 of SET 3, so it's DS1 index : 10*(3-1)+2 = 20 + 2 = 22)
      * select your tiles and copy them (CTRL + C)
      * CTRL + 9 (enter SET 9, it contains DS1 #81 to #90)
      * 4 (activate DS1 4 of SET 9, so it's DS1 index : 10*(9-1)+4 = 80 + 4 = 84)
      * place your tiles

   Note : when more than 10 ds1 are loaded, the menu bar contains 2 rows, as an
          additional row appear to help you to navigate through the different ds1.
          This 2nd row shows you the current SET, and current DS1 you're on, as well
          as the full path of the ds1.

TILDE = Hide / Show the 2nd row of the menu bar (it's activated by default when a .ini
        was used to load all the ds1).

        F12 = more gamma correction
SHIFT + F12 = less gamma correction

Up, Down, Left, Right         = scroll (speed in the ds1edit.ini)
Mouse on the border of screen = scroll (speed in the ds1edit.ini)

Home = center the map

C, Mouse button 3 (usually the wheel when pressed)
   Center the map to the tile (or sub-tile) the mouse is pointing. It also
   automatically set the zoom to the value defined in ds1edit.ini right after.

+ - (keypad)
- = (keyboard - NOT keypad))
Mouse wheel
   zooming in or out. Available zooms are 1:1, 1:2, 1:4, 1:8 and 1:16

P = Print screen (screenshot)

SHIFT + P
   BIG screenshot. This will make a .bmp that contains the entire ds1 image, not
   only the visible screen : how to dump the entire ds1 into 1 image in 1 second


Left click (maintain pressed) = select area of tiles
this is when the left button is not pressed anymore that theses
keys are checked, for the action to do on the area : 
   no key = make a new selection
   SHIFT  = add the area to the previous selection
   CTRL   = remove the area from the previous selection
   H      = Hide the current visible layers of the area (useful to see behind)

   The selection is base on WHAT YOU SEE. By changing the
   LAYERS TOGGLE [F1 to F8, or F11], you can select/deselect a floor,
   a wall, a shadow, multiple floor/walls/shadow, etc... 

   Tips : you can press the left button, then toggle your layers, then
   make your area selection just like you want, then change the layers toggle,
   press the action key to do (shift, ctrl or H) and released the left button
   only at the end. Most of the command still work while selecting an area.

I
SHIFT + I
CTRL  + I
    A powerful tool : select all Identical tiles in the map, as the ones you're
    clicking the mouse onto.

    Choose a floor, press the 'I' key and maintained it pressed, click on the floor,
    and all the same floors on the whole map are selected (even if they're on another
    layers). This can work even if there are several tiles on the cell, in that case
    you'll have a selection composed of several type of tiles.

    Another useful thing is that you can still use the Shift and Ctrl key to
    respectively add all these tiles to the current selection, or delete them all from
    the selection : choose a cell which have only 1 floor, select all the same floors
    with the 'I' key. Now, choose another floor, but press Shift + 'I' : you'll have
    in your large selection 2 type of floors now. Press Ctrl + 'I' and click on one
    floor of the selection, and you'll have only 1 type of floor in the selection.

    The tiles type that we'll be search are based on the visibility of the layers, but
    the search in the whole map to select them is done even on hidden tiles.

    Useful to have an idea about the density of some tiles in the map too.


S = Show (opposite of Hide) all tiles of all layers

CTRL + S = save the ds1, in its current state.
   Incremental backups. First, it renames the current .ds1 to the first free name
   with numbers it found, then it save the current map.

   For instance, say that you are editing the "duriel.ds1" for the first time.
   When you'll ask to save the map, it'll rename "duriel.ds1" to "duriel-000.ds1"
   and save the current state of the map into the "duriel.ds1" file.

   Next time you'll make a save, it'll rename "duriel.ds1" to "duriel-001.ds1"
   and save the current state of the map into the "duriel.ds1" file.

   So, the "duriel-000.ds1" is always you original .ds1, and the next numbers
   are the following versions. You're last version is always the "duriel.ds1",
   without the numbers in the name. Numbers are for backups.


CTRL + U = Undo precedents TILES modifications, up to the very start.
   Note that when you make some multiple undo and then start to make new
   modifications, the undo buffers are reseted : the ones you have just used
   to get back to the current state of the map are lost. Also you can't undo
   an undo : there's only 1 way of undo.

N = Night preview. 2 modes : lowest quality, better quality. This is just a toy.
    While in Tile / Object / Path mode, press N and you're in the Night preview
    lowest quality. Press again N and this time you're in the better quality.
    To exit one of the 2 Night preview modes, press Tab, or press N until you goes
    back the original edit mode.


COPY / PASTE
------------
CTRL + C = copy the current selection. Right after you have pressed
these keys, a preview appears, following your mouse movements. The red
part of this preview tell that you will erase previous tiles at that
particular place, green part indicate that the tiles are the same or
there were no tiles before.

If you try to copy / paste some tiles from a ds1 to another, you can expect
big distortions. If you want to avoid them, only copy / paste between ds1
that have the exact same Type Id (in lvlTypes.txt), and the exact same
Def Id (lvlPrest.txt). This is the way the game use tiles that make
difficult the use of certain tiles in another ds1.


CUT'N PASTE (same as *MOVING* Tiles)
------------------------------------
CTRL + X = Cut and Copy the selection.
This works almost the same way as the COPY / PASTE, except that instead of CTRL + C
you use CTRL + X. You'll move the SELECTION OF TILES around with the mouse : this time
it isn't a *COPY* that you're moving, it's the original selection of tiles. There are
still green and red tiles to indicated where are conflicts.


DELETE
------
DEL (keypad)
DEL (keyboard) = delete tiles
    select some tiles, even in different layers, then press a DEL key to delete them.


"STRANGE TILES"
---------------
they just have 4 lines around the cell and have numbers. This is for debugging
purpose : such tiles are either tiles that weren't found in any .dt1, or they are
tiles that have the flag Hidden set, and therefore my editor don't display it
completely (but if you right-click on them you can see the original tile).





Right click = edit the current tile
===================================

click the OK or the CANCEL button to exit this edit screen.

The Wall1......Floor4 change the tabs, and jump you directly to the tile
if there is one in this layer, else to the upper-left corner of the tiles list.

The 'Shadow' button works the same way, except that it appear only if there's
some shadow tiles in the dt1 list of the ds1, else it's not here at all.

click on the tabs to see other type of tiles

F9 toggle the Special Tiles drawing priority

F12 (and Shift + F12) change the gamma correction

F11 (and Shift F11) change the display mode of the shadows

the arrows scroll the tiles, mouse on the screen's border too.

Home return you to the left border

Enter has the same effect as the OK button

Page Up & Page Down scroll the tiles by 1 page, vertically
Ctrl + Page Up & Ctrl + Page Down scroll the tiles by 1 page, horizontally

The "special" tiles in the 'walls' buttons are important, do not delete them
from the ds1, they are the entry points of the map, corpse location, TP
location, corner of an area where some layers become invisible when the
player goes inside...

The names of the DT1 where the FIRST Tiles of each line are coming from are on the
left side of the 1st Tile column, just scroll to left a little, you'll end to see them.





CTRL + SHIFT + Right-Click = Expert Tile Editing
================================================
When you're on a tile, do this combination, and a special window will appear.
In this one you can edit the datas that are in the ds1, even if they're not
editable in the editor in another way. You'll have to edit them at a bit level.
It's as if you were hexediting a ds1.

Each line is 1 layer in the map:

F1, F2         = Floor 1, Floor 2 layers
SH             = Shadow layer
W1, W2, W3, W4 = Wall 1, Wall 2, Wall 3, Wall 4 layers
SUB            = Substitution layer (only present in very few ds1)

Some bits / group of bits are identified, so you have some text to help you, but
some other bits are still unknown.

The 2 important columns are "Hidden" and "Unwalkable", but you can edit the other
datas too.

Vis0 to Vis7 (warps) that don't have gfx must be hidden, or else the game will display
the infamous Green Tile when you'll play the map. Set the bit to 1 for the layer that
has the Vis, and the Green Tile will disappear.

The "Unwalkable" bit makes the entire Tile unwalkable, without editing any DT1. You can
use the Unwalkable infos to help you (Space Key before entering this window).

==> there are 2 possible window, depending if there's a tile selection or not. The White
numbers indicates the bit value is the same for all tiles of the selection. The Blue
numbers indicates the bit value is different, depending of the tiles inside the selection.
A Green minus ('-') indicate that this bit is not editable, because no tiles of the
selection is in this layer.

==> there's no undo possible, so take care.





In the 'Object' editing mode
============================

Objects are in the 'Obj.txt' file, open it with Excel or D2Excel if you're
planning to edit it, there's Tabulations that mustn’t go away.

Objects are displayed as 2 parts : a Label with infos, and the Feet
(the object itself). The label don't exists in the game, it's just internal to
this editor, so when you'll reload a previously edited ds1, the labels will be
in their standard place. By moving label you can see behind, and select the
objects that may be behind. Selecting an Object require you to target the feet,
and if you move it, the label follow (the reverse is not true).

The infos of the label are :
   * 1st line   : description
   * 2nd line...
      - #nn     : object number
      - Type-Id : Type & Id of the object
      - Pnn     : number of paths for this object

Right click over 1 object / label : edit its type and Id (change object)
   while in this mode : moving the mouse at the top or the bottom of the
   'Object List' Window scroll it (speed of this scroll is in ds1edit.ini,
   at the 'obj_edit_scroll = ' line.

   Type 1 objects are NPC & Monsters, Type 2 are usually objects, but
   sometimes NPC (Hratli in act 3 for instance).

   Type 1 objects can be in all acts, but not the Type 2, hence the
   desactivation of the other acts in the list when it's a Type 2 object.

   You can press the Enter key instead of clicking the OK button (either the
   one on the Keypad or the regular one)

   You can also press the Up & Down keys, to scroll the object's list.
   Note that the line "winobj_can_scroll_keyb" in the ds1edit.ini must be set
   to "NO" to enable this scrolling.

   While this window is display, the options defined in the ds1edit.ini takes
   effects : you will be able to scroll the *main Tile* window or not. Keyboard
   and mouse scrolling are 2 distinct options in the .ini


Insert             : insert (add) a new object

Left Click         : select an object / label, or deselect all if not over an
                     object / label

Shift + Left click : add an object / label to the selection, but you can't
                     select objects AND labels, that's one or the other.

Ctrl + left click  : remove from the selection


Posible actions to the selection are ...

Alt (or AltGr) + Left Click on the selection : start moving the selection,
         you can release the Alt just after. Left click to drop it.

DEL (keyboard)
DEL (keypad) : delete the selection (only if objects)

Ctrl + C : copy the selection (only if objects)

Note : right after an unwanted modification, you can Undo it : Ctrl + U. It
   works after a move / delete / copy / insert, but not a selection modification.
   there's just 1 step of undo for Objects, multiple undo don't works for them (yet).

F3  : still toggle the animation layer state

I   : like in Tile Editing Mode, you can select all identical Label / Objects
      in the map with just 1 click.





Quick loading
=============
The 1st time the Editor is launched, it extracts the palettes from the Mpq,
and make the appropriate color maps at run-time, then it save them all. Next
time you want to edit a map, since the palettes and the colormaps are on your
disk already, the editor just load them, which is a lot quicker. Theses files
are in the directory 'data' of the Editor, and they all have the .bin extension.
If the Editor don't find them at startup, it re-creates them at run-time.





Multiple ds1 loading
====================
you have to make a special text file. Look at the 'Demo.ini', it's an example.
This one open Tristram, then a little special map (for trees), then Duriel. The
first 2 numbers are Type Id & Def Id (which can be -1, see below), in that order,
then there's the file. Now launch '_Multiple ds1 demo.bat' : it give this ini file
to the editor, and redirect the standard output to a file.

When working with multiple ds1 at the same time, as said before, copy / paste is
to use carefully. In our exemple, Tristram and the Trees have the exact same Type &
Def Id, so they have the exact same list of dt1, so you can safely copy / paste
between this 2 files. Now, Duriel have a complete different sets of dt1, so if
you try to copy a House from Tristram and paste it into Duriel, you'll get nothing
interesting : the editor (like the game will) remap the tiles of tristram with the
dt1 list of Duriel, hence theses weird tiles. Same problem when copying tiles from
Duriel to Tristram of course.

You can open up to 100 (one thousand !) ds1 at a time, so 100 lines max in the special
test file (you can use the name you want btw, like a full path).





Working with Isilweo's ds1 editor
=================================
Isilweo's ds1 editor don't support all .ds1 in the game as there are many version
of them, and the editor was done when we didn't know how it was working. If you try
to open an old ds1, it won't work in Isilweo's editor.

An easy way to solve this is to use my editor, as it can read all kind of versions
of ds1, and save the maps right after, as it always save them in the last version (18),
which is the one the Isilweo's ds1 editor understand too.

Also, my editor always make the map with 2 Floor layers and 4 Wall layers, even if
they just have a few originaly. So it's useful to copy/paste tiles from different .ds1
that didn't have the same number of layers at first. This feature can be disable by a
line in the ds1edit.ini.





Resizing a map, without Isilweo's ds1 editor
============================================
It's possible, it's an option in the command line. For instance the line

   win_ds1edit tri_town4.ds1 11 300 -resize 250 100 > debug_tri_town4.txt

will resize the ds1 of Tristram to 250 width * 100 height tiles. Increasing a map is
simple, the editor just adds some empty tiles, but decreasing a map delete the
objects that are now out of the bound of the map, take care of this ! The paths of a
NPC can be out of bound too. If this happen edit them, or delete the original NPC,
that way his paths will be deleted as well. Now you can insert a new object to replace
the one you deleted.

Note : in Levels.txt / LvlPrest.txt, the size that you must enter are minus by 1. So
if you were trying to use that new Tristram, you'd enter the size 249 and 99 in the .txt




Paths editing
=============
The paths editing is still very basic, but with patience it's posible to do what you want.

    you select 1 and only 1 object in the Object Editing mode. It can be either a
    Type 1 or a Type 2 object, but only Type 1 (Monster / NPC) objects can use paths in the game.
    You then click on the ALL NEW button to delete the current paths and create new ones, by
    clicking on the map. To change the (hardcoded) "Action" (animation and / or sound) that the
    NPC will do at a particular place, click on the (Enter) SET ACTION button and type a number
    in the keypad, then press the Enter key and the next paths that you'll place will use that
    new Action value. When you have finished, click on the OK button, and the NPC have now whole
    new Paths. CANCEL will restore the original NPC paths (if any).

Note : never place objects at the same coordinates if at least one of them has paths. If the case
       happen, the editor will delete the paths the next time it'll load the DS1, this is to
       avoid a crash in the game.
