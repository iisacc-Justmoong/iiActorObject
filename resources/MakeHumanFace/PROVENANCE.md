# MakeHuman CC0 facial assets

This package contains unmodified graphic assets, not application code.

The eyes and original license texts come from the official MakeHuman repository
at `a8bc2d54ff0ac92e78ff71431b1023eda42bf482`:
https://github.com/makehumancommunity/makehuman/tree/a8bc2d54ff0ac92e78ff71431b1023eda42bf482/makehuman/data/eyes

The teeth_base and tongue01 files come from the official system assets pack:
https://static.makehumancommunity.org/assets/assetpacks/makehuman_system_assets.html
The publisher lists these system assets as CC0. Their MHCLO and material files
also contain the explicit original CC0 statements. Original asset license texts
are retained here as LICENSE.ASSETS.md and LICENSE.md; the latter distinguishes
assets from AGPL application code. No AGPL Python, shader or application source
has been included.

UPSTREAM-SHA256.json records original paths, sizes and SHA-256 digests. Archive
entries additionally record ZIP CRC-32, publisher URL, total archive size and
HTTP ETag. Selected compressed entries were acquired using exact HTTP ranges;
the full archive was not retained or claimed to have a verified whole-file hash.
The original archive index is retained as build/acquisition/MakeHumanSystemAssetsIndex.json.

catalog.json is authored by iiActorObject and selects three parts plus PBR
roughness approximations. It is not an upstream file. Original mesh, mapping,
image and material contents remain unchanged. The original LitSphere shader is
not reproduced by the standard base-color image/PBR constant adapter.
