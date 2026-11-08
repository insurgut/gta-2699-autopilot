# GTA V Atlas map

Map artwork: Rockstar Games. Unofficial map, no affiliation.
Tile source: https://github.com/VIRUXE/gtav-map-tiles (atlas/4/{x}-{y}.webp), retrieved 2026-11-06. Tiles stitched into one 4096 x 4096 WebP.
Source provenance: publicly circulated GTA V map renders; see upstream README. Map artwork remains the property of Rockstar Games and is not covered by the application GPL.
Previous approximate calibration reference: https://github.com/RiceaRaul/gta-v-map-leaflet/blob/main/src/constants/crs.constants.ts (centerX 117.3, centerY 172.8, scaleX 0.02072, scaleY 0.0205 at 256px). Standard Atlas calibration, not a surveyed District-specific correction.
0.1.29 refines alignment against the coordinate grid in the same VIRUXE tile source: zoom-zero origin (117.25,172.53125), uniform scale 1/48.5. Pixel-grid fitting is not an in-game survey of District custom geometry.
This bundled atlas is the fallback for all map views.

## ArchiVe Satellite for the navigator

Satellite render: https://archive.l7y.media/map by Lucas7yoshi/L7Y.Media, based on Rockstar Games game data. The program downloads public WebP tiles in the background on startup, first at overview resolution and then at full navigator resolution. The navigator, route editor and trip history share the saved files. Visible areas are prioritized, so the tile host receives the requested tile indices. The bundled VIRUXE atlas is used while satellite tiles are unavailable.

Coordinate reference: https://archive.l7y.media/api/maps, gta5, extent X -4140 to 4860 and Y -5100 to 8400. The cache covers zooms 4, 6 and 8 at 256 px per tile, with 32768 pixels across 9000 game units at zoom 8. District-specific map changes have not been surveyed.
