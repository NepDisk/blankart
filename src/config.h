/** SRB2 CMake Configuration */

#ifndef __CONFIG_H__
#define __CONFIG_H__

/* DO NOT MODIFY config.h DIRECTLY! It will be overwritten by cmake.
 * If you want to change a configuration option here, modify it in
 * your CMakeCache.txt. config.h.in is used as a template for CMake
 * variables, so you can insert them here too.
 */

#ifdef CMAKECONFIG

#define ASSET_HASH_MAIN_KART     ""
#define ASSET_HASH_GFX_PK3       ""
#define ASSET_HASH_TEXTURES_PK3  ""
#define ASSET_HASH_CHARS_PK3     ""
#define ASSET_HASH_MAPS_PK3      ""
#ifdef USE_PATCH_FILE
#define ASSET_HASH_PATCH_PK3     ""
#endif

#define SRB2_COMP_REVISION       "209e81c9b"
#define SRB2_COMP_BRANCH         "v2dev3"
// This is done with configure_file instead of defines in order to avoid
// recompiling the whole target whenever the working directory state changes
#define SRB2_COMP_UNCOMMITTED
#ifdef SRB2_COMP_UNCOMMITTED
#define COMPVERSION_UNCOMMITTED
#endif

#define SRB2_COMP_LASTCOMMIT         "Add support to cmake for uncommited changes text and clean up git utils"

#define CMAKE_ASSETS_DIR         "/home/maple/build/blankart/assets"

#else

/* Manually defined asset hashes for non-CMake builds
 * Last updated 2019 / 01 / 18 - Kart v1.0.2 - Main assets
 * Last updated 2020 / 08 / 30 - Kart v1.3 - patch.kart
 */

#define ASSET_HASH_MAIN_KART     "00000000000000000000000000000000"
#define ASSET_HASH_GFX_PK3       "00000000000000000000000000000000"
#define ASSET_HASH_TEXTURES_PK3  "00000000000000000000000000000000"
#define ASSET_HASH_CHARS_PK3     "00000000000000000000000000000000"
#define ASSET_HASH_MAPS_PK3      "00000000000000000000000000000000"
#ifdef USE_PATCH_FILE
#define ASSET_HASH_PATCH_PK3     "00000000000000000000000000000000"
#endif

#endif
#endif
