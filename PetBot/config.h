// =====================================================================
//  PetBot - YOUR SETTINGS  |  Tomar nijer setting (eita bodlao)
// =====================================================================
//  Change these to match you and your city, then upload.
//  Nijer naam ar shohor boshao, tarpor upload koro.
// =====================================================================
#pragma once

// ---- Owner (robot er malik) --------------------------------------------
// Used in greeting, Happy Birthday song and Telegram messages.
// The Bangla name picture is separate: edit "BN_NAME" in tools/bangla_text.json
// and run tools/make_bangla.ps1 (Windows) to regenerate PetBot/bangla_text.h
#define OWNER_NAME     "Al-Mamun"

// ---- City (weather + prayer times) ------------------------------------
// Find your latitude/longitude on Google Maps (right click -> coordinates).
#define CITY_NAME      "Dinajpur"
#define CITY_NAME_BN   "দিনাজপুর"
#define CITY_LAT       25.62
#define CITY_LON       88.64

// ---- Time zone ---------------------------------------------------------
#define TZ_HOURS       6.0             // UTC offset in hours (Bangladesh = 6)
#define TZ_INFO        "<+06>-6"       // POSIX TZ string (Bangladesh, no DST)

// Hijri date shift in days (moon sighting). Bangladesh: -1.
// Can also be changed later from Telegram: /hijri 1  or  /hijri -1
#define HIJRI_ADJUST   -1

// ---- Network names -----------------------------------------------------
#define DEVICE_NAME    "PetBot"        // name in your router's device list
#define MDNS_NAME      "petbot"        // phone page: http://petbot.local
#define AP_NAME        "PetBot-Setup"  // WiFi setup hotspot (first time)

// ---- Weather moods -----------------------------------------------------
#define COLD_BELOW     20.0f           // feels cold below this (°C)
#define HOT_ABOVE      33.0f           // feels hot above this (°C)

// ---- Secrets -----------------------------------------------------------
// WiFi password and Telegram bot token are NEVER stored in code:
//   * WiFi: set from your phone using the "PetBot-Setup" hotspot
//   * Telegram token: paste it on the phone page (Settings -> Telegram)
// WiFi (OTA) upload password: run tools/set_ota_password.ps1 -> makes secrets.h
// (secrets.h is in .gitignore, so it never goes to GitHub)
