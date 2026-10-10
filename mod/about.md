# GD Pathfinder Tools

In-game helpers for the [gd-pathfinder](https://github.com/TokyoDemain6/Gd-pathfinder) project.

## Check and export a level

Open a level's page (an online level, or one of your own) and press the **PF** button. A popup says whether the pathfinder's simulator can handle the level. Press **Export** to save it as a normal `.gmd` file.

## Dump objects

Play a level, pause, and press the **OBJ** button. Every object in the level is written to a CSV file with its position, type and hitbox as the game sees them, next to a `.gmd` of the same level.

## Where the files go

Everything is saved to the `exports` folder inside this mod's save folder. The popup after each export has an **Open Folder** button.

This mod does not play or beat levels, and it never changes a level. It only writes files.
