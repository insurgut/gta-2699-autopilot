# Third-party notices
Original District sources retain their MIT notice in LICENSE-DISTRICT-MIT.txt.

Vehicle OBJ/MTL assets supplied by the user in lowpoly_vehicles(1).zip.
Original asset bytes preserved; see assets/vehicles/SOURCE.txt for checksum.
VisualizationSockets.dll was inspected for transport metadata only; it is not
bundled and no implementation from it is a dependency of this application.

## GTA V Atlas map

Rockstar Games map artwork, unofficial. Tile source and attribution in assets/map/NOTICE.md. Calibration reference: RiceaRaul/gta-v-map-leaflet. The map artwork is not covered by the application GPL.


- Notification icons: Lucide (ISC, assets/notifications/LICENSE.txt), Feather alert-triangle (MIT, assets/notifications/FEATHER-LICENSE.txt). Obtained through the GitHub Contents API, retained as local SVG and embedded alpha masks. Sources: https://github.com/lucide-icons/lucide and https://github.com/feathericons/feather.

Vehicle paint palette: RGB values extracted from GTA V by DurtyFree/gta-v-data-dumps (vehicleColors.json). Read-only layout reference: TupoyeMenu/GTAV-Classes, fwEntity and CVehicleDrawHandler. The main BGRA paint path comes from the user-supplied District b2699 differential report. The indexed palette is used as a fallback.
https://github.com/DurtyFree/gta-v-data-dumps/blob/master/vehicleColors.json
https://github.com/TupoyeMenu/GTAV-Classes

## libwebp 1.6.0

DistrictWebP.dll is built from the standalone decoder in the official libwebp 1.6.0 release. See third_party/libwebp/COPYING and PATENTS.
Source: https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.6.0.tar.gz
SHA-256: e4ab7009bf0629fd11982d4c2aa83964cf244cffba7347ecd39019a9e38c4564
