# ds1edit — Tutorial 1: Warps, Roofs, and Walkable Info

> ## Origin
>
> **Original author:** Paul Siramy (`siramy_paul@yahoo.com`)
> **Original page:** `http://paul.siramy.free.fr/_divers/ds1/doc/tut01/index.html`
> — "Ds1 Editor - Tutorial 1"
> **Archived copy (do not edit):**
> [`docs/preservation/siramy/paul.siramy.free.fr/_divers/ds1/doc/tut01/index.html`](../preservation/siramy/paul.siramy.free.fr/_divers/ds1/doc/tut01/index.html)
> (sha256 `5981681b…182d5d`, retrieved live, per `docs/preservation/MANIFEST.tsv`)
> **Original dates (from the page's own meta tags):** created **2003-07-29**,
> last revised **2004-07-01**.
>
> > **Date contradiction.** `docs/preservation/README.md` dates this page
> > **2011**. The file's own `Date-Revision-yyyymmdd` meta tag says
> > **20040701**, and the parent documentation page's revision history
> > independently records "29 Jully 2003 : Started Tutorial 1". The README's
> > date appears to be capture- or mtime-derived, not a content date. **The meta
> > tag is authoritative; 2011 is wrong.** (The README is owned elsewhere and was
> > not edited from here.)
>
> **Changed here:** converted from HTML to markdown; stray `<title>` text removed
> from the body; the page's own section headings restored as real headings and a
> working table of contents added; the `LvlWarp.txt` column table — which the
> original conversion had flattened into ~110 lines of one-value-per-line
> fragments — rebuilt as a table; the `txt_and_ds1.zip` filename restored; dead
> download links marked unavailable instead of left as `(#)`; verification notes
> added where a claim about the **DS1 format** could be checked against this
> repository's own parser. Siramy's wording, his warnings, and his admissions of
> what he did not know are unchanged, including his spelling.
>
> **Rights status: the position taken.** No page in this archive carries a
> licence, a copyright notice, or a republication grant. The project proceeds
> on an explicit fair-use judgment rather than seeking the author's
> permission, recorded with its reasoning in
> [BOOK-STATUS.md](../BOOK-STATUS.md). If Paul Siramy objects, the terms
> change.
>
> **Version scope.** This book's baseline is **1.13c** — every claim below is
> written as true of 1.13c unless a callout says otherwise. Siramy states below
> that this tutorial "was made with the **Patch 1.10 Beta**", and the page was
> last revised **2004-07-01**; that dating is his own and is left as he wrote
> it. Everything describing **what the editor's UI did** is historical
> description of a 2004 build of `win_ds1edit` and is **unverifiable** — that
> binary is not in this repository. Claims about the **DS1 file format** and
> the **`.txt` table columns** are separable from the tool: they have now been
> re-checked against genuine **vanilla 1.13c archives** (`Patch_D2.mpq` /
> `D2Exp.mpq`, read with `tools/d2mpq.py` — see
> [`docs/vanilla-data.md`](../vanilla-data.md)), not against this repository's
> PD2-derived `assets/excel/` cache that the previous pass used. Every one of
> the data claims re-checked this way matched Siramy's numbers exactly, with
> one correction (the `LvlWarp.txt` `l`/`r` pairing count) and one
> reclassification: the previously reported "Id 83 is taken" conflict turns out
> to be a Project Diablo 2 artifact, not a Blizzard version difference. See the
> `## Version differences` table at the end and the callouts inline.
>
> **On the example ZIPs.** The downloads named below are not redistributed with
> DS1Edit: they contain Diablo II data. They were part of Paul Siramy's original
> release, at <http://paul.siramy.free.fr/_divers/ds1/dl_ds1edit.html>. None of
> them were archived, and the host is gone.

---

Back to the main [**Documentation Page**](../getting-started/manual.md)

## Contents

- [Overview](#overview)
- [Get all the files](#get-all-the-files)
- [Create a new Mod directory](#create-a-new-mod-directory)
- [1 ds1 for the 4 Town Variations](#1-ds1-for-the-4-town-variations)
- [A new Trap Door](#a-new-trap-door)
- [Trap Door in Town and Graveyard](#trap-door-in-town-and-graveyard)
- [Connect Act 1 Town to Graveyard](#connect-act-1-town-to-graveyard)
- [A new Tent Warp](#a-new-tent-warp)
- [1-way Warp to Tristram](#1-way-warp-to-tristram)
- [A new House in Town](#a-new-house-in-town)

## Overview

This tutorial will explain step by step how to work with some advanced stuff of Map Editing :

*   Adding 3 different kind of Warps between levels
*   Adding a house with Roofs that disapear when the player enter it
*   Resizing a preset level
*   Deals with walkable infos of some tiles

It was made with the Patch 1.10 Beta.

## Get all the files

First, let's extract these files from the MPQs :

*   from Patch\_D2.mpq :
    *   Data\\Global\\Excel\\ Levels.txt
    *   Data\\Global\\Excel\\ LvlPrest.txt
    *   Data\\Global\\Excel\\ LvlTypes.txt
    *   Data\\Global\\Excel\\ Objects.txt 
*   from D2Exp.mpq :
    *   Data\\Global\\Excel\\ LvlWarp.txt
    *   Data\\Global\\Excel\\ ObjType.txt
    *   Data\\Global\\Tiles\\Act1\\Town\\TownE1.ds1
    *   Data\\Global\\Tiles\\Act1\\Tristram\\Tri\_Town4.ds1
    *   Data\\Global\\Tiles\\Act2\\Town\\LutN.ds1  

*   from D2Data.mpq :
    *   Data\\Global\\Tiles\\Act1\\Barracks\\JailEWarpNext.ds1
    *   Data\\Global\\Tiles\\Act1\\Caves\\CaveDR1.ds1
    *   Data\\Global\\Tiles\\Act1\\Caves\\CaveEDown1.ds1
    *   Data\\Global\\Tiles\\Act1\\Caves\\CaveESpec.ds1
    *   Data\\Global\\Tiles\\Act1\\Caves\\CaveNSpec.ds1
    *   Data\\Global\\Tiles\\Act1\\Caves\\CaveSSpec.ds1
    *   Data\\Global\\Tiles\\Act1\\Caves\\CaveWSpec.ds1
    *   Data\\Global\\Tiles\\Act1\\Graveyard\\Gravey.ds1
    *   Data\\Global\\Tiles\\Act1\\Outdoors\\Cott1.ds1

If you prefer to don't extract them yourself, then take this zip : **txt_and_ds1.zip** (68 KB), it has them all. *(Download no longer available — the ZIP was not archived, and it contained Diablo II game data that is not redistributed here.)*

## Create a new Mod directory

Now, prepare a new Mod directory : in your Diablo II directory, if you already have a Data directory, then rename it to something else (like my\_old\_data for instance). Now, create a Data directory, inside this one create a Global directory, inside this one create an Excel directory as well as a Tiles directory. Your Diablo II directory should now looks like :

*   Diablo II
    *   Data
        *   Global
            *   Excel
            *   Tiles

Put the 6 .txt in this Excel directory (Levels.txt , LvlPrest.txt, LvlTypes.txt, LvlWarp.txt, Objects.txt and ObjType.txt). They are the .txt that you'll edit later. Now in this Tiles directory, create Act1, and inside this directory create Town and Graveyard. Your Tiles directory should now looks like this :

*   Tiles
    *   Act1
        *   Graveyard
        *   Town

You'll put later TownE1.ds1 in the Town directory and Gravey.ds1 in the Graveyard directory, but just left them for now.

Now that we have a Mod Directory, update the Ds1edit.ini of the Ds1 Editor : the line mod\_dir should now point to your Diablo II directory, like for instance :

> mod\_dir = C:\\Program Files\\Diablo II

This way the Ds1 Editor is now able to use the exact same .txt than the game.

## 1 ds1 for the 4 Town Variations

We'll now avoid us some works, as we just want to edit 1 ds1 of the Rogue Encampment, not to do the same changments in all the 4 variations (North, South, East and West). So, let's open (from your Mod Directory) the file LvlPrest.txt in Excel. The line Act 1 - Town 1 has 4 different files in the columns File1 to File4, they are :

*   File1 \= Act1/Town/TownN1.ds1
*   File2 \= Act1/Town/TownE1.ds1
*   File3 \= Act1/Town/TownS1.ds1
*   File4 \= Act1/Town/TownW1.ds1

Copy / paste the File2 value into the File1, File3 and File4 columns. Now, whichever the orientation the game will want the Town to be, it'll take in all case the ds1 Act1/Town/TownE1.ds1, the one we'll edit soon. Save, and close LvlPrest.txt.

> **Verified against vanilla 1.13c** (`Patch_D2.mpq\data\global\excel\LvlPrest.txt`,
> read with `tools/d2mpq.py` — see [`docs/vanilla-data.md`](../vanilla-data.md)):
> the row `Act 1 - Town 1` (Def 1, LevelId 1) carries File1–File4 =
> `Act1/Town/TownN1.ds1`, `TownE1.ds1`, `TownS1.ds1`, `TownW1.ds1` — exactly the
> four Siramy lists, in that order. This repository's checked-in `assets/excel/`
> is a Project Diablo 2-derived copy of the same table and matched too, but
> every data claim in this chapter is now cited against the vanilla archive,
> not that cache.

## A new Trap Door

Before trying to make a new trap Door, check that tutorial I made for the Phrozen Keep:  
   [Adding ANY Monsters and ANY Objects to a DS1 (by Paul Siramy)](../guides/monsters-and-objects.md)  
This is an easier alternative, if you're using the patch 1.10.

But If you prefer to go the hard way : in the win\_ds1edit directory, put the file LutN.ds1 and let's make a .bat to open it with the editor.

Lut\_Gholein.bat :

> @echo off  
> win\_ds1edit LutN.ds1 12 301 > log.txt 

Launch the Lut\_Gholein.bat . If all was correct you're now able to view the Act 2 Town. Go to the upper part of the Town, near Greiz :

|  |  |
|---|---|
| ![](../assets/images/tut01_01.gif) | ![](../assets/images/tut01_02.gif) |
| Here is the Trap Door that we'll use in the Rogue Encampment and the Graveyard to connect them. | This Warp is composed of 2 elements : an invisible Vis (here the Vis 2), and the Trap Door object over it, right in the middle. |

There's just 1 problem : the Trap Door is an object of Act 2, and we want to use it in Act 1. We'll solve this by using an object of Act 1 and replace it by the Trap Door datas. We'll use the Inactive Cairn Stone. Of course that means that if you don't edit the file Data\\Global\\Tiles\\Act1\\Outdoors\\Cairn2.ds1 then when the player will go into the Stony Fields he'll find a Trap Door instead of the Stone , so you'd better have to edit the ds1 and remove that object :

![](../assets/images/tut01_03.gif)

We won't do the change in this tutorial, as it is left as an excercise to the reader, but to help you a bit, this .ds1 LvlType.txt ID is 2, and its LvlPrest.txt DEF is 160.

Now, let's edit our .txt :

*   As you can see on a precedent screenshot the Trap Door is ID 74. So, in Objects.txt, let's go to the line that has ID 74. It's the line 76 in MS-Excel, its name is TrappDoor, so no error, that's the good one.The Inactive Cairn Stone is ID 22 (StoneTheta), so copy the entire row of the Trap Door and paste it over this row that have ID 22.

*   Now in ObjType.txt, copy the Trap Door row onto the Stone 6 row (line 76 onto line 24 in MS-Excel). Save and Close ObjType.txt.

*   Finally, in the data\\obj.txt that you'll find in the win\_ds1edit directory, made the corresponding changes. You don't \*have\* to do it to make it works in the game, but it's better if we're in sync. A search for "Trap Door" in obj.txt gives the line 272 (Trap Door (74)), and a search for "Theta" gives the line 73 (Cairn Stone, Theta (inactive) (22)). So, copy the entire row 272 onto the row 73, and don't forget to change these columns back to their original values :

> *   Act    =  1
> *   Type \=  2
> *   Id     = 11
> 
> Close and save obj.txt.

> **Verified against vanilla 1.13c** (`objects.txt` and `ObjType.txt`, both read
> straight from the game's own archives with `tools/d2mpq.py`): Id **74** is
> `TrappDoor` on **line 76**; Id **22** is `StoneTheta` on **line 24** —
> matching the PD2-derived `assets/excel/objects.txt` cache the earlier pass
> used, exactly. `TrappDoor` also has `OpenWarp = 1` and `StoneTheta` has
> `OpenWarp = 0` — the value Siramy's note further down depends on. `ObjType.txt`
> (`D2Exp.mpq`) confirms the second instruction too: **line 76** is `Trap Door`
> (Token `TD`), **line 24** is `Stone 6` (Token `S6`) — exactly the "line 76
> onto line 24" pairing Siramy describes. That instruction was reported
> unverified in the previous pass, because `ObjType.txt` is not checked into
> this repository; reading it straight from the vanilla `D2Exp.mpq` settles it.
>
> `assets/editor/obj.txt` is a different file again — the **DS1 editor's own**
> bundled object table, not a game data table, so it is correctly and
> permanently absent from every MPQ; it is checked here as a tracked copy of
> the editor's own file instead, not against vanilla. In it, **line 272** is
> `Trap Door (74)` with Act 2, Type 2, Id 0, and **line 73** is
> `Cairn Stone, Theta (inactive) (22)` with **Act 1, Type 2, Id 11** — precisely
> the three values Siramy tells you to restore after the paste. Every line
> number on this page is exact.
>
> **Version note (1.09d):** the `objects.txt` `Name` column for Id 74 reads
> `Trap Door` (with a space) on 1.09d, not `TrappDoor`. By 1.13c — and evidently
> by the 1.10 beta Siramy used, since he reports seeing `TrappDoor` "so no
> error" — it reads `TrappDoor`. See `## Version differences`.

You now have a working Trap Door in Act 1, but no more Inactive Cairn Stone.

## Trap Door in Town and Graveyard

In the win\_ds1edit directory, put the files Gravey.ds1 and TownE1.ds1. Let's make 2 .bat to open them with the editor.

Graveyard.bat :

> @echo off  
> win\_ds1edit Gravey.ds1 2 108 > log.txt 

TownE1.bat :

> @echo off  
> win\_ds1edit TownE1.ds1 1 1 > log.txt

Launch the Graveyard.bat and you're now editing the Graveyard :

|  |  |
|---|---|
| ![](../assets/images/tut01_04.gif) | ![](../assets/images/tut01_05.gif) |
| First, press the Space key, in order to be able to view the walkables infos, this help to choose a good place. We'll place our Trap Door here, at Cell 5, 2 (check the coordinates on the bottom / left corner of the screen). | Right-click , press the Wall 1 button, click on the Special button, choose a Vis 7 and press the OK button. It has a problem tough : to avoid a Green Tile to appear in the game, we have to make this one Invisible, since it don't use any graphics. |

![](../assets/images/tut01_06.gif)

Press Shift + Ctrl + Right-click and we're in the Expert Tile Editing window. Here, since  
our Vis is on the Wall Layer 1, we set the Hidden bit of the line W1 to 1. Then press OK.  
We now have an invisible Vis in the game, so the infamous Green Tile won't appear.  
All that left is to place the Trap Door object itself onto this Special Tile.

![](../assets/images/tut01_07.gif)

![](../assets/images/tut01_08.gif)

Press Tab to go in Objects Editing mode, place the cursor in the middle of the Tile, press Insert and change the new object (with a right-click on the label) to be the Trap Door we have made before (Act 1, Type 2, Id 11 in the ds1).  
Press Ctrl + S to Save, then Esc to Quit.

Here are 2 screenshots of this  
new Warp, in-game.

*(Conversion note: that caption belongs to the figure `tut01_08.gif` directly
above — a single image holding the two in-game shots side by side. The original
page paired image and caption in an HTML table cell; the pairing is restored
here.)*

Note : that's because the Trap Door in Objects.txt has the column OpenWarp set to 1 that we can read the warp destination, but it isn't required : if you set this column to 0, then the trap Door will still work, but when selected its description will stay "Trap Door" instead of "To The Rogue Encampment", which can be usefull if you want to easily hide the destination name.

Now, let's do the same for the Rogue Encampment. Launch the TownE1.bat :

 ![](../assets/images/tut01_09.gif)

![](../assets/images/tut01_10.gif)

Go to Cell 25, 20, and as for the Graveyard place a Vis 7 there, makes it invisible, place the Trap Door object over it, Save and Quit.

This will be our Trap Door warp in Town.

Since we'll test soon that these 2 trap Doors are working, copy TownE1.ds1 into your Mod directory, at the location Data\\Global\\Tiles\\Act1\\Town, and Gravey.ds1 at the location Data\\Global\\Tiles\\Act1\\Graveyard.

## Connect Act 1 Town to Graveyard

Here's now your Golden Rule :  

* * *

**All the Vis present in the ds1 MUST be defined in Levels.txt**  

* * *

If we were trying our Mod now, before any Levels.txt editing, this Assertation would arise :

> Assertion Failure  
> Location : D2Common\\DATATBLS\\LvlTbls.cpp, line #1047  
> Expression : Error in DRLG related to stairs that connect levels

If this assert appear, check carefully all your ds1, write down the Vis numbers they're using, and in Levels.txt check that all of these Vis are linked to the correct level, using the correct Vis.

So, now let's edit Levels.txt, in order to link these 2 Trap Doors together. Go to your Mod directory, and open Data\\Global\\Excel\\Levels.txt there in Excel.

*   The Rogue Encampment is the line with Id 1 "Act 1 - Town", as you can verify if you check the column LevelName btw. 
*   The Graveyard is the line with Id 17 "Act 1 - Graveyard"  

If you don't link the Levels together correctly, this assertion can popup :

> Assertion Failure  
> Location : D2Common\\DRLG\\DrlgRoom.cpp, line #604  
> Expression : hDungeonRoomOther

With this one check in Levels.txt that you don't have linked a Vis to the wrong other Level : to be correctly linked, 2 levels must referenced each others. If you link the Vis 7 of Act 1 Town incorrectly to the Crypt, instead of the Graveyard, then this error appear since the Crypt is not referencing Act 1 Town in all its Vis.

Note : this can also have another meanings, as this part of code seems to make several type of checks. You need to know that the last column and the last row of tiles in a ds1 are not used by the game (they're overlapped by their adjacent ds1 at run-time). So if you try to put the Trap Door in the Cell 24, 0 of the Graveyard (the far right corner), since it's a forbiden place, the game can't place the Vis here, and this Assertion comes.

In addition, some Warps are composed of more than 1 Tile, like the stairs of the Caves for instance, so even if the Vis itself is not on a forbidden place, maybe one of the tiles that are changed at run-time for making the cave stairs hilighten \*are\* on such a forbidden place. So, as another rule :  

* * *

**Don't place Vis too close of the Borders of the ds1**  

* * *

What we want is to connect our 2 levels together, using our new Vis. Here, we have created the exact same Vis number (7) in the 2 ds1, but it's not required, it just helps us to keep things organized.

*   In the Vis7 column of the "Act 1 - Town" line, put the Id of the Graveyard (17).
*   In the Vis7 column of the "Act 1 - Graveyard" line, put the Id of the Act 1 Town (1).

This has taken care of the link between the 2 levels, but not the warps themselves. Vis are for the logical connection between levels (the columns Vis0 to Vis7), but all of these Vis have their corresponding Warp columns (Warp0 to Warp7). These Warps columns are ID of a line in LvlWarp.txt. 1 Warp in this .txt define the orientation of the warp, the area where the mouse must be in order to activate the warp, and it indicate if the game must replace some tiles at run-time to hilight the warp.

For us, it's easy. Since we have used the Trap Door of Lut Gholein, we just have to use the same Warp index. Check in Levels.txt the line "Act 2 - Town" (Id 40). The Invisilbe Vis under the Trap Door was a Vis 2, so get the value in the Warp 2 column, here 19.

> **Verified against vanilla 1.13c**: `Levels.txt` Id **40** is `Act 2 - Town`
> (LevelName `Lut Gholein`), and its **Vis2 = 47, Warp2 = 19**. `LvlWarp.txt`
> Id **19** is named `Act 2 Town to Sewer Trap` — the trap door Siramy is
> standing on. `Levels.txt` Id **1** is `Act 1 - Town` (`Rogue Encampment`),
> Id **17** is `Act 1 - Graveyard` (`Burial Grounds`), Id **38** is
> `Act 1 - Tristram`. All four level Ids on this page are correct. Both Act 1
> rows start with Vis6/Vis7 empty and Warp6/Warp7 = **-1**, which is the
> "forgot to set it" state he warns about two paragraphs later. These rows are
> identical on vanilla 1.09d too — Blizzard did not touch any of these fields
> across that span of patches.

So, for the Act 1 Town and the Graveyard, put 19 in the columns Warp7 of these 2 levels.

If you forgot to set a Warp column (and therefore left it to \-1), then you can have this assertion :

> Assertion Failure  
> Location : D2Common\\DRLG\\DrlgVer.cpp, line #109  
> Expression : FALSE

If you have a Vis, then you must fill its corresponding Warp column as well.

We have added Warps to the 2 ds1, we have filled their Vis and Warp columns, now we can test. Just use the usual \-direct -txt method, and with any Player go in the Act 1 Town, and the Trap Doors should work in both ways, without any warning or error. Don't try to go outside of the Town tough, since you're likely to have a bridge that go to a black wall : we have forced the use of only 1 ds1 for the 4 Town variations, so there's only 1 possible way... But there's a trap Door anyway, so you can escape.

This zip contains the files necessary to test our modifications so far : **trap\_door.zip** (52 KB). You'll find the ds1 of the Rogue Encampment and the Graveyard, and the 4 .txt of the game we have modified. It's a normal Mod that needs the -direct -txt method. *(Download no longer available — not archived, and it contained Diablo II game data.)*

## A new Tent Warp

This is taken from an idea of Kingpin, from [**this topic**](http://d2mods.com/forum/viewtopic.php?t=14484) in a Phrozen Keep forum.

![](../assets/images/tut01_11.gif)

As you see it's possible to make Warps from any elements of the maps. Here, not only we will recreate this warp, but we'll also improve it, as we'll make our own warp settings : area where the mouse must be to activate the warp, exact position of the warp to enter it, and location where the player is heading to when he 's coming out of the tent.

To make our own warp, we'll use LvlWarp.txt. We can add our own warps in this file, and once you understand what the columns are for it's not very hard to make them.

But before doing any .txt editing, let's find what will be our settings for this warp.

![](../assets/images/tut01_12.gif)

![](../assets/images/tut01_13.gif)

First, let's use the Ds1 Editor. Place a Hidden Vis 6 of orientation 11 on the Wall layer 2, exactly here. Move the Rogue and the Torch objects to somewhere else, Save, press the P key to take a screenshot, and Quit.

Open this screenshot in an image editor, like Paint Shop Pro. In the game a Tile is a group of 5 x 5 sub-tiles, as this screenshot show.  

> **Verified.** `sub_tiles_flags[25]` (`src/structs.h:1175`) and
> `max_subtile_width = new_width * 5` (`src/core/ds1.c:960`) fix a tile at
> 5×5 sub-tiles in the DS1/DT1 format itself — a structural constant of the
> file format, not a value from the Excel tables, so it does not vary between
> vanilla and any mod and needed no re-check against the vanilla archive.

We now have to decide which sub-tile will be our base : this is the sub-tile where the player appear when he exit the warp.

![](../assets/images/tut01_14.gif)

![](../assets/images/tut01_15.gif)

It's better if we choose the center of the tent entrance, on the ground level, so, here, let's choose this one, at coordinate 2,1 in the sub-tile coordinates system of this Tile.  

Note : this sub-tile can be far away of the Vis, not necessary inside the Special Tile. The warp Act 2 Sewer Dock to Town is like that.

Let's define the sensitive area we want for the tent warp. We draw a box that include exactly the tent entrance.

|  |  |
|---|---|
| ![](../assets/images/tut01_16.gif) | ![](../assets/images/tut01_17.gif) |
| If we take the bottom of the Base sub-tile as a reference, and consider it the base of the mouse coordinates system, then the upper / left corner of the mouse area is the point A, at coordinates \-20, -111, in pixels. | The Width (W) and the Height (H) of the box is 46 x 112 pixels. |

![](../assets/images/tut01_18.gif)

The last data we need is the location from the **base** where the player  
will head to, when he'll exit the tent. Here, we'll make him walk to  
sub-tile 0, 5 from the base. We can make it far away, but in any case  
the Player has the possibility to stop the auto-walk by clicking elsewhere.

We can now edit LvlWarp.txt, since we have all the infos we need to make a new warp.

In this file, if we check carefully, we'll see that all warps have the column Direction set to b, except some of the Expansion, which have l and r. Coincidently, the warps that don't have b are a pair of warps with the same Id, but one with Direction l, the other with r. So, l stands for Left, r for Right, and b for Both. The Orientation of the Vis (10 or 11), will be used by the game to know which one of these 2 lines it'll use : if the Vis is an Orientation 10, this will be the line with the l Direction, else if it's a Vis of Orientation 11, the game will choose the line with the r Direction.

> **Corrected against vanilla 1.13c.** The previous pass checked this
> repository's PD2-derived `assets/excel/LvlWarp.txt` and found `b` on 80 rows,
> `l` on 6, `r` on 6, across six Ids. Reading the same table straight out of
> vanilla `D2Exp.mpq` gives a different count: `b` on **78** rows, `l` on **5**
> and `r` on **5** — the ten non-`b` rows are exactly **five Ids (71, 73, 74,
> 81, 82), each appearing once as `l` and once as `r`**. Vanilla's
> `LvlWarp.txt` runs only to **Id 82** (88 records total, Ids 0–82, with those
> five duplicated); the sixth pair the earlier pass counted, at Id 85, does not
> exist in vanilla — it is a row Project Diablo 2 added, not a Blizzard one.
> The table is byte-identical between vanilla 1.09d and 1.13c, so this is not a
> patch-version difference either — see the Mod note below and the
> `## Version differences` table. Siramy's observation that the non-`b` warps
> come in same-Id `l`/`r` pairs is exactly right, for five Ids on every genuine
> Blizzard patch checked here. The second half of his claim — that the game
> picks the row by the Vis's orientation 10 vs 11 — is a statement about
> **D2Common's runtime behaviour**, not about the table, and is **unverified**
> here; it was not checked in a disassembler.

Note : if you create a Warp that don't use the Direction b, then be sure to make 2 lines in LvlWarp.txt for this warp Id, one for the Left, one for the Right, else this assertion (that we already described before) will arise :

> Assertion Failure  
> Location : D2Common\\DATATBLS\\LvlTbls.cpp, line #1047  
> Expression : Error in DRLG related to stairs that connect levels

This time it's indicating that it can't find the warp data in LvlWarp.txt, either because the Id is just not present, or because the expected Direction is not present.

To create a new warp, go to the bottom of the file, and in the first empty row edit the columns like this :

| Column | Value | | Column | Value |
|---|---|---|---|---|
| **Name** | `Act 1 Small Tent R` | | **OffsetX** | `2` |
| **Id** | `83` | | **OffsetY** | `1` |
| **SelectX** | `-20` | | **LitVersion** | `0` |
| **SelectY** | `-111` | | **Tiles** | `2` |
| **SelectDX** | `46` | | **Direction** | `b` |
| **SelectDY** | `112` | | **Beta** | `0` |
| **ExitWalkX** | `0` | | | |
| **ExitWalkY** | `5` | | | |

<!-- Conversion note: the original page laid these fourteen column/value pairs
     out in a two-up HTML grid. The first markdown conversion flattened that
     grid into ~110 lines of one-fragment-per-line text, which destroyed the
     pairing between each column name and its value. The pairs above are
     restored verbatim from the archived HTML
     (_divers/ds1/doc/tut01/index.html, lines 753-938). -->

> **Mod note (Project Diablo 2) — read before following this step if you are
> modding on top of PD2.** Siramy says to use "the first empty row" and picks
> **Id 83**. On the **1.10 beta** table he was working from, 83 was free, and
> it still is: checked against genuine **vanilla `LvlWarp.txt`**, both **1.09d
> and 1.13c** (byte-identical between the two — see `## Version differences`),
> the highest Id present is **82**, and 83 is empty. On a stock 1.13c install,
> Siramy's instruction is correct exactly as written.
>
> The earlier pass of this chapter checked against this repository's
> checked-in `assets/excel/LvlWarp.txt` instead, and reported Id 83 as already
> taken by `Act 4 Mesa to Hellcaves` — a warning worth keeping, but a **mod**
> warning, not a **version** one. Vanilla Diablo II's Act 4 has no Mesa or
> Hellcaves — its areas are the Outer Steppes, Plains of Despair, River of
> Flame, Chaos Sanctuary and Pandemonium Fortress — so that row is Project
> Diablo 2's own addition, appended past vanilla's Id 82 the same way PD2 adds
> the extra `l`/`r` pair at Id 85 (above) and the fifty-three extra
> `objects.txt` rows documented in [`docs/vanilla-data.md`](../vanilla-data.md).
> **If you are modding on top of PD2** (or any table where warps have already
> been added past Id 82), check what currently occupies your next free row
> before reusing Siramy's Id 83 — on the copy checked here it holds
> `Act 4 Mesa to Hellcaves`, and following the instruction literally would
> overwrite it. On stock vanilla, take his Id 83 as given; on any modified
> table, take the *method* (append a row, give it an unused Id) and pick your
> own free Id.

The column Name is not used by the game, but it's better to have a good description.

Here we have set the column Direction to b, because even if we know that it is definitively for a Right entrance, it'll avoid some possible assertion later, if you use this warp ID with a Vis of Orientation 10 by mistake for instance.

The column LitVersion is used by the game to know if it has to replace some tiles of a warp at run-time by their hilightened version. Since we don't have such graphics for our tent, we set it to 0, this avoid some glitchs, like part of the tent changing to mud when the mouse is over the entrance. Anyway, the hilight process is hardcoded, and linked to the warp Id, so unless you're doing DLL editing, never use a LitVersion of 1 when you create warps with a new Id.

## 1-way Warp to Tristram

We'll make the Tent connected to Tristram, but without turning back, just to show that it's possible (and easy) to do.

First, put the Tri\_Town4.ds1 in the Ds1 Editor directory, and make this .bat ;

Tristram.bat :

> @echo off  
> win\_ds1edit Tri\_Town4.ds1 11 300 > log.txt

Launch it, and at cell coordinates 10, 38 place an Hidden Vis 6 on Wall layer 1, delete the Fallen object that's near, then place 1 non-selectable object on the Vis (here, just a light), and just for fun place some Fog Water around the Vis like in this exemple, to make it looks like a special area :

![](../assets/images/tut01_19.gif)

As you have noticed, it's the same process as the Trap Doors we have created before, except that the Object on the Vis is different. If you don't put any Object on the Vis, then the Warp will still work, but it'll be selectable by the Player, something we don't want here. So we place an object, but the trick is that the object is not selectable, so despite the Warp IS working, since the Player is unable to select the object, he can't activate this side of the Warp, so it's a 1-way warp :

![](../assets/images/tut01_20.gif)

Now, let's test our new Warps, from the Rogue Encampment to Tristram. Open Levels.txt in Excel. The Act 1 Town is still the Level Id 1, Tristram is the Level Id 38, we have used 2 Vis 6, for the Warp 6 in Town we have created our own Warp, and for Tristram the Vis 6 will be like the Trap Door.

*   Act 1 - Town :
    *   Vis6 \= 38 (to Tristram)
    *   Warp6 \= 83 (our new Tent entrance warp Id from LvlWarp.txt)  

*   Tristram :
    *   Vis6 \= 1 (to Town)
    *   Warp6 \= 19 (Trap Door)

In your Mod directory :

*   in the Data\\Global\\Tiles\\Act1 directory, create the Tristram directory and place Tri\_Town4.ds1 inside.  

*   Place the new version of TownE1.ds1 in Data\\Global\\Tiles\\Act1\\Town.  

*   LvlWarp.txt should be in your Data\\Global\\Excel directory.

You can now test this Mod, and the Rogue encampment's Tent to Tristram's Fog Warp. The Trap Door between the Town and the Graveyard is of course still working.

This zip contains the files necessary to test our modifications so far : **tent.zip** (56.1 KB). You'll find the files of the trap\_door.zip with the new and updated files we have made since. *(Download no longer available — not archived, and it contained Diablo II game data.)*

## A new House in Town

We'll add a House in the Rogue Encampment, one with Roofs that disapear when you walk inside. It's not very hard, but we have to take care of some details else it won't work.

First, don't build them from scratch, always take a model, because in order to have Roofs that disapear, you have to use and place specific special tiles, and the details of how these ones are working is still unknown. So, we'll take the file Data\\Global\\Tiles\\Act1\\Outdoors\\Cott1.ds1 as a model.

Since we want to copy / paste the house in another ds1, we have to load 2 ds1 at once in the ds1 editor, which is done by a .ini that we use as a parameter to win\_ds1edit.exe :

Cottage and Town.bat :

> @echo off  
> win\_ds1edit cottage\_town.ini > log.txt

cottage\_town.ini :

> 2 47 Cott1.ds1  
> 1 1 TownE1.ds1  

Place Cott1.ds1, this .ini and this .bat in the directory of the ds1 editor, then launch the .bat. Select the house, press the keys Ctrl + C to copy it, press the key 2 to view the 2nd ds1 and ... damn, where's the house ? What are these weird tiles ?

|  |  |
|---|---|
| ![](../assets/images/tut01_21.gif) | ![](../assets/images/tut01_22.gif) |
| Select the house, and copy it | Try to paste it in the other ds1, and bad things happens |

We forgot to edit LvlTypes.txt : This file contains several lists of DT1, and our Rogue Encampment don't use yet the graphics of the Cottage house. Before beeing able to use the cottage tiles, we have to include them to the list of the active DT1 of this ds1.

Take LvlTypes.txt and place it into the Data\\Global\\Excel directory of your Mod directory. Then open it with Excel. The Rogue Encampment ds1 is using the LvlType 1. It already have some DT1 like Act1/Town/Floor.dt1, Act1/Town/Fence.dt1, Act1/Town/trees.dt1... but nowhere a cottage.

The Cott1.ds1 is using the LvlType 2 (Wilderness), and here we can find a cottage : the column File25 of the LvlType 2 row has the dt1 Act1/Outdoors/Cottages.dt1. Copy this filename, and paste it into a free slot of the LvlType 1 row, here in the File7 column, you can now save and close this file.

We have added a gfx to the DT1 list of the Town... but it won't use it yet. We have just added the \*possibility\* for this ds1 to use it, we didn't set the ds1 parameters to actually use it. This is done in LvlPrest.txt, with the column Dt1Mask. This column is a bitfield. We currently find the value 959. In binary this value is 1110111111. There are 32 bits maximum, and we have 32 columns File, from File1 to File32, each bit indicate if the ds1 is using the filename in the corresponding column. If we read the bits from the lowest to the highest (from right to left), we'll find that this ds1 is using the columns File1 to File10, except the File7 (which had a 0 before we add our cottage dt1 in there, so that's why).

Let's add the column File7 to this Dt1Mask. Each column is a power of 2. File1=1, File2=2, File3=4, File4=8, File5=16, File6=32 and finally File7\=64. So we add 64 to 959, and our new value is 1023. Replace 959 by 1023 in LvlPrest.txt, save and close the file.

> **Verified against vanilla 1.13c** (`LvlPrest.txt` and `LvlTypes.txt`, both
> from `Patch_D2.mpq`). Row `Act 1 - Town 1`: **Dt1Mask is 959**. 959 =
> `0b1110111111`; reading bit 0 as File1, the bits set are File1–File6, File8,
> File9 and File10 — that is **File1 to File10 except File7**, exactly as
> Siramy reads it. Adding 64 gives 1023 = `0b1111111111`. `LvlTypes.txt`,
> LvlType 1 (`Act 1 - Town`): File 1 = `Act1/Town/Floor.dt1`, File 3 =
> `Act1/Town/Fence.dt1`, File 6 = `Act1/Town/trees.dt1`, no cottage anywhere —
> and **File 7 holds the literal value `0`**, the table's own sentinel for an
> unused slot (the PD2-derived cache the earlier pass checked reads this cell
> as blank rather than `0`; either way, it is the free slot Siramy finds).
> `LvlTypes.txt`, LvlType 2 (`Act 1 - Wilderness`): **File 25 =
> `Act1/Outdoors/Cottages.dt1`**, the exact column and filename he names. The
> bitfield arithmetic and the column layout both hold against the real
> archive. If you don't like doing it manually, you can use this tool to help you : **[dt1mask Maker v1.0](http://phrozenkeep.hugelaser.com/filecenter/dload.php?action=file&file_id=129)**.

Now we're ready. Launch again the Cottage and Town.bat , make some place in Town, copy/paste the house (floor + walls + special tiles) into town, save, quit, and we're done with the editor :

![](../assets/images/tut01_23.gif)

![](../assets/images/tut01_24.gif)

Near Charsi, remove the Vehicle, the Rogue and the Chicken, to make some place

You can now safely copy / paste the house right there.

Save and quit, and place the new TownE1.ds1 into your Mod directory, at its usual Data\\Global\\Tiles\\Act1\\Town place. We can now test the house in game :

![](../assets/images/tut01_25.gif)  
Hmmm, the house is there but the Roofs are not disapearing...

It seems we forgot something but what ? Open LvlPrest.txt, make a search for "cottage", and you'll find the line Act 1 - Cottages 1 at Def 47. If we now compare 2 columns, the Pops and PopPad, we'll see that the Town is not using them, while the Cottage house does. Just copy/paste these 2 values in the row of Town (Pops = 2 and PopPad \= \-4), and the house will finally works as expected :

> **Verified against vanilla 1.13c**: `LvlPrest.txt` row `Act 1 - Cottages 1` is
> **Def 47** and carries **Pops = 2, PopPad = -4**; the `Act 1 - Town 1` row
> carries Pops = 0, PopPad = 0. Siramy's Def number and both values are exact.
> His statement that their meaning "is not well know" still stands — nothing in
> this repository's parsers reads or interprets `Pops`/`PopPad`, so **the
> mechanism remains unexplained**.

![](../assets/images/tut01_26.gif)

The meaning of the columns Pops and PopPad is not well know, but at least it's clear they have something to do with Areas of Tiles that disapear when the player walk somewhere (presence and count of areas / special tiles). That's why it's important to always follow a model, because we can't yet guess these values from scratch.

This zip contains the files necessary to test our modifications so far : **house.zip** (58.3 KB). *(Download no longer available — not archived, and it contained Diablo II game data.)*

To be continued....

*(It never was. The page's last revision is 2004-07-01, and two of the four
topics promised in the Overview — resizing a preset level, and working with
walkable infos — are only touched in passing. The unfinished state is the
author's; it has not been filled in here.)*

## Version differences

Every data claim in this chapter was re-checked against genuine vanilla
archives for both **1.13c** (this book's baseline) and **1.09d**, using
`tools/d2mpq.py`. Where a table differs from what this repository's
PD2-derived `assets/excel/` cache previously showed, that is a **mod**
difference, not a version one — see the Mod note under
[A new Tent Warp](#a-new-tent-warp) — and is called out separately below.

| What | 1.13c | 1.09d | Siramy's 1.10 beta |
|---|---|---|---|
| `LvlWarp.txt` — records / highest Id | 88 records, Ids 0–82, **Id 83 free** | identical — byte-identical file to 1.13c | reports Id 83 as "the first empty row" — consistent |
| `LvlWarp.txt` `l`/`r` Direction pairs | 5 Ids: 71, 73, 74, 81, 82 | identical (same file) | not stated by Siramy |
| `objects.txt` Id 74, `Name` column | `TrappDoor` | `Trap Door` (with a space) | `TrappDoor` — Siramy reads it "so no error", matching 1.13c |
| `levels.txt` total records | 137 | 133 | not checked |
| `Levels.txt` Ids 1 / 17 / 38 / 40 (LevelName, Vis/Warp columns this chapter uses) | as cited throughout this chapter | identical | not checked |

**Mod difference, not shown above** (Project Diablo 2, checked against this
repository's `assets/excel/` cache rather than a vanilla archive): `LvlWarp.txt`
gains a 6th `l`/`r` pair at Id 85 and a row at Id 83 (`Act 4 Mesa to
Hellcaves`, an area that does not exist in vanilla Diablo II); `objects.txt`
gains 53 rows past vanilla's 573 (`docs/vanilla-data.md`). None of this is a
Blizzard patch difference.

No version difference was found, or was checked, for: `LvlPrest.txt` Dt1Mask
arithmetic (Def 1, Def 47, Def 160), `LvlTypes.txt` File 1/3/6/7/25 for
LvlTypes 1 and 2, `objects.txt`/`ObjType.txt` line numbers for Ids 74 and 22,
`assets/editor/obj.txt` line numbers (not a game table — see above), and the
DS1/DT1 format's 5×5 sub-tile grid (a file-format constant, not game data).

---

*Verification report:
[`../getting-started/siramy-conversions.verification.md`](../getting-started/siramy-conversions.verification.md).*