# Importing Town Data into OpenTTD

To aid players in scenario creation, OpenTTD's Scenario Editor can import town data from external JSON files. This enables players to use an image editing program to align town coordinates with a real-world heightmap using a map underlay, instead of guessing at the correct locations in Scenario Editor itself.

This town data consists of a JSON file storing an array of town data objects, each containing a name, location, target OpenTTD population, and whether it is marked as a city in the game.

This document describes the standard format for this JSON file and outlines a workflow for creating this data effectively.

## Table of contents

- Why load external data?
- How to use this feature
  - Creating geodata
    - Town data format standards
    - Town data values
  - Loading geodata into OpenTTD
- Tutorial: Creating town data

## Why load external data?

There are three benefits to using an image editing program to create towns instead of the OpenTTD Scenario Editor.

1. Placing towns accurately is much easier using a map underlay such as OpenStreetMap to match town locations with the corresponding heightmap.
2. Storing town data in a JSON file instead of as an OpenTTD Scenario (.scn) doesn't require choosing your NewGRF house set before placing towns.
3. Town coordinates are scaled by the map size, so you can load the data onto whatever size map you like.

## How to use this feature

### Creating geodata

Town data is a text file in the JSON format, with a list of towns, each containing a coordinate location and properties: name, population, and whether or not it should be a city in OpenTTD.

The format of this file is standardized for importing into OpenTTD and must be followed for OpenTTD to properly parse the data.

For use in OpenTTD, you will also need a matching heightmap of the terrain features, as a PNG.

#### Town data format standards

The following code sample is complete and can be used in OpenTTD.
- The list of towns is enclosed in an array marked with square brackets `[]`
- Each town is enclosed in curly braces `{}`, with a comma after each town except for the last in the list.
- The properties separated by commas except for the last.
- Property names are enclosed in double quotes `""` with a colon `:` separating it from the property value.
- The name property value is enclosed in double quotes `"London"`, while all other property values `44910`, `true`, etc., are not.

```
[
    {
        "name": "London",
        "population": 44910,
        "city": true,
        "x": 0.7998046875,
        "y": 0.708984375
    },
    {
        "name": "Canterbury",
        "population": 217.16,
        "city": false,
        "x": 0.83251953125,
        "y": 0.828125
    }
]
```

#### Town data values

- Population is scaled down for use in OpenTTD. It is possible to generate huge cities by using a large number, but there is a practical limit to town size. The larger the town, the longer it will take to import town data, since towns are placed at a relatively small size and then expanded until the population is greater than the player-defined target.
- X and Y coordinates are a proportion of the total map dimension, between 0 and 1. Just take the pixel coordinates of the town's location in the corresponding heightmap (more detail in the tutorial below) and divide each by the maximum value.
  - For example, London is at `726, 1638` in a 1024 px by 2048 px heightmap, so `726 / 1024 = 0.7998046875` and `1638 / 2048 = 0.708984375` gives the correct coordinates for OpenTTD.
  - The reason for these proportional coordinates is so the data can be used for any map size.
- 0,0 is (approximately) the very top tile in OpenTTD. You can see tile coordinates in-game with the Land Info Tool.
- In most image editing programs, 0,0 is the top-left corner of the image. You can rotate the image however you want relative to compass north to orient the map to your liking. Make sure you crop and resize the image before recording town locations.
- In OpenTTD, X and Y axis are swapped compared to most image editing programs and the standard Cartesian coordinate system. From the 0,0 origin at top left, X is the axis along the left side and Y is the axis along the right side. You can still measure X and Y coordinates in your image editing program, just swap them before importing into OpenTTD or towns won't line up with your heightmap.

### Loading geodata into OpenTTD
Using geodata to create a real-world location in OpenTTD is done in the Scenario Editor.

1. Choose the NewGRFs you want to use in the game.
2. Load the heightmap which you created in the geodata workflow. Either rotation will work, but the clockwise rotation is considered "correct" and the coordinates in the Land Info Tool will match your data; counter clockwise maps will align properly but the coordinates won't match your data.
3. In the Town Generation window, click `Load from file` and choose the .json file containing town data. The default directory to search for town data is `OpenTTD\scenario\heightmap`.
4. (Optional) Manually add industries, rivers, trees, and objects.
5. Save the game as a Scenario and exit to the main menu.
6. Load the game with Play Scenario and enjoy.

Sometimes it's not possible to place a town, such as when the heightmap is very rough and a flat tile can't be found with a 16-tile radius of the target tile. In such cases, a sign will be placed on the target tile with the name of the town. The player can then place the town manually or change the heightmap settings and try again. This fallback also helps debug errors with data creation, such as if towns end up in the ocean.

## Tutorial: Creating town data

1. Load both your heightmap and a labeled map like OpenStreetMap as layers in an image editing program. You can use a free/open-source program like QGIS to acquire, align, and export these map images, if you like.
2. Crop the image to your desired bounds, ensuring the aspect ratio is supported in OpenTTD (1:1, 1:2, 1:4, etc.).
3. Resize the image to one of OpenTTD's supported map sizes, such as 512 px by 1024 px. Some image editors let you do this part of step 2. You can always load heightmaps and town data at a reduced size, so you may want to make this larger than your intended use in case you want it later.
4. Use the labeled map layer to find the pixel coordinates of each town you'd like to include in your map. In GIMP this is displayed in the bottom left corner of the image window, and in Photoshop you need to enable the Info panel (F8) and switch to pixel units of measurement if not already.
5. Some spreadsheets including Google Sheets can export data as JSON, so you may want to record it there, to export after step 8. Or you can build the JSON file manually.
6. Adjust population numbers for OpenTTD.
7. Change coordinates from pixels to proportion (0-1) of the total dimension: `x / maximum_x` and `y / maximum_y`, as described in "Town data values" above.
8. Swap X and Y coordinates before importing to OpenTTD, since OpenTTD uses a reverse X and Y system than most image editors.
9. Save the heightmap and town data files in your `OpenTTD\scenario\heightmap` folder.
