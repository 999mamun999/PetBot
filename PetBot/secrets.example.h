// =====================================================================
//  PetBot secrets (example)
// =====================================================================
//  Do NOT put real secrets in this file. Instead run:
//      powershell -ExecutionPolicy Bypass -File tools\set_ota_password.ps1
//  It creates PetBot/secrets.h (ignored by git) and tools/ota_password.txt.
//
//  Without secrets.h, WiFi (OTA) upload is simply turned OFF -
//  everything else works, and you upload with the USB cable.
// =====================================================================
#pragma once

// MD5 hash of your OTA password (never the password itself)
// #define OTA_PASS_MD5 "0123456789abcdef0123456789abcdef"
