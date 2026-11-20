/*
  ===========================================================
   PetBot  -  a tiny desk pet robot
   Designed & Developed by Al-Mamun  (https://github.com/999mamun999)
   ESP8266 (NodeMCU) + MPU6050 + 0.96" 2-color OLED (I2C)

   License: GPL-3.0 with additional attribution terms (see LICENSE and NOTICE).
   The original credits (start-up credit screen + GitHub QR / About card)
   must be kept in every copy and modified version.

   Your own settings (name, city, time zone ...): config.h
  ===========================================================
   Display: 2-color OLED -> upor er 16 pixel HOLUD, baki NEEL.
     - Holud band: mood er lekha / animation.
       SLEEP mode e: Time -> Day -> Date -> Temperature -> Zzz (slide kore ghure)
     - Neel part: chokh + mukh
     - Touch sensor e 5 bar tap -> CLOCK mode on, abar 5 bar tap -> normal

   Wiring (OLED ar MPU6050 duitai eki pin e):
     VCC -> 3.3V   GND -> GND   SDA -> D2   SCL -> D1

   Library: Adafruit SSD1306, Adafruit GFX Library, WiFiManager (tzapu)

   WiFi setup (prothom bar):
     Phone er WiFi theke "PetBot-Setup" e connect koro -> ekta page khulbe
     -> tomar WiFi select kore password dao -> Save. Robot nijei mone rakhbe.
     (Page na khulle browser e 192.168.4.1 e jao)

   WiFi bodlate chaile:
     Touch sensor chepe dhore RST button chapo, 3 second dhore rakho
     -> purono WiFi muche jabe, "PetBot-Setup" abar chalu hobe.

   Serial Monitor (115200):
     0-9, a-g = je kono face     p = shob face demo     h = help
*/

#include "config.h"                   // ⚙️ tomar setting (naam, shohor, timezone ...)
#if __has_include("secrets.h")
#include "secrets.h"                  // 🔒 OTA password er hash (git e jay na)
#endif
#include "credits.h"                  // ©️ original credit (bodlano jabe na)
#define STR_(x) #x
#define STR(x)  STR_(x)
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/TomThumb.h>              // khub chhoto font (credits, QR er pashe)
#include "qr_github.h"                   // 🐙 GitHub QR (tools/make_qr.py banay)
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>                // 📡 WiFi diye code upload (USB chara)
#define ARDUINOJSON_USE_LONG_LONG 1
#include <ArduinoJson.h>
#include "telegram_ca.h"             // Telegram er HTTPS certificate (GoDaddy root)
#include <time.h>
#include <math.h>
#include "bangla_text.h"            // Bangla lekhar chhobi (tools/make_bangla.ps1 diye banano)
#include <Servo.h>
#include <EEPROM.h>

// Gaan er lyrics er ek line (karaoke). at = kon note theke shuru (ba ms, gaan onujayi)
struct Lyric { uint32_t at; const char* en; const BnText* bn; };
#include <string.h>

// ---------------- Pins & addresses ----------------
#define PIN_SDA   D2
#define PIN_SCL   D1
#define OLED_ADDR 0x3C
#define PIN_TOUCH D5                // TTP223 touch sensor (SIG)
#define PIN_BUZZER D6               // 2 pin buzzer: + -> D6, - -> GND
#define PIN_ARM_L  D7               // bam haat servo (holud tar)
#define PIN_ARM_R  D8               // dan haat servo (holud tar)

// ---------------- Haat (servo) calibrate ----------------
// DOWN = haat shoja niche jhule thakle servo er angle, UP = haat shoja upore thakle angle.
// Egulo shudhu shuru-r man - Serial e 'Q' diye calibrate kore board e save kora jay.
#define ARM_D7_DOWN  27             // ekta robot e test kore pawa man - nijer ta Serial "Q" diye calibrate koro
#define ARM_D7_UP   180
#define ARM_D8_DOWN 180
#define ARM_D8_UP    20
#define ARM_MAX     160             // haat koto degree porjonto upore uthbe
#define ARM_MIN      20             // ekdom niche na: shorir e chap dile servo atke giye onek current tane
uint8_t mpuAddr = 0x68;             // na pele 0x69 o try kora hoy

// ---------------- WiFi / time ----------------
// AP_NAME, TZ_INFO: config.h e
#define NTP_1    "pool.ntp.org"
#define NTP_2    "time.google.com"

// ---------------- Weather (Open-Meteo, kono API key lage na) ----------------
// Shohor er latitude/longitude: config.h (CITY_LAT / CITY_LON)
#define WEATHER_LAT  STR(CITY_LAT)
#define WEATHER_LON  STR(CITY_LON)
const unsigned long WEATHER_EVERY = 15UL * 60UL * 1000UL;   // 15 minute por por update

// Weather mood: koto degree e thanda / gorom dhorbe -> config.h (COLD_BELOW / HOT_ABOVE)
const unsigned long WEATHER_MOOD_EVERY = 3UL * 60UL * 1000UL;  // 3 min por por mone koriye dey
const unsigned long WEATHER_MOOD_MS    = 9000;                 // koto khon dekhabe

// ---------------- MPU orientation ----------------
#define TILT_X_DIR  1
#define TILT_Y_DIR  1
#define SWAP_XY     0
#define Z_DIR       1

// Start screen e je naam dekhabe -> config.h (OWNER_NAME)

// Power on hole shob face ekbar kore dekhabe (1 = on, 0 = off)
#define DEMO_ON_BOOT 0              // ekhon chalu hole greeting + welcome golpo (demo: serial e 'p')

// ---------------- Timing ----------------
// Kew na chhuile / sthir rakhle (je kono obosthay) - robot er chhoto golpo:
//   3:00 "khelbe?" -> 3:20 birokto -> 3:40 raag (adi!) -> 4:00 bored -> 4:40 hai -> 5:00 ghum
const unsigned long PLEAD_AFTER  = 180000UL;
const unsigned long ANNOY_AFTER  = 200000UL;
const unsigned long ANGRY_AFTER  = 220000UL;
const unsigned long BORED_AFTER  = 240000UL;
const unsigned long SLEEPY_AFTER = 280000UL;
const unsigned long SLEEP_AFTER  = 300000UL;
const unsigned long BLINK_MS     = 160;
const unsigned long DEMO_STEP_MS = 2500;
const unsigned long PAGE_MS      = 3000;   // sleep band e protita page koto khon
const unsigned long SLIDE_MS     = 400;    // slide animation

// ---------------- Screen layout ----------------
#define SCREEN_W 128
#define SCREEN_H 64
#define BAND_H   16

// EMO style: boro chouko chokh, kona gol (rounded square), vitore moni nei
const int   EYE_W    = 30;
const int   EYE_H    = 30;
const int   EYE_R    = 8;
const int   EYE_GAP  = 16;
const int   LEFT_CX  = SCREEN_W / 2 - EYE_GAP / 2 - EYE_W / 2;
const int   RIGHT_CX = SCREEN_W / 2 + EYE_GAP / 2 + EYE_W / 2;
const int   EYE_CY   = 33;
const int   MOUTH_Y  = 54;
const float MAX_LX   = 20;
const float MAX_LY   = 3;

Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, -1);
WiFiManager wm;

// ---------------- Moods ----------------
enum Mood {
  NORMAL, HAPPY, LAUGH, LOVE, WINK, SAD, CRY, ANGRY, SURPRISED, SCARED,
  CONFUSED, SUSPICIOUS, BORED, EXCITED, SLEEPY, SLEEP, DIZZY,
  COLD, HOT, RAINY, STORMY, SUNNY,             // weather onujayi mood
  MUSIC,                                       // gaan gaiche (3 tap)
  PLEAD, ANNOYED,                              // keu na khelle: "khelbe?" -> birokto
  SNEEZE, STRETCH, HUM,                        // majhe majhe nijer moto: hanchi, gaa-moda, gun-gun
  WHISTLE,                                     // 😏 siti bajano (fweet-fweeew~)
  MOOD_COUNT
};

const char* const MOOD_NAME[MOOD_COUNT] = {
  "NORMAL", "HAPPY", "LAUGH", "LOVE", "WINK", "SAD", "CRY", "ANGRY", "SURPRISED",
  "SCARED", "CONFUSED", "SUSPICIOUS", "BORED", "EXCITED", "SLEEPY", "SLEEP", "DIZZY",
  "COLD", "HOT", "RAINY", "STORMY", "SUNNY", "MUSIC", "PLEAD", "ANNOYED",
  "SNEEZE", "STRETCH", "HUM", "WHISTLE"
};

const char* const MOOD_LABEL[MOOD_COUNT] = {
  "", "yay!", "haha!", "", ";)", "...", "huhu", "hmph!", "!!",
  "eek!", "??", "hmm..", "meh", "wow!", "zz", "", "@_@",
  // weather reminder (holud band e scroll kore)
  "Cold! Wear a jacket", "Hot! Drink water", "Rain! Take umbrella",
  "Storm! Stay inside", "Sunny! Wear sunglasses",
  "la la la~", "please?", "uff...",
  "ACHOO!", "aaah~", "hmm hmm~", "hey there~"
};

const char* const DAY_NAME[7] = {
  "SUNDAY", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY"
};
const char* const MON_NAME[12] = {
  "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"
};
const char* const MON_FULL[12] = {
  "JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE",
  "JULY", "AUGUST", "SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER"
};

// ---------------- State ----------------
Mood mood = NORMAL;
unsigned long moodUntil = 0;
unsigned long moodSince = 0;                 // ei mood kokhon shuru holo
unsigned long now = 0;

unsigned long nextWeatherMoodAt = 0;
#define PA_COUNT 10                         // namaz alert er dhoron (prayerAlertShow)
uint8_t prayerFlags = 0;                    // bit0 = Telegram e namaz alert
#define SPECIAL_COUNT 10                     // bishesh din koyta (SPDAYS[] er shathe milate hobe)
#define BDAY_IDX 9                           // SPDAYS e jonmodin er jayga
uint8_t bdayDay = 0, bdayMon = 0;            // 🎂 malik er jonmodin (0 = set kora nei)
uint16_t bdayYear = 0;
uint8_t alarmH = 7, alarmM = 0, bedH = 23, bedM = 0;   // ⏰ alarm, 🌙 ghumer shomoy
bool alarmOn = false, bedOn = false;
bool alarmRinging = false;                   // ⏰ alarm ekhon bajche
bool use24 = false;                          // 🕐 24 ghontar shomoy (clock HOME e 3 s chepe dhorle bodlay)
unsigned long fmtShowUntil = 0;              // format bodlale kichukhon "24 HOUR" lekha
unsigned long qrUntil = 0;                   // 🐙 7 tap: GitHub QR dekhano (ei shomoy porjonto)
unsigned long screenHoldUntil = 0;           // test: kono screen dhore rakha (/api/splash)
float batV = 0;                              // 🔋 LiPo voltage (A0)
bool batOk = false;                          // A0 te battery er tar ache?
int batPct = -1;                             // battery % (-1 = jana nei)
unsigned long batShowUntil = 0;              // 8 tap: battery screen
unsigned long otaWaitUntil = 0;              // 📡 WiFi update ashche: ei shomoy porjonto Telegram bondho, RAM khali
#define otaWaiting() (otaWaitUntil && (long)(millis() - otaWaitUntil) < 0)   // macro: upore function dile Arduino prototype golmal kore
#define EE_HIJRI_MAGIC 105
#define EE_HIJRI_ADJ   104
int8_t hijriAdj = HIJRI_ADJUST;                       // Hijri tarikh BD er chand dekha onujayi shoranu (din)
Mood lastWeatherMood = NORMAL;

bool shotRequested = false;                   // serial 'X': screen er chhobi pathao
unsigned long msgSince = 0, msgUntil = 0;     // phone theke asha message kokhon theke kotokkhon
bool langEn = false;                          // true = puro English, false = Bangla (touch 10 s chepe dhorle bodlay)
bool langToggled = false;                     // ei touch e bhasha already bodlano hoyeche

bool idleAngry = false;                       // ei raag ta "keu khele na" er jonno (adi!)
bool pleadRepeated = false;                   // "khelbe?" ekbar abar jiggesh kora hoyeche kina

struct Imu { float ax, ay, az, gx, gy, gz, temp; } imu = {0, 0, 1, 0, 0, 0, 0};
float gOffX = 0, gOffY = 0, gOffZ = 0;
bool  mpuOk = false;

float lookX = 0, lookY = 0, tLookX = 0, tLookY = 0;
float tRoll = 0;                              // kaat korle matha helano: ek chokh upore, ek chokh niche

// Protita chokh alada spring er moto nore (ekta age, ekta ektu pore pouchay, halka dol khay)
struct EyeState { float x, y, vx, vy; };
EyeState eyeL = {0, 0, 0, 0}, eyeR = {0, 0, 0, 0};
float openness = 1, scale = 1, blinkK = 1;
bool  blinking = false, wasTilted = false;
float shakeScore = 0;

bool clockMode = false;

// Clock mode er view gulo: HOME -> (tap) DATE -> BANGLA -> HIJRI -> DAY -> WEATHER -> ROOM TEMP -> TIMER -> HOME
enum ClockView { CV_HOME, CV_DATE, CV_BANGLA, CV_HIJRI, CV_PRAYER, CV_DAY, CV_WEATHER, CV_ROOM, CV_TIMER, CV_COUNT };
const char* const CV_TITLE[CV_COUNT] = { "", "DATE", "BANGLA", "HIJRI", "NAMAZ", "DAY", "WEATHER", "ROOM TEMP", "TIMER" };

// Timer
bool timerRunning = false, timerAlarm = false;
unsigned long timerLeftMs = 0, timerEndAt = 0, alarmStart = 0;
const char* const DAY_TITLE[7] = {
  "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};
int clockView = CV_HOME;
unsigned long viewChangedAt = 0;

bool  weatherOk = false;
float weatherTemp = 0;
int   weatherCode = -1;
unsigned long nextWeather = 3000;

unsigned long blinkStart = 0, nextBlink = 2000;
unsigned long nextSaccade = 0, nextRandomMood = 10000;
unsigned long lastActivity = 0, surpriseCooldown = 0;

bool cryAfterDizzy = false;
unsigned long lastDizzyAt = 0;
int  rockSign = 0, rockCount = 0;
unsigned long lastRockAt = 0, tiltSince = 0;
bool tiltFired = false;

bool demoMode = DEMO_ON_BOOT;
int  demoIdx = 0;
unsigned long nextDemo = 0;

bool wifiWasConnected = false;

bool touchPrev = false, longFired = false, touching = false;
bool gameMode = false;                      // 🎮 khela cholche (6 tap)
bool touchIgnore = false;                   // khela theke ber howar por touch chhara porjonto kichu korbe na
unsigned long gameArmUntil = 0;             // khelay haat shudhu fol er shomoy ektu nore
enum { GE_PRESS, GE_TAP, GE_HOLD, GE_LONG, GE_SHAKE };   // khelar ghotona
volatile unsigned long touchRiseMs = 0;     // touch er thik muhurto (interrupt) - reaction game er jonno
unsigned long touchDownAt = 0, lastTapAt = 0;
int  tapCount = 0;

// ===========================================================
//  Buzzer (D6) - background e shur baje, animation thame na
// ===========================================================
struct Note { uint16_t f; uint16_t d; };        // f = frequency (0 = chup), d = koto ms

static const Note S_BOOT[] PROGMEM     = {{523,90},{659,90},{784,90},{1047,180}};
static const Note S_CHIRP[] PROGMEM    = {{2200,25}};
static const Note S_HAPPY[] PROGMEM    = {{1568,70},{0,20},{2093,120}};
static const Note S_LAUGH[] PROGMEM    = {{1800,50},{0,30},{1500,50},{0,30},{1800,50},{0,30},{1500,50},{0,30},{2000,90}};
static const Note S_LOVE[] PROGMEM     = {{1319,90},{0,40},{1319,90},{0,40},{1047,220}};
static const Note S_WINK[] PROGMEM     = {{2400,40},{0,30},{3000,60}};
static const Note S_SAD[] PROGMEM      = {{784,200},{659,200},{523,350}};
static const Note S_CRY[] PROGMEM      = {{880,120},{830,120},{784,120},{740,120},{698,300}};
static const Note S_ANGRY[] PROGMEM    = {{180,120},{0,30},{160,120},{0,30},{200,250}};
static const Note S_SURPRISE[] PROGMEM = {{800,35},{1200,35},{1800,35},{2600,90}};
static const Note S_SCARED[] PROGMEM   = {{600,40},{700,40},{600,40},{700,40},{600,40},{700,40}};
static const Note S_CONFUSED[] PROGMEM = {{600,100},{750,100},{950,160}};
static const Note S_SUSP[] PROGMEM     = {{500,150},{0,60},{450,200}};
static const Note S_EXCITED[] PROGMEM  = {{1047,50},{1319,50},{1568,50},{2093,50},{0,40},{2093,50},{2637,120}};
static const Note S_YAWN[] PROGMEM     = {{700,150},{600,150},{500,200},{400,300}};
static const Note S_DIZZY[] PROGMEM    = {{1200,60},{900,60},{1300,60},{800,60},{1400,60},{700,60},{1000,120}};
static const Note S_CLK_ON[] PROGMEM   = {{2000,60},{0,40},{2600,90}};
static const Note S_CLK_OFF[] PROGMEM  = {{2600,60},{0,40},{2000,90}};
static const Note S_TICK[] PROGMEM     = {{3000,15}};
static const Note S_BEEP[] PROGMEM     = {{2500,40}};
static const Note S_START[] PROGMEM    = {{1500,60},{2500,90}};
static const Note S_PAUSE[] PROGMEM    = {{2500,60},{1500,90}};
static const Note S_RESET[] PROGMEM    = {{1000,120}};
static const Note S_ALARM[] PROGMEM    = {{2800,120},{0,80},{2800,120},{0,80},{2800,120},{0,500}};
static const Note S_COLD[] PROGMEM     = {{500,40},{0,30},{500,40},{0,30},{500,40},{0,30},{450,140}};       // dant kapa
static const Note S_HOT[] PROGMEM      = {{900,120},{700,160},{500,260}};                                    // "phew"
static const Note S_RAIN[] PROGMEM     = {{2093,40},{0,120},{1568,40},{0,160},{2349,40},{0,100},{1760,60}};  // tup-tap
static const Note S_STORM[] PROGMEM    = {{120,200},{0,50},{100,300},{90,300}};                              // gorgor
static const Note S_SUNNY[] PROGMEM    = {{1047,70},{1319,70},{1568,70},{2093,150},{0,40},{2093,80}};
static const Note S_PLEAD[] PROGMEM    = {{880,120},{988,120},{0,60},{1047,140},{1175,140},{1319,180},{1568,420}};  // "plee-ease?" (proshno er moto upore)
static const Note S_ANNOYED[] PROGMEM  = {{330,180},{0,80},{294,180},{0,80},{262,380}};                           // "uff-uff-uff"
static const Note S_SIGH[] PROGMEM     = {{600,120},{560,120},{520,140},{470,180},{420,260},{380,400}};            // dirgho nishash
// Hanchi: "a.. a.. a.." (1.3 s) -> "ACHOO!" -> "hihi"
static const Note S_SNEEZE[] PROGMEM   = {{600,300},{650,300},{720,400},{0,300},
                                  {1800,40},{1200,40},{2200,40},{900,60},{1500,40},{700,120},{0,250},
                                  {1400,60},{0,40},{1600,60},{0,40},{1800,90}};
static const Note S_STRETCH[] PROGMEM  = {{400,220},{480,220},{560,260},{660,400},{0,200},{620,200},{520,300}};    // aaaah~
static const Note S_HUM1[] PROGMEM     = {{784,300},{880,300},{784,300},{659,450},{0,120},{698,300},{659,300},{587,600}};
static const Note S_HUM2[] PROGMEM     = {{523,250},{659,250},{784,500},{0,100},{740,250},{659,250},{587,250},{523,600}};
// Wolf whistle: "fweet... fweeeew~" (upore uthe, tarpor upore uthe niche name)
static const Note S_WHISTLE[] PROGMEM  = {{900,25},{1100,25},{1350,25},{1650,25},{2000,25},{2400,40},{2600,90},{0,160},
                                  {1100,25},{1400,25},{1750,25},{2150,25},{2500,40},{2700,100},
                                  {2500,40},{2200,40},{1900,50},{1600,60},{1350,140}};

#define PLAY(s) playSound(s, sizeof(s) / sizeof(s[0]))

bool soundOn = true;                // serial e 'm' dile mute / unmute
bool moodSoundEnabled = true;       // nijer moto random mood e shobdo hobe na
const Note* sndNotes = nullptr;
uint16_t sndLen = 0, sndIdx = 0;              // 16-bit: lomba gaan (jatiyo shongeet ~450 note)
unsigned long sndNextAt = 0;

void playSound(const Note* notes, uint16_t len) {
  if (!soundOn && !alarmRinging) return;               // ⏰ alarm mute thakleo bajbe (ghum bhangate hobe!)
  sndNotes = notes;
  sndLen = len;
  sndIdx = 0;
  sndNextAt = now;
}

void updateSound() {
  if (!sndNotes || (long)(now - sndNextAt) < 0) return;
  if (sndIdx >= sndLen) {
    noTone(PIN_BUZZER);
    sndNotes = nullptr;
    return;
  }
  Note n; memcpy_P(&n, &sndNotes[sndIdx++], sizeof(Note));   // note gulo flash e (PROGMEM)
  // Lomba note e shesh e ektu chup, jate pashapashi eki note alada alada shona jay (gaan e dorkar)
  if (n.f) tone(PIN_BUZZER, n.f, n.d > 80 ? n.d - 30 : n.d);
  else     noTone(PIN_BUZZER);
  sndNextAt += n.d;                    // nikhut taal: deri hoyeo shomoy jome na (lyrics er sathe mile)
  if ((long)(now - sndNextAt) > 300) sndNextAt = now + n.d;   // onek pichiye gele abar shuru theke gona
}

// ===========================================================
//  Gaan (3 tap e ekta ekta kore) - shob purono, copyright-mukto shur
// ===========================================================
// Note er frequency (Hz)
#define NC5 523
#define ND5 587
#define NE5 659
#define NF5 698
#define NG5 784
#define NA5 880
#define NB5 988
#define NC6 1047
#define ND6 1175
#define NE6 1319
#define NF6 1397
#define NG6 1568
#define NB4  494
#define NFS5 740
#define NCS6 1109
#define NFS6 1480
#define NG4  392

// ---------- Amar Sonar Bangla (Rabindranath Tagore) ----------
// Cantorion er free sheet (CC BY-SA) theke pora, G major, 6/8. Buzzer e porishkar shonate 1 octave upore.
#define E8  330            // ek-er-ashtom (eighth)
#define S16 165            // sixteenth
#define Q4  660            // quarter
#define DQ  990            // dotted quarter
#define A_P1  {NFS5,E8},{NG5,E8},{NFS5,E8},{NE5,E8},{ND5,E8},{NE5,S16},{ND5,S16}
#define A_P2  {NC5,E8},{NB4,3*E8},{NB4,E8},{NC5,E8}
#define A_P3  {ND5,E8},{ND5,S16},{NE5,S16},{NFS5,S16},{NG5,S16},{NFS5,E8},{NE5,E8},{ND5,S16},{NE5,S16}
#define A_P4  {NC5,E8},{ND5,Q4},{NE5,E8},{NFS5,E8},{NE5,E8}
#define A_P10 {ND6,E8},{NC6,Q4},{NB5,E8},{NA5,E8},{NB5,E8}
#define A_P11 {NG5,S16},{NA5,S16},{NFS5,Q4},{NG5,E8},{NFS5,S16},{NG5,S16},{NA5,E8}
#define A_P16 {ND5,DQ+E8},{NG5,E8},{NFS5,E8}
#define A_P18 {ND6,E8},{ND6,E8},{NE6,S16},{NFS6,S16},{NE6,E8},{ND6,E8},{NE6,S16},{ND6,S16}
#define A_P21 {ND6,DQ+E8},{NCS6,E8},{NCS6,E8}
#define A_P22 {NCS6,DQ},{ND6,DQ}
#define A_P23 {NCS6,S16},{ND6,S16},{NCS6,S16},{NE6,S16},{ND6,E8},{NC6,S16},{NB5,S16},{NB5,E8},{NA5,S16},{NG5,S16}
#define A_P24 {NG5,E8},{NB5,Q4},{NB5,E8},{NB5,E8},{NCS6,E8}
#define A_P26 {NCS6,E8},{ND6,E8},{NC6,S16},{NB5,S16+E8},{NB5,E8},{NCS6,E8}
#define A_P27 {NCS6,E8},{ND6,Q4},{NE6,E8},{NFS6,E8},{NE6,E8}
#define A_P29 {NCS6,Q4},{ND6,E8},{ND6,E8},{ND6,E8},{NE6,E8}
#define A_P30 {NC6,S16},{ND6,S16},{NC6,Q4},{NB5,E8},{NA5,E8},{NG5,E8}
#define A_P31 {NA5,E8},{NC6,E8},{NC6,E8},{NB5,E8},{NA5,Q4+DQ+E8},{ND6,E8},{ND6,E8}

static const Note SONG_ANTHEM[] PROGMEM = {
  {NG5,E8},{NA5,E8},                                                         // "A-mar"
  A_P1, A_P2, A_P3, A_P4,                                                    // m1-4
  {ND5,Q4},{ND5,E8},{ND5,E8},{ND5,Q4},                                       // m5
  {NE5,S16},{NFS5,S16},{NG5,Q4},{NA5,E8},{NA5,E8},{NG5,E8},                  // m6
  {NA5,E8},{NA5,E8},{NB5,S16},{NC6,S16},{NB5,E8},{NG5,Q4},                   // m7
  {NA5,E8},{NA5,E8},{NB5,S16},{NC6,S16},{NB5,E8},{NA5,Q4+DQ},{0,E8},{ND6,E8},{ND6,E8},   // m8-9
  A_P10, A_P11, A_P1, A_P2, A_P3, A_P4, A_P16,                               // m10-16
  {NG5,E8},{NB5,Q4},{NB5,E8},{NB5,E8},{NC6,E8},                              // m17
  A_P18,                                                                     // m18
  {NCS6,E8},{ND6,E8},{NC6,S16},{NB5,S16+E8},{NB5,E8},{NC6,E8},               // m19
  {NC6,E8},{ND6,Q4},{NE6,E8},{NFS6,E8},{NE6,E8},                             // m20
  A_P21, A_P22, A_P23, A_P24, A_P18, A_P26, A_P27, A_P21, A_P29, A_P30,      // m21-30
  A_P31,                                                                     // m31-32
  A_P10, A_P11, A_P1, A_P2, A_P3, A_P4,                                      // m33-38
  {ND5,DQ+DQ+Q4},{ND5,E8},{ND5,E8},{ND5,E8},{NE5,S16},{ND5,S16},             // m39-40
  {NC5,E8},{ND5,E8},{NE5,S16},{ND5,S16},{NC5,E8},{NB4,Q4+Q4},{NB4,E8},{NB4,E8},{NB4,E8},{NC5,E8},   // m41-42
  {ND5,E8},{NFS5,Q4},{NFS5,E8},{NFS5,S16},{NG5,S16},{NA5,S16},{NG5,S16},     // m43
  {NFS5,Q4},{NG5,E8},{NFS5,E8},{ND5,Q4},                                     // m44
  {NE5,E8},{NFS5,Q4},{NG5,E8},{NA5,E8},{NB5,S16},{NA5,S16},                  // m45
  {NG5,E8},{NFS5,Q4},{ND5,E8},{NFS5,Q4},                                     // m46
  A_P1, A_P4, A_P16, A_P24, A_P18, A_P26, A_P27, A_P21, A_P22, A_P23,        // m47-56
  A_P24, A_P18, A_P26,                                                       // m57-59
  {NCS6,E8},{ND6,Q4+E8},{NCS6,E8},{NCS6,E8},                                 // m60
  A_P29, A_P30, A_P31, A_P10, A_P11, A_P1, A_P2, A_P3, A_P4,                 // m61-70
  {ND5,DQ+300}                                                               // m71
};

// ---------- Bangla chora (lokoj shur, mone theke banano - thik na lagle bodlano jabe) ----------
static const Note SONG_CHANDMAMA[] PROGMEM = {   // আয় আয় চাঁদ মামা
  {NE5,400},{NE5,400},{NG5,400},{NG5,400},{NE5,800},          // আয় আয় চাঁদ মামা
  {ND5,400},{NE5,400},{ND5,400},{NC5,800},                    // টিপ দিয়ে যা
  {NE5,400},{NE5,400},{NG5,400},{NA5,400},{NG5,400},{NE5,800},// চাঁদের কপালে চাঁদ
  {ND5,400},{NE5,400},{ND5,400},{NC5,1000},                   // টিপ দিয়ে যা
  {NG5,400},{NG5,400},{NA5,400},{NG5,400},{NE5,400},{NG5,800},// ধান ভানলে কুঁড়ো দেব
  {NE5,400},{NE5,400},{NG5,400},{NE5,400},{ND5,800},          // মাছ কাটলে মুড়ো দেব
  {ND5,400},{ND5,400},{NE5,400},{ND5,400},{NC5,400},{ND5,800},// কালো গাইয়ের দুধ দেব
  {NE5,400},{ND5,400},{NC5,400},{ND5,400},{NC5,1200}          // দুধ খাবার বাটি দেব
};

static const Note SONG_TAITAI[] PROGMEM = {      // তাই তাই তাই মামার বাড়ি যাই
  {NG5,300},{NG5,300},{NG5,600},                              // তাই তাই তাই
  {NE5,300},{NG5,300},{NA5,300},{NG5,300},{NE5,600},          // মামার বাড়ি যাই
  {NE5,300},{NE5,300},{NG5,300},{NG5,300},{NA5,300},{NA5,300},{NG5,600},   // মামার বাড়ি ভারি মজা
  {NE5,300},{ND5,300},{NE5,300},{NC5,600},                    // কিল চাপড় নাই
  {NG5,300},{NG5,300},{NG5,600},
  {NE5,300},{NG5,300},{NA5,300},{NG5,300},{NE5,600},
  {NE5,300},{NE5,300},{NG5,300},{NG5,300},{NA5,300},{NA5,300},{NG5,600},
  {NE5,300},{ND5,300},{NE5,300},{NC5,900}
};

static const Note SONG_BDAY[] PROGMEM = {        // Happy Birthday
  {NG5,300},{NG5,100},{NA5,400},{NG5,400},{NC6,400},{NB5,800},
  {NG5,300},{NG5,100},{NA5,400},{NG5,400},{ND6,400},{NC6,800},
  {NG5,300},{NG5,100},{NG6,400},{NE6,400},{NC6,400},{NB5,400},{NA5,800},
  {NF6,300},{NF6,100},{NE6,400},{NC6,400},{ND6,400},{NC6,900}
};

static const Note SONG_TWINKLE[] PROGMEM = {     // Twinkle Twinkle Little Star
  {NC5,380},{NC5,380},{NG5,380},{NG5,380},{NA5,380},{NA5,380},{NG5,760},
  {NF5,380},{NF5,380},{NE5,380},{NE5,380},{ND5,380},{ND5,380},{NC5,760},
  {NG5,380},{NG5,380},{NF5,380},{NF5,380},{NE5,380},{NE5,380},{ND5,760},
  {NG5,380},{NG5,380},{NF5,380},{NF5,380},{NE5,380},{NE5,380},{ND5,760},
  {NC5,380},{NC5,380},{NG5,380},{NG5,380},{NA5,380},{NA5,380},{NG5,760},
  {NF5,380},{NF5,380},{NE5,380},{NE5,380},{ND5,380},{ND5,380},{NC5,900}
};

static const Note SONG_JINGLE[] PROGMEM = {      // Jingle Bells
  {NE5,250},{NE5,250},{NE5,500},{NE5,250},{NE5,250},{NE5,500},
  {NE5,250},{NG5,250},{NC5,375},{ND5,125},{NE5,1000},
  {NF5,250},{NF5,250},{NF5,375},{NF5,125},{NF5,250},{NE5,250},{NE5,250},{NE5,125},{NE5,125},
  {NE5,250},{ND5,250},{ND5,250},{NE5,250},{ND5,500},{NG5,500},
  {NE5,250},{NE5,250},{NE5,500},{NE5,250},{NE5,250},{NE5,500},
  {NE5,250},{NG5,250},{NC5,375},{ND5,125},{NE5,1000},
  {NF5,250},{NF5,250},{NF5,375},{NF5,125},{NF5,250},{NE5,250},{NE5,250},{NE5,125},{NE5,125},
  {NG5,250},{NG5,250},{NF5,250},{ND5,250},{NC5,1000}
};

static const Note SONG_ODE[] PROGMEM = {         // Ode to Joy (Beethoven)
  {NE5,350},{NE5,350},{NF5,350},{NG5,350},{NG5,350},{NF5,350},{NE5,350},{ND5,350},
  {NC5,350},{NC5,350},{ND5,350},{NE5,350},{NE5,525},{ND5,175},{ND5,700},
  {NE5,350},{NE5,350},{NF5,350},{NG5,350},{NG5,350},{NF5,350},{NE5,350},{ND5,350},
  {NC5,350},{NC5,350},{ND5,350},{NE5,350},{ND5,525},{NC5,175},{NC5,800}
};

// ---------- Meme e bikhyato, copyright-mukto shur ----------
#define NAS5 932
#define NDS6 1245
#define NA6  1760
#define MK(e, h) {NA5,e},{NB5,e},{NC6,e},{ND6,e},{NE6,e},{NC6,e},{NE6,h}, \
                 {NDS6,e},{NB5,e},{NDS6,h},{ND6,e},{NAS5,e},{ND6,h}, \
                 {NA5,e},{NB5,e},{NC6,e},{ND6,e},{NE6,e},{NC6,e},{NE6,e},{NA6,e}, \
                 {NG6,e},{NE6,e},{NC6,e},{NE6,e},{NG6,h}
static const Note SONG_MOUNTAIN[] PROGMEM = {   // In the Hall of the Mountain King (Grieg) - protibar aro druto 😈
  MK(220, 440), MK(160, 320), MK(110, 220), {NA6,600}
};

#define CC(e, q) {NC5,q},{ND5,e},{NF5,e},{NE5,e},{ND5,e},{NG5,q},{NG5,q}, \
                 {NG5,e},{NA5,e},{NE5,e},{NF5,e},{ND5,q},{ND5,q}, \
                 {ND5,e},{NF5,e},{NE5,e},{ND5,e},{NC5,e},{NC6,e},{NB5,e},{NA5,e}, \
                 {NG5,e},{NF5,e},{NE5,e},{ND5,e},{NC5,q}
static const Note SONG_CANCAN[] PROGMEM = {     // Can-Can (Offenbach)
  CC(160, 320), CC(160, 320), {NC6,500}
};

// ---------- English chora (shob copyright-mukto, lokoj) ----------
static const Note SONG_MARY[] PROGMEM = {        // Mary Had a Little Lamb
  {NE5,350},{ND5,350},{NC5,350},{ND5,350},{NE5,350},{NE5,350},{NE5,700},
  {ND5,350},{ND5,350},{ND5,700},{NE5,350},{NG5,350},{NG5,700},
  {NE5,350},{ND5,350},{NC5,350},{ND5,350},{NE5,350},{NE5,350},{NE5,350},{NE5,350},
  {ND5,350},{ND5,350},{NE5,350},{ND5,350},{NC5,900}
};

static const Note SONG_ROW[] PROGMEM = {         // Row Row Row Your Boat
  {NC5,660},{NC5,660},{NC5,440},{ND5,220},{NE5,660},
  {NE5,440},{ND5,220},{NE5,440},{NF5,220},{NG5,1320},
  {NC6,220},{NC6,220},{NC6,220},{NG5,220},{NG5,220},{NG5,220},
  {NE5,220},{NE5,220},{NE5,220},{NC5,220},{NC5,220},{NC5,220},
  {NG5,440},{NF5,220},{NE5,440},{ND5,220},{NC5,1320}
};

static const Note SONG_LONDON[] PROGMEM = {      // London Bridge Is Falling Down
  {NG5,525},{NA5,175},{NG5,350},{NF5,350},{NE5,350},{NF5,350},{NG5,700},
  {ND5,350},{NE5,350},{NF5,700},{NE5,350},{NF5,350},{NG5,700},
  {NG5,525},{NA5,175},{NG5,350},{NF5,350},{NE5,350},{NF5,350},{NG5,700},
  {ND5,700},{NG5,700},{NE5,350},{NC5,1000}
};

static const Note SONG_MACDONALD[] PROGMEM = {   // Old MacDonald Had a Farm
  {NG5,350},{NG5,350},{NG5,350},{ND5,350},{NE5,350},{NE5,350},{ND5,700},
  {NB5,350},{NB5,350},{NA5,350},{NA5,350},{NG5,700},
  {ND5,350},{NG5,350},{NG5,350},{NG5,350},{ND5,350},{NE5,350},{NE5,350},{ND5,700},
  {NB5,350},{NB5,350},{NA5,350},{NA5,350},{NG5,1000}
};

static const Note SONG_JOHN[] PROGMEM = {        // Are You Sleeping (Brother John / Frere Jacques)
  {NC5,400},{ND5,400},{NE5,400},{NC5,400},{NC5,400},{ND5,400},{NE5,400},{NC5,400},
  {NE5,400},{NF5,400},{NG5,800},{NE5,400},{NF5,400},{NG5,800},
  {NG5,200},{NA5,200},{NG5,200},{NF5,200},{NE5,400},{NC5,400},
  {NG5,200},{NA5,200},{NG5,200},{NF5,200},{NE5,400},{NC5,400},
  {NC5,400},{NG4,400},{NC5,800},{NC5,400},{NG4,400},{NC5,900}
};

// ---------- Lyrics (karaoke) ----------
// (struct Lyric file er upore declare kora: at = kon note theke line shuru, byMs hole ms)
#define LE(i, s) { i, s, nullptr }
#define LB(i, b) { i, nullptr, &b }

static const Lyric LY_BDAY[] = {
  LE(0,"Happy birthday"), LE(4,"to you!"), LE(6,"Happy birthday"), LE(10,"to you!"),
  LE(12,"Happy birthday"), LE(16,"dear " OWNER_NAME "!"), LE(19,"Happy birthday"), LE(23,"to you!") };
static const Lyric LY_TWINKLE[] = {
  LE(0,"Twinkle twinkle"), LE(4,"little star"), LE(7,"How I wonder"), LE(11,"what you are"),
  LE(14,"Up above the"), LE(18,"world so high"), LE(21,"Like a diamond"), LE(25,"in the sky"),
  LE(28,"Twinkle twinkle"), LE(32,"little star"), LE(35,"How I wonder"), LE(39,"what you are") };
static const Lyric LY_JINGLE[] = {
  LE(0,"Jingle bells"), LE(3,"jingle bells"), LE(6,"jingle all the way"), LE(11,"Oh what fun it is"),
  LE(16,"to ride in a"), LE(20,"one horse open sleigh"), LE(25,"Hey!"),
  LE(26,"Jingle bells"), LE(29,"jingle bells"), LE(32,"jingle all the way"), LE(37,"Oh what fun it is"),
  LE(42,"to ride in a"), LE(46,"one horse open sleigh!") };
static const Lyric LY_MARY[] = {
  LE(0,"Mary had a"), LE(4,"little lamb"), LE(7,"little lamb"), LE(10,"little lamb"),
  LE(13,"Mary had a"), LE(17,"little lamb its"), LE(21,"fleece was white"), LE(24,"as snow!") };
static const Lyric LY_ROW[] = {
  LE(0,"Row row row"), LE(3,"your boat"), LE(5,"gently down"), LE(8,"the stream"),
  LE(10,"merrily merrily"), LE(16,"merrily merrily"), LE(22,"life is but"), LE(25,"a dream") };
static const Lyric LY_LONDON[] = {
  LE(0,"London bridge is"), LE(4,"falling down"), LE(7,"falling down"), LE(10,"falling down"),
  LE(13,"London bridge is"), LE(17,"falling down"), LE(20,"my fair lady!") };
static const Lyric LY_MACDONALD[] = {
  LE(0,"Old MacDonald"), LE(4,"had a farm"), LE(7,"E-I-E-I-O!"),
  LE(12,"And on his farm"), LE(16,"he had a cow"), LE(20,"E-I-E-I-O!") };
static const Lyric LY_JOHN[] = {
  LE(0,"Are you sleeping"), LE(4,"are you sleeping"), LE(8,"Brother John?"), LE(11,"Brother John?"),
  LE(14,"Morning bells"), LE(17,"are ringing"), LE(20,"morning bells"), LE(23,"are ringing"),
  LE(26,"Ding dang dong"), LE(29,"ding dang dong") };
static const Lyric LY_CHANDMAMA[] = {
  LB(0,BL_C1), LB(5,BL_C2), LB(9,BL_C3), LB(15,BL_C2),
  LB(19,BL_C5), LB(25,BL_C6), LB(30,BL_C7), LB(36,BL_C8) };
static const Lyric LY_TAITAI[] = {
  LB(0,BL_T1), LB(3,BL_T2), LB(8,BL_T3), LB(15,BL_T4),
  LB(19,BL_T1), LB(22,BL_T2), LB(27,BL_T3), LB(34,BL_T4) };

// Jatiyo shongeet: measure onujayi shomoy (ms). Measure m shuru = 660 + (m-1)*1980.
// Sheet e lyrics chilo na, tai line gulo measure dhore anumane boshano.
#define AM(m) (660 + ((m) - 1) * 1980)
static const Lyric LY_ANTHEM[] = {
  LB(0,BL_A01),            LB(AM(3),BL_A02),        LB(AM(5),BL_A03),        LB(AM(7),BL_A04),
  LB(AM(9) + 1320,BL_A05), LB(AM(12),BL_A06),       LB(AM(14),BL_A02),       LB(AM(16) + 1320,BL_A08),
  LB(AM(19),BL_A09),       LB(AM(21),BL_A10),       LB(AM(24),BL_A11),       LB(AM(28),BL_A12),
  LB(AM(35),BL_A13),       LB(AM(39),BL_A14),       LB(AM(41),BL_A15),       LB(AM(43),BL_A16),
  LB(AM(47),BL_A17),       LB(AM(50),BL_A18),       LB(AM(52),BL_A19),       LB(AM(54),BL_A10),
  LB(AM(57),BL_A21),       LB(AM(60),BL_A22),       LB(AM(67),BL_A13) };

// name = Serial/English naam, bn = holud e Bangla naam (thakle), anthem = jatiyo shongeet (shomman er screen)
struct Song {
  const char* name; const Note* notes; uint16_t len; const BnText* bn; bool anthem;
  const Lyric* lyr; uint8_t lyrN; bool byMs;
};
#define SONG(n, a, b, an)         { n, a, (uint16_t)(sizeof(a) / sizeof(a[0])), b, an, nullptr, 0, false }
#define SONGL(n, a, b, an, l, ms) { n, a, (uint16_t)(sizeof(a) / sizeof(a[0])), b, an, l, (uint8_t)(sizeof(l) / sizeof(l[0])), ms }
static const Song SONGS[] = {
  SONGL("Happy Birthday",     SONG_BDAY,      nullptr,          false, LY_BDAY,      false),
  SONGL("Twinkle Star",       SONG_TWINKLE,   nullptr,          false, LY_TWINKLE,   false),
  SONGL("Jingle Bells",       SONG_JINGLE,    nullptr,          false, LY_JINGLE,    false),
  SONG ("Ode to Joy",         SONG_ODE,       nullptr,          false),
  SONGL("Amar Sonar Bangla",  SONG_ANTHEM,    &BN_S_ANTHEM,     true,  LY_ANTHEM,    true),
  SONGL("Ai Ai Chand Mama",   SONG_CHANDMAMA, &BN_S_CHANDMAMA,  false, LY_CHANDMAMA, false),
  SONGL("Tai Tai Tai",        SONG_TAITAI,    &BN_S_TAITAI,     false, LY_TAITAI,    false),
  SONGL("Mary Had a Lamb",    SONG_MARY,      nullptr,          false, LY_MARY,      false),
  SONGL("Row Your Boat",      SONG_ROW,       nullptr,          false, LY_ROW,       false),
  SONGL("London Bridge",      SONG_LONDON,    nullptr,          false, LY_LONDON,    false),
  SONGL("Old MacDonald",      SONG_MACDONALD, nullptr,          false, LY_MACDONALD, false),
  SONGL("Brother John",       SONG_JOHN,      nullptr,          false, LY_JOHN,      false),
  SONG ("Mountain King",      SONG_MOUNTAIN,  nullptr,          false),
  SONG ("Can-Can",            SONG_CANCAN,    nullptr,          false),
};
unsigned long songStartAt = 0;
int  lyricShown = -1;                // kon lyric line ekhon dekhacche
unsigned long lyricSince = 0;        // oi line kokhon shuru holo (lomba line scroll er jonno)

// ---------- 4 tap: random funny meme shobdo + miliye face ----------
static const Note FX_SADTROMBONE[] PROGMEM = { {392,380},{370,380},{349,380},{330,180},{311,120},{330,120},{311,120},{330,120},{311,500} };
static const Note FX_DRAMATIC[] PROGMEM    = { {392,260},{0,90},{370,260},{0,90},{311,1100} };
static const Note FX_TADA[] PROGMEM        = { {523,110},{659,110},{784,110},{0,60},{1047,600} };
static const Note FX_RIMSHOT[] PROGMEM     = { {110,90},{0,60},{140,90},{0,80},{3200,25},{2900,25},{3400,25},{3000,25},{3300,25},{2800,25},{3100,60} };
static const Note FX_RIP[] PROGMEM         = { {466,600},{466,450},{466,150},{466,600},{554,450},{523,150},{523,450},{466,150},{466,450},{440,150},{466,900} };
static const Note FX_BOING[] PROGMEM       = { {200,40},{300,40},{450,40},{650,40},{850,60},{700,40},{550,40},{700,40},{550,40},{620,40},{520,40},{580,80} };

struct FunnySfx { const Note* notes; uint16_t len; Mood mood; unsigned long ms; const char* label; };
#define FX(a, m, ms, l) { a, (uint16_t)(sizeof(a) / sizeof(a[0])), m, ms, l }
static const FunnySfx FUNNY[] = {
  FX(FX_SADTROMBONE, SAD,       3000, "wah wah..."),
  FX(FX_DRAMATIC,    SURPRISED, 2200, "DUN DUN!"),
  FX(FX_TADA,        EXCITED,   1800, "TA-DA!"),
  FX(FX_RIMSHOT,     WINK,      1500, "ba-dum-tss"),
  FX(FX_RIP,         CRY,       5200, "R.I.P."),
  FX(FX_BOING,       DIZZY,     1500, "boing!"),
  FX(S_WHISTLE,      WHISTLE,   2600, "hey there~"),
};
const int FUNNY_COUNT = sizeof(FUNNY) / sizeof(FUNNY[0]);
int lastFunny = -1;
const char* bandOverride = nullptr;         // ei shomoy holud e mood er lekhar bodole eta dekhay
unsigned long bandOverrideUntil = 0;

// Holud e Bangla chhobi kichukhon dekhanor jonno (welcome message)
const BnText* bandBnOverride = nullptr;
unsigned long bandBnUntil = 0;
const BnText* sceneBand = nullptr;           // drishyo (alarm/namaz/utsob) er holud lekha: drishyo thamle eta-o muche
void showBandBn(const BnText& b, unsigned long ms) { bandBnOverride = &b; bandBnUntil = now + ms; }

// ---------- Greeting er por "welcome" golpo (protibar random) ----------
static const BnText* const W_ARR[3] PROGMEM = { &BN_W_A1, &BN_W_A2, &BN_W_A3 };   // অবশেষে তুমি এলে! / তুমি এসেছো! / আবার দেখা হলো!
static const BnText* const W_FEEL[3] PROGMEM = { &BN_W_F1, &BN_W_F2, &BN_W_F3 };   // মিস করেছি / কোথায় ছিলে? / বসে ছিলাম
static const Mood          W_FEEL_MOOD[3] = { LOVE, PLEAD, HAPPY };
static const BnText* const W_INV[3] PROGMEM = { &BN_W_I1, &BN_W_I2, &BN_W_I3 };   // চলো খেলি! / মজা হবে! / আদর করে দাও!
static const Mood          W_INV_MOOD[3] = { HAPPY, LAUGH, WINK };
int welcomeStep = -1;
unsigned long welcomeNext = 0;
int wA = 0, wF = 0, wI = 0;

void startWelcome() {
  welcomeStep = todaySpecial() >= 0 ? 9 : 0;          // aj bishesh din hole age utsob er drishyo
  welcomeNext = now + 300;
  wA = random(3); wF = random(3); wI = random(3);
}

void updateWelcome() {
  if (welcomeStep < 0 || (long)(now - welcomeNext) < 0) return;
  unsigned long ms;
  switch (welcomeStep) {
    case 9:  ms = todaySpecial() == BDAY_IDX ? 14000 : 7000; startFest(todaySpecial(), ms); welcomeNext = now + ms; welcomeStep = 0; return;
    case 0:  ms = 3200; setMood(EXCITED, ms);         showBandBn(*W_ARR[wA], ms);  break;   // 🤩 eshe gecho!
    case 1:  ms = 3600; setMood(W_FEEL_MOOD[wF], ms); showBandBn(*W_FEEL[wF], ms); break;   // ❤️ mon er kotha
    case 2:  ms = 3000; setMood(W_INV_MOOD[wI], ms);  showBandBn(*W_INV[wI], ms);  break;   // 😊 chol kheli
    default: welcomeStep = -1; return;
  }
  welcomeNext = now + ms;
  welcomeStep++;
  lastActivity = now;
}

// Bhasha bodlano: Bangla <-> English (board e save thake)
void toggleLanguage() {
  langEn = !langEn;
  saveLang();
  moodSoundEnabled = false;
  setMood(EXCITED, 2500);
  moodSoundEnabled = true;
  playSound(FX_TADA, sizeof(FX_TADA) / sizeof(FX_TADA[0]));
  showBandBn(BN_LANG, 2500);                           // "বাংলা!" / "English!"
  Serial.println(langEn ? "Language: English" : "Language: Bangla");
}

void playFunnySound() {
  int k = random(FUNNY_COUNT);
  if (k == lastFunny) k = (k + 1) % FUNNY_COUNT;  // eki ta pashapashi na
  lastFunny = k;
  const FunnySfx& f = FUNNY[k];
  moodSoundEnabled = false;
  setMood(f.mood, f.ms);
  moodSoundEnabled = true;
  playSound(f.notes, f.len);
  bandOverride = f.label;
  bandOverrideUntil = now + f.ms;
  Serial.printf_P(PSTR("Funny: %s\n"), f.label);
}
const int SONG_COUNT = sizeof(SONGS) / sizeof(SONGS[0]);
int songIdx = -1;

bool anthemPlaying() { return mood == MUSIC && songIdx >= 0 && SONGS[songIdx].anthem; }

// Porer gaan bajao (protibar 3 tap e)
void playNextSong() {
  songIdx = (songIdx + 1) % SONG_COUNT;
  const Song& s = SONGS[songIdx];
  unsigned long dur = 0;
  for (int i = 0; i < s.len; i++) { Note n; memcpy_P(&n, &s.notes[i], sizeof(Note)); dur += n.d; }
  setMood(MUSIC, dur + 400);                       // gaan shesh hole nijei normal e fire
  playSound(s.notes, s.len);
  songStartAt = now;
  lyricShown = -1;
  Serial.printf_P(PSTR("Gaan: %s\n"), s.name);
}

// ===========================================================
//  Haat (2 ta SG90 servo) - mood onujayi nore
//  pos: 0 = haat shoja niche, 90 = soja shamne, 180 = shoja upore
// ===========================================================
Servo servoArm[2];
const uint8_t ARM_PIN[2]  = { PIN_ARM_L, PIN_ARM_R };
const char* const ARM_NAME[2] = { "D7", "D8" };
// Ei SG90 gulo 2400us e pura 180 ghorto na, tai max pulse 2600 (pura shima porjonto ghore)
const int     ARM_PULSE_MAX[2] = { 2600, 2600 };
int   armDown[2] = { ARM_D7_DOWN, ARM_D8_DOWN };  // calibrate kore bodlano jay (EEPROM e save)
int   armUp[2]   = { ARM_D7_UP,   ARM_D8_UP   };
float armPos[2] = { 0, 0 };
bool  armAttached[2] = { false, false };
unsigned long armStillSince[2] = { 0, 0 };
bool  armsEnabled = true;            // serial e 'v' dile haat on/off
unsigned long armsMovingUntil = 0;   // nijer haat norle MPU te kapuni lage - ei shomoy "chhoa" dhorbo na

// Calibrate mode
bool  calibMode  = false;
int   calibSel   = 0;                // 0 = D7 er haat, 1 = D8 er haat
int   calibAngle[2] = { 90, 90 };    // calibrate e servo er shorasori angle
bool  calibSweep = false;
unsigned long sweepStart = 0;
long  calibNum   = 0;

// pos (0 niche .. 180 upore) theke servo er angle
int armAngle(int i, float pos) {
  return constrain(armDown[i] + (int)roundf((armUp[i] - armDown[i]) * pos / 180.0f), 0, 180);
}

void loadArmCalib() {
  EEPROM.begin(512);                                   // 0-15 haat/bhasha, 16-107 Telegram/hijri/namaz, 108-251 ghum/reminder, 256-268 alarm
  if (EEPROM.read(0) == 0xA8) {
    armDown[0] = EEPROM.read(1);
    armUp[0]   = EEPROM.read(2);
    armDown[1] = EEPROM.read(3);
    armUp[1]   = EEPROM.read(4);
    Serial.printf_P(PSTR("Haat calibrate load: D7 down=%d up=%d, D8 down=%d up=%d\n"), armDown[0], armUp[0], armDown[1], armUp[1]);
  }
  loadLang();                                          // bhasha (EEPROM eki shathe khola)
  loadFmt();                                           // 12 / 24 ghonta
  Serial.println(langEn ? "Language: English" : "Language: Bangla");
  tgLoadCfg();                                         // Telegram token + malik
  loadHijriAdj();                                      // Hijri tarikh er BD shongshodhon
  loadPrayerCfg();                                     // namaz alert er setting
  loadSched();                                         // ghum, jonmodin, reminder
  loadAlarms();                                        // ⏰ 4 ta alarm
  loadBat();                                           // 🔋 battery mapar shothikota
  loadRecords();                                       // 🏆 khelar record
}

// Bhasha EEPROM e (byte 9 = chinho, byte 10 = 1 hole English)
void loadLang() {
  if (EEPROM.read(9) == 0xB1) langEn = EEPROM.read(10) == 1;
}
void loadFmt() { uint8_t v = EEPROM.read(118); use24 = (v == 0x24); }
void saveFmt() { EEPROM.write(118, use24 ? 0x24 : 0x12); EEPROM.commit(); }
void toggleTimeFormat() {
  use24 = !use24;
  saveFmt();
  fmtShowUntil = millis() + 2500;
  PLAY(S_CLK_ON);
  Serial.println(use24 ? F("Shomoy: 24 ghonta") : F("Shomoy: 12 ghonta"));
}

void saveLang() {
  EEPROM.write(9, 0xB1);
  EEPROM.write(10, langEn ? 1 : 0);
  EEPROM.commit();
}

void saveArmCalib() {
  EEPROM.write(0, 0xA8);
  EEPROM.write(1, (uint8_t)armDown[0]);
  EEPROM.write(2, (uint8_t)armUp[0]);
  EEPROM.write(3, (uint8_t)armDown[1]);
  EEPROM.write(4, (uint8_t)armUp[1]);
  EEPROM.commit();
}

void printCalib() {
  Serial.printf_P(PSTR("Calib: D7 down=%d up=%d angle=%d | D8 down=%d up=%d angle=%d | select=%s\n"),
                armDown[0], armUp[0], calibAngle[0], armDown[1], armUp[1], calibAngle[1], ARM_NAME[calibSel]);
}

// Calibrate mode er key:
//   l = D7 bacho, r = D8 bacho, w = 0->180 dhire ghorao (display e angle dekhay)
//   <shonkhya>g = oi angle e jao,  <shonkhya>n = DOWN set,  <shonkhya>p = UP set
//   d / m / u = niche / soja shamne / upore (set kora man diye), s = save, x = save chara ber
void handleCalibKey(char c) {
  if (c >= '0' && c <= '9') { calibNum = min(999L, calibNum * 10 + (c - '0')); return; }
  int i = calibSel;
  int num = constrain((int)calibNum, 0, 180);
  switch (c) {
    case 'l': calibSel = 0; calibSweep = false; break;
    case 'r': calibSel = 1; calibSweep = false; break;
    case 'w': calibSweep = true; sweepStart = now; calibAngle[i] = 0; break;
    case 'g': calibSweep = false; calibAngle[i] = num; break;
    case 'n': armDown[i] = num; break;
    case 'p': armUp[i]   = num; break;
    case 'd': calibSweep = false; calibAngle[i] = armAngle(i, ARM_MIN); break;
    case 'm': calibSweep = false; calibAngle[i] = armAngle(i, 90); break;
    case 'u': calibSweep = false; calibAngle[i] = armAngle(i, 160); break;
    case 's': saveArmCalib(); calibMode = false; calibSweep = false; Serial.println(F("Haat calibrate SAVE holo")); break;
    case 'x': calibMode = false; calibSweep = false; Serial.println(F("Calibrate bondho (save hoy ni)")); break;
    case '\r': case '\n': case ' ': return;
    default: break;
  }
  calibNum = 0;
  printCalib();
}

// Shorasori haat ke ekta jaygay pathano (greeting er moto blocking animation er jonno)
void armDirect(int i, float pos) {
  if (!armsEnabled) return;
  pos = constrain(pos, (float)ARM_MIN, (float)ARM_MAX);
  if (!armAttached[i]) { servoArm[i].attach(ARM_PIN[i], 500, ARM_PULSE_MAX[i]); armAttached[i] = true; }
  servoArm[i].write(armAngle(i, pos));
  armPos[i] = pos;
  armStillSince[i] = millis();
}

// Protita mood e dui haat kothay thakbe (l = bam, r = dan)
void armTargets(float &l, float &r) {
  l = 0; r = 0;
  switch (mood) {
    case HAPPY:      l = 20; r = 120 + 30 * sinf(now / 150.0f); break;          // hello 👋
    case LAUGH:      l = r = 35 + 15 * sinf(now / 90.0f); break;                 // pet chapre hashi
    case LOVE:       l = 75 + 8 * sinf(now / 400.0f);                            // jhapte dhora 🤗
                     r = 75 - 8 * sinf(now / 400.0f); break;
    case WINK:       r = 110; break;                                             // thumbs up
    case CRY:        l = 110 + 10 * sinf(now / 130.0f);                          // chokh mocha
                     r = 110 - 10 * sinf(now / 130.0f); break;
    case ANGRY:      l = 60 + 25 * sinf(now / 90.0f);                            // "hmph!" haat jhakano
                     r = 60 - 25 * sinf(now / 90.0f); break;
    case SURPRISED:  l = r = 160; break;                                         // dui haat upore 🙌
    case SCARED:     l = r = 140 + 4 * sinf(now / 40.0f); break;                 // mukh dhaka, kapche
    case CONFUSED:   r = 150 + 8 * sinf(now / 200.0f); break;                    // matha chulkano
    case SUSPICIOUS: r = 70; break;                                              // thute haat
    case EXCITED:    l = 135 + 25 * sinf(now / 110.0f);                          // dui haat nachano
                     r = 135 + 25 * sinf(now / 110.0f + PI); break;
    case DIZZY:      l = 60 + 40 * sinf(now / 300.0f);
                     r = 60 + 40 * sinf(now / 300.0f + 2.0f); break;
    case COLD:       l = r = 30 + 4 * sinf(now / 45.0f); break;                  // nijeke jorie kapche
    case HOT:        r = 100 + 20 * sinf(now / 100.0f); break;                   // haat diye batash
    case RAINY:      r = 150; break;                                             // chhata dhora
    case STORMY:     l = r = 150 + 5 * sinf(now / 50.0f); break;                 // matha dhaka
    case SUNNY:      r = 140; break;                                             // salute
    case STRETCH: {                                                              // 🙆 dui haat aste upore, tarpor niche
      float k = min(1.0f, (now - moodSince) / 1400.0f);
      if (now - moodSince > 2100) k = 0;
      l = r = ARM_MIN + (160 - ARM_MIN) * k; break;
    }
    case WHISTLE:    l = 40; r = 115; break;                                     // dan haat mukhe: angul diye siti
    case HUM:        l = 35 + 12 * sinf(now / 300.0f);                           // halka dolano
                     r = 35 - 12 * sinf(now / 300.0f); break;
    case SNEEZE:     if (now - moodSince > 1300 && now - moodSince < 1800) { l = r = 100; }   // hanchi te jhakuni
                     break;
    case PLEAD:      l = 85 + 10 * sinf(now / 180.0f);                           // dui haat bariye "please"
                     r = 85 + 10 * sinf(now / 180.0f + PI); break;
    case ANNOYED:    r = 35 + 18 * fabsf(sinf(now / 140.0f)); break;             // haat diye thak-thak
    case MUSIC:
      if (anthemPlaying()) { l = ARM_MIN; r = 150; break; }                     // jatiyo shongeet: salute 🫡
      l = 90 + 40 * sinf(now / 250.0f);                                         // gaan er tale nach 💃
      r = 90 - 40 * sinf(now / 250.0f); break;
    default: break;                                    // NORMAL, SAD, BORED, SLEEPY, SLEEP: haat niche
  }
}

void updateArms() {
  if (!armsEnabled) return;

  if (calibMode) {                                     // calibrate: servo ke shorasori angle e rakhi
    if (calibSweep) {                                  // proti 1.2 second e 10 degree kore ghore
      int a = (int)((now - sweepStart) / 1200) * 10;
      if (a >= 180) { a = 180; calibSweep = false; }
      if (a != calibAngle[calibSel]) {
        calibAngle[calibSel] = a;
        PLAY(S_TICK);
        Serial.printf_P(PSTR("Sweep %s: %d\n"), ARM_NAME[calibSel], a);
      }
    }
    for (int i = 0; i < 2; i++) {
      if (!armAttached[i]) { servoArm[i].attach(ARM_PIN[i], 500, ARM_PULSE_MAX[i]); armAttached[i] = true; }
      servoArm[i].write(calibAngle[i]);
    }
    return;
  }

  float t[2];
  if (gameMode) {                                      // 🎮 khelay haat chup (jhakuni dhorte shubidha), fol e ektu nache
    t[0] = t[1] = 0;
    if ((long)(now - gameArmUntil) < 0) armTargets(t[0], t[1]);
  } else if (clockMode) {
    t[0] = t[1] = 0;
    if (timerAlarm) {                                  // timer shesh: dui haat nachiye dake
      t[0] = 130 + 25 * sinf(now / 120.0f);
      t[1] = 130 + 25 * sinf(now / 120.0f + PI);
    }
  } else {
    armTargets(t[0], t[1]);
  }

  for (int i = 0; i < 2; i++) {
    float target = constrain(t[i], (float)ARM_MIN, (float)ARM_MAX);
    float d = target - armPos[i];
    if (fabsf(d) > 1.0f) {
      armPos[i] += constrain(d, -9.0f, 9.0f);          // aste aste nore, jhot kore na
      armStillSince[i] = now;
      if (!armAttached[i]) { servoArm[i].attach(ARM_PIN[i], 500, ARM_PULSE_MAX[i]); armAttached[i] = true; }
      servoArm[i].write(armAngle(i, armPos[i]));
      armsMovingUntil = now + 800;
    } else if (armAttached[i] && now - armStillSince[i] > 700) {
      servoArm[i].detach();                            // thamle servo bondho: kapuni nei, current bache
      armAttached[i] = false;
    }
  }
}

void armsInit() {
  armDirect(0, 0);
  armDirect(1, 0);
  delay(500);
  for (int i = 0; i < 2; i++) { servoArm[i].detach(); armAttached[i] = false; }
}

void playMoodSound(Mood m) {
  switch (m) {
    case HAPPY:      PLAY(S_HAPPY);    break;
    case LAUGH:      PLAY(S_LAUGH);    break;
    case LOVE:       PLAY(S_LOVE);     break;
    case WINK:       PLAY(S_WINK);     break;
    case SAD:        PLAY(S_SAD);      break;
    case CRY:        PLAY(S_CRY);      break;
    case ANGRY:      PLAY(S_ANGRY);    break;
    case SURPRISED:  PLAY(S_SURPRISE); break;
    case SCARED:     PLAY(S_SCARED);   break;
    case CONFUSED:   PLAY(S_CONFUSED); break;
    case SUSPICIOUS: PLAY(S_SUSP);     break;
    case EXCITED:    PLAY(S_EXCITED);  break;
    case SLEEPY:     PLAY(S_YAWN);     break;
    case DIZZY:      PLAY(S_DIZZY);    break;
    case COLD:       PLAY(S_COLD);     break;
    case HOT:        PLAY(S_HOT);      break;
    case RAINY:      PLAY(S_RAIN);     break;
    case STORMY:     PLAY(S_STORM);    break;
    case SUNNY:      PLAY(S_SUNNY);    break;
    case PLEAD:      PLAY(S_PLEAD);    break;
    case ANNOYED:    PLAY(S_ANNOYED);  break;
    case BORED:      PLAY(S_SIGH);     break;
    case SNEEZE:     PLAY(S_SNEEZE);   break;
    case STRETCH:    PLAY(S_STRETCH);  break;
    case HUM:        if (random(2)) PLAY(S_HUM1); else PLAY(S_HUM2); break;
    case WHISTLE:    PLAY(S_WHISTLE);  break;
    default: break;                    // NORMAL, SLEEP: chup
  }
}

// ===========================================================
//  MPU6050
// ===========================================================
bool mpuWrite(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(mpuAddr);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool mpuRead() {
  Wire.beginTransmission(mpuAddr);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(mpuAddr, (uint8_t)14) != 14) return false;

  int16_t raw[7];
  for (int i = 0; i < 7; i++) {
    uint8_t hi = Wire.read();
    uint8_t lo = Wire.read();
    raw[i] = (int16_t)((hi << 8) | lo);
  }
  imu.ax   = raw[0] / 16384.0f;
  imu.ay   = raw[1] / 16384.0f;
  imu.az   = raw[2] / 16384.0f;
  imu.temp = raw[3] / 340.0f + 36.53f;
  imu.gx   = raw[4] / 131.0f - gOffX;
  imu.gy   = raw[5] / 131.0f - gOffY;
  imu.gz   = raw[6] / 131.0f - gOffZ;
  return true;
}

void i2cScan() {
  Serial.print(F("I2C scan: "));
  int found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("0x"));
      Serial.print(a, HEX);
      Serial.print(F(" "));
      found++;
    }
  }
  Serial.println(found ? "" : "kichui pawa jay ni");
}

bool mpuBegin() {
  mpuAddr = 0x68;
  if (!mpuWrite(0x6B, 0x00)) {
    mpuAddr = 0x69;
    if (!mpuWrite(0x6B, 0x00)) return false;
  }
  mpuWrite(0x1A, 0x03);
  mpuWrite(0x1B, 0x00);
  mpuWrite(0x1C, 0x00);
  return true;
}

void calibrateGyro() {
  float sx = 0, sy = 0, sz = 0;
  int n = 0;
  gOffX = gOffY = gOffZ = 0;
  for (int i = 0; i < 200; i++) {
    if (mpuRead()) { sx += imu.gx; sy += imu.gy; sz += imu.gz; n++; }
    delay(5);
  }
  if (n > 0) { gOffX = sx / n; gOffY = sy / n; gOffZ = sz / n; }
}

float tiltX() { return (SWAP_XY ? imu.ay : imu.ax) * TILT_X_DIR; }
float tiltY() { return (SWAP_XY ? imu.ax : imu.ay) * TILT_Y_DIR; }

// ===========================================================
//  Mood logic
// ===========================================================
void setMood(Mood m, unsigned long durationMs) {
  if (m != mood && moodSoundEnabled) playMoodSound(m);
  if (m != mood) moodSince = now;
  mood = m;
  moodUntil = durationMs ? now + durationMs : 0;
}

bool calmMood() {
  switch (mood) {
    case NORMAL: case HAPPY: case WINK: case LAUGH: case SUSPICIOUS:
    case EXCITED: case BORED: case CONFUSED:
    case COLD: case HOT: case RAINY: case STORMY: case SUNNY:
    case PLEAD: case ANNOYED: case SNEEZE: case STRETCH: case HUM: case WHISTLE:
      return true;
    default:
      return false;
  }
}

bool canBlink() {
  return !(mood == SLEEP || mood == DIZZY || mood == LAUGH || mood == WINK || mood == SUNNY);
}

void updateMoodTimeout() {
  if (moodUntil && (long)(now - moodUntil) >= 0) {
    if (mood == DIZZY) {
      if (cryAfterDizzy) setMood(CRY, 4000);
      else               setMood(ANGRY, 1500);
    } else {
      setMood(NORMAL, 0);
    }
  }
}

void randomMood() {
  if (mood != NORMAL || (long)(now - nextRandomMood) < 0) return;
  int r = random(100);
  moodSoundEnabled = false;                        // nijer moto mood e chup thake
  if      (r < 18) setMood(HAPPY, 2000);
  else if (r < 28) setMood(WINK, 900);
  else if (r < 35) setMood(LAUGH, 2000);
  else if (r < 41) setMood(LOVE, 2500);
  else if (r < 51) setMood(SUSPICIOUS, 2500);
  else if (r < 58) setMood(EXCITED, 2000);
  else if (r < 62) setMood(CONFUSED, 2000);
  else {
    moodSoundEnabled = true;                       // ei tin ta cute kaj shobdo shoho
    if      (r < 72) setMood(SNEEZE, 2700);        // 🤧 a.. a.. ACHOO!
    else if (r < 82) setMood(STRETCH, 2800);       // 🙆 gaa-moda aaah~
    else if (r < 91) setMood(HUM, 3000);           // 🎵 gun-gun
    else if (r < 96) setMood(WHISTLE, 2600);       // 😏 siti
  }
  moodSoundEnabled = true;
  nextRandomMood = now + random(7000, 16000);
}

// Ekhon weather onujayi kon mood hobe (kichu na hole NORMAL)
Mood currentWeatherMood() {
  if (!weatherOk) return NORMAL;
  int c = weatherCode;
  if (c >= 95)                                       return STORMY;
  if ((c >= 51 && c <= 67) || (c >= 80 && c <= 82))  return RAINY;
  if (weatherTemp <= COLD_BELOW)                     return COLD;
  if (weatherTemp >= HOT_ABOVE)                      return HOT;
  if (c <= 1 && timeValid()) {
    time_t t = time(nullptr);
    int h = localtime(&t)->tm_hour;
    if (h >= 7 && h < 17)                            return SUNNY;   // din er bela shurjo
  }
  return NORMAL;
}

// Majhe majhe weather mood + reminder dekhay (weather bodlale shathe shathe)
void weatherReminder() {
  Mood wm = currentWeatherMood();
  if (wm != lastWeatherMood) {
    lastWeatherMood = wm;
    nextWeatherMoodAt = now;                         // notun weather: jotojaldi dekhao
  }
  if (wm == NORMAL || mood != NORMAL) return;        // ghum / onno mood e disturb korbe na
  if ((long)(now - nextWeatherMoodAt) < 0) return;
  setMood(wm, WEATHER_MOOD_MS);
  nextWeatherMoodAt = now + WEATHER_MOOD_EVERY;
  Serial.print(F("Weather mood: "));
  Serial.println(MOOD_NAME[wm]);
}

// Kotokkhon keu chhoyni tar upor robot er mood
Mood idleStageMood(unsigned long idle) {
  if (idle >= SLEEP_AFTER)  return SLEEP;
  if (idle >= SLEEPY_AFTER) return SLEEPY;
  if (idle >= BORED_AFTER)  return BORED;
  if (idle >= ANGRY_AFTER)  return ANGRY;
  if (idle >= ANNOY_AFTER)  return ANNOYED;
  if (idle >= PLEAD_AFTER)  return PLEAD;
  return NORMAL;
}

// Robot ki ekhon "keu khele na" golpo er moddhe? (khelbe/birokto/adi/bored)
bool waitingToPlay() {
  return mood == PLEAD || mood == ANNOYED || mood == BORED || (mood == ANGRY && idleAngry);
}

void updateIdleStory() {
  if (mood != ANGRY) idleAngry = false;
  Mood want = idleStageMood(now - lastActivity);

  if (want == NORMAL) {
    if (waitingToPlay()) {                             // 🎉 keu eshe chhuyeche! khushi te lafano
      idleAngry = false;
      setMood(EXCITED, 2500);
      bandOverride = "yay!";
      bandOverrideUntil = now + 2500;
      surpriseCooldown = now + 3000;                   // ei dhakka te abar chomke uthbe na
      Serial.println(F("Idle: keu eseche - yay!"));
    } else if (mood == SLEEPY) {
      setMood(NORMAL, 0);
    }
    return;
  }

  if (mood == NORMAL || waitingToPlay() || mood == SLEEPY) {
    if (mood != want || (want == ANGRY && !idleAngry)) {
      if (want == PLEAD) pleadRepeated = false;
      setMood(want, 0);
      idleAngry = (want == ANGRY);
      Serial.print(F("Idle: "));
      Serial.println(MOOD_NAME[want]);
    }
    if (mood == PLEAD && !pleadRepeated && now - moodSince > 10000) {   // 10 s por abar "please?"
      PLAY(S_PLEAD);
      pleadRepeated = true;
    }
  }
}

void updateBehaviour() {
  float g   = sqrtf(imu.ax * imu.ax + imu.ay * imu.ay + imu.az * imu.az);
  float dev = fabsf(g - 1.0f);
  float gm  = sqrtf(imu.gx * imu.gx + imu.gy * imu.gy + imu.gz * imu.gz);

  // Nijer haat norle robot ektu kape - oita ke "keu chhuyeche" dhorbo na
  bool selfMove = (long)(now - armsMovingUntil) < 0;
  bool activity = !selfMove && (dev > 0.08f || gm > 12.0f);
  bool bump     = !selfMove && (dev > 0.35f || gm > 120.0f);
  // jore jhakuni: haat er kapuni (0.3g er kom) theke onek boro, tai selfMove diye bad dei na
  bool shakeHit = dev > 0.60f || gm > 250.0f;

  shakeScore = shakeScore * 0.92f + (shakeHit ? 1.0f : 0.0f);
  if (activity) lastActivity = now;

  float tx = tiltX(), ty = tiltY();

  // 1) Jhakano -> DIZZY (10s er moddhe abar hole pore kanna)
  if (shakeScore > 3.5f && mood != DIZZY) {
    cryAfterDizzy = lastDizzyAt != 0 && (now - lastDizzyAt) < 10000;
    lastDizzyAt = now;
    setMood(DIZZY, 3000);
    shakeScore = 0;
    return;
  }
  if (mood == DIZZY) return;

  // 2) Ulta -> SCARED
  // Ulta korle bhoy pay - kintu ulta obosthay 5 second sthir thakle shanto hoye jay (pore ghumate pare)
  if (imu.az * Z_DIR < -0.6f && now - lastActivity < 5000) { setMood(SCARED, 700); return; }

  // 3) Ghum / bored theke utha
  if (mood == SLEEP && activity) {
    setMood(SURPRISED, 1200);
    surpriseCooldown = now + 2500;
    return;
  }

  // 3b) "Keu khele na" golpo - keu eshe chhuyle/narale "yay!" (dhakka-r SURPRISED er age dhorte hobe)
  updateIdleStory();
  if (mood == EXCITED && bandOverride) return;

  // 4) Aste aste dolano -> LOVE
  int s = tx > 0.15f ? 1 : (tx < -0.15f ? -1 : 0);
  if (s != 0 && s != rockSign) {
    if (rockSign != 0) {
      rockCount = (now - lastRockAt < 1200) ? rockCount + 1 : 1;
      lastRockAt = now;
    }
    rockSign = s;
  }
  if (rockCount >= 4) {
    rockCount = 0;
    setMood(LOVE, 3500);
    return;
  }

  // 5) Dhakka / uthano -> SURPRISED
  if (bump && (long)(now - surpriseCooldown) >= 0 && calmMood()) {
    setMood(SURPRISED, 1000);
    surpriseCooldown = now + 2500;
    return;
  }

  // 6) Onekkhon kaat kore rakha -> CONFUSED
  bool bigTilt = fabsf(tx) > 0.3f || fabsf(ty) > 0.3f;
  if (bigTilt) {
    if (tiltSince == 0) tiltSince = now;
    // shudhu haat e dhore kaat korle CONFUSED - kaat obosthay sthir pore thakle na
    else if (!tiltFired && now - tiltSince > 3000 && now - lastActivity < 3000 && calmMood()) {
      setMood(CONFUSED, 2500);
      tiltFired = true;
    }
  } else {
    tiltSince = 0;
    tiltFired = false;
  }


  // 8) Majhe majhe nijer moto mood
  randomMood();
}

// ===========================================================
//  Demo + Serial commands
// ===========================================================
char moodKey(int i) {
  if (i < 10) return '0' + i;
  if (i < 17) return 'a' + (i - 10);
  return 'A' + (i - 17);                             // weather mood: A-E
}

void printHelp() {
  Serial.println();
  Serial.println(F("---- Mood commands ----"));
  for (int i = 0; i < MOOD_COUNT; i++) {
    Serial.print(F("  "));
    Serial.print(moodKey(i));
    Serial.print(F(" = "));
    Serial.println(MOOD_NAME[i]);
  }
  Serial.println(F("  p = shob face ekbar demo"));
  Serial.println(F("  k = clock mode on/off"));
  Serial.println(F("  n = clock er porer view"));
  Serial.println(F("  + = timer +1 min,  s = timer start/pause,  r = timer reset"));
  Serial.println(F("  z = buzzer test,   m = sound on/off"));
  Serial.println(F("  w = ekhoner weather mood dekhao"));
  Serial.println(F("  v = haat (servo) on/off"));
  Serial.println(F("  j = porer gaan bajao (touch e 3 tap)"));
  Serial.println(F("  y = random funny meme shobdo (touch e 4 tap)"));
  Serial.println(F("  i = 'keu khele na' golpo test (3 min er jaygay 2 s por shuru)"));
  Serial.println(F("  U = upload er age 'system update' screen (2 min opekkha kore)"));
  Serial.println(F("  ! = bhasha bodlao Bangla <-> English (touch 10 s chepe dhorleo)"));
  Serial.println(F("  L = siti (whistle) face"));
  Serial.println(F("  W = WiFi reset (mone rakha WiFi muche restart)"));
  Serial.println(F("  h = ei help"));
}

void setClockMode(bool on) {
  clockMode = on;
  demoMode = false;
  lastActivity = now;
  if (on) {
    clockView = CV_HOME;
    viewChangedAt = now;
    PLAY(S_CLK_ON);
    Serial.println(F("Clock mode ON"));
  } else {
    Serial.println(F("Clock mode OFF"));
    moodSoundEnabled = false;
    setMood(HAPPY, 1500);                    // clock theke fire eshe khushi
    moodSoundEnabled = true;
    PLAY(S_CLK_OFF);
  }
}

void startDemo() {
  demoMode = true;
  demoIdx = 0;
  nextDemo = now;
}

void updateDemo() {
  if ((long)(now - nextDemo) < 0) return;
  if (demoIdx >= MOOD_COUNT) {
    demoMode = false;
    setMood(NORMAL, 0);
    lastActivity = now;
    Serial.println(F("Demo shesh"));
    return;
  }
  setMood((Mood)demoIdx, 0);
  Serial.print(F("Demo: "));
  Serial.println(MOOD_NAME[demoIdx]);
  demoIdx++;
  nextDemo = now + DEMO_STEP_MS;
}

void handleSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (calibMode) { handleCalibKey(c); continue; }   // calibrate cholle shob key oikhane jay
    if (c == 'G') { gameMode ? gameExit() : gameEnter(); continue; }   // 🎮 khela on/off
    if (gameMode && (c == '.' || c == ',' || c == ';' || c == '~')) {   // khelar test: tap / chepe dhora / lomba / jhakuni
      if (c == '.') gameEvent(GE_PRESS);
      gameEvent(c == '.' ? GE_TAP : c == ',' ? GE_HOLD : c == ';' ? GE_LONG : GE_SHAKE);
      continue;
    }
    if (c == 'Q') {                                    // haat calibrate shuru
      calibMode = true;
      armsEnabled = true;
      calibSel = 0;
      calibSweep = false;
      calibNum = 0;
      for (int i = 0; i < 2; i++) calibAngle[i] = armAngle(i, armPos[i]);   // ekhon jekhane ache shekhan theke
      Serial.println(F("Haat calibrate: l=D7 r=D8, w=ghorao, <n>g=angle e jao, <n>n=DOWN, <n>p=UP, d/m/u, s=save, x=ber"));
      printCalib();
      continue;
    }
    int idx = -1;
    if (c >= '0' && c <= '9')      idx = c - '0';
    else if (c >= 'a' && c <= 'g') idx = 10 + (c - 'a');
    else if (c >= 'A' && c <= 'E') idx = 17 + (c - 'A');
    else if (c == 'F' || c == 'j') { demoMode = false; clockMode = false; playNextSong(); }
    else if (c == 'y') { demoMode = false; clockMode = false; playFunnySound(); }
    else if (c == 'U') showUpdateScreen();       // upload er age "system update" screen
    else if (c == '!') { demoMode = false; clockMode = false; toggleLanguage(); }   // bhasha bodlao
    else if (c == 'X') shotRequested = true;      // screen er chhobi PC te pathao (debug)
    else if (c == 'T') tgSelfTest();              // Telegram command bujha test (internet chara)
    else if (c == 'R') startScene(8, EXCITED, BN_AL_WAKE, 8000);   // alarm er drishyo (8 = FS_SUNRISE) (test, Telegram chara)
    else if (c == 'Z') startScene(3, SLEEP, BN_AL_BED, 8000);     // ghumer drishyo (3 = FS_MOON) (test)
    else if (c == 'S') {                                                    // alarm/reminder command bujha test
      static const char* const SC[] = { "/alarm 6:45am", "/alarm 3:30pm ekbar", "/alarm 22:15", "/alarm off 2", "/alarm del 3", "/list" };
      for (auto q : SC) { String raw = q; String c2 = tgNorm(raw); int sp = c2.indexOf(' '); if (sp > 0) c2 = c2.substring(0, sp);
        Serial.print(F("SCTEST ")); Serial.print(q); Serial.print(F(" -> ")); Serial.println(c2 == "list" ? schedListText() : tgSchedCommand(c2, raw)); }
    }    else if (c == 'N') { Serial.println(tgPrayerText()); Serial.println(tgRozaText()); }   // namaz er lekha (Telegram er moto)
    else if (c == 'P') {                         // namaz / sehri / iftar alert test (protibar porer ta)
      static int pk = 0;
      prayerAlertShow(pk, true);
      pk = (pk + 1) % PA_COUNT;
    }
    else if (c == 'Y') {                         // bishesh din er drishyo test (protibar porer ta)
      static int fk = 0;
      startFest(fk, 7000);
      fk = (fk + 1) % SPECIAL_COUNT;
    }
    else if (c >= 'G' && c <= 'L') idx = 23 + (c - 'G');   // PLEAD ANNOYED SNEEZE STRETCH HUM WHISTLE
    else if (c == '?') {                          // ekhon kon mood, koto khon keu chhoyni
      Serial.printf_P(PSTR("Mood: %s, idle %lu s\n"), MOOD_NAME[mood], (now - lastActivity) / 1000);
      Serial.print(F("Web: http://"));
      Serial.println(WiFi.localIP());
    }
    else if (c == 'i') {                          // test: "keu khele na" golpo 2 second por shuru
      demoMode = false; clockMode = false;
      setMood(NORMAL, 0);
      lastActivity = now - (PLEAD_AFTER - 2000);
      Serial.println(F("Idle golpo test: 2 s por 'khelbe?' shuru"));
    }
    else if (c == 'W') {                          // serial theke WiFi reset + restart
      Serial.println(F("WiFi reset hocche, restart..."));
      wm.resetSettings();
      delay(500);
      ESP.restart();
    }
    else if (c == 'w') {
      Mood wm = currentWeatherMood();
      if (wm != NORMAL) { demoMode = false; clockMode = false; setMood(wm, WEATHER_MOOD_MS); }
      Serial.print(F("Ekhoner weather mood: "));
      Serial.println(MOOD_NAME[wm]);
    }
    else if (c == 'p')             { startDemo(); Serial.println(F("Demo shuru")); }
    else if (c == 'h')             printHelp();
    else if (c == 'k')             setClockMode(!clockMode);
    else if (c == 'n' && clockMode) nextClockView();
    else if (c == '+')             timerAddMinute();
    else if (c == 's')             timerToggle();
    else if (c == 'r')             timerReset();
    else if (c == 'z')             PLAY(S_BOOT);
    else if (c == 'v') {                          // haat on / off
      armsEnabled = !armsEnabled;
      if (!armsEnabled) for (int i = 0; i < 2; i++) { servoArm[i].detach(); armAttached[i] = false; }
      Serial.println(armsEnabled ? "Haat ON" : "Haat OFF");
    }
    else if (c == 'm') {
      soundOn = !soundOn;
      if (!soundOn) { noTone(PIN_BUZZER); sndNotes = nullptr; }
      Serial.println(soundOn ? "Sound ON" : "Sound OFF (mute)");
    }

    if (idx >= 0 && idx < MOOD_COUNT) {
      demoMode = false;
      clockMode = false;
      lastActivity = now;
      setMood((Mood)idx, idx == NORMAL ? 0 : 5000);
      Serial.print(F("Mood: "));
      Serial.println(MOOD_NAME[idx]);
    }
  }
}

// ===========================================================
//  Touch sensor (TTP223 on D5)
//    1 bar touch      -> HAPPY (ghumonto thakle aste kore uthe)
//    2 bar touch      -> LOVE
//    3 bar touch      -> gaan (protibar porer ta)
//    4 bar touch      -> random funny meme shobdo
//    5 bar touch      -> CLOCK mode on / off
//    6 bar touch      -> 🎮 KHELA (menu: tap = porer, chepe dhora = shuru)
//    Clock mode e 1 tap -> porer screen
//      HOME -> DATE -> DAY -> WEATHER -> ROOM TEMP -> TIMER -> HOME
//    TIMER screen e: 2 tap = +1 min, chepe dhora = start/pause, 3 tap = reset
//    chepe dhore rakha -> aram (HAPPY), 3s er beshi -> kat-kutu (LAUGH)
// ===========================================================
void handleTouch() {
  bool t = digitalRead(PIN_TOUCH) == HIGH;
  touching = t;
  if (gameMode) { gameTouch(t); touchPrev = t; return; }   // 🎮 khelay touch er kaj alada
  if (touchIgnore) {                                       // khela bondho korar shei touch: chhere dile tobe shadharon
    if (!t) touchIgnore = false;
    touchPrev = t; tapCount = 0; longFired = true;
    return;
  }

  if (t && !touchPrev && alarmRinging) {          // ⏰ alarm bajche: touch = bondho
    wakeAlarmStop();
    touchIgnore = true; touchPrev = t;
    return;
  }
  if (t && !touchPrev && (qrUntil || batShowUntil)) {   // 🐙 QR / 🔋 battery dekhacche: touch = bondho
    qrUntil = 0; batShowUntil = 0;
    touchIgnore = true; touchPrev = t;
    setMood(HAPPY, 1500);
    return;
  }

  if (t && !touchPrev) {                          // touch shuru
    touchDownAt = now;
    welcomeStep = -1;                             // touch korle welcome golpo theme jay
    langToggled = false;
    if (messageActive()) msgUntil = now;          // message pora hoye gele touch e bondho
    if (!timerAlarm) PLAY(S_CHIRP);
    longFired = false;
    if (demoMode) { demoMode = false; Serial.println(F("Demo bondho (touch)")); }
  }

  if (t && !clockMode) {
    lastActivity = now;
    unsigned long held = now - touchDownAt;
    if (held > 10000) {                           // 10 s chepe dhorle bhasha bodlay (Bangla <-> English)
      if (!langToggled) { toggleLanguage(); langToggled = true; }
      longFired = true;
    } else if (held > 3000) {
      if (mood != LAUGH) { setMood(LAUGH, 0); Serial.println(F("Touch: kat-kutu (LAUGH)")); }
      longFired = true;
    } else if (held > 800 && !longFired) {
      setMood(HAPPY, 0);
      Serial.println(F("Touch: chepe dhora (HAPPY)"));
      longFired = true;
    }
  }

  // Clock HOME e 3 second chepe dhorle 12 <-> 24 ghonta
  if (t && clockMode && clockView == CV_HOME && !timerAlarm && !longFired && now - touchDownAt > 3000) {
    toggleTimeFormat();
    longFired = true;
  }

  // Timer screen e chepe dhorle start / pause
  if (t && clockMode && clockView == CV_TIMER && !timerAlarm && !longFired && now - touchDownAt > 800) {
    timerToggle();
    longFired = true;
  }

  if (!t && touchPrev) {                          // touch chere dilo
    if (longFired) {
      if (!clockMode && !langToggled) setMood(HAPPY, 1200);
      tapCount = 0;
    } else if (now - touchDownAt < 600) {
      tapCount++;
      lastTapAt = now;
    }
  }

  if (!t && tapCount > 0 && now - lastTapAt > 500) {
    if (tapCount >= 8) {
      timerAlarm = false;
      showBattery();                              // 🔋 8 tap = battery
    } else if (tapCount == 7) {
      timerAlarm = false;
      showQR();                                   // 🐙 7 tap = GitHub QR
    } else if (tapCount == 6) {
      timerAlarm = false;
      gameEnter();                                // 🎮 6 tap = khela
    } else if (tapCount == 5) {
      timerAlarm = false;
      setClockMode(!clockMode);
    } else if (clockMode) {
      if (timerAlarm)                                  timerAlarm = false;   // alarm bondho
      else if (clockView == CV_TIMER && tapCount == 2) timerAddMinute();
      else if (clockView == CV_TIMER && tapCount >= 3) timerReset();
      else                                             nextClockView();      // porer screen
    } else if (tapCount == 3) {
      playNextSong();                             // 3 tap = gaan, protibar porer ta
    } else if (tapCount == 4) {
      playFunnySound();                           // 4 tap = random funny meme shobdo 😂
    } else if (tapCount >= 2) {
      setMood(LOVE, 3000);
      Serial.println(F("Touch: double tap (LOVE)"));
    } else if (mood == SLEEP) {
      setMood(SLEEPY, 1500);                      // aste kore ghum bhanga
      Serial.println(F("Touch: tap (ghum theke uthche)"));
    } else if (mood == EXCITED && bandOverride) {
      // "yay!" cholche (keu khelte eseche) - oita-i thakuk
    } else {
      setMood(HAPPY, 2000);
      Serial.println(F("Touch: tap (HAPPY)"));
    }
    tapCount = 0;
  }

  touchPrev = t;
}

// ===========================================================
//  WiFi + time
// ===========================================================
bool timeValid() { return time(nullptr) > 1700000000; }

// Chalu howar shomoy touch 3 second chepe dhore rakhle mone rakha WiFi muche dey
bool checkWifiResetGesture() {
  if (digitalRead(PIN_TOUCH) != HIGH) return false;

  const unsigned long HOLD_MS = 3000;
  unsigned long t0 = millis();
  while (digitalRead(PIN_TOUCH) == HIGH) {
    unsigned long held = millis() - t0;
    int bw = held >= HOLD_MS ? 118 : (int)(held * 118 / HOLD_MS);

    display.clearDisplay();
    drawLabel("WiFi Reset", 0);
    drawBn(BN_HOLD, 18, 0);                                   // টাচ ধরে রাখো...
    display.drawRoundRect(4, 42, 120, 10, 3, SSD1306_WHITE);
    display.fillRoundRect(5, 43, bw, 8, 2, SSD1306_WHITE);
    display.display();

    if (held >= HOLD_MS) {
      if (soundOn) tone(PIN_BUZZER, 2600, 150);
      display.clearDisplay();
      drawLabel("WiFi Reset", 0);
      drawBn(BN_ERASED1, 18, 0);                              // WiFi মুছে গেছে
      drawBn(BN_ERASED2, 38, 0);                              // এবার টাচ ছেড়ে দাও
      display.display();
      while (digitalRead(PIN_TOUCH) == HIGH) delay(10);   // chhere dewa porjonto
      delay(300);
      return true;
    }
    delay(30);
  }

  // 3 second er age chhere dile kichu hobe na
  display.clearDisplay();
  drawLabel("WiFi Reset", 0);
  drawBn(BN_CANCEL, 28, 0);                                   // বাতিল
  display.display();
  delay(600);
  return false;
}

void wifiBegin(bool forgetSaved) {
  WiFi.mode(WIFI_STA);
  WiFi.hostname(DEVICE_NAME);                 // router er list e "PetBot" naam dekhabe
  if (forgetSaved) {
    wm.resetSettings();                    // mone rakha WiFi muche fela
    Serial.println(F("Saved WiFi muche fela holo"));
  }
  wm.setDebugOutput(false);
  wm.setConfigPortalBlocking(false);       // setup cholar somoy o robot er face cholbe
  wm.setConnectTimeout(15);
  wm.setConfigPortalTimeout(300);          // 5 min por setup WiFi bondho
  if (wm.autoConnect(AP_NAME)) {
    Serial.print(F("WiFi connected: "));
    Serial.println(WiFi.SSID());
    wifiWasConnected = true;
  } else {
    Serial.println("WiFi nai. Phone theke '" AP_NAME "' e connect kore WiFi set koro.");
  }
  configTime(TZ_INFO, NTP_1, NTP_2);
}

// Internet theke weather ana (15 min por por, error hole 1 min por abar)
void updateWeather() {
  if (WiFi.status() != WL_CONNECTED || (long)(now - nextWeather) < 0) return;
  nextWeather = now + WEATHER_EVERY;

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(4000);
  const char* url = "http://api.open-meteo.com/v1/forecast?latitude=" WEATHER_LAT
                    "&longitude=" WEATHER_LON "&current=temperature_2m,weather_code";
  if (!http.begin(client, url)) { nextWeather = now + 60000; return; }

  int code = http.GET();
  if (code == 200) {
    String body = http.getString();
    int c = body.indexOf("\"current\":{");
    int t = c >= 0 ? body.indexOf("\"temperature_2m\":", c) : -1;
    int w = c >= 0 ? body.indexOf("\"weather_code\":", c) : -1;
    if (t >= 0 && w >= 0) {
      weatherTemp = body.substring(t + 17).toFloat();
      weatherCode = body.substring(w + 15).toInt();
      weatherOk = true;
      Serial.printf_P(PSTR("Weather: %.1f C, code %d\n"), weatherTemp, weatherCode);
    } else {
      nextWeather = now + 60000;
    }
  } else {
    Serial.printf_P(PSTR("Weather error: %d\n"), code);
    nextWeather = now + 60000;
  }
  http.end();
}

// ===========================================================
//  📱 Phone theke control: robot er nijer web page (http://<IP>  ba  http://petbot.local)
// ===========================================================
ESP8266WebServer web(80);
bool webStarted = false;

// Phone theke asha message (screen e dekhay)
String msgLines[8];
const BnText* msgBand = &BN_MSG;            // message er holud lekha (মেসেজ এসেছে! / মনে করিয়ে দিচ্ছি!)
int    msgLineCount = 0;
static const Note S_MSG[] PROGMEM = {{1319,120},{0,40},{1047,120},{0,40},{1568,260}};   // "ding-ding-dong"

bool messageActive() { return msgUntil && (long)(now - msgUntil) < 0; }

// Lekha ke shobdo dhore line e bhaga kora (FreeSans 9pt, 124 px chowra)
void wrapMessage(const String& s) {
  msgLineCount = 0;
  String line = "", word = "";
  String src = s + " ";
  for (unsigned int i = 0; i < src.length() && msgLineCount < 8; i++) {
    char c = src[i];
    if (c == ' ' || c == '\n') {
      String test = line.length() ? line + " " + word : word;
      if (advanceOf(FreeSans9pt7bGlyphs, test.c_str()) > 124 && line.length()) {
        msgLines[msgLineCount++] = line;
        line = word;
      } else {
        line = test;
      }
      word = "";
      if (c == '\n' && msgLineCount < 8) { msgLines[msgLineCount++] = line; line = ""; }
    } else {
      word += c;
    }
  }
  if (line.length() && msgLineCount < 8) msgLines[msgLineCount++] = line;
}

void showMessage(String t) {
  String clean = "";                                   // shudhu English akkhor (Bangla font robot e nei)
  for (unsigned int i = 0; i < t.length() && clean.length() < 140; i++) {
    char c = t[i];
    if (c >= 32 && c <= 126) clean += c;
    else if (c == '\n') clean += '\n';
  }
  clean.trim();
  if (!clean.length()) return;
  wrapMessage(clean);
  msgBand = &BN_MSG;
  msgSince = now;
  msgUntil = now + max(6000UL, (unsigned long)(clean.length() * 160 + (msgLineCount > 3 ? (msgLineCount - 3) * 2000 : 0)));
  welcomeStep = -1;
  demoMode = false;
  lastActivity = now;
  moodSoundEnabled = false;
  setMood(EXCITED, msgUntil - now);                    // message er shomoy haat nache
  moodSoundEnabled = true;
  PLAY(S_MSG);
  Serial.print(F("Message: "));
  Serial.println(clean);
}

void drawEnvelope(int x, int y) {
  display.drawRect(x, y, 13, 9, SSD1306_WHITE);
  display.drawLine(x, y, x + 6, y + 5, SSD1306_WHITE);
  display.drawLine(x + 12, y, x + 6, y + 5, SSD1306_WHITE);
}

void drawMessage() {
  display.clearDisplay();
  drawBandBitmap(*msgBand);                            // মেসেজ এসেছে! / মনে করিয়ে দিচ্ছি!
  if ((now / 400) % 2) { drawEnvelope(1, 3); drawEnvelope(114, 3); }
  int first = 0;
  if (msgLineCount > 3) {                              // lomba hole 2 s por por ek line kore upore uthe
    first = (int)((now - msgSince) / 2000);
    if (first > msgLineCount - 3) first = msgLineCount - 3;
  }
  display.setTextColor(SSD1306_WHITE);
  for (int i = 0; i < 3 && first + i < msgLineCount; i++) {
    const char* s = msgLines[first + i].c_str();
    int w = advanceOf(FreeSans9pt7bGlyphs, s);
    printFont(&FreeSans9pt7b, (SCREEN_W - w) / 2, 30 + i * 15, s);
  }
  display.display();
}

// ---------- Command (phone page + Telegram duijon ei use kore) ----------
// QR / battery / test screen khola thakle bondho (phone ba Telegram theke onno kichu korle)
#define closeOverlays() do { qrUntil = 0; batShowUntil = 0; screenHoldUntil = 0; } while (0)

void cmdMood(Mood m) {
  closeOverlays();
  clockMode = false; demoMode = false; welcomeStep = -1; msgUntil = 0;
  lastActivity = now;
  setMood(m, m == NORMAL ? 0 : 6000);
}
const char* cmdSong(int i) {                           // i < 0 hole porer gaan
  closeOverlays();
  clockMode = false; msgUntil = 0;
  if (i >= 0) songIdx = constrain(i, 0, SONG_COUNT - 1) - 1;
  playNextSong();
  return SONGS[songIdx].name;
}
void cmdFunny()  { closeOverlays(); clockMode = false; msgUntil = 0; playFunnySound(); }
bool cmdClock()  { closeOverlays(); msgUntil = 0; setClockMode(!clockMode); return clockMode; }
void cmdLang()   { clockMode = false; toggleLanguage(); }
bool cmdMute() {
  soundOn = !soundOn;
  if (!soundOn) { noTone(PIN_BUZZER); sndNotes = nullptr; }
  return soundOn;
}
void cmdSleep()  { closeOverlays(); clockMode = false; msgUntil = 0; setMood(SLEEP, 0); }
void cmdWake()   { closeOverlays(); clockMode = false; lastActivity = now; setMood(SURPRISED, 1200); }
Mood cmdWeather() {                                    // NORMAL = bishesh kichu na
  Mood wm = currentWeatherMood();
  if (wm != NORMAL) { clockMode = false; setMood(wm, WEATHER_MOOD_MS); }
  return wm;
}
void cmdTimer(int mn) {
  mn = constrain(mn, 1, 99);
  msgUntil = 0;
  if (!clockMode) setClockMode(true);
  clockView = CV_TIMER; viewChangedAt = now;
  timerRunning = false; timerLeftMs = (unsigned long)mn * 60000UL;
  timerToggle();
}
String timeStr() {
  if (!timeValid()) return "--:--";
  time_t t = time(nullptr);
  struct tm* tm = localtime(&t);
  char b[16];
  if (use24) { snprintf(b, sizeof(b), "%02d:%02d", tm->tm_hour, tm->tm_min); return String(b); }
  int h = tm->tm_hour % 12; if (!h) h = 12;
  snprintf(b, sizeof(b), "%d:%02d %s", h, tm->tm_min, tm->tm_hour < 12 ? "AM" : "PM");
  return String(b);
}
String weatherStr(const char* deg) {
  if (!weatherOk) return "--";
  String s = String((int)roundf(weatherTemp));
  s += deg; s += ' '; s += weatherText(weatherCode);
  return s;
}

// ===========================================================
//  ✈️ Telegram bot: je kono jayga theke (mobile data / onno WiFi) PetBot ke control
//  - Token phone page e boshano hoy (EEPROM e thake, code e na)
//  - Shudhu "malik" (pair kora Telegram account) er kotha shune
//  - Long-poll: message ashle shathe shathe pay, face animation thame na
// ===========================================================
#define TG_HOST      "api.telegram.org"
#define EE_TG_MAGIC  16                // 0xC7 = Telegram config ache
#define EE_TG_TOKEN  17                // 63 byte
#define EE_TG_OWNER  80                // 8 byte (chat id)
#define EE_TG_NAME   88                // 16 byte (malik er naam)
#define TG_MAGIC     0xC7
#define TG_POLL_S    20                // ekta getUpdates koto second opekkha kore
#define TG_PAIR_MS   600000UL          // pair korar shomoy: 10 min
#define TG_MAXBUF    3072              // uttor er shorbochcho size
#define TG_QMAX      3

char    tgToken[64] = "";
int64_t tgOwner = 0;
char    tgOwnerName[16] = "";
String  tgBot = "";                    // bot er username (getMe theke)
String  tgState = "off";               // page e dekhanor jonno
unsigned long tgPairUntil = 0;

BearSSL::WiFiClientSecure tgCli;
BearSSL::X509List* tgCA = nullptr;
BearSSL::Session   tgSession;          // abar connect korle druto (TLS session resume)
int     tgRxSize = 0;                  // TLS receive buffer (MFLN thakle 2048, na hole 16384)
#define TR_NONE 0                      // kon request cholche
#define TR_ME   1
#define TR_CMDS 2
#define TR_SEND 3
#define TR_POLL 4
uint8_t tgReq = TR_NONE;
String  tgBuf;
unsigned long tgReqAt = 0, tgRetryAt = 0;
uint8_t tgFails = 0;
bool    tgNeedMe = true, tgNeedCmds = true, tgSaidOnline = false;
int64_t tgOffset = 0;
String  tgQ[TG_QMAX];                  // pathanor opekkhay thaka uttor (sendMessage body)
uint8_t tgQn = 0;

String i64str(int64_t v) {
  char b[24]; char* p = b + 23; *p = 0;
  bool neg = v < 0;
  uint64_t u = neg ? (uint64_t)(-(v + 1)) + 1 : (uint64_t)v;
  do { *--p = '0' + (u % 10); u /= 10; } while (u);
  if (neg) *--p = '-';
  return String(p);
}

void tgLoadCfg() {
  if (EEPROM.read(EE_TG_MAGIC) != TG_MAGIC) return;
  for (int i = 0; i < 63; i++) tgToken[i] = EEPROM.read(EE_TG_TOKEN + i);
  tgToken[63] = 0;
  uint64_t o = 0;
  for (int i = 0; i < 8; i++) o |= (uint64_t)EEPROM.read(EE_TG_OWNER + i) << (8 * i);
  tgOwner = (int64_t)o;
  for (int i = 0; i < 15; i++) tgOwnerName[i] = EEPROM.read(EE_TG_NAME + i);
  tgOwnerName[15] = 0;
  if (tgToken[0]) tgState = "connecting";              // chalu howar por: "off" na, connect er opekkha
  Serial.printf_P(PSTR("Telegram: token %s, malik %s\n"), tgToken[0] ? "ache" : "nei", tgOwner ? tgOwnerName : "nei");
}

void tgSaveCfg() {
  EEPROM.write(EE_TG_MAGIC, TG_MAGIC);
  for (int i = 0; i < 63; i++) EEPROM.write(EE_TG_TOKEN + i, tgToken[i]);
  uint64_t o = (uint64_t)tgOwner;
  for (int i = 0; i < 8; i++) EEPROM.write(EE_TG_OWNER + i, (uint8_t)(o >> (8 * i)));
  for (int i = 0; i < 15; i++) EEPROM.write(EE_TG_NAME + i, tgOwnerName[i]);
  EEPROM.commit();
}

String jsonEsc(const String& s) {
  String o; o.reserve(s.length() + 8);
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if (c == '\n') o += "\\n";
    else if ((uint8_t)c >= 0x20) o += c;
  }
  return o;
}

// Niche thaka button (Telegram er keyboard)
static const char TG_KB[] PROGMEM =
  ",\"reply_markup\":{\"keyboard\":["
  "[\"❤️ Love\",\"😊 Happy\",\"😂 Laugh\"],"
  "[\"🎵 Song\",\"🤪 Funny\",\"😉 Wink\"],"
  "[\"😴 Sleep\",\"☀️ Wake\",\"⏰ Clock\"],"
  "[\"🌤️ Weather\",\"📊 Status\",\"❓ Help\"]"
  "],\"resize_keyboard\":true,\"is_persistent\":true}";

// "/" chaple je command list ashe
static const char TG_CMDS[] PROGMEM = "{\"commands\":["
  "{\"command\":\"help\",\"description\":\"Shob command + button\"},"
  "{\"command\":\"status\",\"description\":\"PetBot ki korche\"},"
  "{\"command\":\"date\",\"description\":\"Bangla + Hijri tarikh\"},"
  "{\"command\":\"namaz\",\"description\":\"Aj er namaz er shomoy\"},"
  "{\"command\":\"sehri\",\"description\":\"Sehri o Iftar er shomoy\"},"
  "{\"command\":\"azan\",\"description\":\"Namaz alert on/off\"},"
  "{\"command\":\"game\",\"description\":\"Robot e khelar menu\"},"
  "{\"command\":\"alarm\",\"description\":\"Alarm, je kono shomoy (/alarm 3pm)\"},"
  "{\"command\":\"format\",\"description\":\"12 / 24 ghontar shomoy\"},"
  "{\"command\":\"battery\",\"description\":\"Battery koto % ache\"},"
  "{\"command\":\"bedtime\",\"description\":\"Ghumer shomoy (/bedtime 11pm)\"},"
  "{\"command\":\"remind\",\"description\":\"Reminder (/remind 30m Pani khao)\"},"
  "{\"command\":\"birthday\",\"description\":\"Jonmodin (/birthday 15-03)\"},"
  "{\"command\":\"list\",\"description\":\"Alarm o reminder er list\"},"
  "{\"command\":\"ask\",\"description\":\"Magic 8-ball proshno\"},"
  "{\"command\":\"dice\",\"description\":\"Chhokka chalo\"},"
  "{\"command\":\"rps\",\"description\":\"Pathor-kagoj-kachi (/rps rock)\"},"
  "{\"command\":\"love\",\"description\":\"Love face\"},"
  "{\"command\":\"happy\",\"description\":\"Happy face\"},"
  "{\"command\":\"laugh\",\"description\":\"Hasi\"},"
  "{\"command\":\"song\",\"description\":\"Porer gaan (/song 5)\"},"
  "{\"command\":\"songs\",\"description\":\"Gaaner list\"},"
  "{\"command\":\"funny\",\"description\":\"Funny sound\"},"
  "{\"command\":\"sleep\",\"description\":\"Ghum pare dao\"},"
  "{\"command\":\"wake\",\"description\":\"Jagao\"},"
  "{\"command\":\"clock\",\"description\":\"Clock on/off\"},"
  "{\"command\":\"weather\",\"description\":\"" CITY_NAME " er weather\"},"
  "{\"command\":\"timer\",\"description\":\"Timer (/timer 5)\"},"
  "{\"command\":\"mute\",\"description\":\"Sound on/off\"},"
  "{\"command\":\"lang\",\"description\":\"Bangla/English\"}"
  "]}";

void tgQueue(int64_t chat, const String& text, bool keyboard) {
  if (tgQn >= TG_QMAX) return;
  String b = "{\"chat_id\":";
  b += i64str(chat);
  b += ",\"text\":\"";
  b += jsonEsc(text);
  b += "\"";
  if (keyboard) b += FPSTR(TG_KB);
  b += "}";
  tgQ[tgQn++] = b;
}

void tgNotify(const String& text) {                    // malik ke nijer theke janano
  if (tgToken[0] && tgOwner) tgQueue(tgOwner, text, false);
}

bool tgPairing() { return tgPairUntil && (long)(millis() - tgPairUntil) < 0; }

void tgStartPair() {
  tgPairUntil = millis() + TG_PAIR_MS;
  if (!tgPairUntil) tgPairUntil = 1;
  showMessage("Telegram: open your bot, send /start");
}

void tgRestart() {                                     // notun token / muche fela
  tgCli.stop();
  tgReq = TR_NONE; tgBuf = "";
  tgNeedMe = tgNeedCmds = true; tgSaidOnline = false;
  tgBot = ""; tgQn = 0; tgRetryAt = 0; tgFails = 0; tgOffset = 0;
  tgState = tgToken[0] ? "connecting" : "off";
}

bool tgConnect() {
  if (!tgCA) {
    String pem = FPSTR(TELEGRAM_CA);                   // flash theke RAM e ene parse
    tgCA = new BearSSL::X509List(pem.c_str());
  }
  tgCli.stop();
  tgCli.setTrustAnchors(tgCA);
  tgCli.setX509Time(time(nullptr));
  tgCli.setSession(&tgSession);
  if (!tgRxSize) {                                     // server choto TLS packet (MFLN) mane? tahole kom RAM lage
    bool mf = tgCli.probeMaxFragmentLength(TG_HOST, 443, 2048);
    tgRxSize = mf ? 2048 : 16384;
    Serial.printf_P(PSTR("Telegram MFLN: %s -> rx buffer %d\n"), mf ? "ache" : "nei", tgRxSize);
  }
  uint32_t heap = ESP.getFreeHeap();
  if (heap < (uint32_t)tgRxSize + 9000) {
    Serial.printf_P(PSTR("Telegram: RAM kom (%u)\n"), heap);
    tgState = "RAM kom";
    return false;
  }
  tgCli.setBufferSizes(tgRxSize, 1024);
  tgCli.setTimeout(6000);
  unsigned long t0 = millis();
  bool ok = tgCli.connect(TG_HOST, 443);
  Serial.printf_P(PSTR("Telegram connect: %s (%lu ms, heap %u -> %u)\n"), ok ? "OK" : "HOY NI",
                millis() - t0, heap, ESP.getFreeHeap());
  if (!ok) {
    char err[80];
    int e = tgCli.getLastSSLError(err, sizeof(err));
    Serial.printf_P(PSTR("  SSL error %d: %s\n"), e, err);
  }
  return ok;
}

void tgRequest(uint8_t r, const char* method, const String& body) {
  String q; q.reserve(190 + body.length());
  q = "POST /bot"; q += tgToken; q += '/'; q += method;
  q += " HTTP/1.1\r\nHost: " TG_HOST "\r\nContent-Type: application/json\r\nContent-Length: ";
  q += body.length();
  q += "\r\nConnection: keep-alive\r\n\r\n";
  q += body;
  tgCli.write((const uint8_t*)q.c_str(), q.length());
  tgReq = r; tgReqAt = millis(); tgBuf = "";
}

// 1 = uttor shesh, 0 = ekhono ashche, -1 = bhul, -2 = uttor onek boro
int tgReadResp(int& code, String& body) {
  uint8_t tmp[256];
  while (tgCli.available() && tgBuf.length() < TG_MAXBUF) {
    int n = tgCli.read(tmp, min((int)sizeof(tmp), (int)(TG_MAXBUF - tgBuf.length())));
    if (n <= 0) break;
    tgBuf.concat((const char*)tmp, n);
  }
  int he = tgBuf.indexOf("\r\n\r\n");
  if (he < 0) return tgBuf.length() >= TG_MAXBUF ? -1 : 0;
  code = tgBuf.substring(9, 12).toInt();
  String head = tgBuf.substring(0, he);
  head.toLowerCase();
  int cl = head.indexOf("content-length:");
  if (cl >= 0) {
    long len = head.substring(cl + 15).toInt();
    if ((long)tgBuf.length() >= he + 4 + len) { body = tgBuf.substring(he + 4, he + 4 + len); return 1; }
  } else if (head.indexOf("chunked") >= 0) {
    if (tgBuf.endsWith("\r\n0\r\n\r\n")) {
      body = "";
      int p = he + 4;
      while (p < (int)tgBuf.length()) {
        int le = tgBuf.indexOf("\r\n", p);
        if (le < 0) break;
        long sz = strtol(tgBuf.substring(p, le).c_str(), nullptr, 16);
        if (sz <= 0) break;
        body += tgBuf.substring(le + 2, le + 2 + sz);
        p = le + 2 + sz + 2;
      }
      return 1;
    }
  } else if (!tgCli.connected() && !tgCli.available()) {
    body = tgBuf.substring(he + 4);
    return 1;
  }
  return tgBuf.length() >= TG_MAXBUF ? -2 : 0;
}

String tgNorm(const String& s) {                       // "❤️ Love" -> "love", "/song@PetBot 3" -> "song 3"
  String o; bool at = false;
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '@') { at = true; continue; }
    if (c == ' ' || c == '\n') { at = false; if (o.length() && !o.endsWith(" ")) o += ' '; continue; }
    if (at) continue;
    if (isalnum((uint8_t)c) && (uint8_t)c < 128) o += (char)tolower(c);
  }
  o.trim();
  return o;
}

static const char* const TG_MOOD_KEY[] = {
  "normal", "happy", "laugh", "love", "wink", "sad", "cry", "angry", "surprise", "scared",
  "confused", "suspicious", "bored", "excited", "sleepy", "", "dizzy",
  "cold", "hot", "rainy", "stormy", "cool", "", "please", "annoyed", "sneeze", "stretch", "hum", "whistle"
};
static const char* const TG_MOOD_EMOJI[] = {
  "😐", "😊", "😂", "😍", "😉", "😢", "😭", "😤", "😲", "😱",
  "🤔", "🤨", "😑", "🤩", "😪", "😴", "😵",
  "🥶", "🥵", "🌧️", "⛈️", "😎", "🎵", "🥺", "😒", "🤧", "🙆", "🎵", "😗"
};

int tgMoodOf(const String& c) {
  for (int i = 0; i < MOOD_COUNT && i < (int)(sizeof(TG_MOOD_KEY) / sizeof(TG_MOOD_KEY[0])); i++)
    if (TG_MOOD_KEY[i][0] && c == TG_MOOD_KEY[i]) return i;
  if (c == "surprised") return SURPRISED;
  if (c == "sunny") return SUNNY;
  return -1;
}

bool tgKnown(const String& c) {
  if (tgMoodOf(c) >= 0) return true;
  static const char* const K[] = { "start", "help", "menu", "status", "song", "songs", "funny", "clock",
                                   "weather", "sleep", "wake", "mute", "sound", "lang", "timer",
                                   "date", "tarikh", "hijri", "namaz", "namaj", "prayer", "salat",
                                   "sehri", "iftar", "azan", "game", "khela", "ask", "dice", "rps",
                                   "rock", "paper", "scissors", "pathor", "kagoj", "kachi",
                                   "alarm", "bedtime", "birthday", "jonmodin", "remind", "reminder", "list", "format", "github", "battery", "bat" };
  for (auto k : K) if (c == k) return true;
  return false;
}

String tgHelp() {
  String s = F("🤖 " CREDIT_TITLE " by " CREDIT_AUTHOR "\n\n");
  s += F("💬 Je kono English lekha pathao, ami screen e dekhabo!\n\n");
  s += F("😊 Face: /love /happy /laugh /wink /sad /cry /angry /surprise /scared /confused /bored /excited /cool /please /whistle /sneeze /dizzy\n");
  s += F("🎵 /song porer gaan, /songs list, /song 5\n");
  s += F("😂 /funny   ⏰ /clock   🌤️ /weather\n");
  s += F("😴 /sleep   ☀️ /wake\n");
  s += F("⏱️ /timer 5 (5 minute)\n");
  s += F("📅 /date Bangla + Hijri tarikh, porer bishesh din\n");
  s += F("🕌 /namaz aj er namaz er shomoy, /sehri /iftar\n");
  s += F("🔔 /azan on|off: namaz er shomoy Telegram e janabo\n");
  s += F("🎮 /game khela, /ask 🎱, /dice 🎲, /rps rock ✊\n");
  s += F("⏰ /alarm 7:00 ba /alarm 3:30pm ekbar (4 ta porjonto), /alarm del 1\n");
  s += F("🌙 /bedtime 11pm  🎂 /birthday 15-03  🕐 /format (12/24 ghonta)\n");
  s += F("🔋 /battery koto charge ache  🐙 /github\n");
  s += F("📝 /remind 30m Pani khao, /remind 9:30pm Medicine, /list\n");
  s += F("🌙 /hijri 1 ba /hijri -1: Hijri tarikh 1 din shorao\n");
  s += F("🔇 /mute   🌐 /lang   📊 /status");
  return s;
}

String tgStatusText() {
  String s = F("🤖 PetBot\n");
  s += F("😊 Mood: "); s += clockMode ? "CLOCK" : MOOD_NAME[mood]; s += '\n';
  s += F("🕐 "); s += timeStr(); s += '\n';
  s += F("🌤️ " CITY_NAME ": "); s += weatherStr("°C"); s += '\n';
  s += soundOn ? "🔊 Sound on\n" : "🔇 Mute\n";
  s += F("🌐 "); s += langEn ? "English\n" : "Bangla\n";
  s += F("📶 WiFi: "); s += WiFi.SSID(); s += F(" ("); s += WiFi.RSSI(); s += F(" dBm)\n");
  unsigned long up = millis() / 60000UL;
  s += F("⏱️ Chalu: "); s += up / 60; s += F("h "); s += up % 60; s += F("m");
  return s;
}

// Malik er message / command
void tgCommand(int64_t chat, const String& raw) {
  String n = tgNorm(raw);
  unsigned int i = 0;
  while (i < n.length() && isalpha((uint8_t)n[i])) i++;
  String cmd = n.substring(0, i);
  String rest = n.substring(i); rest.trim();
  int arg = rest.length() ? rest.toInt() : -1;
  bool isCmd = raw.startsWith("/") || (rest.length() == 0 && tgKnown(cmd));

  if (!isCmd) {                                        // shadharon lekha -> screen e
    bool ascii = false;
    for (unsigned int k = 0; k < raw.length(); k++) if (isalnum((uint8_t)raw[k]) && (uint8_t)raw[k] < 128) ascii = true;
    if (!ascii) { tgQueue(chat, F("😅 Amar screen e shudhu English akkhor dekhano jay. English e likho!"), false); return; }
    showMessage(raw);
    tgQueue(chat, F("📩 PetBot er screen e dekhacche!"), false);
    return;
  }

  int m = tgMoodOf(cmd);
  String r;
  bool kb = false;
  if (m >= 0) {
    cmdMood((Mood)m);
    r = TG_MOOD_EMOJI[m]; r += F(" PetBot ekhon "); r += MOOD_NAME[m]; r += F("!");
  } else if (cmd == "start" || cmd == "help" || cmd == "menu") {
    tgPairUntil = 0;
    r = tgHelp(); kb = true;
  } else if (cmd == "status") {
    r = tgStatusText();
  } else if (cmd == "namaz" || cmd == "namaj" || cmd == "prayer" || cmd == "salat") {
    r = tgPrayerText();
  } else if (cmd == "sehri" || cmd == "iftar") {
    r = tgRozaText();
  } else if (cmd == "game" || cmd == "khela") {
    gameEnter();
    r = F("🎮 PetBot e khelar menu khulechi! Robot e tap = porer khela, chepe dhoro = shuru.\nEkhan thekeo khelo: /ask proshno, /dice, /rps rock");
  } else if (cmd == "ask") {
    int a = gameRemote8(-1);
    r = F("🎱 ম্যাজিক বল বলছে:\n\n"); r += g8Answer(a);
  } else if (cmd == "dice") {
    int v = gameRemoteDice();
    r = F("🎲 ছক্কা পড়লো: "); r += bnDigitsUtf(v);
    if (v == 6) r += F("! 🎉 Chhokka! Abar chalo!");
  } else if (cmd == "rps" || cmd == "rock" || cmd == "paper" || cmd == "scissors" || cmd == "pathor" || cmd == "kagoj" || cmd == "kachi") {
    String w = cmd == "rps" ? rest : cmd;
    int you = (w == "rock" || w == "pathor") ? 0 : (w == "paper" || w == "kagoj") ? 1 : (w == "scissors" || w == "kachi") ? 2 : -1;
    if (you < 0) r = F("✊✋✌️ Lekho: /rps rock, /rps paper ba /rps scissors");
    else r = gameRemoteRps(you);
  } else if (cmd == "azan") {
    if (rest == "on") prayerFlags |= 1;
    else if (rest == "off") prayerFlags &= ~1;
    else prayerFlags ^= 1;
    savePrayerCfg();
    r = (prayerFlags & 1) ? F("🔔 Namaz er shomoy hole Telegram e janabo (5 waqt). Bondho: /azan off")
                          : F("🔕 Telegram e namaz alert bondho. Chalu: /azan on\n(Robot er screen e alert cholbe)");
  } else if (cmd == "alarm" || cmd == "bedtime" || cmd == "birthday" || cmd == "jonmodin" || cmd == "remind" || cmd == "reminder") {
    r = tgSchedCommand(cmd == "reminder" ? String(F("remind")) : cmd, raw);
  } else if (cmd == "battery" || cmd == "bat") {
    r = tgBatteryText(raw);
    if (batOk) showBattery();
  } else if (cmd == "github") {
    showQR();
    r = F("🐙 " CREDIT_AUTHOR " er GitHub:\n" CREDIT_URL "\n\n(PetBot er screen e QR dekhacchi, 30 second)");
  } else if (cmd == "format") {
    toggleTimeFormat();
    r = use24 ? F("🕓 Ekhon 24 ghontar shomoy (jemon 17:30)") : F("🕐 Ekhon 12 ghontar shomoy (jemon 5:30 PM)");
  } else if (cmd == "list") {
    r = schedListText();
  } else if (cmd == "date" || cmd == "tarikh") {
    r = tgDateText();
  } else if (cmd == "hijri") {
    if (arg > 0) {                                     // /hijri 1 ba /hijri -1: chand dekha onujayi 1 din shorao
      int v = raw.indexOf('-') >= 0 ? -arg : arg;
      hijriAdj = constrain(hijriAdj + v, -3, 3);
      saveHijriAdj();
      r = F("✅ Hijri tarikh shoranu holo.\n\n");
    }
    r += tgDateText();
  } else if (cmd == "songs") {
    r = F("🎶 Gaaner list:\n");
    for (int k = 0; k < SONG_COUNT; k++) { r += F("/song"); r += k + 1; r += F("  "); r += SONGS[k].name; r += '\n'; }
  } else if (cmd == "song") {
    r = F("🎵 Bajacchi: "); r += cmdSong(arg > 0 ? arg - 1 : -1);
  } else if (cmd == "funny") {
    cmdFunny(); r = F("😂 Haha!");
  } else if (cmd == "clock") {
    r = cmdClock() ? "⏰ Clock on" : "😊 Clock off, face e firlam";
  } else if (cmd == "weather") {
    Mood wm = cmdWeather();
    r = F("🌤️ " CITY_NAME ": "); r += weatherStr("°C");
    if (wm != NORMAL) { r += F("\n"); r += TG_MOOD_EMOJI[wm]; r += F(" PetBot ekhon "); r += MOOD_NAME[wm]; }
  } else if (cmd == "sleep") {
    cmdSleep(); r = F("😴 Ghumiye porlam... Zzz");
  } else if (cmd == "wake") {
    cmdWake(); r = F("☀️ Jege gechi!");
  } else if (cmd == "mute" || cmd == "sound") {
    r = cmdMute() ? "🔊 Sound on" : "🔇 Mute";
  } else if (cmd == "lang") {
    cmdLang(); r = langEn ? "🌐 English" : "🌐 Bangla";
  } else if (cmd == "timer") {
    int mn = arg > 0 ? arg : 5;
    cmdTimer(mn);
    r = F("⏱️ "); r += constrain(mn, 1, 99); r += F(" minute er timer shuru! Shesh hole janabo.");
  } else {
    r = F("🤔 Bujhlam na. /help dekho");
  }
  tgQueue(chat, r, kb);
}

void tgHandle(int64_t chat, const char* name, const String& text, long date, bool priv) {
  time_t t = time(nullptr);
  if (date && (long)(t - date) > 120) return;          // robot bondho thakar shomoyer purono message bad
  if (chat == tgOwner && tgOwner) { tgCommand(chat, text); return; }
  if (!priv) return;
  if (tgPairing() && tgNorm(text).startsWith("start")) {   // notun malik
    tgOwner = chat;
    strncpy(tgOwnerName, name, 15); tgOwnerName[15] = 0;
    tgSaveCfg();
    tgPairUntil = 0;
    String w = "🎉 Hi "; w += name; w += F("! Ami PetBot 🤖\nEkhon theke ami shudhu tomar kotha shunbo.\n\n");
    w += tgHelp();
    tgQueue(chat, w, true);
    String sm = "Telegram connected! Hi "; sm += name;
    showMessage(sm);
    Serial.printf_P(PSTR("Telegram: notun malik %s\n"), name);
  } else {
    tgQueue(chat, F("🙂 Ami PetBot. Ami shudhu amar malik er kotha shuni."), false);
  }
}

void tgParseUpdates(const String& body) {
  JsonDocument filter;
  JsonObject f = filter["result"].add<JsonObject>();
  f["update_id"] = true;
  f["message"]["date"] = true;
  f["message"]["text"] = true;
  f["message"]["chat"]["id"] = true;
  f["message"]["chat"]["type"] = true;
  f["message"]["from"]["first_name"] = true;
  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, body, DeserializationOption::Filter(filter));
  if (e) { Serial.printf_P(PSTR("Telegram JSON bhul: %s\n"), e.c_str()); return; }
  for (JsonObject u : doc["result"].as<JsonArray>()) {
    int64_t id = u["update_id"].as<int64_t>();
    if (id >= tgOffset) tgOffset = id + 1;
    JsonObject m = u["message"];
    if (m.isNull() || !m["text"].is<const char*>()) continue;
    String text = m["text"].as<const char*>();
    const char* name = m["from"]["first_name"] | "";
    const char* type = m["chat"]["type"] | "";
    Serial.print(F("Telegram: ")); Serial.println(text);
    tgHandle(m["chat"]["id"].as<int64_t>(), name, text, m["date"] | 0L, strcmp(type, "private") == 0);
  }
}

// Serial 'T': nokol Telegram update diye command bujha test (kichu pathay na)
void tgSelfTest() {
  static uint8_t k = 0;
  static const char* const T[] = { "\\u2764\\ufe0f Love", "/song@PetBot 3", "Khete esho Mamun!", "/timer 2", "/status", "\\u0986\\u09ae\\u09bf", "/xyz", "/date" };
  int64_t o = tgOwner;
  tgOwner = 111;
  String b = F("{\"ok\":true,\"result\":[{\"update_id\":");
  b += 900 + k;
  b += F(",\"message\":{\"message_id\":1,\"from\":{\"id\":111,\"is_bot\":false,\"first_name\":\"Test\"},\"chat\":{\"id\":111,\"first_name\":\"Test\",\"type\":\"private\"},\"date\":");
  b += (long)time(nullptr);
  b += F(",\"text\":\"");
  b += T[k];
  b += F("\"}}]}");
  k = (k + 1) % (sizeof(T) / sizeof(T[0]));
  tgQn = 0;
  tgParseUpdates(b);
  Serial.print(F("TGTEST offset=")); Serial.println(i64str(tgOffset));
  for (int i = 0; i < tgQn; i++) { Serial.print(F("TGTEST reply: ")); Serial.println(tgQ[i]); }
  tgQn = 0;
  tgOwner = o;
  tgOffset = 0;
}

void tgDone(int code, const String& body) {
  uint8_t r = tgReq;
  tgReq = TR_NONE; tgBuf = "";
  if (code == 401 || code == 404) {                    // token bhul
    tgState = "token bhul";
    Serial.printf_P(PSTR("Telegram: token bhul (HTTP %d)\n"), code);
    tgCli.stop(); tgRetryAt = millis() + 600000UL;
    return;
  }
  if (code == 409) { tgState = "onno kothao chalu (409)"; tgRetryAt = millis() + 30000; return; }
  if (r == TR_SEND && code != 200 && code != 429 && code < 500) code = 200;   // pathano gelo na: bad dao
  if (code != 200) {
    Serial.printf_P(PSTR("Telegram HTTP %d\n"), code);
    tgRetryAt = millis() + (code == 429 ? 15000 : 5000);
    return;
  }
  tgFails = 0;
  tgState = "ok";
  if (r == TR_ME) {
    int p = body.indexOf("\"username\":\"");
    if (p >= 0) { p += 12; tgBot = body.substring(p, body.indexOf('"', p)); }
    tgNeedMe = false;
    Serial.print(F("Telegram bot: @")); Serial.println(tgBot);
    if (!tgSaidOnline && tgOwner) {
      tgSaidOnline = true;
      String s = F("🟢 PetBot online! 🤖\n📶 WiFi: "); s += WiFi.SSID();
      s += F("\n🏠 Bari theke: http://"); s += WiFi.localIP().toString();
      tgNotify(s);
    }
  } else if (r == TR_CMDS) {
    tgNeedCmds = false;
  } else if (r == TR_SEND) {
    for (int i = 1; i < tgQn; i++) tgQ[i - 1] = tgQ[i];
    if (tgQn) tgQ[--tgQn] = "";
  } else if (r == TR_POLL) {
    tgParseUpdates(body);
  }
}

void tgLoop() {
  if (!tgToken[0]) { tgState = "off"; return; }
  if (otaWaiting()) return;                            // 📡 update er shomoy Telegram chup (RAM lagbe)
  if (tgReq != TR_NONE) {
    int code = 0; String body;
    int r = tgReadResp(code, body);
    if (r == 1) { tgDone(code, body); return; }
    if (r == -2 && tgReq == TR_POLL) {                 // message onek boro: bad diye samne jao
      int p = tgBuf.indexOf("\"update_id\":");
      if (p >= 0) tgOffset = atoll(tgBuf.c_str() + p + 12) + 1;
      tgNotify(F("😅 Message ta onek boro chilo, chhoto kore pathao."));
    }
    unsigned long lim = tgReq == TR_POLL ? (TG_POLL_S + 15) * 1000UL : 15000UL;
    if (r < 0 || millis() - tgReqAt > lim || (!tgCli.connected() && !tgCli.available())) {
      Serial.println(F("Telegram: uttor pelam na, abar connect korbo"));
      tgCli.stop(); tgReq = TR_NONE; tgBuf = "";
      tgRetryAt = millis() + 2000;
    }
    return;
  }
  if ((long)(millis() - tgRetryAt) < 0) return;
  if (!timeValid()) { tgState = "time er opekkha"; return; }
  if (!tgCli.connected()) {
    // connect korte ~3 s lage (face theme thake), tai gaan / welcome / message cholle pore kori
    if (sndNotes || welcomeStep >= 0 || messageActive() || gameMode) return;
    tgState = "connecting";
    if (!tgConnect()) {
      tgFails++;
      if (tgState != "RAM kom") tgState = "connect hoy ni";
      tgRetryAt = millis() + min(60000UL, 5000UL * tgFails);
      return;
    }
  }
  if (tgNeedMe)        tgRequest(TR_ME, "getMe", "{}");
  else if (tgNeedCmds) tgRequest(TR_CMDS, "setMyCommands", String(FPSTR(TG_CMDS)));
  else if (tgQn)       tgRequest(TR_SEND, "sendMessage", tgQ[0]);
  else {
    String b = "{\"offset\":"; b += i64str(tgOffset);
    b += ",\"timeout\":" + String(TG_POLL_S) + ",\"limit\":1,\"allowed_updates\":[\"message\"]}";
    tgRequest(TR_POLL, "getUpdates", b);
  }
}

#include "web_page.h"                      // WEB_PAGE + FAVICON_SVG (phone er HTML page)
#include "web_page_gz.h"                   // WEB_PAGE er gzip (tools\make_web.ps1 banay)

void webOk(const String& t) { web.send(200, "text/plain; charset=utf-8", t); }

// Shob web route ekta function e (protita web.on() er alada memory lage - ekhane ~3KB RAM bache)
void webDispatch() {
  const String& u = web.uri();
  if (u == F("/")) { // gzip kora page (chhoto, druto, kete jay na)
    web.sendHeader("Content-Encoding", "gzip");
    web.send_P(200, "text/html; charset=utf-8", (const char*)WEB_PAGE_GZ, WEB_PAGE_GZ_LEN);
  } else if (u == F("/favicon.svg")) {
    web.sendHeader("Cache-Control", "max-age=86400"); web.send_P(200, "image/svg+xml", FAVICON_SVG);
  } else if (u == F("/favicon.ico")) {
    web.sendHeader("Cache-Control", "max-age=86400"); web.send_P(200, "image/svg+xml", FAVICON_SVG);
  } else if (u == F("/api/mode")) { // normal / game / clock
    webOk(webSetMode(web.arg("m")));
  } else if (u == F("/api/view")) { // clock er kon screen
    webOk(webClockView(web.arg("v").toInt()));
  } else if (u == F("/api/info")) {
    web.send(200, "application/json; charset=utf-8", webInfoJson());
  } else if (u == F("/api/azan")) {
    webOk(webToggleAzan());
  } else if (u == F("/api/battery")) {
    screenHoldUntil = 0; showBattery(); webOk(batOk ? String(batPct) + "% (" + String(batV, 2) + "V)" : String(F("tar lagano nei")));
  } else if (u == F("/api/qr")) {
    screenHoldUntil = 0; showQR(); webOk("🐙 GitHub QR");
  } else if (u == F("/api/splash")) { // test: shuru er screen (p=1 naam, p=2 credits) 8 s dekhao
    String p = web.arg("p");
    if (p == "2") drawCredits(3); else if (p == "3") drawSplashQR(); else drawSplashTitle(6, 6);
    screenHoldUntil = millis() + 8000;
    webOk("splash");
  } else if (u == F("/api/shot")) { // 📸 screen er chhobi (1024 byte hex) - USB chara test er jonno
    const uint8_t* b = display.getBuffer();
    String h; h.reserve(2050);
    static const char HX[] = "0123456789ABCDEF";
    for (int i = 0; i < SCREEN_W * SCREEN_H / 8; i++) { h += HX[b[i] >> 4]; h += HX[b[i] & 15]; }
    web.send(200, "text/plain", h);
  } else if (u == F("/api/fmt")) { // 12 <-> 24
    toggleTimeFormat(); webOk(use24 ? "🕓 24 ghonta" : "🕐 12 ghonta");
  } else if (u == F("/api/otaprep")) { // 📡 upload script: "update ashche, RAM khali koro"
    otaWaitUntil = millis() + 120000UL;
    tgCli.stop(); tgReq = TR_NONE;
    tgBuf = String(); tgBot = String();                // String er jayga-o ferot (shudhu khali na)
    for (int i = 0; i < TG_QMAX; i++) tgQ[i] = String();
    tgQn = 0;
    if (tgCA) { delete tgCA; tgCA = nullptr; }         // certificate list (~1.5KB) - pore abar banabe
    for (int i = 0; i < 8; i++) msgLines[i] = String();
    noTone(PIN_BUZZER); sndNotes = nullptr;
    if (gameMode) gameExit();
    String r = String(ESP.getFreeHeap()); r += F(" (boro tukro "); r += ESP.getMaxFreeBlockSize(); r += ')';
    webOk(r);
  } else if (u == F("/api/sched")) { // alarm, ghum, jonmodin, reminder
    webSched();
  } else if (u == F("/api/mood")) {
    int m = web.arg("m").toInt();
    if (m < 0 || m >= MOOD_COUNT || m == MUSIC) { webOk("bhul face"); return; }
    cmdMood((Mood)m);
    webOk(MOOD_NAME[m]);
  } else if (u == F("/api/msg")) {
    showMessage(web.arg("t")); webOk("message gelo");
  } else if (u == F("/api/song")) {
    webOk(cmdSong(web.hasArg("i") ? web.arg("i").toInt() : -1));
  } else if (u == F("/api/songs")) {
    String j = "[";
    for (int i = 0; i < SONG_COUNT; i++) { if (i) j += ","; j += "\""; j += SONGS[i].name; j += "\""; }
    web.send(200, "application/json", j + "]");
  } else if (u == F("/api/funny")) {
    cmdFunny(); webOk("haha");
  } else if (u == F("/api/clock")) {
    webOk(cmdClock() ? "clock on" : "clock off");
  } else if (u == F("/api/lang")) {
    cmdLang(); webOk(langEn ? "English" : "Bangla");
  } else if (u == F("/api/mute")) {
    webOk(cmdMute() ? "sound on" : "mute");
  } else if (u == F("/api/sleep")) {
    cmdSleep(); webOk("ghum");
  } else if (u == F("/api/wake")) {
    cmdWake(); webOk("jege geche");
  } else if (u == F("/api/weather")) {
    Mood wm = cmdWeather();
    webOk(wm == NORMAL ? "weather shadharon" : MOOD_NAME[wm]);
  } else if (u == F("/api/timer")) {
    String a = web.arg("a");
    if (a.length()) { webOk(webTimerAction(a)); return; }               // toggle / reset / add
    int mn = constrain(web.arg("min").toInt(), 1, 99);
    cmdTimer(mn);
    webOk(String(mn) + " min timer");
  } else if (u == F("/api/tg/token") && web.method() == HTTP_POST) {
    String t = web.arg("token");
    t.trim();
    bool ok = t.length() >= 30 && t.length() <= 62 && t.indexOf(':') > 0;
    for (unsigned int i = 0; i < t.length(); i++) {
      char c = t[i];
      if (!(isalnum((uint8_t)c) || c == ':' || c == '_' || c == '-')) ok = false;
    }
    if (!ok) { webOk("✗ token thik na, abar copy koro"); return; }
    strncpy(tgToken, t.c_str(), 63); tgToken[63] = 0;
    tgSaveCfg();
    tgRestart();
    if (!tgOwner) tgStartPair();
    webOk(tgOwner ? "✓ token save holo" : "✓ save! ekhon Telegram e bot ke /start pathao");
  } else if (u == F("/api/tg/pair")) {
    if (!tgToken[0]) { webOk("age token dao"); return; }
    tgStartPair();
    webOk("Telegram e bot ke /start pathao (10 min)");
  } else if (u == F("/api/tg/forget")) {
    tgToken[0] = 0; tgOwner = 0; tgOwnerName[0] = 0; tgPairUntil = 0;
    tgSaveCfg();
    tgRestart();
    webOk("Telegram muche gelo");
  } else if (u == F("/api/game")) { // 🎮 phone theke khela
    String g = web.arg("g");
    if (g == "8ball")      webOk(g8Answer(gameRemote8(-1)));
    else if (g == "dice")  webOk(String(F("chhokka: ")) + gameRemoteDice());
    else if (g == "rock" || g == "paper" || g == "scissors")
      webOk(gameRemoteRps(g == "rock" ? 0 : g == "paper" ? 1 : 2));
    else if (g == "off")   { if (gameMode) gameExit(); webOk("khela bondho"); }
    else if (g == "testrec" || g == "testcatch") webOk(gameTestRecord(g == "testrec"));   // test: notun record er screen
    else if (web.hasArg("start")) webOk(webGameStart(web.arg("start").toInt()));
    else                   { gameEnter(); webOk("khelar menu"); }
  } else if (u == F("/api/status")) {
    String j = "{\"mood\":\"";
    j += clockMode ? "CLOCK" : MOOD_NAME[mood];
    j += "\",\"time\":\""; j += timeStr();
    j += "\",\"weather\":\""; j += weatherStr("C");
    j += "\",\"lang\":\""; j += langEn ? "EN" : "BN";
    j += "\",\"sound\":\""; j += soundOn ? "sound on" : "mute";
    j += "\",\"tg\":\""; j += jsonEsc(tgState);
    j += "\",\"tgBot\":\""; j += jsonEsc(tgBot);
    j += "\",\"tgOwner\":\""; j += tgOwner ? jsonEsc(tgOwnerName[0] ? String(tgOwnerName) : String("ache")) : String("");
    j += "\",\"tgPair\":"; j += tgPairing() ? (int)((tgPairUntil - millis()) / 60000UL) + 1 : 0;
    j += ",\"heap\":"; j += ESP.getFreeHeap();
    j += webStatusExtra();                                                  // mode, screen, timer
    j += "}";
    web.send(200, "application/json; charset=utf-8", j);
  } else {
    web.send(404, "text/plain", String(F("Not found: ")) + u);
  }
}


void startWeb() {
  web.onNotFound(webDispatch);                   // shob API webDispatch() e
  web.begin();
  MDNS.begin(MDNS_NAME);
  MDNS.addService("http", "tcp", 80);
  otaSetup();                                     // 📡 WiFi upload chalu
  webStarted = true;
  Serial.print(F("Phone e kholo: http://"));
  Serial.print(WiFi.localIP());
  Serial.println(F("  (ba http://petbot.local)"));
}

void wifiLoop() {
  wm.process();
  bool c = WiFi.status() == WL_CONNECTED;
  if (c && !wifiWasConnected) {
    Serial.print(F("WiFi connected: "));
    Serial.println(WiFi.SSID());
  }
  wifiWasConnected = c;
  if (c && !webStarted) startWeb();
  if (webStarted) { web.handleClient(); MDNS.update(); ArduinoOTA.handle(); }
  if (c) { tgLoop(); festTelegram(); }           // ✈️ Telegram (+ bishesh din er shubhechha)
}

// ===========================================================
//  Blink + look + hands + smooth animation
// ===========================================================
void updateBlink() {
  if (!canBlink()) { blinking = false; blinkK = 1; return; }

  if (!blinking && (long)(now - nextBlink) >= 0) { blinking = true; blinkStart = now; }

  if (blinking) {
    float t = (now - blinkStart) / (float)BLINK_MS;
    if (t >= 1.0f) {
      blinking = false;
      blinkK = 1;
      nextBlink = now + (random(100) < 20 ? 120 : random(2000, 6000));
    } else {
      blinkK = fabsf(1.0f - 2.0f * t);
    }
  }
}

void updateLook() {
  float tx = tiltX(), ty = tiltY();
  bool tilted = mpuOk && (fabsf(tx) > 0.2f || fabsf(ty) > 0.2f);
  float oldX = tLookX;
  tRoll = 0;

  switch (mood) {
    case SLEEP: case SAD: case CRY:
      tLookX = 0; tLookY = MAX_LY;
      break;
    case DIZZY:
      tLookX = sinf(now / 120.0f) * 6;
      tLookY = cosf(now / 150.0f) * 2;
      break;
    case SUSPICIOUS:
      tLookX = (((now / 1200) % 2) ? 0.8f : -0.8f) * MAX_LX;
      tLookY = 0;
      break;
    case RAINY:                                       // upore akashe takiye thake
      tLookX = sinf(now / 1500.0f) * 6;
      tLookY = -MAX_LY;
      break;
    case HOT:                                         // klanto, niche takano
      tLookX = 0; tLookY = MAX_LY;
      break;
    case WHISTLE:                                     // ekbar pashe "kauke" dekhe, abar tomar dike
      tLookX = (((now - moodSince) / 900) % 2) ? 0 : MAX_LX * 0.6f;
      tLookY = 2;                                     // ektu niche: upore bhuru r jayga
      break;
    case HUM:                                         // gun-gun er tale matha dole
      tLookX = sinf(now / 450.0f) * 7;
      tLookY = sinf(now / 220.0f) * 1.5f;
      break;
    case STRETCH:                                     // gaa-moda: upore takiye
      tLookX = 0; tLookY = -MAX_LY;
      break;
    case PLEAD:                                       // shoja tomar dike takiye "please?"
      tLookX = 0; tLookY = 0;
      break;
    case ANNOYED:                                     // chokh ghurano (eye-roll)
      tLookX = sinf(now / 600.0f) * MAX_LX * 0.8f;
      tLookY = -MAX_LY;
      break;
    case MUSIC:                                       // gaan er tale matha dolano
      tLookX = sinf(now / 500.0f) * 8;
      tLookY = sinf(now / 150.0f) * 2;
      lookY  = tLookY;
      break;
    case LAUGH: case EXCITED:
      tLookX = 0;
      tLookY = sinf(now / 90.0f) * 2;
      lookY  = tLookY;
      break;
    default:
      if (tilted) {
        tLookX = constrain(tx * 30.0f, -MAX_LX, MAX_LX);
        tLookY = constrain(ty * 15.0f, -MAX_LY, MAX_LY);
        tRoll  = constrain(tx * 10.0f, -4.0f, 4.0f);   // je dike kaat, shei diker chokh ektu niche
        nextSaccade = now + 1500;
      } else if (wasTilted) {
        tLookX = 0; tLookY = 0;
      } else if ((long)(now - nextSaccade) >= 0) {
        if (random(100) < 35) { tLookX = 0; tLookY = 0; }
        else {
          tLookX = random(-(int)MAX_LX, (int)MAX_LX + 1);
          tLookY = random(-(int)MAX_LY, (int)MAX_LY + 1);
        }
        nextSaccade = now + random(1200, 4000);
      }
      break;
  }
  wasTilted = tilted;

  // Manush boro kore chokh ghurale onek somoy polok fele - robot o majhe majhe tai kore
  if (fabsf(tLookX - oldX) > 10 && !blinking && random(100) < 35) nextBlink = now;
}

void springEye(EyeState& e, float tx, float ty, float k, float damp) {
  e.vx = (e.vx + (tx - e.x) * k) * damp;
  e.vy = (e.vy + (ty - e.y) * k) * damp;
  e.x += e.vx;
  e.y += e.vy;
}

void animate() {
  float tOpen = 1.0f, tScale = 1.0f;
  switch (mood) {
    case SAD:       tOpen  = 0.85f; break;
    case CRY:       tOpen  = 0.75f; break;
    case ANGRY:     tOpen  = 0.80f; break;
    case SURPRISED: tScale = 1.20f; break;
    case SCARED:    tScale = 0.70f; break;
    case EXCITED:   tScale = 1.10f; break;
    case PLEAD:     tScale = 1.12f; break;           // boro boro puppy chokh
    case WHISTLE:   tScale = 0.80f; break;           // chokh ektu choto: upore bhuru r jayga
    case LOVE:      tScale = 1.08f; break;           // bhalobasay chokh ektu boro
    case SLEEPY:    tOpen  = 0.35f; break;
    case SLEEP:     tOpen  = 0.06f; break;
    case COLD:      tOpen  = 0.75f; tScale = 0.90f; break;
    case STORMY:    tScale = 0.70f; break;
    default: break;
  }
  float openRate = (mood == SLEEP) ? 0.04f : 0.25f;   // ghumate gele dhire dhire chokh bujhe ashe
  openness += (tOpen  - openness) * openRate;
  scale    += (tScale - scale)    * 0.30f;
  lookX    += (tLookX - lookX)    * 0.30f;
  lookY    += (tLookY - lookY)    * 0.30f;

  // Je dike takay, shei diker chokh puro poth jay, onnota 75% (3D matha ghorano effect)
  float txL = tLookX * (tLookX < 0 ? 1.0f : 0.75f);
  float txR = tLookX * (tLookX > 0 ? 1.0f : 0.75f);
  springEye(eyeL, txL, tLookY - tRoll, 0.22f, 0.70f);   // bam chokh ektu druto
  springEye(eyeR, txR, tLookY + tRoll, 0.17f, 0.72f);   // dan chokh ektu dhire, beshi dol
}

// ===========================================================
//  Drawing helpers
// ===========================================================
void drawHeartC(int cx, int cy, int s, uint16_t color) {
  int r = s / 2;
  display.fillCircle(cx - r, cy - r / 2, r, color);
  display.fillCircle(cx + r, cy - r / 2, r, color);
  display.fillTriangle(cx - 2 * r + 1, cy - r / 2 + 2,
                       cx + 2 * r - 1, cy - r / 2 + 2,
                       cx, cy + s, color);
}

void drawHeart(int cx, int cy, int s) { drawHeartC(cx, cy, s, SSD1306_WHITE); }

// LOVE: gal e lojjar dag "///"
void drawBlush(int x, int y) {
  for (int i = 0; i < 3; i++)
    display.drawLine(x + i * 4 - 4, y + 2, x + i * 4 - 2, y - 1, SSD1306_WHITE);
}

// MUSIC: choto ♪ note
void drawNoteIcon(int x, int y) {
  display.fillCircle(x, y, 2, SSD1306_WHITE);
  display.drawFastVLine(x + 2, y - 7, 7, SSD1306_WHITE);
  display.drawLine(x + 2, y - 7, x + 5, y - 4, SSD1306_WHITE);
}

// MUSIC: dui pash diye ♪ note dulte dulte upore uthe
void drawFloatingNotes() {
  for (int i = 0; i < 3; i++) {
    unsigned long t = (now + i * 1000UL) % 3000UL;
    int y = SCREEN_H + 2 - (int)(t * 44UL / 3000UL);
    int x = ((i % 2) ? 116 : 7) + (int)(sinf((now + i * 600UL) / 280.0f) * 3);
    if (y > BAND_H + 8) drawNoteIcon(x, y);
  }
}

// LOVE: dui pash diye choto heart dulte dulte upore uthe
void drawFloatingHearts() {
  for (int i = 0; i < 3; i++) {
    unsigned long t = (now + i * 1100UL) % 3300UL;
    int y = SCREEN_H + 4 - (int)(t * 52UL / 3300UL);
    int x = ((i % 2) ? 119 : 9) + (int)(sinf((now + i * 700UL) / 300.0f) * 3);
    if (y > BAND_H + 4) drawHeart(x, y, 4 + (i == 1 ? 1 : 0));
  }
}

void drawDrop(int x, int y) {
  display.fillCircle(x, y, 2, SSD1306_WHITE);
  display.fillTriangle(x - 2, y - 1, x + 2, y - 1, x, y - 5, SSD1306_WHITE);
}

void drawChevron(int cx, int cy, int dir) {          // dir +1 = '>', -1 = '<'
  const int hw = 8, hh = 7;
  for (int t = -1; t <= 1; t++) {
    display.drawLine(cx - dir * hw, cy - hh + t, cx + dir * hw, cy + t, SSD1306_WHITE);
    display.drawLine(cx + dir * hw, cy + t, cx - dir * hw, cy + hh + t, SSD1306_WHITE);
  }
}

void drawSpiral(int cx, int cy, float phase) {
  float px = cx, py = cy;
  for (float t = 0; t < 4 * PI; t += 0.25f) {
    float r = t * 0.95f;
    float x = cx + r * cosf(t + phase);
    float y = cy + r * sinf(t + phase);
    display.drawLine((int)px, (int)py, (int)x, (int)y, SSD1306_WHITE);
    display.drawLine((int)px + 1, (int)py, (int)x + 1, (int)y, SSD1306_WHITE);
    px = x; py = y;
  }
}

void drawSparkle(int x, int y, int s) {
  display.drawLine(x - s, y, x + s, y, SSD1306_WHITE);
  display.drawLine(x, y - s, x, y + s, SSD1306_WHITE);
}

// Mukh er jonno: bend > 0 = hashi, bend < 0 = mon kharap, tilt = ekdike baka
void drawCurve(int cx, int cy, int hw, float bend, float tilt) {
  int px = cx - hw;
  int py = (int)roundf(cy - tilt);
  for (int i = -hw + 1; i <= hw; i++) {
    float u = (float)i / hw;
    int y = (int)roundf(cy + bend * (1 - u * u) + tilt * u);
    display.drawLine(px, py, cx + i, y, SSD1306_WHITE);
    display.drawLine(px, py + 1, cx + i, y + 1, SSD1306_WHITE);
    px = cx + i; py = y;
  }
}

void drawWave(int cx, int cy, int hw, float amp, float phase) {
  int px = cx - hw;
  int py = cy + (int)roundf(amp * sinf(-hw * 0.9f + phase));
  for (int i = -hw + 1; i <= hw; i++) {
    int y = cy + (int)roundf(amp * sinf(i * 0.9f + phase));
    display.drawLine(px, py, cx + i, y, SSD1306_WHITE);
    display.drawLine(px, py + 1, cx + i, y + 1, SSD1306_WHITE);
    px = cx + i; py = y;
  }
}

void drawRing(int cx, int cy, int r) {
  display.fillCircle(cx, cy, r, SSD1306_WHITE);
  if (r > 2) display.fillCircle(cx, cy, r - 2, SSD1306_BLACK);
}

void drawMouth() {
  int mx = SCREEN_W / 2 + (int)(lookX * 0.6f);
  int my = MOUTH_Y + (int)lookY;

  switch (mood) {
    case NORMAL:     drawCurve(mx, my, 6, 2, 0); break;
    case HAPPY:      drawCurve(mx, my - 1, 9, 4, 0); break;

    case LAUGH: case EXCITED: {                       // khola hashi (D shape)
      int r = (mood == LAUGH) ? 7 + (int)(sinf(now / 70.0f) * 1.5f) : 7;
      display.fillCircle(mx, my - 3, r, SSD1306_WHITE);
      display.fillRect(mx - r - 1, my - 4 - r, 2 * r + 3, r + 1, SSD1306_BLACK);
      break;
    }

    case LOVE:                                        // cute "ω" mukh
      drawCurve(mx - 3, my, 3, 2, 0);
      drawCurve(mx + 3, my, 3, 2, 0);
      break;
    case WINK:       drawCurve(mx + 2, my, 7, 2, -2); break;     // baka hashi
    case SAD:        drawCurve(mx, my + 2, 7, -3, 0); break;
    case CRY:        drawCurve(mx, my + 2, 8, -4 + sinf(now / 100.0f), 0); break;

    case ANGRY:                                       // cute gal fulano "hmph!" - choto ulta mukh
      drawCurve(mx, my + 1, 4, -2, 0);
      break;

    case SURPRISED:  drawRing(mx, my, 5); break;                 // "O"
    case SCARED:     drawWave(mx, my, 8, 1.5f, now / 40.0f); break;
    case CONFUSED:   drawWave(mx + 3, my, 7, 1.2f, 0); break;

    case SUSPICIOUS:
      display.drawLine(mx - 5, my + 1, mx + 6, my - 1, SSD1306_WHITE);
      display.drawLine(mx - 5, my + 2, mx + 6, my,     SSD1306_WHITE);
      break;

    case BORED:
      display.drawLine(mx - 6, my,     mx + 6, my,     SSD1306_WHITE);
      display.drawLine(mx - 6, my + 1, mx + 6, my + 1, SSD1306_WHITE);
      break;

    case SLEEPY: {                                    // majhe majhe hai tola
      unsigned long ph = now % 6000;
      if (ph < 1500) drawRing(mx, my, 2 + (int)(4 * sinf(PI * ph / 1500.0f)));
      else           drawCurve(mx, my, 4, 0, 0);
      break;
    }

    case SLEEP:                                       // shanto choto hashi "‿"
      drawCurve(mx, my + (int)(sleepBreath() * 1.2f), 3, 1, 0);
      break;

    case DIZZY:      drawWave(mx, my, 8, 2, now / 60.0f); break;

    case COLD: {                                      // thandai dant kapche
      int hh = ((now / 80) % 2) ? 7 : 4;
      display.fillRoundRect(mx - 8, my - hh / 2, 17, hh, 1, SSD1306_WHITE);
      display.drawFastHLine(mx - 8, my, 17, SSD1306_BLACK);
      for (int x = mx - 4; x <= mx + 4; x += 4)
        display.drawFastVLine(x, my - hh / 2, hh, SSD1306_BLACK);
      break;
    }

    case HOT: {                                       // gorome hapacche, jibh ber kora
      int r = 4 + (int)(sinf(now / 120.0f) * 1.5f);
      drawRing(mx, my - 1, r);
      display.fillCircle(mx, my + r - 1, 3, SSD1306_WHITE);
      break;
    }

    case MUSIC:                                       // gaan er tale mukh khole-bondho hoy
      drawRing(mx, my, (sndNotes && (sndIdx % 2)) ? 5 : 3);
      break;

    case SNEEZE: {                                    // 🤧 mukh boro hote hote "ACHOO!"
      unsigned long t = now - moodSince;
      if (t < 1300)      drawRing(mx, my, 2 + (int)(t * 4 / 1300));
      else if (t < 1800) drawRing(mx, my, 7);
      else               drawCurve(mx, my, 5, 2, 0);   // hehe
      break;
    }
    case STRETCH:                                     // 🙆 hai er moto boro "o"
      drawRing(mx, my, 3 + (int)(sinf(PI * min(1.0f, (now - moodSince) / 2200.0f)) * 3));
      break;
    case HUM:                                         // 🎵 gun-gun: note er sathe mukh nore
      drawRing(mx, my, (sndNotes && (sndIdx % 2)) ? 4 : 2);
      break;
    case WHISTLE: {                                   // 😗 thot gol kore siti, ♪ note pashe bhese jay
      drawRing(mx + 2, my + 1, 3);                    // ♪ note gulo chokh akar pore (drawFrame e)
      break;
    }

    case PLEAD:                                       // 🥺 kapa kapa choto ulta mukh
      drawCurve(mx, my + 1, 4, -1.5f + sinf(now / 110.0f) * 0.8f, 0);
      break;

    case ANNOYED:                                     // 😒 baka chapta thot
      display.drawLine(mx - 6, my + 1, mx + 6, my - 2, SSD1306_WHITE);
      display.drawLine(mx - 6, my + 2, mx + 6, my - 1, SSD1306_WHITE);
      break;

    case RAINY:      drawCurve(mx, my + 1, 5, -2, 0); break;
    case STORMY:     drawWave(mx, my, 8, 1.5f, now / 35.0f); break;
    case SUNNY:      drawCurve(mx + 2, my - 1, 9, 3, -2); break;   // cool hashi
    default: break;
  }
}

// side: -1 = bam chokh, +1 = dan chokh
void drawEye(int cx, int cy, float w, float h, int side) {
  int iw = max(2, (int)w);
  int ih = max(2, (int)h);
  int x  = cx - iw / 2;
  int y  = cy - ih / 2;
  // EMO er moto gol-kona chouko; obak / bhoy pele puro gol
  int r  = (mood == SURPRISED || mood == SCARED || mood == STORMY || mood == PLEAD) ? min(iw, ih) / 2 : EYE_R;
  r = min(r, min(iw / 2, ih / 2));

  display.fillRoundRect(x, y, iw, ih, r, SSD1306_WHITE);

  int cut = (int)(ih * 0.45f);

  if (mood == HAPPY || mood == MUSIC || mood == STRETCH || mood == HUM || (mood == WINK && side < 0)) {
    int R = (int)(iw * 0.72f);
    display.fillCircle(cx, y + (int)(ih * 0.55f) + R, R, SSD1306_BLACK);
  }
  else if (mood == SAD || mood == CRY || mood == RAINY) {
    if (side < 0) display.fillTriangle(x - 1, y - 1, x + iw, y - 1, x - 1,  y + cut, SSD1306_BLACK);
    else          display.fillTriangle(x - 1, y - 1, x + iw, y - 1, x + iw, y + cut, SSD1306_BLACK);
  }
  else if (mood == ANGRY) {                            // halka baka bhuru - cute raag
    int c2 = (int)(ih * 0.32f);
    if (side < 0) display.fillTriangle(x - 1, y - 1, x + iw, y - 1, x + iw, y + c2, SSD1306_BLACK);
    else          display.fillTriangle(x - 1, y - 1, x + iw, y - 1, x - 1,  y + c2, SSD1306_BLACK);
  }
  else if (mood == PLEAD && ih > 14) {                 // 🥺 puppy chokh: boro kalo moni upore takiye, shada jhilik
    int pr = iw / 4;
    int px = cx, py = cy + 1;                          // moni majhkhane: shoja tomar dike
    display.fillCircle(px, py, pr, SSD1306_BLACK);
    display.fillCircle(px - side * pr / 3, py - pr / 3, 2, SSD1306_WHITE);   // jhilik dui chokhe ayna-r moto
    display.drawPixel(px + side * pr / 3, py + pr / 3, SSD1306_WHITE);
    int c2 = (int)(ih * 0.18f);                        // halka mon kharap bhuru
    if (side < 0) display.fillTriangle(x - 1, y - 1, x + iw, y - 1, x - 1,  y + c2, SSD1306_BLACK);
    else          display.fillTriangle(x - 1, y - 1, x + iw, y - 1, x + iw, y + c2, SSD1306_BLACK);
  }
  else if (mood == WHISTLE && side > 0) {              // 😉 dan chokhe wink (^)
    int R = (int)(iw * 0.72f);
    display.fillCircle(cx, y + (int)(ih * 0.55f) + R, R, SSD1306_BLACK);
  }
  else if (mood == ANNOYED) {                          // 😒 adh-bondho chapta chokh
    display.fillRect(x - 1, y - 1, iw + 2, (int)(ih * 0.5f) + 1, SSD1306_BLACK);
  }
  else if (mood == BORED || mood == SUSPICIOUS || mood == HOT) {
    float lid = (mood == BORED) ? 0.5f : (mood == HOT) ? 0.45f : (side > 0 ? 0.55f : 0.40f);
    display.fillRect(x - 1, y - 1, iw + 2, (int)(ih * lid) + 1, SSD1306_BLACK);
  }
  else if (mood == LOVE && ih > 14) {                  // chokher moni = dhuk-dhuk kora kalo heart
    int s = (iw < 30 ? 6 : 8) + (int)((now / 300) % 2);
    int hy = cy - 2;
    drawHeartC(cx, hy, s, SSD1306_BLACK);
    int r = s / 2;
    display.fillRect(cx - r - 1, hy - r / 2 - 2, 2, 2, SSD1306_WHITE);   // heart er upor jhilik
  }
  else if (mood == EXCITED && ih > 12) {               // 🤩 tara-r moto chakchake chokh, shoja tomar dike
    int pr = iw / 5;
    display.fillCircle(cx, cy, pr, SSD1306_BLACK);
    int s = ((now / 150) % 2) ? 2 : 1;                 // jhilik jhikmik kore
    display.drawLine(cx - side * 2 - s, cy - 2, cx - side * 2 + s, cy - 2, SSD1306_WHITE);
    display.drawLine(cx - side * 2, cy - 2 - s, cx - side * 2, cy - 2 + s, SSD1306_WHITE);
  }

  // EMO er moto puro bhora chokh - vitore kono moni nei
}

// Raag er 💢 chinho (pulse kore)
void drawAngerMark(int x, int y) {
  int k = ((now / 250) % 2) ? 4 : 3;
  display.drawLine(x - k, y - 1, x - 1, y - 1, SSD1306_WHITE); display.drawLine(x - 1, y - 1, x - 1, y - k, SSD1306_WHITE);
  display.drawLine(x + 1, y - k, x + 1, y - 1, SSD1306_WHITE); display.drawLine(x + 1, y - 1, x + k, y - 1, SSD1306_WHITE);
  display.drawLine(x - k, y + 1, x - 1, y + 1, SSD1306_WHITE); display.drawLine(x - 1, y + 1, x - 1, y + k, SSD1306_WHITE);
  display.drawLine(x + 1, y + 1, x + k, y + 1, SSD1306_WHITE); display.drawLine(x + 1, y + 1, x + 1, y + k, SSD1306_WHITE);
}

// Raage matha theke dhoya beroy (dui pashe choto gol puff upore uthe)
void drawSteam(int cy) {
  for (int side = -1; side <= 1; side += 2) {
    for (int i = 0; i < 2; i++) {
      unsigned long t = (now + i * 450UL) % 900UL;
      int r = 1 + (int)(t * 3UL / 900UL);
      int x = (side < 0 ? 7 : 120) + side * (int)(t * 4UL / 900UL);
      int y = cy + 4 - (int)(t * 16UL / 900UL);
      if (y - r > BAND_H) display.drawCircle(x, y, r, SSD1306_WHITE);
    }
  }
}

void drawTears(int lx, int rx, int cy, float w, float h) {
  for (int side = -1; side <= 1; side += 2) {
    int ex    = (side < 0 ? lx : rx) - side * (int)(w / 2 - 4);   // vitorer kona theke pani
    int start = cy + (int)(h / 2) + 3;
    int fall  = (int)((now / 45 + (side > 0 ? 7 : 0)) % 14);
    drawDrop(ex, start + fall);
  }
}

// Holud band er lekha (smooth FreeSansBold 9pt font)
void drawLabel(const char* s, int xoff) {
  if (strlen(s) == 0) return;
  int w = advanceOf(FreeSansBold9pt7bGlyphs, s);
  display.setTextColor(SSD1306_WHITE);
  printFont(&FreeSansBold9pt7b, (SCREEN_W - w) / 2 + xoff, 13, s);
}

// ---------- Sleep band: Time / Day / Date / Temp / Zzz ----------
enum SleepPage { PG_TIME, PG_DAY, PG_DATE, PG_TEMP, PG_ZZZ, PG_WEATHER, PG_NUMDATE, PG_MONTH, PG_SPECIAL };

const char* weatherText(int c) {
  if (c == 0)               return "CLEAR";
  if (c <= 2)               return "FAIR";
  if (c == 3)               return "CLOUDY";
  if (c == 45 || c == 48)   return "FOG";
  if (c >= 51 && c <= 57)   return "DRIZZLE";
  if (c >= 61 && c <= 67)   return "RAIN";
  if (c >= 71 && c <= 77)   return "SNOW";
  if (c >= 80 && c <= 82)   return "SHOWER";
  if (c >= 85 && c <= 86)   return "SNOW";
  if (c >= 95)              return "STORM";
  return "";
}

// s = scale (1 = choto icon, 2 = boro icon)
void drawCloud(int cx, int cy, int s) {
  display.fillCircle(cx - 4 * s, cy + s, 3 * s, SSD1306_WHITE);
  display.fillCircle(cx,         cy - s, 4 * s, SSD1306_WHITE);
  display.fillCircle(cx + 4 * s, cy + s, 3 * s, SSD1306_WHITE);
  display.fillRect(cx - 4 * s, cy + s, 8 * s + 1, 3 * s + 1, SSD1306_WHITE);
}

void drawSun(int cx, int cy, int r, int s) {
  display.fillCircle(cx, cy, r, SSD1306_WHITE);
  float spin = (s > 1) ? now / 2000.0f : 0;          // boro shurjo aste ghore
  for (int i = 0; i < 8; i++) {
    float a = i * PI / 4 + spin;
    display.drawLine(cx + cosf(a) * (r + 2 * s), cy + sinf(a) * (r + 2 * s),
                     cx + cosf(a) * (r + 4 * s), cy + sinf(a) * (r + 4 * s), SSD1306_WHITE);
  }
}

void drawWeatherIcon(int cx, int cy, int c, int s) {
  if (c == 0) {
    drawSun(cx, cy, 3 * s, s);
  } else if (c <= 2) {
    drawSun(cx - 2 * s, cy - 2 * s, 2 * s, s);
    drawCloud(cx + s, cy + 2 * s, s);
  } else if (c == 45 || c == 48) {
    for (int i = -1; i <= 1; i++)
      display.fillRect(cx - 6 * s, cy + i * 4 * s, 12 * s + 1, s, SSD1306_WHITE);
  } else if (c == 3) {
    drawCloud(cx, cy, s);
  } else {
    drawCloud(cx, cy - 3 * s, s);
    if (c >= 95) {                                   // bajj
      for (int k = 0; k < s; k++) {
        display.drawLine(cx + s + k, cy + 2 * s, cx - 2 * s + k, cy + 5 * s, SSD1306_WHITE);
        display.drawLine(cx - 2 * s + k, cy + 5 * s, cx + s + k, cy + 5 * s, SSD1306_WHITE);
        display.drawLine(cx + s + k, cy + 5 * s, cx - 2 * s + k, cy + 8 * s, SSD1306_WHITE);
      }
    } else if ((c >= 71 && c <= 77) || c == 85 || c == 86) {   // borof
      for (int i = -1; i <= 1; i++)
        display.fillRect(cx + i * 4 * s, cy + 4 * s + (i & 1) * 2 * s, 2 * s, 2 * s, SSD1306_WHITE);
    } else {                                          // brishti, pani pore
      int d = ((now / 120) % 3) * s;
      for (int i = -1; i <= 1; i++)
        for (int k = 0; k < s; k++)
          display.drawLine(cx + i * 4 * s + k, cy + 3 * s + d, cx + i * 4 * s - s + k, cy + 5 * s + d, SSD1306_WHITE);
    }
  }
}

void drawSleepPage(int page, int xoff) {
  time_t t = time(nullptr);
  struct tm* tm = localtime(&t);
  char buf[16];

  switch (page) {
    case PG_TIME: {
      int h = use24 ? tm->tm_hour : tm->tm_hour % 12;
      if (!use24 && h == 0) h = 12;
      bool colon = (t % 2) == 0;                       // colon blink kore
      snprintf(buf, sizeof(buf), colon ? (use24 ? "%02d:%02d" : "%d:%02d") : (use24 ? "%02d %02d" : "%d %02d"), h, tm->tm_min);
      int w = advanceOf(FreeSansBold9pt7bGlyphs, buf);
      int x = (SCREEN_W - (use24 ? w : w + 15)) / 2 + xoff;
      display.setTextColor(SSD1306_WHITE);
      printFont(&FreeSansBold9pt7b, x, 13, buf);
      if (!use24) {
        display.setTextSize(1);
        display.setCursor(x + w + 3, 6);
        display.print(tm->tm_hour < 12 ? "AM" : "PM");
      }
      break;
    }
    case PG_DAY:
      drawLabel(DAY_NAME[tm->tm_wday], xoff);
      break;
    case PG_DATE:
      snprintf(buf, sizeof(buf), "%d %s %02d", tm->tm_mday, MON_NAME[tm->tm_mon], tm->tm_year % 100);
      drawLabel(buf, xoff);
      break;
    case PG_TEMP: {
      snprintf(buf, sizeof(buf), "%.1f", imu.temp);
      int w  = advanceOf(FreeSansBold9pt7bGlyphs, buf);
      int cw = advanceOf(FreeSansBold9pt7bGlyphs, "C");
      int x  = (SCREEN_W - (w + 6 + cw)) / 2 + xoff;
      printFont(&FreeSansBold9pt7b, x, 13, buf);
      display.drawCircle(x + w + 2, 3, 2, SSD1306_WHITE);       // degree chinho
      printFont(&FreeSansBold9pt7b, x + w + 6, 13, "C");
      break;
    }
    case PG_WEATHER: {                                 // icon + 24°C + CLOUDY
      int tC = (int)roundf(weatherTemp);
      snprintf(buf, sizeof(buf), "%d", tC);
      const char* desc = weatherText(weatherCode);
      int tw = advanceOf(FreeSansBold9pt7bGlyphs, buf);
      int cw = advanceOf(FreeSansBold9pt7bGlyphs, "C");
      int dw = strlen(desc) * 6;
      int total = 16 + 4 + tw + 6 + cw + 5 + dw;
      int x = (SCREEN_W - total) / 2 + xoff;
      drawWeatherIcon(x + 8, 8, weatherCode, 1);
      x += 20;
      display.setTextColor(SSD1306_WHITE);
      printFont(&FreeSansBold9pt7b, x, 13, buf);
      x += tw;
      display.drawCircle(x + 2, 3, 2, SSD1306_WHITE);  // degree chinho
      x += 6;
      printFont(&FreeSansBold9pt7b, x, 13, "C");
      x += cw + 5;
      display.setTextSize(1);
      display.setCursor(x, 6);
      display.print(desc);
      break;
    }
    case PG_NUMDATE:
      snprintf(buf, sizeof(buf), "%02d/%02d/%04d", tm->tm_mday, tm->tm_mon + 1, tm->tm_year + 1900);
      drawLabel(buf, xoff);
      break;
    case PG_MONTH:
      drawLabel(MON_FULL[tm->tm_mon], xoff);
      break;
    case PG_SPECIAL: {                                 // aj bishesh din (Bangla chhobi / English)
      int sp = todaySpecial();
      if (sp < 0) break;
      const BnText& b = specialBn(sp);
      if (langEn) drawLabel(enStr(b), xoff);
      else display.drawBitmap((SCREEN_W - b.w) / 2 + xoff, (BAND_H - b.h) / 2, b.bmp, b.w, b.h, SSD1306_WHITE);
      break;
    }
    default:
      drawLabel("Z z Z", xoff);
      break;
  }
}

// Ghumer shomoy holud ongshe time/din/tarikh (Zzz ekhon neel ongshe)
void drawSleepBand() {
  if (!timeValid()) {                                   // time na thakle: chand ar mitimiti tara
    display.fillCircle(64, 8, 6, SSD1306_WHITE);
    display.fillCircle(67, 6, 5, SSD1306_BLACK);        // ordhek chand
    for (int i = 0; i < 5; i++) {
      int sx = 12 + i * 26 + (i > 1 ? 14 : 0);
      if (((now / 400) + i) % 3) display.drawPixel(sx, 4 + (i * 5) % 9, SSD1306_WHITE);
      else drawSparkle(sx, 4 + (i * 5) % 9, 2);
    }
    return;
  }

  int pages[5];
  int n = 0;
  if (todaySpecial() >= 0) pages[n++] = PG_SPECIAL;
  pages[n++] = PG_TIME;
  pages[n++] = PG_DAY;
  pages[n++] = PG_DATE;
  if (weatherOk)  pages[n++] = PG_WEATHER;
  else if (mpuOk) pages[n++] = PG_TEMP;

  drawPagesSliding(pages, n);
}

// ---------- Ghumer animation (neel ongshe) ----------
float sleepBreath() { return sinf(now / 600.0f); }      // ~3.8 second e ekta shanto nishash

// Shanto bondho chokh: 3px mota norom "◡" rekha
void drawSleepEye(int cx, int cy, int hw) {
  for (int t = 0; t < 2; t++) drawCurve(cx, cy + t, hw, 3, 0);   // drawCurve nijei 2px, mot 3px
}

// Rekha diye aka mosrin "Z" (s = ordhek size)
void drawZ(int x, int y, int s) {
  for (int t = 0; t < 2; t++) {
    display.drawLine(x - s, y - s + t, x + s, y - s + t, SSD1306_WHITE);     // upor
    display.drawLine(x + s - t, y - s, x - s - t, y + s, SSD1306_WHITE);     // kona-kuni
    display.drawLine(x - s, y + s - t, x + s, y + s - t, SSD1306_WHITE);     // nich
  }
}

// 3 ta Z: chokher dan pash theke choto theke boro hoye dulte dulte upore uthe
void drawSleepZ(int startX, int startY) {
  const unsigned long LIFE = 3000;
  for (int i = 0; i < 3; i++) {
    unsigned long t = (now - moodSince + i * (LIFE / 3)) % LIFE;
    float p = t / (float)LIFE;
    int s = 2 + (int)(p * 4);                           // 2 -> 6
    int x = startX + (int)(p * 12) + (int)(sinf(p * 7.0f + i) * 2);
    int y = startY - (int)(p * 22);
    if (y - s > BAND_H + 1) drawZ(x, y, s);
  }
}

// Holud band e page gulo ekta por ekta dan dik theke slide kore ashe
void drawPagesSliding(const int* pages, int n) {
  int idx = (int)((now / PAGE_MS) % n);
  unsigned long ph = now % PAGE_MS;

  if (ph < SLIDE_MS && n > 1) {
    float k = ph / (float)SLIDE_MS;
    k = 1 - (1 - k) * (1 - k);
    int prev = (idx + n - 1) % n;
    drawSleepPage(pages[prev], -(int)(SCREEN_W * k));
    drawSleepPage(pages[idx],  (int)(SCREEN_W * (1 - k)));
  } else {
    drawSleepPage(pages[idx], 0);
  }
}

// ---------- CLOCK mode ----------
//  5 tap = clock on/off.  Clock e 1 tap = porer screen.
//  HOME : holud e weather/day/date/month slide, neel e boro time
//  Baki : holud e naam (DATE / DAY / WEATHER / ROOM TEMP / TIMER), neel e details

// Font e lekhar chowra (pixel)
int advanceOf(const GFXglyph* g, const char* s) {
  int w = 0;
  for (; *s; s++) {
    uint8_t c = (uint8_t)*s;
    if (c >= 0x20 && c <= 0x7E) w += pgm_read_byte(&g[c - 0x20].xAdvance);
  }
  return w;
}

void printFont(const GFXfont* f, int x, int baseY, const char* s) {
  display.setFont(f);
  display.setTextSize(1);
  display.setCursor(x, baseY);
  display.print(s);
  display.setFont();                                    // abar normal font
}

void nextClockView() {
  clockView = (clockView + 1) % CV_COUNT;
  viewChangedAt = now;
  PLAY(S_TICK);
  Serial.print(F("Clock view: "));
  Serial.println(CV_TITLE[clockView]);
}

// BnText er English lekha flash e thake -> RAM e copy kore dey (3 ta ghurano buffer)
const char* enStr(const BnText& b) {
  static char buf[3][48];
  static uint8_t k = 0;
  if (!b.enP) return nullptr;
  k = (k + 1) % 3;
  strncpy_P(buf[k], b.enP, 47);
  buf[k][47] = 0;
  return buf[k];
}
// Bangla lekha (chhobi) ke 18px uchu line er majhe, majkhane boshay
void drawBn(const BnText& t, int yTop, int xoff) {
  if (langEn && enStr(t)) {                                // English mode: oi lekhar English
    display.setTextColor(SSD1306_WHITE);
    int w = advanceOf(FreeSansBold9pt7bGlyphs, enStr(t));
    if (w <= SCREEN_W - 2) {
      printFont(&FreeSansBold9pt7b, (SCREEN_W - w) / 2 + xoff, yTop + 14, enStr(t));
    } else {
      display.setTextSize(1);
      display.setCursor((SCREEN_W - (int)strlen(enStr(t)) * 6) / 2 + xoff, yTop + 5);
      display.print(enStr(t));
    }
    return;
  }
  int y = yTop + (18 - (int)t.h) / 2;
  display.drawBitmap((SCREEN_W - t.w) / 2 + xoff, y, t.bmp, t.w, t.h, SSD1306_WHITE);
}

// Neel ongshe 1 ba 2 line Bangla message
void drawSmallMsg(const BnText* a, const BnText* b, int xoff) {
  if (a && b) { drawBn(*a, 18, xoff); drawBn(*b, 38, xoff); }
  else if (a) { drawBn(*a, 28, xoff); }
}

// HOME: holud e Weather -> Day -> Date -> Month slide, neel e boro time
void drawHome(struct tm* tm, time_t t, int xoff) {
  int pages[5];
  int n = 0;
  if (todaySpecial() >= 0) pages[n++] = PG_SPECIAL;
  if (weatherOk) pages[n++] = PG_WEATHER;
  pages[n++] = PG_DAY;
  pages[n++] = PG_NUMDATE;
  pages[n++] = PG_MONTH;
  if ((long)(millis() - fmtShowUntil) < 0) drawLabel(use24 ? "24 HOUR" : "12 HOUR", xoff);   // format bodlalo
  else drawPagesSliding(pages, n);

  int h = use24 ? tm->tm_hour : tm->tm_hour % 12;
  if (!use24 && h == 0) h = 12;
  char hh[3], mm[3];
  snprintf(hh, sizeof(hh), use24 ? "%02d" : "%d", h);
  snprintf(mm, sizeof(mm), "%02d", tm->tm_min);

  const GFXglyph* g = FreeSansBold24pt7bGlyphs;
  int aH = advanceOf(g, hh), aC = advanceOf(g, ":"), aM = advanceOf(g, mm);
  int x  = (SCREEN_W - (aH + aC + aM)) / 2 + xoff;
  const int baseY = 57;

  printFont(&FreeSansBold24pt7b, x, baseY, hh);
  if (t % 2 == 0) printFont(&FreeSansBold24pt7b, x + aH, baseY, ":");   // colon blink
  printFont(&FreeSansBold24pt7b, x + aH + aC, baseY, mm);
}

// ---------- Timer ----------
unsigned long timerRemaining() {
  if (!timerRunning) return timerLeftMs;
  long left = (long)(timerEndAt - now);
  return left > 0 ? (unsigned long)left : 0;
}

void timerAddMinute() {
  if (timerRemaining() + 60000UL > 99UL * 60000UL) return;
  if (timerRunning) timerEndAt += 60000UL;
  else              timerLeftMs += 60000UL;
  Serial.printf_P(PSTR("Timer: %lu min\n"), (timerRemaining() + 999) / 60000);
  PLAY(S_BEEP);
}

void timerToggle() {
  if (timerRunning) {
    timerLeftMs = timerRemaining();
    timerRunning = false;
    Serial.println(F("Timer: pause"));
    PLAY(S_PAUSE);
  } else if (timerLeftMs > 0) {
    timerEndAt = now + timerLeftMs;
    timerRunning = true;
    Serial.println(F("Timer: start"));
    PLAY(S_START);
  }
}

void timerReset() {
  timerRunning = false;
  timerLeftMs = 0;
  Serial.println(F("Timer: reset"));
  PLAY(S_RESET);
}

void updateTimer() {
  if (timerRunning && (long)(now - timerEndAt) >= 0) {
    timerRunning = false;
    timerLeftMs = 0;
    timerAlarm = true;
    alarmStart = now;
    if (!clockMode) setClockMode(true);           // je kono mode theke timer screen e chole ashe
    clockView = CV_TIMER;
    viewChangedAt = now;
    Serial.println(F("Timer shesh! TIME UP"));
    tgNotify(F("⏰ Timer shesh! TIME UP ⏱️"));
  }
  if (timerAlarm && now - alarmStart > 60000UL) timerAlarm = false;   // 1 min por nijei thambe
  if (timerAlarm && !sndNotes) PLAY(S_ALARM);                         // alarm bajte thake
}

void drawViewTimer(int xoff) {
  unsigned long left = timerRemaining();

  // Holud band er dane: play / pause chinho
  if (timerRunning) {
    display.fillTriangle(114 + xoff, 3, 114 + xoff, 12, 122 + xoff, 7, SSD1306_WHITE);
  } else if (left > 0) {
    display.fillRect(115 + xoff, 3, 2, 10, SSD1306_WHITE);
    display.fillRect(120 + xoff, 3, 2, 10, SSD1306_WHITE);
  }

  if (!timerRunning && left == 0) {               // kivabe use korbe
    display.setTextSize(1);
    display.setCursor(4 + xoff, 20); display.print("2 tap : +1 min");
    display.setCursor(4 + xoff, 30); display.print("Hold  : start/pause");
    display.setCursor(4 + xoff, 40); display.print("3 tap : reset");
    display.setCursor(4 + xoff, 50); display.print("1 tap : porer screen");
    return;
  }

  if (!timerRunning && (now / 500) % 2) return;   // pause thakle digit blink kore

  unsigned long sec = (left + 999) / 1000;
  char buf[6];
  snprintf(buf, sizeof(buf), "%02lu:%02lu", sec / 60, sec % 60);
  int w = advanceOf(FreeSansBold24pt7bGlyphs, buf);
  printFont(&FreeSansBold24pt7b, (SCREEN_W - w) / 2 + xoff, 57, buf);
}

void drawTimerAlarm() {
  drawLabel("TIMER", 0);
  bool on = (now / 300) % 2;
  if (on) display.fillRect(0, BAND_H, SCREEN_W, SCREEN_H - BAND_H, SSD1306_WHITE);
  const char* s = "TIME UP!";
  int w = advanceOf(FreeSansBold12pt7bGlyphs, s);
  display.setTextColor(on ? SSD1306_BLACK : SSD1306_WHITE);
  printFont(&FreeSansBold12pt7b, (SCREEN_W - w) / 2, 48, s);
  display.setTextColor(SSD1306_WHITE);
}

void drawViewDate(struct tm* tm, int xoff) {           // [19]  NOV
  char d[3], y[5];                                      //       2026
  snprintf(d, sizeof(d), "%d", tm->tm_mday);
  snprintf(y, sizeof(y), "%d", tm->tm_year + 1900);
  int aD = advanceOf(FreeSansBold24pt7bGlyphs, d);
  int aMo = advanceOf(FreeSansBold12pt7bGlyphs, MON_NAME[tm->tm_mon]);
  int aY  = advanceOf(FreeSansBold12pt7bGlyphs, y);
  int total = aD + 8 + max(aMo, aY);
  int x = (SCREEN_W - total) / 2 + xoff;

  printFont(&FreeSansBold24pt7b, x, 57, d);
  printFont(&FreeSansBold12pt7b, x + aD + 8, 36, MON_NAME[tm->tm_mon]);
  printFont(&FreeSansBold12pt7b, x + aD + 8, 58, y);
}

void drawViewDay(struct tm* tm, int xoff) {            // Wednesday + S M T W T F S
  const char* name = DAY_TITLE[tm->tm_wday];
  int w = advanceOf(FreeSansBold12pt7bGlyphs, name);
  printFont(&FreeSansBold12pt7b, (SCREEN_W - w) / 2 + xoff, 37, name);

  const char* L = "SMTWTFS";
  const int cw = 16;
  int sx = (SCREEN_W - 7 * cw) / 2 + xoff;
  display.setTextSize(1);
  for (int i = 0; i < 7; i++) {
    int x = sx + i * cw;
    if (i == tm->tm_wday) {                             // aajker din highlight
      display.fillRoundRect(x + 1, 46, 14, 12, 3, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(x + 6, 48);
    display.print(L[i]);
  }
  display.setTextColor(SSD1306_WHITE);
}

void drawViewWeather(int xoff) {                       // [icon]  18°C
  if (!weatherOk) { drawSmallMsg(&BN_WEATHER_NEWS, &BN_NOT_YET, xoff); return; }

  drawWeatherIcon(28 + xoff, 38, weatherCode, 2);

  char buf[6];
  snprintf(buf, sizeof(buf), "%d", (int)roundf(weatherTemp));
  int x = 58 + xoff;
  int aT = advanceOf(FreeSansBold18pt7bGlyphs, buf);
  printFont(&FreeSansBold18pt7b, x, 45, buf);
  display.drawCircle(x + aT + 3, 23, 3, SSD1306_WHITE);   // degree
  printFont(&FreeSansBold12pt7b, x + aT + 9, 40, "C");
  display.setTextSize(1);
  display.setCursor(x, 52);
  display.print(weatherText(weatherCode));
}

void drawViewRoom(int xoff) {                          // ghorer temperature (MPU6050)
  if (!mpuOk) { drawSmallMsg(&BN_NO_SENSOR, nullptr, xoff); return; }

  char buf[8];
  snprintf(buf, sizeof(buf), "%.1f", imu.temp);
  int aT = advanceOf(FreeSansBold18pt7bGlyphs, buf);
  int x = (SCREEN_W - (aT + 20)) / 2 + xoff;
  printFont(&FreeSansBold18pt7b, x, 50, buf);
  display.drawCircle(x + aT + 3, 28, 3, SSD1306_WHITE);
  printFont(&FreeSansBold12pt7b, x + aT + 9, 45, "C");
}

// ===========================================================
//  📅 Bangla tarikh (বঙ্গাব্দ, Bangladesh er 2019 songshodhito panjika) + Hijri + bishesh din
// ===========================================================
static const BnText* const BD_BIG[10] PROGMEM = { &BD_B0, &BD_B1, &BD_B2, &BD_B3, &BD_B4, &BD_B5, &BD_B6, &BD_B7, &BD_B8, &BD_B9 };
static const BnText* const BD_SML[10] PROGMEM = { &BD_S0, &BD_S1, &BD_S2, &BD_S3, &BD_S4, &BD_S5, &BD_S6, &BD_S7, &BD_S8, &BD_S9 };
static const BnText* const BN_BMON[12] PROGMEM = { &BN_BM1, &BN_BM2, &BN_BM3, &BN_BM4, &BN_BM5, &BN_BM6,
                                           &BN_BM7, &BN_BM8, &BN_BM9, &BN_BM10, &BN_BM11, &BN_BM12 };
static const BnText* const BN_HMON[12] PROGMEM = { &BN_HM1, &BN_HM2, &BN_HM3, &BN_HM4, &BN_HM5, &BN_HM6,
                                           &BN_HM7, &BN_HM8, &BN_HM9, &BN_HM10, &BN_HM11, &BN_HM12 };
static const BnText* const BN_SEASON[6] PROGMEM = { &BN_SEA1, &BN_SEA2, &BN_SEA3, &BN_SEA4, &BN_SEA5, &BN_SEA6 };

// Telegram er jonno Unicode Bangla naam ('|' diye alada, flash e)
static const char BM_UTF[] PROGMEM = "বৈশাখ|জ্যৈষ্ঠ|আষাঢ়|শ্রাবণ|ভাদ্র|আশ্বিন|কার্তিক|অগ্রহায়ণ|পৌষ|মাঘ|ফাল্গুন|চৈত্র";
static const char HM_UTF[] PROGMEM = "মহররম|সফর|রবিউল আউয়াল|রবিউস সানি|জমাদিউল আউয়াল|জমাদিউস সানি|রজব|শাবান|রমজান|শাওয়াল|জিলকদ|জিলহজ";
static const char SEA_UTF[] PROGMEM = "গ্রীষ্মকাল|বর্ষাকাল|শরৎকাল|হেমন্তকাল|শীতকাল|বসন্তকাল";

// Hijri: hisab (tabular) + Bangladesh er chand dekha onujayi shamanyo shoranor din
// 2026 e BD te Eid-ul-Fitr 21 Mar, Eid-ul-Adha 28 May -> -1 din milche. Telegram e /hijri 1 ba /hijri -1 diye thik kora jay.

void loadHijriAdj() {
  if (EEPROM.read(EE_HIJRI_MAGIC) == 0xE5) hijriAdj = (int8_t)EEPROM.read(EE_HIJRI_ADJ);
}
void saveHijriAdj() {
  EEPROM.write(EE_HIJRI_ADJ, (uint8_t)hijriAdj);
  EEPROM.write(EE_HIJRI_MAGIC, 0xE5);
  EEPROM.commit();
}

long jdnOf(int y, int m, int d) {                      // Gregorian -> Julian Day Number
  int a = (14 - m) / 12;
  long yy = y + 4800 - a;
  int mm = m + 12 * a - 3;
  return d + (153 * mm + 2) / 5 + 365 * yy + yy / 4 - yy / 100 + yy / 400 - 32045;
}

bool gLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

// 1 Boishakh = 14 April. Boishakh-Bhadro 31 din, baki 30, Falgun 29 (leap year e 30)
void banglaDate(const struct tm* tm, int& d, int& m, int& y) {
  int gy = tm->tm_year + 1900, gm = tm->tm_mon + 1, gd = tm->tm_mday;
  int y0 = (gm > 4 || (gm == 4 && gd >= 14)) ? gy : gy - 1;
  y = y0 - 593;
  long days = jdnOf(gy, gm, gd) - jdnOf(y0, 4, 14);
  static const uint8_t L[12] = { 31, 31, 31, 31, 31, 30, 30, 30, 30, 30, 29, 30 };
  for (m = 0; m < 11; m++) {
    int len = L[m] + ((m == 10 && gLeap(y0 + 1)) ? 1 : 0);
    if (days < len) break;
    days -= len;
  }
  d = days + 1;
}

long hijriToJd(int y, int m, int d) {
  return d + (59L * (m - 1) + 1) / 2 + (y - 1) * 354L + (3 + 11L * y) / 30 + 1948440L - 1;
}

void hijriDate(const struct tm* tm, int& d, int& m, int& y) {   // m: 1..12
  long jd = jdnOf(tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday) + hijriAdj;
  y = (int)((30L * (jd - 1948440L) + 10646L) / 10631L);
  m = (int)ceilf((jd - (29 + hijriToJd(y, 1, 1))) / 29.5f) + 1;
  if (m < 1) m = 1;
  if (m > 12) m = 12;
  d = (int)(jd - hijriToJd(y, m, 1)) + 1;
}

// ---------- Bishesh din ----------
#define FS_NONE     0
#define FS_FLAG     1                   // 🇧🇩 potaka
#define FS_MINAR    2                   // শহীদ মিনার
#define FS_MOON     3                   // 🌙 chand-tara
#define FS_FIREWORK 4                   // 🎆 atoshbaji
#define FS_FLOWER   5                   // 🌼 phul
#define FS_MOSQUE   6                   // 🕌 moshjid (namaz / sehri / iftar)
#define FS_CAKE     7                   // 🎂 jonmodin er cake
#define FS_SUNRISE  8                   // 🌅 alarm: shurjo uthche
#define FS_BELL     9                   // 🔔 alarm: ghonta dulche
#define FS_BATTERY 10                   // 🪫 charge kom

static const char TGS_NY[]   PROGMEM = "🎆 Happy New Year! Notun bochor onek bhalo katuk 🎉";
static const char TGS_FAL[]  PROGMEM = "🌼 Pohela Falgun! Boshonto eshe geche 🌸";
static const char TGS_EK[]   PROGMEM = "🕊️ Amar Ekushe: Shohid Dibosh o Antorjatik Matribhasha Dibosh. Bhasha shohidder proti gobhir shroddha.";
static const char TGS_IND[]  PROGMEM = "🇧🇩 Aj Mohan Swadhinota Dibosh! Shob shohidder proti shroddha.";
static const char TGS_BOI[]  PROGMEM = "🎉 Shubho Noboborsho! Pohela Boishakh er onek shubhechha 🌞";
static const char TGS_VIC[]  PROGMEM = "🇧🇩 Aj Mohan Bijoy Dibosh! Bijoyer shubhechha 🎉";
static const char TGS_RAM[]  PROGMEM = "🌙 Ramadan Mubarak! Rojar mash shuru holo.";
static const char TGS_EIDF[] PROGMEM = "🌙 Eid Mubarak! ✨ Eid-ul-Fitr er onek onek shubhechha!";
static const char TGS_EIDA[] PROGMEM = "🌙 Eid Mubarak! Eid-ul-Adha er shubhechha! 🐄";
static const char TGS_BDAY[] PROGMEM = "🎂🎉 Shubho Jonmodin " OWNER_NAME "! 🎈 Onek onek bhalobasha ar shubhechha. Aj tomar din, khub moja koro! 🥳";

struct SpecialDay { bool hijri; uint8_t m, d; const BnText* bn; uint8_t scene; Mood mood; const char* tg; const char* nm; };
static const SpecialDay SPDAYS[] = {
  { false, 1,  1,  &BN_SP_NEWYEAR,  FS_FIREWORK, EXCITED, TGS_NY,   "English New Year" },
  { false, 2,  13, &BN_SP_FALGUN,   FS_FLOWER,   LOVE,    TGS_FAL,  "Pohela Falgun" },
  { false, 2,  21, &BN_SP_EKUSHE,   FS_MINAR,    SAD,     TGS_EK,   "Amar Ekushe (21 Feb)" },
  { false, 3,  26, &BN_SP_INDEP,    FS_FLAG,     HAPPY,   TGS_IND,  "Swadhinota Dibosh" },
  { false, 4,  14, &BN_SP_BOISHAKH, FS_FIREWORK, EXCITED, TGS_BOI,  "Pohela Boishakh" },
  { false, 12, 16, &BN_SP_VICTORY,  FS_FLAG,     HAPPY,   TGS_VIC,  "Bijoy Dibosh" },
  { true,  9,  1,  &BN_SP_RAMADAN,  FS_MOON,     HAPPY,   TGS_RAM,  "Ramadan shuru" },
  { true,  10, 1,  &BN_SP_EID,      FS_MOON,     EXCITED, TGS_EIDF, "Eid-ul-Fitr" },
  { true,  12, 10, &BN_SP_EID,      FS_MOON,     EXCITED, TGS_EIDA, "Eid-ul-Adha" },
  { false, 0,  0,  &BN_SP_BDAY,     FS_CAKE,     EXCITED, TGS_BDAY, "Tomar Jonmodin 🎂" },   // BDAY_IDX: din setting theke
};
static_assert(sizeof(SPDAYS) / sizeof(SPDAYS[0]) == SPECIAL_COUNT, "SPECIAL_COUNT bodlao");
const BnText& specialBn(int i) { return *SPDAYS[i].bn; }

int specialOn(const struct tm* tm) {                   // oi dine bishesh din? (index, na hole -1)
  int hd, hm, hy;
  if (bdayDay && bdayMon == tm->tm_mon + 1 && bdayDay == tm->tm_mday) return BDAY_IDX;   // 🎂 shobar age
  hijriDate(tm, hd, hm, hy);
  for (int i = 0; i < SPECIAL_COUNT; i++) {
    const SpecialDay& s = SPDAYS[i];
    if (!s.m) continue;
    if (s.hijri ? (s.m == hm && s.d == hd) : (s.m == tm->tm_mon + 1 && s.d == tm->tm_mday)) return i;
  }
  return -1;
}

int todaySpecial() {                                   // din e ekbar hisab, tarpor mone rakhe
  static int cachedYday = -1, cachedYear = -1, cached = -1;
  static int8_t cachedAdj = 0;
  if (!timeValid()) return -1;
  time_t t = time(nullptr);
  struct tm tmv = *localtime(&t);
  if (tmv.tm_yday != cachedYday || tmv.tm_year != cachedYear || cachedAdj != hijriAdj) {
    cachedYday = tmv.tm_yday; cachedYear = tmv.tm_year; cachedAdj = hijriAdj;
    cached = specialOn(&tmv);
  }
  return cached;
}

// Porer bishesh din: koto din baki (index ferot, days e din)
int nextSpecial(int& days) {
  if (!timeValid()) return -1;
  time_t t = time(nullptr);
  for (int k = 1; k <= 400; k++) {
    time_t tk = t + (time_t)k * 86400;
    struct tm tmv = *localtime(&tk);
    int i = specialOn(&tmv);
    if (i >= 0) { days = k; return i; }
  }
  return -1;
}

// ---------- Utsob er drishyo (face er bodole) ----------
uint8_t festScene = FS_NONE;
unsigned long festUntil = 0, festSince = 0;
unsigned long nextFestAt = 0;
static const Note S_MOON[] PROGMEM  = {{1568,90},{0,40},{2093,90},{0,40},{2637,120},{0,60},{3136,260}};   // tara jhikimiki
static const Note S_FLAG[] PROGMEM  = {{523,160},{659,160},{784,160},{1047,420}};

bool festActive() { return festScene != FS_NONE && (long)(now - festUntil) < 0 && moodSince == festSince; }

// Face er bodole ekta drishyo (potaka / minar / moshjid ...) + holud e lekha, ms porjonto
void startScene(uint8_t scene, Mood m, const BnText& band, unsigned long ms) {
  clockMode = false;
  msgUntil = 0;
  moodSoundEnabled = false;
  setMood(m, ms);
  moodSoundEnabled = true;
  festScene = scene; festSince = moodSince; festUntil = now + ms;
  showBandBn(band, ms);
  sceneBand = &band;
  lastActivity = now;
}

void startFest(int i, unsigned long ms) {
  if (i < 0 || i >= SPECIAL_COUNT) return;
  const SpecialDay& s = SPDAYS[i];
  startScene(s.scene, s.mood, *s.bn, ms);
  switch (s.scene) {
    case FS_FLAG:     PLAY(S_FLAG);    break;
    case FS_MOON:     PLAY(S_MOON);    break;
    case FS_FIREWORK: PLAY(S_EXCITED); break;
    case FS_FLOWER:   PLAY(S_HAPPY);   break;
    case FS_CAKE:     PLAY(SONG_BDAY); break;          // 🎂 Happy Birthday gaan
    default: break;                                    // শহীদ মিনার: chup, shroddha
  }
  Serial.printf_P(PSTR("Bishesh din: %s\n"), s.nm);
}

// Bishesh dine majhe majhe (45 min por por) utsob er drishyo
void festReminder() {
  int sp = todaySpecial();
  if (sp < 0 || mood != NORMAL || welcomeStep >= 0) return;
  if (!nextFestAt) nextFestAt = now + 45UL * 60000UL;
  if ((long)(now - nextFestAt) < 0) return;
  startFest(sp, 7000);
  nextFestAt = now + 45UL * 60000UL;
}

// Telegram e shokal 8 tar por din e ekbar shubhechha
void festTelegram() {
  static int sentYday = -1;
  int sp = todaySpecial();
  if (sp < 0 || !tgOwner) return;
  time_t t = time(nullptr);
  struct tm* tm = localtime(&t);
  if (tm->tm_hour < (sp == BDAY_IDX ? 0 : 8) || tm->tm_yday == sentYday) return;   // jonmodin: rat 12 tay-i!
  sentYday = tm->tm_yday;
  String s = FPSTR(SPDAYS[sp].tg);
  if (sp == BDAY_IDX && bdayYear) { s += F("\n🎈 Aj tumi "); s += tm->tm_year + 1900 - bdayYear; s += F(" e porle!"); }
  tgNotify(s);
}

// শহীদ মিনার: pechone lal shurjo (bhora britto); stombho britto er upor kalo, baire shada
void minarRect(int x, int y, int w, int h) {
  for (int yy = y; yy < y + h; yy++)
    for (int xx = x; xx < x + w; xx++) {
      int dx = xx - 64, dy = yy - 35;
      display.drawPixel(xx, yy, (dx * dx + dy * dy <= 17 * 17) ? SSD1306_BLACK : SSD1306_WHITE);
    }
}

void drawMinar() {
  display.fillCircle(64, 35, 17, SSD1306_WHITE);                      // lal shurjo
  // majher boro stombho: 5 ta khuti + upore shamne jhuke thaka matha
  minarRect(53, 31, 2, 29); minarRect(74, 31, 2, 29);
  minarRect(58, 31, 1, 29); minarRect(64, 31, 1, 29); minarRect(70, 31, 1, 29);
  minarRect(53, 31, 23, 1);
  minarRect(51, 22, 27, 2); minarRect(51, 24, 2, 6); minarRect(76, 24, 2, 6);   // jhuke thaka matha
  minarRect(57, 24, 1, 6); minarRect(64, 24, 1, 6); minarRect(71, 24, 1, 6);
  // dui pashe boro + choto stombho (jora khuti, upore ar-kathi)
  const int bx[2] = { 38, 83 }, sx[2] = { 27, 96 };
  for (int i = 0; i < 2; i++) {
    minarRect(bx[i], 41, 2, 19); minarRect(bx[i] + 6, 41, 2, 19); minarRect(bx[i], 41, 8, 2);
    minarRect(sx[i], 47, 2, 13); minarRect(sx[i] + 4, 47, 2, 13); minarRect(sx[i], 47, 6, 2);
  }
  display.drawFastHLine(18, 61, 92, SSD1306_WHITE);                   // bedi
  display.drawFastHLine(14, 63, 100, SSD1306_WHITE);
  for (int i = 0; i < 5; i++) {                                       // phul (shroddhanjoli)
    int fx = 44 + i * 10;
    display.drawPixel(fx, 59, SSD1306_WHITE);
    display.drawPixel(fx - 1, 60, SSD1306_WHITE);
    display.drawPixel(fx + 1, 60, SSD1306_WHITE);
  }
}
void drawMoonScene() {
  int cy = 40 + (int)roundf(sinf((now - festSince) / 700.0f) * 2);
  display.fillCircle(56, cy, 15, SSD1306_WHITE);                      // ordhek chand
  display.fillCircle(63, cy - 4, 13, SSD1306_BLACK);
  drawSparkle(80, cy - 6, ((now / 300) % 2) ? 4 : 3);                 // tara
  static const uint8_t SX[7] = { 10, 24, 100, 116, 92, 18, 120 };
  static const uint8_t SY[7] = { 24, 52, 26, 46, 58, 36, 60 };
  for (int i = 0; i < 7; i++) {
    if (((now / 250) + i * 3) % 7 < 2) drawSparkle(SX[i], SY[i], 2);
    else display.drawPixel(SX[i], SY[i], SSD1306_WHITE);
  }
}

void drawFireworks() {
  static const uint8_t CX[3] = { 28, 66, 102 };
  static const uint8_t CY[3] = { 36, 30, 40 };
  for (int i = 0; i < 3; i++) {
    unsigned long t = (now - festSince + i * 500) % 1500;
    if (t < 300) {                                                     // rocket upore uthche
      int y = 63 - (int)((63 - CY[i]) * t / 300);
      display.fillRect(CX[i], y, 2, 4, SSD1306_WHITE);
    } else {                                                           // fete chhoriye pore, nichu hoy
      float p = (t - 300) / 1200.0f;
      float r = 4 + p * 13.0f;
      int drop = (int)(p * p * 8);
      for (int k = 0; k < 12; k++) {
        float a = k * 0.5236f;
        int x1 = CX[i] + (int)roundf(cosf(a) * r), y1 = CY[i] + (int)roundf(sinf(a) * r) + drop;
        if (p < 0.55f) {
          int x0 = CX[i] + (int)roundf(cosf(a) * r * 0.55f), y0 = CY[i] + (int)roundf(sinf(a) * r * 0.55f) + drop;
          display.drawLine(x0, y0, x1, y1, SSD1306_WHITE);
          display.fillCircle(x1, y1, 1, SSD1306_WHITE);
        } else if ((now / 90 + k + i) % 3) {
          display.fillRect(x1, y1, 2, 2, SSD1306_WHITE);              // jhikimiki kore nibhe jay
        }
      }
      if (p < 0.25f) display.fillCircle(CX[i], CY[i], 3 - (int)(p * 8), SSD1306_WHITE);
    }
  }
}
void drawFlower(int x, int y, int r) {
  for (int k = 0; k < 5; k++) {
    float a = k * 1.2566f;
    display.fillCircle(x + (int)(cosf(a) * r * 1.4f), y + (int)(sinf(a) * r * 1.4f), r, SSD1306_WHITE);
  }
  display.fillCircle(x, y, r, SSD1306_BLACK);
  display.drawPixel(x, y, SSD1306_WHITE);
}

void drawFlowerScene() {
  static const uint8_t FX[3] = { 28, 64, 100 };
  for (int i = 0; i < 3; i++) {
    float sway = sinf((now - festSince) / 500.0f + i * 1.3f) * 3.0f;
    int top = (i == 1) ? 30 : 36;
    int fx = FX[i] + (int)sway;
    display.drawLine(FX[i], 63, fx, top, SSD1306_WHITE);              // daal
    display.drawLine(FX[i], 56, FX[i] - 5, 52, SSD1306_WHITE);         // pata
    drawFlower(fx, top, i == 1 ? 4 : 3);
  }
  for (int k = 0; k < 4; k++) {                                        // pore pore pora papri
    unsigned long t = (now - festSince + k * 700) % 2800;
    int px = (k * 31 + 12 + (int)(sinf(t / 300.0f) * 4)) % SCREEN_W;
    int py = BAND_H + (int)(t * (SCREEN_H - BAND_H) / 2800);
    display.fillCircle(px, py, 1, SSD1306_WHITE);
  }
}

void drawFestScene() {
  switch (festScene) {
    case FS_FLAG:     drawAnthemScreen(); break;
    case FS_MINAR:    drawMinar();        break;
    case FS_MOON:     drawMoonScene();    break;
    case FS_FIREWORK: drawFireworks();    break;
    case FS_FLOWER:   drawFlowerScene();  break;
    case FS_MOSQUE:   drawMosque();       break;
    case FS_CAKE:     drawCake();         break;
    case FS_SUNRISE:  drawSunrise();      break;
    case FS_BELL:     drawBell();         break;
    case FS_BATTERY:  drawBatteryScene(); break;
  }
}

// ---------- Clock e Bangla / Hijri tarikh er view ----------
int bnNumWidth(int n, const BnText* const* dg) {
  char b[8]; snprintf(b, sizeof(b), "%d", n);
  int w = 0;
  for (int i = 0; b[i]; i++) w += dg[b[i] - '0']->w + (i ? 1 : 0);
  return w;
}
int drawBnNum(int x, int y, int n, const BnText* const* dg) {
  char b[8]; snprintf(b, sizeof(b), "%d", n);
  for (int i = 0; b[i]; i++) {
    const BnText* g = dg[b[i] - '0'];
    display.drawBitmap(x, y, g->bmp, g->w, g->h, SSD1306_WHITE);
    x += g->w + 1;
  }
  return x;
}

// Boro line: "৬ অগ্রহায়ণ" (beshi chowra hole aste aste scroll), chhoto line: "১৪৩৩ বঙ্গাব্দ"
void drawDateLines(int day, const BnText& mon, int year, const BnText& word, int xoff) {
  if (langEn) {
    char b[28];
    snprintf(b, sizeof(b), "%d %s", day, enStr(mon));
    const GFXfont* f = &FreeSansBold12pt7b;
    int w = advanceOf(FreeSansBold12pt7bGlyphs, b);
    if (w > SCREEN_W - 4) { f = &FreeSansBold9pt7b; w = advanceOf(FreeSansBold9pt7bGlyphs, b); }
    int x = (SCREEN_W - w) / 2;
    if (w > SCREEN_W - 2) {
      long over = w - SCREEN_W + 4, t = (long)(now - viewChangedAt) - 1000;
      if (t < 0) t = 0;
      long dx = (t * 30 / 1000) % (over * 2 + 40);
      if (dx > over) dx = over;
      x = 2 - (int)dx;
    }
    printFont(f, x + xoff, 38, b);
    snprintf(b, sizeof(b), "%d %s", year, enStr(word));
    w = advanceOf(FreeSans9pt7bGlyphs, b);
    printFont(&FreeSans9pt7b, (SCREEN_W - w) / 2 + xoff, 59, b);
    return;
  }
  int wd = bnNumWidth(day, BD_BIG);
  int total = wd + 5 + mon.w;
  int x = (SCREEN_W - total) / 2;
  if (total > SCREEN_W - 2) {
    long over = total - SCREEN_W + 4, t = (long)(now - viewChangedAt) - 1000;
    if (t < 0) t = 0;
    long dx = (t * 30 / 1000) % (over * 2 + 40);
    if (dx > over) dx = over;
    x = 2 - (int)dx;
  }
  x += xoff;
  x = drawBnNum(x, 17, day, BD_BIG) + 4;
  display.drawBitmap(x, 17, mon.bmp, mon.w, mon.h, SSD1306_WHITE);

  int wy = bnNumWidth(year, BD_SML);
  int t2 = wy + 4 + word.w;
  int x2 = (SCREEN_W - t2) / 2 + xoff;
  x2 = drawBnNum(x2, 45, year, BD_SML) + 3;
  display.drawBitmap(x2, 45, word.bmp, word.w, word.h, SSD1306_WHITE);
}

void drawViewBangla(struct tm* tm, int xoff) {
  int d, m, y;
  banglaDate(tm, d, m, y);
  int sp = todaySpecial();
  drawBandBitmap(sp >= 0 ? *SPDAYS[sp].bn : *BN_SEASON[m / 2]);   // holud: ritu ba bishesh din
  drawDateLines(d, *BN_BMON[m], y, BN_BANGABDA, xoff);
}

void drawViewHijri(struct tm* tm, int xoff) {
  int d, m, y;
  hijriDate(tm, d, m, y);
  int sp = todaySpecial();
  drawBandBitmap(sp >= 0 ? *SPDAYS[sp].bn : BN_HIJRI_T);
  drawDateLines(d, *BN_HMON[m - 1], y, BN_HIJRI, xoff);
}

// Telegram er jonno: Unicode Bangla shonkhya ar naam
String bnDigitsUtf(int n) {
  char b[8]; snprintf(b, sizeof(b), "%d", n);
  String s;
  for (int i = 0; b[i]; i++) { s += (char)0xE0; s += (char)0xA7; s += (char)(0xA6 + (b[i] - '0')); }
  return s;
}
String pgmField(const char* p, int idx) {             // '|' diye alada list theke idx-tom
  String all = FPSTR(p);
  int st = 0;
  for (int i = 0; i < idx; i++) { st = all.indexOf('|', st); if (st < 0) return ""; st++; }
  int en = all.indexOf('|', st);
  return en < 0 ? all.substring(st) : all.substring(st, en);
}

String tgDateText() {
  if (!timeValid()) return F("🕐 Ekhono shomoy pai ni");
  time_t t = time(nullptr);
  struct tm tmv = *localtime(&t);
  int d, m, y, hd, hm, hy;
  banglaDate(&tmv, d, m, y);
  hijriDate(&tmv, hd, hm, hy);
  String s = F("📅 আজ ");
  s += bnDigitsUtf(d); s += ' '; s += pgmField(BM_UTF, m); s += ' '; s += bnDigitsUtf(y);
  s += F(" বঙ্গাব্দ ("); s += pgmField(SEA_UTF, m / 2); s += F(")\n🌙 ");
  s += bnDigitsUtf(hd); s += ' '; s += pgmField(HM_UTF, hm - 1); s += ' '; s += bnDigitsUtf(hy);
  s += F(" হিজরি\n🗓️ "); s += DAY_TITLE[tmv.tm_wday]; s += F(", ");
  s += tmv.tm_mday; s += ' '; s += MON_FULL[tmv.tm_mon]; s += ' '; s += tmv.tm_year + 1900;
  int sp = todaySpecial();
  if (sp >= 0) { s += F("\n\n"); s += FPSTR(SPDAYS[sp].tg); }
  int days = 0, nx = nextSpecial(days);
  if (nx >= 0) { s += F("\n\n⏭️ Porer bishesh din: "); s += SPDAYS[nx].nm; s += F(" ("); s += days; s += F(" din baki)"); }
  return s;
}

// ===========================================================
//  🕌 Namaz er shomoy (config.h er shohor) + Ramadan e Sehri / Iftar
//  Bangladesh er niyom: Karachi (Fajr/Isha 18°), Asr Hanafi.
//  Shotorkota (ihtiyat): Sehri shesh = Fajr - 2 min, Dhuhr +2, Asr/Maghrib/Isha +1
// ===========================================================
#define PR_LAT CITY_LAT
#define PR_LNG CITY_LON
#define PR_TZ  TZ_HOURS
#define EE_PR_MAGIC 106
#define EE_PR_FLAGS 107
enum { PR_FAJR, PR_RISE, PR_DHUHR, PR_ASR, PR_MAGH, PR_ISHA, PR_N };
int  prMin[PR_N];                       // din er koto minute e (0..1439)
int  prYday = -1, prYear = -1;
bool prRamadan = false, prFriday = false;
static const char PR_UTF[] PROGMEM = "ফজর|সূর্যোদয়|যোহর|আসর|মাগরিব|এশা|সাহরি|ইফতার|জুমা";

void loadPrayerCfg() { if (EEPROM.read(EE_PR_MAGIC) == 0xA7) prayerFlags = EEPROM.read(EE_PR_FLAGS); }
void savePrayerCfg() { EEPROM.write(EE_PR_FLAGS, prayerFlags); EEPROM.write(EE_PR_MAGIC, 0xA7); EEPROM.commit(); }

void prSunPos(double jd, double& dec, double& eqt) {
  double D = jd - 2451545.0;
  double g = fmod(357.529 + 0.98560028 * D, 360.0);
  double q = fmod(280.459 + 0.98564736 * D, 360.0);
  double L = fmod(q + 1.915 * sin(g * DEG_TO_RAD) + 0.020 * sin(2 * g * DEG_TO_RAD), 360.0);
  double e = 23.439 - 0.00000036 * D;
  double ra = atan2(cos(e * DEG_TO_RAD) * sin(L * DEG_TO_RAD), cos(L * DEG_TO_RAD)) * RAD_TO_DEG / 15.0;
  ra = fmod(ra + 24.0, 24.0);
  eqt = q / 15.0 - ra;
  eqt -= 24.0 * round(eqt / 24.0);
  dec = asin(sin(e * DEG_TO_RAD) * sin(L * DEG_TO_RAD)) * RAD_TO_DEG;
}

double prJd0 = 0;
double prAngleTime(double angle, double t, bool before) {   // shurjo "angle" degree niche/upore kokhon
  double dec, eqt;
  prSunPos(prJd0 + t / 24.0, dec, eqt);
  double noon = 12.0 - eqt;
  double v = (-sin(angle * DEG_TO_RAD) - sin(dec * DEG_TO_RAD) * sin(PR_LAT * DEG_TO_RAD)) /
             (cos(dec * DEG_TO_RAD) * cos(PR_LAT * DEG_TO_RAD));
  v = constrain(v, -1.0, 1.0);
  double T = acos(v) * RAD_TO_DEG / 15.0;
  return before ? noon - T : noon + T;
}

void prCompute(const struct tm* tm) {
  int y = tm->tm_year + 1900, m = tm->tm_mon + 1, d = tm->tm_mday;
  int yy = y, mm = m;
  if (mm <= 2) { yy--; mm += 12; }
  int A = yy / 100, B = 2 - A + A / 4;
  prJd0 = floor(365.25 * (yy + 4716)) + floor(30.6001 * (mm + 1)) + d + B - 1524.5 - PR_LNG / (15.0 * 24.0);

  double dec, eqt;
  prSunPos(prJd0 + 0.5, dec, eqt);
  double h[PR_N];
  h[PR_FAJR]  = prAngleTime(18.0, 5, true);
  h[PR_RISE]  = prAngleTime(0.833, 6, true);
  h[PR_DHUHR] = 12.0 - eqt;
  prSunPos(prJd0 + 13.0 / 24.0, dec, eqt);
  double asrAng = -atan(1.0 / (2.0 + tan(fabs(PR_LAT - dec) * DEG_TO_RAD))) * RAD_TO_DEG;   // Hanafi
  h[PR_ASR]   = prAngleTime(asrAng, 13, false);
  h[PR_MAGH]  = prAngleTime(0.833, 18, false);
  h[PR_ISHA]  = prAngleTime(18.0, 18, false);
  static const int8_t IHT[PR_N] = { 0, 0, 2, 1, 1, 1 };
  double adj = PR_TZ - PR_LNG / 15.0;
  for (int i = 0; i < PR_N; i++) prMin[i] = (int)lround((h[i] + adj) * 60.0) + IHT[i];

  int hd, hm, hy;
  hijriDate(tm, hd, hm, hy);
  prRamadan = (hm == 9);
  prFriday = (tm->tm_wday == 5);
}

bool prEnsure() {                                      // aj er shomoy hisab kora ache? (din e ekbar)
  if (!timeValid()) return false;
  time_t t = time(nullptr);
  struct tm tmv = *localtime(&t);
  if (tmv.tm_yday != prYday || tmv.tm_year != prYear) {
    prCompute(&tmv);
    prYday = tmv.tm_yday; prYear = tmv.tm_year;
    Serial.printf_P(PSTR("Namaz: Fajr %d:%02d Dhuhr %d:%02d Asr %d:%02d Maghrib %d:%02d Isha %d:%02d%s\n"),
                    prMin[0] / 60, prMin[0] % 60, prMin[2] / 60, prMin[2] % 60, prMin[3] / 60, prMin[3] % 60,
                    prMin[4] / 60, prMin[4] % 60, prMin[5] / 60, prMin[5] % 60, prRamadan ? " (Ramadan)" : "");
  }
  return true;
}

int nowMinute() {
  time_t t = time(nullptr);
  struct tm* tm = localtime(&t);
  return tm->tm_hour * 60 + tm->tm_min;
}

// Porer ghotona: kind 0..5 = namaz, 6 = Sehri shesh, 7 = Iftar. minute (kal er hole +1440)
int prNext(int nowM, int& atMin) {
  int bestK = -1, best = 100000;
  for (int k = 0; k < 8; k++) {
    int m;
    if (k < PR_N) {
      if (prRamadan && k == PR_MAGH) continue;         // Ramadan e Maghrib = Iftar
      m = prMin[k];
    } else if (prRamadan) {
      m = (k == 6) ? prMin[PR_FAJR] - 2 : prMin[PR_MAGH];
    } else continue;
    int mm = m > nowM ? m : m + 1440;                  // aj shesh hole kal (prai eki shomoy)
    if (mm < best) { best = mm; bestK = k; }
  }
  atMin = best;
  return bestK;
}

const BnText& prNameBn(int k) {
  switch (k) {
    case PR_FAJR:  return BN_PR_FAJR;
    case PR_RISE:  return BN_PR_RISE;
    case PR_DHUHR: return prFriday ? BN_PR_JUMA : BN_PR_DHUHR;
    case PR_ASR:   return BN_PR_ASR;
    case PR_MAGH:  return BN_PR_MAGH;
    case PR_ISHA:  return BN_PR_ISHA;
    case 6:        return BN_PR_SEHRI;
    default:       return BN_PR_IFTAR;
  }
}

// "৫:০৬" er moto Bangla shomoy (12 ghontar) - chhobi diye
void fmtTime12(int m, char* b, int n) {
  m = ((m % 1440) + 1440) % 1440;
  int h = use24 ? m / 60 : (m / 60) % 12;
  if (!use24 && !h) h = 12;
  snprintf(b, n, "%d:%02d", h, m % 60);
}
int bnStrWidth(const char* s, const BnText* const* dg, const BnText* colon) {
  int w = 0;
  for (int i = 0; s[i]; i++) w += (s[i] == ':' ? colon->w : dg[s[i] - '0']->w) + (i ? 1 : 0);
  return w;
}
int drawBnStr(int x, int y, const char* s, const BnText* const* dg, const BnText* colon) {
  for (int i = 0; s[i]; i++) {
    const BnText* g = (s[i] == ':') ? colon : dg[s[i] - '0'];
    display.drawBitmap(x, y, g->bmp, g->w, g->h, SSD1306_WHITE);
    x += g->w + 1;
  }
  return x;
}

void drawViewPrayer(struct tm* tm, int xoff) {
  if (!prEnsure()) return;
  drawBandBitmap(prRamadan ? BN_ROZA_T : BN_NAMAZ_T);         // নামাজের সময় / সাহরি ও ইফতার
  int nowM = tm->tm_hour * 60 + tm->tm_min;
  int at;
  int k = prNext(nowM, at);
  int left = at - nowM;
  const BnText& nm = prNameBn(k);
  char tb[8];
  fmtTime12(at, tb, sizeof(tb));

  if (langEn) {
    char b[24];
    snprintf(b, sizeof(b), "%s %s", enStr(nm), tb);
    int w = advanceOf(FreeSansBold12pt7bGlyphs, b);
    printFont(&FreeSansBold12pt7b, (SCREEN_W - w) / 2 + xoff, 38, b);
    if (left >= 60) snprintf(b, sizeof(b), "in %dh %dm", left / 60, left % 60);
    else            snprintf(b, sizeof(b), left > 0 ? "in %d min" : "now!", left);
    w = advanceOf(FreeSans9pt7bGlyphs, b);
    printFont(&FreeSans9pt7b, (SCREEN_W - w) / 2 + xoff, 59, b);
    return;
  }
  // line 1: "যোহর ১১:৫৩"
  int wt = bnStrWidth(tb, BD_BIG, &BD_BCOLON);
  int x = (SCREEN_W - (nm.w + 6 + wt)) / 2 + xoff;
  display.drawBitmap(x, 17, nm.bmp, nm.w, nm.h, SSD1306_WHITE);
  drawBnStr(x + nm.w + 6, 17, tb, BD_BIG, &BD_BCOLON);
  // line 2: "২ ঘণ্টা ১০ মিনিট" / "আর ১০ মিনিট" / "এখনই!"
  int y2 = 45;
  if (left <= 0) { display.drawBitmap((SCREEN_W - BN_NOW.w) / 2 + xoff, y2, BN_NOW.bmp, BN_NOW.w, BN_NOW.h, SSD1306_WHITE); return; }
  int hh = left / 60, mi = left % 60;
  int w = 0;
  if (hh) w += bnNumWidth(hh, BD_SML) + 3 + BN_HOUR.w + 4;
  else    w += BN_AAR.w + 4;
  if (mi || !hh) w += bnNumWidth(mi, BD_SML) + 3 + BN_MINT.w;
  int x2 = (SCREEN_W - w) / 2 + xoff;
  if (hh) {
    x2 = drawBnNum(x2, y2, hh, BD_SML) + 2;
    display.drawBitmap(x2, y2, BN_HOUR.bmp, BN_HOUR.w, BN_HOUR.h, SSD1306_WHITE);
    x2 += BN_HOUR.w + 4;
  } else {
    display.drawBitmap(x2, y2, BN_AAR.bmp, BN_AAR.w, BN_AAR.h, SSD1306_WHITE);
    x2 += BN_AAR.w + 4;
  }
  if (mi || !hh) {
    x2 = drawBnNum(x2, y2, mi, BD_SML) + 2;
    display.drawBitmap(x2, y2, BN_MINT.bmp, BN_MINT.w, BN_MINT.h, SSD1306_WHITE);
  }
}

// 🕌 Moshjid: majhe gombuj + chand, dui pashe minar, rate tara
void drawMosque() {
  const int cx = 64;
  bool night = false;
  if (timeValid()) { int h = nowMinute() / 60; night = h >= 18 || h < 6; }
  if (night) {
    static const uint8_t SX[6] = { 8, 40, 92, 120, 12, 116 };
    static const uint8_t SY[6] = { 24, 20, 21, 30, 44, 50 };
    for (int i = 0; i < 6; i++) {
      if (((now / 300) + i * 2) % 6 < 2) drawSparkle(SX[i], SY[i], 2);
      else display.drawPixel(SX[i], SY[i], SSD1306_WHITE);
    }
  }
  display.fillCircle(cx, 47, 15, SSD1306_WHITE);                      // boro gombuj
  display.fillRect(36, 47, 57, 15, SSD1306_WHITE);                    // daalan
  display.drawFastVLine(cx, 27, 6, SSD1306_WHITE);                    // churo
  display.fillCircle(cx, 24, 3, SSD1306_WHITE);                       // chand
  display.fillCircle(cx + 2, 23, 3, SSD1306_BLACK);
  display.fillCircle(cx, 55, 4, SSD1306_BLACK);                       // dorja (khilan)
  display.fillRect(cx - 4, 55, 9, 7, SSD1306_BLACK);
  for (int i = 0; i < 2; i++) {                                       // janala
    int wx = i ? 82 : 46;
    display.fillCircle(wx, 52, 2, SSD1306_BLACK);
    display.fillRect(wx - 2, 52, 5, 5, SSD1306_BLACK);
  }
  const int mx[2] = { 20, 102 };
  for (int i = 0; i < 2; i++) {                                       // minar
    display.fillRect(mx[i], 32, 6, 30, SSD1306_WHITE);
    display.fillRect(mx[i] - 2, 40, 10, 2, SSD1306_WHITE);            // baranda
    display.fillCircle(mx[i] + 2, 31, 3, SSD1306_WHITE);              // choto gombuj
    display.drawFastVLine(mx[i] + 2, 25, 4, SSD1306_WHITE);
  }
  display.drawFastHLine(14, 62, 100, SSD1306_WHITE);
  display.drawFastHLine(10, 63, 108, SSD1306_WHITE);
}

// ---------- Alert ----------
static const Note S_AZAN[] PROGMEM  = {{659,350},{0,80},{784,350},{0,80},{988,700}};        // norom 3 shur
static const Note S_IFTAR[] PROGMEM = {{784,150},{988,150},{1175,150},{0,60},{1568,520}};
static const Note S_SEHRI[] PROGMEM = {{1047,220},{0,100},{1319,220},{0,100},{1568,450},{0,500},
                                      {1047,220},{0,100},{1319,220},{0,100},{1568,450},{0,500},
                                      {1047,220},{0,100},{1319,220},{0,100},{1568,450}};

// Alert er dhoron: 0 Fajr, 1 Dhuhr/Juma, 2 Asr, 3 Maghrib, 4 Isha,
//                  5 Sehri jagao (shesh er 60 min age), 6 Sehri shesh hote 10 min, 7 Sehri shesh (= Fajr),
//                  8 Iftar er 10 min age, 9 Iftar
uint16_t prAlertDone = 0;                              // aj kon gulo hoye geche (bit)
int prAlertDay = -1;

int prAlertMinute(int a) {
  int sehriEnd = prMin[PR_FAJR] - 2;
  switch (a) {
    case 0: return prRamadan ? -1 : prMin[PR_FAJR];
    case 1: return prMin[PR_DHUHR];
    case 2: return prMin[PR_ASR];
    case 3: return prRamadan ? -1 : prMin[PR_MAGH];
    case 4: return prMin[PR_ISHA];
    case 5: return prRamadan ? sehriEnd - 60 : -1;
    case 6: return prRamadan ? sehriEnd - 10 : -1;
    case 7: return prRamadan ? prMin[PR_FAJR] : -1;
    case 8: return prRamadan ? prMin[PR_MAGH] - 10 : -1;
    case 9: return prRamadan ? prMin[PR_MAGH] : -1;
  }
  return -1;
}

// Alert dekhano (force = test, shomoy na hoileo)
void prayerAlertShow(int a, bool force) {
  if (!prEnsure() && !force) return;
  const BnText* band;
  const Note* snd = S_AZAN; uint16_t sndLen = sizeof(S_AZAN) / sizeof(S_AZAN[0]);
  Mood md = HAPPY;
  unsigned long ms = 20000;
  bool sound = true;
  switch (a) {
    case 0:  band = &BN_AL_FAJR; sound = false; break;            // bhore chup (shudhu screen)
    case 1:  band = prFriday ? &BN_AL_JUMA : &BN_AL_DHUHR; break;
    case 2:  band = &BN_AL_ASR;  break;
    case 3:  band = &BN_AL_MAGH; break;
    case 4:  band = &BN_AL_ISHA; break;
    case 5:  band = &BN_AL_SEHRI; snd = S_SEHRI; sndLen = sizeof(S_SEHRI) / sizeof(S_SEHRI[0]); md = EXCITED; ms = 60000; break;
    case 6:  band = &BN_AL_SEHEND; break;
    case 7:  band = &BN_AL_FAJR; sound = false; break;
    case 8:  band = &BN_AL_PREIF; snd = S_BEEP; sndLen = 1; break;
    default: band = &BN_AL_IFTAR; snd = S_IFTAR; sndLen = sizeof(S_IFTAR) / sizeof(S_IFTAR[0]); md = EXCITED; ms = 30000; break;
  }
  bool busy = messageActive() || anthemPlaying() || calibMode;
  if (clockMode && !force) {                           // clock e: namaz er view e niye jao (timer cholle na)
    if (!timerRunning && !timerAlarm) { clockView = CV_PRAYER; viewChangedAt = now; }
  } else if (!busy || force) {
    startScene(FS_MOSQUE, md, *band, ms);
  }
  if (sound && !busy) playSound(snd, sndLen);
  Serial.printf_P(PSTR("Namaz alert %d\n"), a);

  // Telegram: Ramadan er alert shob shomoy, namaz er ta /azan on thakle
  if (!force) {
    char tb[8];
    String s;
    int at = prAlertMinute(a);
    fmtTime12(at, tb, sizeof(tb));
    if (a >= 5) {
      if (a == 5)      { s = F("🌙 Sehri er shomoy! Utho! Sehri shesh "); fmtTime12(prMin[PR_FAJR] - 2, tb, sizeof(tb)); s += tb; }
      else if (a == 6) { s = F("⏳ Sehri shesh hote ar 10 minute!"); }
      else if (a == 7) { s = F("🕌 Sehri er shomoy shesh, Fajr er shomoy hoyeche."); }
      else if (a == 8) { s = F("🍽️ Iftar er ar 10 minute baki!"); }
      else             { s = F("🌅 Iftar er shomoy hoyeche! Iftar Mubarak 🤲 ("); s += tb; s += ')'; }
      tgNotify(s);
    } else if (prayerFlags & 1) {
      static const char* const NM[5] = { "Fajr", "Dhuhr", "Asr", "Maghrib", "Isha" };
      s = F("🕌 "); s += (a == 1 && prFriday) ? "Jumu'ah" : NM[a]; s += F(" er shomoy hoyeche ("); s += tb; s += ')';
      tgNotify(s);
    }
  }
}

void prayerAlerts() {
  static unsigned long lastCheck = 0;
  if (now - lastCheck < 2000) return;                  // 2 second e ekbar dekhlei hoy
  lastCheck = now;
  if (!prEnsure()) return;
  if (prYday != prAlertDay) {                          // notun din: shob alert abar
    prAlertDay = prYday;
    prAlertDone = 0;
    int nm = nowMinute();                              // chalu howar age jegulo periye geche, segulo bad
    for (int a = 0; a < PA_COUNT; a++) { int m = prAlertMinute(a); if (m >= 0 && m < nm) prAlertDone |= (1 << a); }
  }
  int nm = nowMinute();
  for (int a = 0; a < PA_COUNT; a++) {
    if (prAlertDone & (1 << a)) continue;
    int m = prAlertMinute(a);
    if (m < 0 || nm < m) continue;
    prAlertDone |= (1 << a);
    if (nm - m <= 3) prayerAlertShow(a, false);        // 3 min er beshi deri hole (robot bondho chilo) chup
  }
}

// ---------- Telegram er lekha ----------
String bnTimeUtf(int m) {
  char b[8];
  fmtTime12(m, b, sizeof(b));
  String s;
  for (int i = 0; b[i]; i++) {
    if (b[i] == ':') s += ':';
    else { s += (char)0xE0; s += (char)0xA7; s += (char)(0xA6 + (b[i] - '0')); }
  }
  return s;
}

String tgPrayerText() {
  if (!prEnsure()) return F("🕐 Ekhono shomoy pai ni");
  String s = F("🕌 আজকের নামাজের সময় (" CITY_NAME_BN ")\n\n");
  for (int k = 0; k < PR_N; k++) {
    s += pgmField(PR_UTF, (k == PR_DHUHR && prFriday) ? 8 : k);
    s += F(" — "); s += bnTimeUtf(prMin[k]); s += '\n';
  }
  if (prRamadan) {
    s += F("\n🌙 সাহরি শেষ "); s += bnTimeUtf(prMin[PR_FAJR] - 2);
    s += F("  |  ইফতার "); s += bnTimeUtf(prMin[PR_MAGH]); s += '\n';
  }
  int nm = nowMinute(), at;
  int k = prNext(nm, at);
  int left = at - nm;
  s += F("\n⏭️ পরের: "); s += pgmField(PR_UTF, k == PR_DHUHR && prFriday ? 8 : k);
  s += F(", আর ");
  if (left >= 60) { s += bnDigitsUtf(left / 60); s += F(" ঘণ্টা "); }
  s += bnDigitsUtf(left % 60); s += F(" মিনিট");
  s += (prayerFlags & 1) ? F("\n\n🔔 Telegram alert: chalu (/azan off)") : F("\n\n🔕 Telegram alert: bondho (/azan on)");
  s += F("\n(Karachi method, Hanafi Asr. Masjid er shathe 1-2 min edik-odik hote pare)");
  return s;
}

String tgRozaText() {
  if (!prEnsure()) return F("🕐 Ekhono shomoy pai ni");
  String s = prRamadan ? F("🌙 আজ রমজান\n") : F("🌙 Ramadan na, tobu aj er hisab:\n");
  s += F("সাহরি শেষ: "); s += bnTimeUtf(prMin[PR_FAJR] - 2);
  s += F("\nইফতার: ");     s += bnTimeUtf(prMin[PR_MAGH]);
  int nm = nowMinute();
  if (nm < prMin[PR_MAGH]) {
    int left = prMin[PR_MAGH] - nm;
    s += F("\n\n⏳ ইফতারের আর ");
    if (left >= 60) { s += bnDigitsUtf(left / 60); s += F(" ঘণ্টা "); }
    s += bnDigitsUtf(left % 60); s += F(" মিনিট");
  }
  if (!prRamadan) {
    int days = 0, nx = nextSpecial(days);
    if (nx == 6) { s += F("\n\n📅 Ramadan shuru hote ar "); s += days; s += F(" din"); }
  }
  return s;
}

// ===========================================================
//  🎮 Khela (6 tap e menu)
//    Menu: tap = porer khela, chepe dhora = shuru
//    Khelay: 2.5 s chepe dhorle menu te fire; menu te 2.5 s = khela bondho. 90 s kichu na korle nijei bondho.
// ===========================================================
enum { G_MENU, G_8BALL, G_DICE, G_RPS, G_REACT, G_CATCH, G_FLAP, G_SIMON, G_HOLE, G_EXIT, G_N };
uint8_t gameId = G_MENU, menuSel = G_8BALL;
uint8_t gState = 0;
unsigned long gT = 0, gLastInput = 0, gShakeAt = 0;
int gVal = 0, gVal2 = 0;                               // khelar choto hishab (answer / dice / pick)
int gForce = -1;                                       // Telegram theke: fol age thekei thik (dice / robot er bachai)
int rpsYou = 0, rpsMe = 0, rpsScoreYou = 0, rpsScoreMe = 0;
unsigned long reactGoAt = 0, reactRise0 = 0;
int reactMs = 0, reactBest = 0;
bool gNewRec = false;                                  // 🏆 ei khelay notun record holo?

// Record gulo EEPROM e (280 chinho, 281-282 chotpote ms, 283 tara, 284 pakhi, 285 mone rakho, 286 gorte bol)

// Notun record: holud e "নতুন রেকর্ড!" ar khelar lekha pala kore
void drawResultBand(const BnText& normal) { drawBandBitmap(gNewRec && (now / 700) % 2 ? BN_REC : normal); }

// Score er charpashe: ulto-rong box jvole-nebhe + konay tara
void recordFx(int x, int y, int w, int h) {
  if (!gNewRec) return;
  if ((now / 260) % 2) display.fillRoundRect(x, y, w, h, 4, SSD1306_INVERSE);
  int s = ((now / 150) % 3) + 2;
  drawSparkle(x - 4, y + 2, s); drawSparkle(x + w + 3, y + h - 3, s);
  drawSparkle(x + w + 3, y + 2, 5 - s); drawSparkle(x - 4, y + h - 3, 5 - s);
}

static const BnText* const G_NAME[G_N] PROGMEM = { &BN_GM_T, &BN_GM_8BALL, &BN_GM_DICE, &BN_GM_RPS, &BN_GM_REACT, &BN_GM_CATCH,
                                                    &BN_GM_FLAP, &BN_GM_SIMON, &BN_GM_HOLE, &BN_GM_EXIT };
static const BnText* const G8_ANS[12] PROGMEM = { &BN_8A1, &BN_8A2, &BN_8A3, &BN_8A4, &BN_8A5, &BN_8A6,
                                          &BN_8A7, &BN_8A8, &BN_8A9, &BN_8A10, &BN_8A11, &BN_8A12 };
static const char G8_UTF[] PROGMEM = "হ্যাঁ, অবশ্যই! ✅|নিশ্চয়ই হবে! ✨|মনে হয় হ্যাঁ 🙂|সম্ভাবনা ভালো 👍|তারারা বলছে হ্যাঁ ⭐|"
                                     "আবার জিজ্ঞেস করো 🔄|এখন বলা যাবে না 🤐|একটু ভাবতে দাও 🤔|মনে হয় না 🙁|না! ❌|একদম না! 🚫|সন্দেহ আছে... 🤨";
static const BnText* const RPS_NAME[3] PROGMEM = { &BN_R_ROCK, &BN_R_PAPER, &BN_R_SCI };
static const Note S_RATTLE[] PROGMEM = {{180,40},{0,40},{220,40},{0,50},{170,40},{0,40},{240,40},{0,60},{200,40}};
static const Note S_WIN[] PROGMEM    = {{784,100},{988,100},{1175,100},{1568,300}};
static const Note S_LOSE[] PROGMEM   = {{392,200},{330,200},{262,400}};
static const Note S_RECORD[] PROGMEM = {{784,110},{988,110},{1175,110},{1568,240},{0,80},{1175,110},{1568,110},{2093,420}};   // 🏆 jeter fanfare

void IRAM_ATTR touchISR() { touchRiseMs = millis(); }

void gameEnter() {
  gameMode = true;
  gameId = G_MENU; menuSel = G_8BALL; gState = 0;
  clockMode = false; demoMode = false; welcomeStep = -1; msgUntil = 0;
  gLastInput = now; gT = now;
  moodSoundEnabled = false; setMood(EXCITED, 1500); moodSoundEnabled = true;
  PLAY(S_EXCITED);
  Serial.println(F("Khela: menu"));
}

void gameExit() {
  gameMode = false;
  if (digitalRead(PIN_TOUCH) == HIGH) touchIgnore = true;   // ekhono chepe ache: language bodlabe na
  lastActivity = now;
  setMood(HAPPY, 2000);
  PLAY(S_CLK_OFF);
  Serial.println(F("Khela bondho"));
}

void gameStart(uint8_t g) {
  gameId = g; gState = 0; gT = now;
  if (g == G_EXIT) { gameExit(); return; }
  if (g == G_DICE && gVal < 1) gVal = 6;
  PLAY(S_START);
  Serial.printf_P(PSTR("Khela shuru: %d\n"), g);
}

// ---------- chhoto chhobi ----------
void draw8Ball(int cx, int cy, int r) {
  display.fillCircle(cx, cy, r, SSD1306_WHITE);
  display.fillCircle(cx, cy, r * 9 / 20, SSD1306_BLACK);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(cx - 2, cy - 3);
  display.print('8');
  display.drawPixel(cx - r / 2, cy - r / 2, SSD1306_BLACK);            // chokchoke alo
  display.drawPixel(cx - r / 2 + 1, cy - r / 2, SSD1306_BLACK);
}

void drawDie(int x, int y, int s, int v) {             // s = pash er mapa
  display.fillRoundRect(x, y, s, s, s / 6, SSD1306_WHITE);
  int r = max(2, s / 10);
  int a = x + s / 4, b = x + s / 2, c = x + 3 * s / 4;
  int p = y + s / 4, q = y + s / 2, w = y + 3 * s / 4;
  auto pip = [&](int px, int py) { display.fillCircle(px, py, r, SSD1306_BLACK); };
  if (v % 2) pip(b, q);
  if (v >= 2) { pip(a, p); pip(c, w); }
  if (v >= 4) { pip(c, p); pip(a, w); }
  if (v == 6) { pip(a, q); pip(c, q); }
}

void drawRpsIcon(int k, int cx, int cy) {              // 0 pathor, 1 kagoj, 2 kanchi
  if (k == 0) {
    display.fillCircle(cx, cy, 10, SSD1306_WHITE);
    display.fillCircle(cx - 8, cy - 5, 5, SSD1306_WHITE);
    display.fillCircle(cx + 7, cy - 6, 5, SSD1306_WHITE);
    display.fillCircle(cx + 2, cy + 8, 5, SSD1306_WHITE);
    display.drawLine(cx - 4, cy - 2, cx + 1, cy + 2, SSD1306_BLACK);    // fatol
    display.drawLine(cx + 3, cy - 4, cx + 6, cy + 1, SSD1306_BLACK);
  } else if (k == 1) {
    display.fillRect(cx - 10, cy - 12, 20, 24, SSD1306_WHITE);
    display.fillTriangle(cx + 4, cy - 12, cx + 10, cy - 12, cx + 10, cy - 6, SSD1306_BLACK);   // bhaj kora kona
    display.drawLine(cx + 4, cy - 12, cx + 4, cy - 6, SSD1306_WHITE);
    display.drawLine(cx + 4, cy - 6, cx + 10, cy - 6, SSD1306_WHITE);
    for (int i = 0; i < 4; i++) display.drawFastHLine(cx - 7, cy - 4 + i * 4, 13, SSD1306_BLACK);
  } else {
    display.drawCircle(cx - 6, cy + 8, 4, SSD1306_WHITE);  display.drawCircle(cx - 6, cy + 8, 3, SSD1306_WHITE);
    display.drawCircle(cx + 6, cy + 8, 4, SSD1306_WHITE);  display.drawCircle(cx + 6, cy + 8, 3, SSD1306_WHITE);
    for (int d = -1; d <= 1; d++) {
      display.drawLine(cx - 4 + d, cy + 5, cx + 8 + d, cy - 12, SSD1306_WHITE);
      display.drawLine(cx + 4 + d, cy + 5, cx - 8 + d, cy - 12, SSD1306_WHITE);
    }
    display.fillCircle(cx, cy - 1, 1, SSD1306_BLACK);
  }
}

void drawZapIcon(int cx, int cy) {
  display.fillTriangle(cx + 3, cy - 13, cx - 7, cy + 2, cx + 1, cy + 2, SSD1306_WHITE);
  display.fillTriangle(cx - 1, cy - 2, cx + 7, cy - 2, cx - 3, cy + 13, SSD1306_WHITE);
}

void drawBasket(int x, int y) {                        // x = bam, 20 px chowra
  display.fillTriangle(x, y, x + 20, y, x + 16, y + 7, SSD1306_WHITE);
  display.fillTriangle(x, y, x + 4, y + 7, x + 16, y + 7, SSD1306_WHITE);
  display.drawFastHLine(x + 3, y + 3, 14, SSD1306_BLACK);
  for (int i = 5; i < 17; i += 4) display.drawFastVLine(x + i, y + 1, 5, SSD1306_BLACK);
}

void drawGameIcon(int g, int cx, int cy) {
  switch (g) {
    case G_8BALL: draw8Ball(cx, cy, 12); break;
    case G_DICE:  drawDie(cx - 12, cy - 12, 24, 5); break;
    case G_RPS:   drawRpsIcon(0, cx - 26, cy); drawRpsIcon(1, cx, cy); drawRpsIcon(2, cx + 26, cy); break;
    case G_REACT: drawZapIcon(cx, cy); break;
    case G_CATCH: drawBasket(cx - 10, cy + 4); drawSparkle(cx, cy - 8, 4); break;
    case G_FLAP:                                        // pakhi + pipe
      display.fillRect(cx + 10, cy - 13, 7, 9, SSD1306_WHITE); display.fillRect(cx + 10, cy + 5, 7, 9, SSD1306_WHITE);
      drawBird(cx - 6, cy, true); break;
    case G_SIMON:                                       // char dike tir
      simArrow(0, cx, cy + 1, true, 0.45f); simArrow(1, cx, cy + 1, false, 0.45f);
      simArrow(2, cx, cy + 1, false, 0.45f); simArrow(3, cx, cy + 1, true, 0.45f); break;
    case G_HOLE:                                        // gorto + bol
      display.fillCircle(cx + 6, cy + 2, 7, SSD1306_WHITE); display.fillCircle(cx + 6, cy + 2, 5, SSD1306_BLACK);
      display.fillCircle(cx - 10, cy - 4, 4, SSD1306_WHITE); break;
    default:                                            // ber howar dorja + tir
      display.drawRect(cx - 4, cy - 12, 16, 24, SSD1306_WHITE);
      display.fillTriangle(cx - 14, cy, cx - 6, cy - 6, cx - 6, cy + 6, SSD1306_WHITE);
      display.fillRect(cx - 6, cy - 2, 8, 4, SSD1306_WHITE);
      break;
  }
}

// Bangla chhobi ke neel ongshe majhe (dorkar hole English)
void drawCenterBn(const BnText& b, int y) {
  if (langEn) {
    int w = advanceOf(FreeSansBold9pt7bGlyphs, enStr(b));
    if (w <= SCREEN_W - 2) { printFont(&FreeSansBold9pt7b, (SCREEN_W - w) / 2, y + 13, enStr(b)); }
    else { display.setTextSize(1); display.setCursor((SCREEN_W - (int)strlen(enStr(b)) * 6) / 2, y + 5); display.print(enStr(b)); }
    return;
  }
  display.drawBitmap((SCREEN_W - b.w) / 2, y, b.bmp, b.w, b.h, SSD1306_WHITE);
}

void gameMoodSound(Mood m, const Note* n, uint16_t len) {
  moodSoundEnabled = false; setMood(m, 2500); moodSoundEnabled = true;
  gameArmUntil = now + 1600;                           // fol er shomoy haat ektu nache, tarpor chup
  if (n) playSound(n, len);
}

// ---------- Boro score + sera (khela shesh er screen) ----------
void drawBigScore(const BnText& label, int v, int best) {
  if (langEn) {
    char b[20]; snprintf(b, sizeof(b), "%s %d", enStr(label), v);
    int w = advanceOf(FreeSansBold12pt7bGlyphs, b);
    printFont(&FreeSansBold12pt7b, (SCREEN_W - w) / 2, 40, b);
    recordFx((SCREEN_W - w) / 2 - 4, 21, w + 8, 24);
  } else {
    int w = label.w + 6 + bnNumWidth(v, BD_BIG);
    int x = (SCREEN_W - w) / 2;
    display.drawBitmap(x, 22, label.bmp, label.w, label.h, SSD1306_WHITE);
    drawBnNum(x + label.w + 6, 17, v, BD_BIG);
    recordFx(x - 4, 16, w + 8, 28);
  }
  drawBestLine(best, false);
}

// ---------- 🐦 Pakhi orao ----------
float fbY = 38, fbV = 0;
float pipeX[3]; int pipeGapY[3]; bool pipePassed[3];
unsigned long fLast = 0;
int flapBest = 0;
#define FLAP_X   30
#define PIPE_W   10
#define PIPE_GAP 24

void drawBird(int x, int y, bool wingUp) {
  display.fillCircle(x, y, 4, SSD1306_WHITE);
  display.drawPixel(x + 2, y - 2, SSD1306_BLACK);                     // chokh
  display.drawLine(x + 5, y, x + 7, y + 1, SSD1306_WHITE);            // thot
  if (wingUp) display.drawLine(x - 4, y - 1, x - 7, y - 5, SSD1306_WHITE);
  else        display.drawLine(x - 4, y + 1, x - 7, y + 4, SSD1306_WHITE);
}

void flapReset() {
  gNewRec = false;
  fbY = 38; fbV = 0; gVal = 0; fLast = now;
  for (int i = 0; i < 3; i++) { pipeX[i] = 130 + i * 52; pipeGapY[i] = random(30, 52); pipePassed[i] = false; }
}

void flapUpdate() {
  float dt = (now - fLast) / 1000.0f; fLast = now;
  if (dt > 0.08f) dt = 0.08f;
  fbV += 250.0f * dt;                                                  // matir taan
  fbY += fbV * dt;
  if (fbY < BAND_H + 4) { fbY = BAND_H + 4; fbV = 0; }
  float speed = min(60.0f, 32.0f + gVal * 1.5f);
  float maxX = 0;
  for (int i = 0; i < 3; i++) maxX = max(maxX, pipeX[i]);
  bool dead = fbY + 4 > SCREEN_H;
  for (int i = 0; i < 3; i++) {
    pipeX[i] -= speed * dt;
    if (pipeX[i] < -PIPE_W) { pipeX[i] = maxX + 52; pipeGapY[i] = random(30, 52); pipePassed[i] = false; maxX = pipeX[i]; }
    if (!pipePassed[i] && pipeX[i] + PIPE_W < FLAP_X - 4) { pipePassed[i] = true; gVal++; PLAY(S_CHIRP); }
    if (FLAP_X + 4 > pipeX[i] && FLAP_X - 4 < pipeX[i] + PIPE_W &&
        (fbY - 3 < pipeGapY[i] - PIPE_GAP / 2 || fbY + 3 > pipeGapY[i] + PIPE_GAP / 2)) dead = true;
  }
  if (dead) {
    gState = 2; gT = now;
    gNewRec = gVal > flapBest;
    if (gNewRec) { flapBest = gVal; saveRecords(); gameMoodSound(EXCITED, S_RECORD, sizeof(S_RECORD) / sizeof(S_RECORD[0])); }
    else gameMoodSound(SAD, S_LOSE, sizeof(S_LOSE) / sizeof(S_LOSE[0]));
  }
}

void drawPipes() {
  for (int i = 0; i < 3; i++) {
    int x = (int)pipeX[i], top = pipeGapY[i] - PIPE_GAP / 2, bot = pipeGapY[i] + PIPE_GAP / 2;
    if (top > BAND_H) { display.fillRect(x, BAND_H, PIPE_W, top - BAND_H, SSD1306_WHITE); display.fillRect(x - 1, top - 3, PIPE_W + 2, 3, SSD1306_WHITE); }
    display.fillRect(x, bot, PIPE_W, SCREEN_H - bot, SSD1306_WHITE);
    display.fillRect(x - 1, bot, PIPE_W + 2, 3, SSD1306_WHITE);
    display.drawFastVLine(x + 2, bot + 3, SCREEN_H - bot - 3, SSD1306_BLACK);   // chakchake dag
  }
}

// ---------- 🧠 Mone rakho (kaat kore tir er order) ----------
// dik: 0 upore (shamne jhokao), 1 dane, 2 niche (pechone), 3 bame
float gTilt0X = 0, gTilt0Y = 0;                         // khela shuru-r shomoy robot je bhabe chhilo = "shoja"
void gameTiltZero() { if (mpuOk && mpuRead()) { gTilt0X = tiltX(); gTilt0Y = tiltY(); } else { gTilt0X = gTilt0Y = 0; } }
float gTiltX() { float v = tiltX() - gTilt0X; return fabsf(v) < 0.05f ? 0 : v; }   // choto kaat bad (dead zone)
float gTiltY() { float v = gTilt0Y - tiltY(); return fabsf(v) < 0.05f ? 0 : v; }   // khelay y ulta chhilo: shamne jhokale bol upore

uint8_t simSeq[40];
int simLen = 0, simIdx = 0, simLit = -1, simBest = 0;
bool simWaitLevel = false;
unsigned long simT = 0;
static const uint16_t SIM_TONE[4] = { 880, 1047, 659, 784 };

void simArrow(int d, int cx, int cy, bool fill, float sc) {
  int o = (int)(22 * sc), s = (int)(8 * sc), b = (int)(8 * sc);
  int x0, y0, x1, y1, x2, y2;
  switch (d) {
    case 0:  x0 = cx; y0 = cy - o - s; x1 = cx - b; y1 = cy - o + s / 2; x2 = cx + b; y2 = y1; break;
    case 2:  x0 = cx; y0 = cy + o + s; x1 = cx - b; y1 = cy + o - s / 2; x2 = cx + b; y2 = y1; break;
    case 3:  x0 = cx - o - s - 8; y0 = cy; x1 = cx - o + s / 2 - 8; y1 = cy - b; x2 = x1; y2 = cy + b; break;
    default: x0 = cx + o + s + 8; y0 = cy; x1 = cx + o - s / 2 + 8; y1 = cy - b; x2 = x1; y2 = cy + b; break;
  }
  if (fill) display.fillTriangle(x0, y0, x1, y1, x2, y2, SSD1306_WHITE);
  else      display.drawTriangle(x0, y0, x1, y1, x2, y2, SSD1306_WHITE);
}

void simTone(int d) { if (soundOn) tone(PIN_BUZZER, SIM_TONE[d], 280); }

void simGameOver() {                                   // khela shesh: record holo kina
  gNewRec = simLen - 1 > simBest;
  if (gNewRec) { simBest = simLen - 1; saveRecords(); gameMoodSound(EXCITED, S_RECORD, sizeof(S_RECORD) / sizeof(S_RECORD[0])); }
  else gameMoodSound(SAD, S_LOSE, sizeof(S_LOSE) / sizeof(S_LOSE[0]));
}

void simNew() {
  gNewRec = false;
  simLen = 2;
  for (int i = 0; i < 40; i++) simSeq[i] = random(4);
  simIdx = 0; simT = now; simLit = -1; gState = 1; gT = now;
  gameTiltZero();
}

int simTiltDir(bool& level) {
  float tx = gTiltX(), ty = gTiltY();
  level = fabsf(tx) < 0.15f && fabsf(ty) < 0.15f;
  if (fabsf(tx) > fabsf(ty)) { if (tx > 0.35f) return 1; if (tx < -0.35f) return 3; }
  else                       { if (ty > 0.35f) return 2; if (ty < -0.35f) return 0; }
  return -1;
}

void simUpdate() {
  if (gState == 1) {                                                   // robot dekhay
    unsigned long el = now - simT;
    int step = el / 700;
    if (step >= simLen) { gState = 2; simIdx = 0; simT = now; simLit = -1; simWaitLevel = true; return; }
    bool on = (el % 700) < 450;
    int d = simSeq[step];
    if (on && simLit != d + step * 4) { simLit = d + step * 4; simTone(d); }
    simLit = on ? d + step * 4 : -1;
  } else if (gState == 2) {                                            // tumi kaat koro
    bool level;
    int d = mpuOk ? simTiltDir(level) : -1;
    if (simWaitLevel) {
      if (level) { simWaitLevel = false; simLit = -1; }
    } else if (d >= 0) {
      simLit = d; simTone(d); simWaitLevel = true; simT = now;
      if (d == simSeq[simIdx]) {
        simIdx++;
        if (simIdx >= simLen) { gState = 3; gT = now; gameMoodSound(HAPPY, S_WIN, sizeof(S_WIN) / sizeof(S_WIN[0])); }
      } else {
        gState = 4; gT = now;
        simGameOver();
      }
    }
    if (gState == 2 && now - simT > 7000) {                            // onek deri
      gState = 4; gT = now;
      simGameOver();
    }
  } else if (gState == 3 && now - gT > 1300) {                         // porer level: ekta beshi
    if (simLen < 40) simLen++;
    simIdx = 0; simT = now; simLit = -1; gState = 1;
  }
}

void simDraw() {
  int lit = -1;
  if (gState == 1 && simLit >= 0) lit = simLit % 4;
  if (gState == 2 && simWaitLevel && simLit >= 0) lit = simLit;
  for (int d = 0; d < 4; d++) simArrow(d, 64, 40, d == lit, 0.75f);
  int lv = max(1, simLen - 1);
  if (langEn) { char b[4]; snprintf(b, sizeof(b), "%d", lv); display.setTextSize(1); display.setCursor(64 - (int)strlen(b) * 3, 37); display.print(b); }
  else drawBnNum(64 - bnNumWidth(lv, BD_SML) / 2, 31, lv, BD_SML);
}

// ---------- ⚫ Gorte bol ----------
float hbX, hbY, hbVX, hbVY;
int holeX, holeY, startX, startY;
int trapX[4], trapY[4], trapN = 0;
unsigned long holeDeadline = 0, hLast = 0;
int holeBest = 0;

bool holeFarFrom(int x, int y, int px, int py, int d) { return (x - px) * (x - px) + (y - py) * (y - py) > d * d; }

void holeLevel() {
  bool left = random(2);
  startX = left ? 10 : 117; startY = random(24, 56);
  holeX = left ? random(80, 118) : random(10, 48); holeY = random(24, 56);
  trapN = min(4, gVal - 1);
  for (int i = 0; i < trapN; i++) {
    int tries = 0;
    do { trapX[i] = random(14, 114); trapY[i] = random(22, 58); tries++; }
    while (tries < 30 && (!holeFarFrom(trapX[i], trapY[i], holeX, holeY, 14) || !holeFarFrom(trapX[i], trapY[i], startX, startY, 16)));
  }
  hbX = startX; hbY = startY; hbVX = hbVY = 0;
  holeDeadline = now + 25000UL - min(10000UL, (unsigned long)(gVal - 1) * 1500UL);
  hLast = now; gT = now;
}

void holeUpdate() {
  float dt = (now - hLast) / 1000.0f; hLast = now;
  if (dt > 0.08f) dt = 0.08f;
  if (mpuOk) { hbVX += gTiltX() * 320.0f * dt; hbVY += gTiltY() * 320.0f * dt; }
  float fr = 1.0f - 1.6f * dt;                                         // ghorshon
  hbVX *= fr; hbVY *= fr;
  hbX += hbVX * dt; hbY += hbVY * dt;
  if (hbX < 3)   { hbX = 3;   hbVX = -hbVX * 0.5f; }
  if (hbX > 124) { hbX = 124; hbVX = -hbVX * 0.5f; }
  if (hbY < BAND_H + 3) { hbY = BAND_H + 3; hbVY = -hbVY * 0.5f; }
  if (hbY > 60)  { hbY = 60;  hbVY = -hbVY * 0.5f; }
  for (int i = 0; i < trapN; i++) {
    if (!holeFarFrom((int)hbX, (int)hbY, trapX[i], trapY[i], 6)) {    // phand: shuru te fire
      hbX = startX; hbY = startY; hbVX = hbVY = 0;
      PLAY(S_ANGRY);
    }
  }
  if (!holeFarFrom((int)hbX, (int)hbY, holeX, holeY, 4)) {             // gorte porlo!
    gState = 3; gT = now;
    gameMoodSound(EXCITED, S_WIN, sizeof(S_WIN) / sizeof(S_WIN[0]));
  } else if ((long)(now - holeDeadline) >= 0) {
    gState = 4; gT = now;
    gNewRec = gVal - 1 > holeBest;
    if (gNewRec) { holeBest = gVal - 1; saveRecords(); gameMoodSound(EXCITED, S_RECORD, sizeof(S_RECORD) / sizeof(S_RECORD[0])); }
    else gameMoodSound(SAD, S_LOSE, sizeof(S_LOSE) / sizeof(S_LOSE[0]));
  }
}

void holeDraw(bool sinking) {
  display.fillCircle(holeX, holeY, 6, SSD1306_WHITE);                  // gorto (bala)
  display.fillCircle(holeX, holeY, 4, SSD1306_BLACK);
  for (int i = 0; i < trapN; i++) {                                    // phand: kanta
    display.drawCircle(trapX[i], trapY[i], 3, SSD1306_WHITE);
    display.drawLine(trapX[i] - 5, trapY[i] - 5, trapX[i] + 5, trapY[i] + 5, SSD1306_WHITE);
    display.drawLine(trapX[i] - 5, trapY[i] + 5, trapX[i] + 5, trapY[i] - 5, SSD1306_WHITE);
  }
  if (sinking) {
    int r = 3 - (int)((now - gT) / 200);
    if (r > 0) display.fillCircle(holeX, holeY, r, SSD1306_WHITE);
  } else {
    display.fillCircle((int)hbX, (int)hbY, 3, SSD1306_WHITE);
  }
}

// Holud e: লেভেল ৩ ........ ১৮ (second baki)
void drawLevelBand(int lv, int secs) {
  if (langEn) {
    char b[24]; snprintf(b, sizeof(b), "Level %d", lv);
    printFont(&FreeSansBold9pt7b, 2, 13, b);
  } else {
    display.drawBitmap(2, -2, BN_LEVEL.bmp, BN_LEVEL.w, BN_LEVEL.h, SSD1306_WHITE);
    drawBnNum(6 + BN_LEVEL.w, -2, lv, BD_SML);
  }
  if (secs >= 0) {
    if (langEn) { char b[6]; snprintf(b, sizeof(b), "%d", secs); printFont(&FreeSansBold9pt7b, SCREEN_W - 2 - advanceOf(FreeSansBold9pt7bGlyphs, b), 13, b); }
    else drawBnNum(SCREEN_W - 3 - bnNumWidth(secs, BD_SML), -2, secs, BD_SML);
    display.drawCircle(SCREEN_W - 12 - bnNumWidth(secs, BD_SML), 7, 4, SSD1306_WHITE);   // ghori
    display.drawLine(SCREEN_W - 12 - bnNumWidth(secs, BD_SML), 7, SCREEN_W - 12 - bnNumWidth(secs, BD_SML), 4, SSD1306_WHITE);
  }
}

// ---------- Ghotona (touch / jhakuni) ----------
void gameEvent(uint8_t e) {
  gLastInput = now;
  if (e == GE_LONG) {                                  // 2.5 s: menu / bondho
    if (gameId == G_MENU) gameExit();
    else { gameId = G_MENU; gState = 0; PLAY(S_CLK_OFF); }
    return;
  }
  switch (gameId) {
    case G_MENU:
      if (e == GE_TAP) { menuSel = menuSel % (G_N - 1) + 1; PLAY(S_TICK); gT = now; }
      else if (e == GE_HOLD) gameStart(menuSel);
      break;

    case G_8BALL:
      if ((e == GE_TAP || e == GE_SHAKE) && gState != 1) { gState = 1; gT = now; gVal = random(12); PLAY(S_RATTLE); }
      break;

    case G_DICE:
      if ((e == GE_TAP || e == GE_SHAKE) && gState != 1) { gState = 1; gT = now; gForce = -1; }
      break;

    case G_RPS:
      if (gState == 0 && e == GE_TAP)  { rpsYou = (rpsYou + 1) % 3; PLAY(S_TICK); }
      else if (gState == 0 && e == GE_HOLD) { gState = 1; gT = now; gForce = -1; PLAY(S_BEEP); }
      else if (gState == 3 && e == GE_TAP) { gState = 0; }
      break;

    case G_REACT:
      if (e == GE_TAP && (gState == 0 || (gState >= 3 && now - gT > 1000))) {   // notun round (fol 1 s porjonto thake)
        gState = 1; gT = now; gVal = random(1500, 4000); gNewRec = false;
        reactRise0 = touchRiseMs;
      }
      break;

    case G_CATCH:
      if (e == GE_TAP && gState != 1) {                         // shuru / abar
        gState = 1; gT = now; gVal = 0; gVal2 = 3;              // gVal = score, gVal2 = jibon
        catchReset();
      }
      break;

    case G_FLAP:                                                // chhoa-matroi ure (tap er opekkha na)
      if (e != GE_PRESS) break;
      if (gState == 1) { fbV = -80.0f; PLAY(S_TICK); }
      else if (gState == 0 || (gState == 2 && now - gT > 700)) { flapReset(); gState = 1; gT = now; fbV = -80.0f; }
      break;

    case G_SIMON:
      if (e == GE_TAP && (gState == 0 || gState == 4)) { simNew(); }
      break;

    case G_HOLE:
      if (e == GE_TAP && (gState == 0 || gState == 4)) { gVal = 1; gNewRec = false; gameTiltZero(); holeLevel(); gState = 1; }
      break;
  }
}

void gameTouch(bool t) {
  static unsigned long down = 0;
  static uint8_t fired = 0;                            // 1 = hold, 2 = long
  if (t && !touchPrev) { down = now; fired = 0; gameEvent(GE_PRESS); }
  if (t && fired == 0 && now - down > 900)  { fired = 1; gameEvent(GE_HOLD); }
  if (t && fired == 1 && now - down > 2500) { fired = 2; gameEvent(GE_LONG); }
  if (!t && touchPrev && fired == 0 && now - down < 900) gameEvent(GE_TAP);
  if (t) lastActivity = now;
}

// ---------- Tara dhoro (tilt game) ----------
#define CATCH_N 4
float cX[CATCH_N], cY[CATCH_N], cV[CATCH_N];
bool  cOn[CATCH_N], cBomb[CATCH_N];
float basketX = 54;
unsigned long cNextSpawn = 0, cLastFrame = 0;
int catchBest = 0;

void catchReset() {
  gNewRec = false;
  for (int i = 0; i < CATCH_N; i++) cOn[i] = false;
  basketX = 54; cNextSpawn = now + 600; cLastFrame = now;
}

void catchUpdate() {
  float dt = (now - cLastFrame) / 1000.0f;
  cLastFrame = now;
  if (dt > 0.1f) dt = 0.1f;
  if (mpuOk) basketX += tiltX() * 260.0f * dt;         // kaat korle jhuri gorie jay
  basketX = constrain(basketX, 0.0f, (float)(SCREEN_W - 21));
  float speed = 22.0f + gVal * 1.6f;                   // score barle druto pore
  if ((long)(now - cNextSpawn) >= 0) {
    for (int i = 0; i < CATCH_N; i++) if (!cOn[i]) {
      cOn[i] = true; cX[i] = random(6, SCREEN_W - 6); cY[i] = BAND_H; cV[i] = speed * (0.8f + random(40) / 100.0f);
      cBomb[i] = gVal >= 5 && random(100) < 22;
      break;
    }
    cNextSpawn = now + max(450L, 1300L - gVal * 35L);
  }
  for (int i = 0; i < CATCH_N; i++) {
    if (!cOn[i]) continue;
    cY[i] += cV[i] * dt;
    if (cY[i] >= 54 && cY[i] <= 60 && cX[i] >= basketX - 2 && cX[i] <= basketX + 22) {
      cOn[i] = false;
      if (cBomb[i]) { gVal2--; PLAY(S_ANGRY); }
      else          { gVal++; PLAY(S_CHIRP); }
    } else if (cY[i] > SCREEN_H + 2) {
      cOn[i] = false;
      if (!cBomb[i]) { gVal2--; PLAY(S_BEEP); }
    }
  }
  if (gVal2 <= 0) {
    gState = 2; gT = now;
    gNewRec = gVal > catchBest;
    if (gNewRec) { catchBest = gVal; saveRecords(); gameMoodSound(EXCITED, S_RECORD, sizeof(S_RECORD) / sizeof(S_RECORD[0])); }
    else gameMoodSound(SAD, S_LOSE, sizeof(S_LOSE) / sizeof(S_LOSE[0]));
  }
}

// Score (Bangla chhoto shonkhya) holud e
void drawScoreBand(int score, int lives) {
  if (langEn) {
    char b[16]; snprintf(b, sizeof(b), "Score %d", score);
    printFont(&FreeSansBold9pt7b, 2, 13, b);
  } else {
    display.drawBitmap(2, -2, BN_SCORE.bmp, BN_SCORE.w, BN_SCORE.h, SSD1306_WHITE);
    drawBnNum(6 + BN_SCORE.w, -2, score, BD_SML);
  }
  for (int i = 0; i < lives; i++) drawHeart(120 - i * 12, 8, 4);
}

// ---------- Protiti khelar chhobi ----------
void gameLoop() {
  if (mpuOk && mpuRead()) {                            // jhakuni dhora
    float g  = sqrtf(imu.ax * imu.ax + imu.ay * imu.ay + imu.az * imu.az);
    float gm = sqrtf(imu.gx * imu.gx + imu.gy * imu.gy + imu.gz * imu.gz);
    bool selfMove = (long)(now - armsMovingUntil) < 0;
    bool shake = fabsf(g - 1.0f) > 0.40f || gm > 220.0f;   // halka jhakunio dhore (accel ba ghurni)
    bool tiltGame = gameId == G_CATCH || gameId == G_SIMON || gameId == G_HOLE;
    if (!selfMove && shake && now - gShakeAt > 1200 && !tiltGame) { gShakeAt = now; gameEvent(GE_SHAKE); }
  }
  bool playing = gState == 1 && (gameId == G_CATCH || gameId == G_FLAP || gameId == G_HOLE) ||
                 (gameId == G_SIMON && (gState == 1 || gState == 2));
  if (playing) gLastInput = now;                       // khela cholle nijei bondho hobe na
  if (now - gLastInput > 90000UL) { gameExit(); return; }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  unsigned long el = now - gT;

  switch (gameId) {
    case G_MENU: {
      drawBandBitmap(((now / 2500) % 2) ? BN_GM_HINT : BN_GM_T);
      int bob = (int)roundf(sinf(now / 300.0f) * 2);
      drawGameIcon(menuSel, 64, 30 + bob);
      drawCenterBn(*G_NAME[menuSel], 45);
      display.fillTriangle(2, 30, 7, 25, 7, 35, SSD1306_WHITE);            // < >
      display.fillTriangle(125, 30, 120, 25, 120, 35, SSD1306_WHITE);
      break;
    }

    case G_8BALL:
      drawBandBitmap(BN_8_ASK);
      if (gState == 1) {                                                   // jhakuni
        int jx = random(-4, 5), jy = random(-3, 4);
        draw8Ball(64 + jx, 40 + jy, 21);
        if (el > 1500) {
          gState = 2; gT = now;
          if (gVal < 5)      gameMoodSound(HAPPY,    S_WIN,  sizeof(S_WIN) / sizeof(S_WIN[0]));
          else if (gVal < 8) gameMoodSound(CONFUSED, S_CONFUSED, sizeof(S_CONFUSED) / sizeof(S_CONFUSED[0]));
          else               gameMoodSound(SUSPICIOUS, S_LOSE, sizeof(S_LOSE) / sizeof(S_LOSE[0]));
        }
      } else if (gState == 2) {                                            // uttor
        const BnText& a = *G8_ANS[gVal];
        drawCenterBn(a, 40 - (langEn ? 9 : a.h / 2));
        int s = ((now / 200) % 2) ? 3 : 2;
        drawSparkle(8, 24, s); drawSparkle(119, 58, s); drawSparkle(119, 24, 5 - s); drawSparkle(8, 58, 5 - s);
      } else {
        draw8Ball(64, 40 + (int)roundf(sinf(now / 400.0f) * 2), 21);
      }
      break;

    case G_DICE: {
      int v = gVal < 1 ? 6 : gVal;
      int yOff = 0;
      if (gState == 1) {                                                   // gorachhe
        static unsigned long lastFace = 0;
        if (now - lastFace > 80) { lastFace = now; gVal = random(1, 7); PLAY(S_TICK); }
        v = gVal;
        yOff = -(int)(fabsf(sinf(el / 90.0f)) * 6);
        if (el > 1000) {
          gState = 2; gT = now;
          if (gForce > 0) { gVal = gForce; v = gVal; gForce = -1; }
          if (gVal == 6) gameMoodSound(EXCITED, FX_TADA, sizeof(FX_TADA) / sizeof(FX_TADA[0]));
          else           gameMoodSound(HAPPY, S_HAPPY, sizeof(S_HAPPY) / sizeof(S_HAPPY[0]));
        }
      }
      drawBandBitmap(gState == 2 && v == 6 ? BN_D_SIX : BN_D_T);
      drawDie(22, 20 + yOff, 40, v);
      if (gState == 2) {
        if (langEn) { char b[4]; snprintf(b, sizeof(b), "%d", v); printFont(&FreeSansBold24pt7b, 82, 56, b); }
        else drawBnNum(86, 26, v, BD_BIG);
        if (v == 6) { int s = ((now / 150) % 2) ? 4 : 2; drawSparkle(76, 22, s); drawSparkle(118, 54, 6 - s); }
      }
      break;
    }

    case G_RPS: {
      // upore: tumi  ২-১  ami
      if (langEn) {
        char b[20]; snprintf(b, sizeof(b), "You %d - %d Me", rpsScoreYou, rpsScoreMe);
        int w = advanceOf(FreeSans9pt7bGlyphs, b);
        printFont(&FreeSans9pt7b, (SCREEN_W - w) / 2, 13, b);
      } else {
        display.drawBitmap(4, -2, BN_YOU.bmp, BN_YOU.w, BN_YOU.h, SSD1306_WHITE);
        display.drawBitmap(SCREEN_W - 4 - BN_ME.w, -2, BN_ME.bmp, BN_ME.w, BN_ME.h, SSD1306_WHITE);
        int w1 = bnNumWidth(rpsScoreYou, BD_SML), w2 = bnNumWidth(rpsScoreMe, BD_SML);
        int x = (SCREEN_W - (w1 + 10 + w2)) / 2;
        drawBnNum(x, -2, rpsScoreYou, BD_SML);
        display.fillRect(x + w1 + 2, 8, 6, 2, SSD1306_WHITE);
        drawBnNum(x + w1 + 10, -2, rpsScoreMe, BD_SML);
      }
      if (gState == 0) {                                                   // bachai
        drawRpsIcon(rpsYou, 30, 31);
        drawCenterBnAt(*RPS_NAME[rpsYou], 30, 45);
        printFont(&FreeSansBold18pt7b, 88, 48, "?");
        if ((now / 3000) % 2) {                                            // majhe majhe kivabe khelbe
          display.fillRect(0, 0, SCREEN_W, BAND_H, SSD1306_BLACK);
          drawBandBitmap(BN_R_PICK);
        }
      } else if (gState == 1) {                                            // এক... দুই... তিন!
        int step = el / 450;
        if (step >= 3) {
          gState = 2; gT = now;
          rpsMe = gForce >= 0 ? gForce : random(3);
          gForce = -1;
          int res = (rpsYou - rpsMe + 3) % 3;                              // 0 draw, 1 tumi jito, 2 ami
          if (res == 1) { rpsScoreYou++; gameMoodSound(SAD, FX_SADTROMBONE, sizeof(FX_SADTROMBONE) / sizeof(FX_SADTROMBONE[0])); }
          else if (res == 2) { rpsScoreMe++; gameMoodSound(LAUGH, S_WIN, sizeof(S_WIN) / sizeof(S_WIN[0])); }
          else gameMoodSound(SURPRISED, S_CONFUSED, sizeof(S_CONFUSED) / sizeof(S_CONFUSED[0]));
          gVal = res;
        } else {
          static int lastStep = -1;
          if (step != lastStep) { lastStep = step; PLAY(S_BEEP); }
          const BnText& c = step == 0 ? BN_R_C1 : (step == 1 ? BN_R_C2 : BN_R_C3);
          drawCenterBn(c, 22);
          int bob = (int)(fabsf(sinf(el / 120.0f)) * 6);
          drawRpsIcon(0, 30, 50 - bob);
          drawRpsIcon(0, 98, 50 - bob);
        }
      } else {                                                             // fol
        drawRpsIcon(rpsYou, 30, 32);
        drawRpsIcon(rpsMe, 98, 32);
        const BnText& r = gVal == 1 ? BN_R_WIN : (gVal == 2 ? BN_R_LOSE : BN_R_DRAW);
        drawCenterBn(r, 46);
        if (el > 1800) gState = 3;                                         // tap = abar
      }
      break;
    }

    case G_REACT: {
      if (gState == 0) {                                                   // shuru
        drawBandBitmap(BN_X_START);
        drawZapIcon(64, 34);
        if (reactBest) drawBestLine(reactBest, true);
      } else if (gState == 1) {                                            // opekkha
        drawBandBitmap(BN_X_WAIT);
        for (int i = 0; i < 3; i++) if ((int)((now / 300) % 4) > i) display.fillCircle(52 + i * 12, 40, 3, SSD1306_WHITE);
        if (touchRiseMs != reactRise0) {                                   // age-i chhuyeche!
          gState = 3; gT = now;
          gameMoodSound(ANNOYED, S_ANGRY, sizeof(S_ANGRY) / sizeof(S_ANGRY[0]));
        } else if (el > (unsigned long)gVal) {
          gState = 2;
          display.fillRect(0, BAND_H, SCREEN_W, SCREEN_H - BAND_H, SSD1306_WHITE);
          drawBandBitmap(BN_X_GO);
          display.display();
          reactGoAt = millis();                                            // screen dekhano matro ghori shuru
          reactRise0 = touchRiseMs;
          PLAY(S_BEEP);
          return;
        }
      } else if (gState == 2) {                                            // EKHON!
        drawBandBitmap(BN_X_GO);
        display.fillRect(0, BAND_H, SCREEN_W, SCREEN_H - BAND_H, SSD1306_WHITE);
        if (touchRiseMs != reactRise0) {
          reactMs = (int)(touchRiseMs - reactGoAt);
          gState = 4; gT = now;
          gNewRec = !reactBest || reactMs < reactBest;
          if (gNewRec) { reactBest = reactMs; saveRecords(); gameMoodSound(EXCITED, S_RECORD, sizeof(S_RECORD) / sizeof(S_RECORD[0])); }
          else if (reactMs < 350) gameMoodSound(EXCITED, S_WIN, sizeof(S_WIN) / sizeof(S_WIN[0]));
          else               gameMoodSound(HAPPY, S_HAPPY, sizeof(S_HAPPY) / sizeof(S_HAPPY[0]));
        } else if (millis() - reactGoAt > 3000) {
          gState = 0;
        }
      } else if (gState == 3) {                                            // age-i!
        gNewRec = false;
        drawBandBitmap(BN_X_EARLY);
        drawCenterBn(BN_X_START, 36);
      } else {                                                             // fol
        const BnText& rt = reactMs < 250 ? BN_X_R1 : reactMs < 350 ? BN_X_R2 : reactMs < 500 ? BN_X_R3 : BN_X_R4;
        drawResultBand(rt);
        if (langEn) {
          char b[12]; snprintf(b, sizeof(b), "%d ms", reactMs);
          int w = advanceOf(FreeSansBold18pt7bGlyphs, b);
          printFont(&FreeSansBold18pt7b, (SCREEN_W - w) / 2, 44, b);
          recordFx((SCREEN_W - w) / 2 - 4, 18, w + 8, 30);
        } else {
          int w = bnNumWidth(reactMs, BD_BIG) + 4 + BN_MS.w;
          int x = (SCREEN_W - w) / 2;
          int x0 = x;
          x = drawBnNum(x, 17, reactMs, BD_BIG) + 3;
          display.drawBitmap(x, 22, BN_MS.bmp, BN_MS.w, BN_MS.h, SSD1306_WHITE);
          recordFx(x0 - 4, 16, w + 8, 28);
        }
        drawBestLine(reactBest, true);
      }
      break;
    }

    case G_CATCH: {
      if (gState == 1) {
        catchUpdate();
        drawScoreBand(gVal, gVal2);
        for (int i = 0; i < CATCH_N; i++) {
          if (!cOn[i]) continue;
          if (cBomb[i]) {
            display.fillCircle((int)cX[i], (int)cY[i], 3, SSD1306_WHITE);
            display.fillCircle((int)cX[i], (int)cY[i], 1, SSD1306_BLACK);
            display.drawLine((int)cX[i] + 2, (int)cY[i] - 3, (int)cX[i] + 4, (int)cY[i] - 5, SSD1306_WHITE);
          } else {
            drawSparkle((int)cX[i], (int)cY[i], 3);
          }
        }
        drawBasket((int)basketX, 56);
      } else if (gState == 2) {                                            // khela shesh
        drawResultBand(BN_C_OVER);
        drawBigScore(BN_SCORE, gVal, catchBest);
      } else {                                                             // shuru
        drawBandBitmap(BN_C_HINT);
        int bx = 54 + (int)roundf(sinf(now / 500.0f) * 30);
        drawSparkle(64, 26 + (int)((now / 40) % 26), 3);
        drawBasket(bx, 56);
      }
      break;
    }

    case G_FLAP: {
      if (gState == 1) {
        flapUpdate();
        drawScoreBand(gVal, 0);
        drawPipes();
        drawBird(FLAP_X, (int)fbY, fbV < 0 && (now / 80) % 2);
      } else if (gState == 2) {
        drawResultBand(BN_C_OVER);
        drawBigScore(BN_SCORE, gVal, flapBest);
      } else {
        drawBandBitmap(BN_F_HINT);
        if (pipeX[0] == 0) flapReset();
        drawPipes();
        drawBird(FLAP_X, 38 + (int)roundf(sinf(now / 250.0f) * 4), (now / 150) % 2);
      }
      break;
    }

    case G_SIMON: {
      simUpdate();
      if (gState == 0) {
        drawBandBitmap(BN_S_START);
        for (int d = 0; d < 4; d++) simArrow(d, 64, 40, (int)((now / 400) % 4) == d, 0.75f);
      } else if (gState == 4) {
        drawResultBand(BN_S_WRONG);
        drawBigScore(BN_LEVEL, max(1, simLen - 1), simBest);
      } else {
        drawBandBitmap(gState == 1 ? BN_S_WATCH : (gState == 3 ? BN_S_RIGHT : BN_S_YOUR));
        simDraw();
      }
      break;
    }

    case G_HOLE: {
      if (gState == 1) {
        holeUpdate();
        long left = (long)(holeDeadline - now);
        drawLevelBand(gVal, left > 0 ? (int)((left + 999) / 1000) : 0);
        holeDraw(false);
      } else if (gState == 3) {                                            // gorte pore gelo
        drawBandBitmap(BN_H_DONE);
        holeDraw(true);
        if (now - gT > 1200) { gVal++; holeLevel(); gState = 1; }
      } else if (gState == 4) {
        drawResultBand(BN_H_TIME);
        drawBigScore(BN_LEVEL, gVal - 1, holeBest);
      } else {
        drawBandBitmap(BN_H_HINT);
        int bx = 30 + (int)((now / 30) % 60);
        display.fillCircle(96, 42, 6, SSD1306_WHITE); display.fillCircle(96, 42, 4, SSD1306_BLACK);
        display.fillCircle(bx, 42, 3, SSD1306_WHITE);
      }
      break;
    }
  }
  display.display();
}

// Test: notun record er screen dekhano (asol score bodlay na)
String gameTestRecord(bool react) {
  if (!gameMode) gameEnter();
  gLastInput = now; gT = now; gNewRec = true;
  if (react) { gameId = G_REACT; gState = 4; reactMs = 234; }
  else       { gameId = G_CATCH; gState = 2; gVal = 12; }
  return F("test");
}

void loadRecords() {
  if (EEPROM.read(280) != 0xC5) return;
  reactBest = EEPROM.read(281) | (EEPROM.read(282) << 8);
  catchBest = EEPROM.read(283); flapBest = EEPROM.read(284);
  simBest = EEPROM.read(285);   holeBest = EEPROM.read(286);
}
void saveRecords() {
  EEPROM.write(280, 0xC5);
  EEPROM.write(281, reactBest & 0xFF); EEPROM.write(282, (reactBest >> 8) & 0xFF);
  EEPROM.write(283, min(catchBest, 255)); EEPROM.write(284, min(flapBest, 255));
  EEPROM.write(285, min(simBest, 255));   EEPROM.write(286, min(holeBest, 255));
  EEPROM.commit();
}

// Niche "সেরা ২১০ মি.সে." (ms = true) ba "সেরা ১২"
void drawBestLine(int v, bool ms) {
  if (langEn) {
    char b[20]; snprintf(b, sizeof(b), ms ? "Best %d ms" : "Best %d", v);
    display.setTextSize(1);
    display.setCursor((SCREEN_W - (int)strlen(b) * 6) / 2, 55);
    display.print(b);
    return;
  }
  int w = BN_BEST.w + 4 + bnNumWidth(v, BD_SML) + (ms ? 4 + BN_MS.w : 0);
  int x = (SCREEN_W - w) / 2;
  display.drawBitmap(x, 46, BN_BEST.bmp, BN_BEST.w, BN_BEST.h, SSD1306_WHITE);
  x = drawBnNum(x + BN_BEST.w + 4, 46, v, BD_SML) + 3;
  if (ms) display.drawBitmap(x, 46, BN_MS.bmp, BN_MS.w, BN_MS.h, SSD1306_WHITE);
}

void drawCenterBnAt(const BnText& b, int cx, int y) {  // cx er majhe
  if (langEn) { display.setTextSize(1); display.setCursor(cx - (int)strlen(enStr(b)) * 3, y + 4); display.print(enStr(b)); return; }
  display.drawBitmap(cx - b.w / 2, y, b.bmp, b.w, b.h, SSD1306_WHITE);
}

// ---------- Telegram theke khela ----------
String g8Answer(int a) { return pgmField(G8_UTF, a); }
int gameRemote8(int a) {
  if (a < 0) a = random(12);
  if (!gameMode) gameEnter();
  gameId = G_8BALL; gState = 1; gT = now; gVal = a; gLastInput = now;
  PLAY(S_RATTLE);
  return a;
}
int gameRemoteDice() {
  if (!gameMode) gameEnter();
  gameId = G_DICE; gState = 1; gT = now; gLastInput = now;
  int v = random(1, 7);
  gForce = v;
  return v;
}
String gameRemoteRps(int you) {
  if (!gameMode) gameEnter();
  gameId = G_RPS; rpsYou = you; gState = 1; gT = now; gLastInput = now;
  // robot er bachai age thekei thik (screen e ek-dui-tin er por dekhabe)
  int me = random(3);
  gForce = me;
  static const char* const E[3] = { "✊", "✋", "✌️" };
  int res = (you - me + 3) % 3;
  String s = F("Tumi "); s += E[you]; s += F("  vs  PetBot "); s += E[me]; s += F("\n\n");
  s += res == 1 ? F("🎉 Tumi jitecho! (PetBot er mon kharap 😢)") : res == 2 ? F("😎 PetBot jiteche!") : F("🤝 Draw!");
  return s;
}

// ===========================================================
//  ⏰ Alarm, 🌙 ghumer shomoy, 📝 reminder, 🎂 jonmodin (setting EEPROM e thake)
//  EEPROM: 108 chinho, 109-112 jonmodin (din, mash, bochor), 113-117 alarm/ghum, 120.. 3 ta reminder
// ===========================================================
#define EE_SC_MAGIC 108
#define EE_SC_REM   120
#define REM_N       3
#define REM_LEN     39
struct Reminder { bool on; uint32_t due; char text[REM_LEN]; };
Reminder rems[REM_N];
unsigned long alarmRingAt = 0, alarmNextBeep = 0, goSleepAt = 0;
int alarmDoneYday = -1, bedDoneYday = -1;
// ⏰ Alarm er shur: ghum bhangano jore. Choto buzzer ~2.5-3.5 kHz e shobcheye jore baje,
// tai shob shur oi range e, fank kom, siren er moto kanpe (du-ta shur druto pala kore)
static const Note S_ALM_1[] PROGMEM = {{2900,70},{0,40},{2900,70},{0,40},{2900,70},{0,40},{2900,70},{0,220}};      // bip-bip-bip-bip
static const Note S_ALM_2[] PROGMEM = {{2700,60},{3300,60},{2700,60},{3300,60},{2700,60},{3300,60},{2700,60},{3300,60},
                                      {2700,60},{3300,60},{2700,60},{3300,60},{0,120}};                                  // siren
static const Note S_ALM_3[] PROGMEM = {{2600,50},{3400,50},{2600,50},{3400,50},{2600,50},{3400,50},{2600,50},{3400,50},
                                      {2600,50},{3400,50},{2600,50},{3400,50},{2600,50},{3400,50},{2600,50},{3400,50}};  // thamar fank nei
void loadSched() {
  if (EEPROM.read(EE_SC_MAGIC) != 0xC9) return;
  bdayDay = EEPROM.read(109); bdayMon = EEPROM.read(110);
  bdayYear = EEPROM.read(111) | (EEPROM.read(112) << 8);
  alarmH = EEPROM.read(113); alarmM = EEPROM.read(114);
  uint8_t fl = EEPROM.read(115);
  alarmOn = fl & 1; bedOn = fl & 2;
  bedH = EEPROM.read(116); bedM = EEPROM.read(117);
  for (int i = 0; i < REM_N; i++) {
    int b = EE_SC_REM + i * 44;
    rems[i].on = EEPROM.read(b) == 1;
    uint32_t d = 0;
    for (int k = 0; k < 4; k++) d |= (uint32_t)EEPROM.read(b + 1 + k) << (8 * k);
    rems[i].due = d;
    for (int k = 0; k < REM_LEN; k++) rems[i].text[k] = EEPROM.read(b + 5 + k);
    rems[i].text[REM_LEN - 1] = 0;
  }
}

void saveSched() {
  EEPROM.write(EE_SC_MAGIC, 0xC9);
  EEPROM.write(109, bdayDay); EEPROM.write(110, bdayMon);
  EEPROM.write(111, bdayYear & 0xFF); EEPROM.write(112, bdayYear >> 8);
  EEPROM.write(113, alarmH); EEPROM.write(114, alarmM);
  EEPROM.write(115, (alarmOn ? 1 : 0) | (bedOn ? 2 : 0));
  EEPROM.write(116, bedH); EEPROM.write(117, bedM);
  for (int i = 0; i < REM_N; i++) {
    int b = EE_SC_REM + i * 44;
    EEPROM.write(b, rems[i].on ? 1 : 0);
    for (int k = 0; k < 4; k++) EEPROM.write(b + 1 + k, (rems[i].due >> (8 * k)) & 0xFF);
    for (int k = 0; k < REM_LEN; k++) EEPROM.write(b + 5 + k, rems[i].text[k]);
  }
  EEPROM.commit();
}

String fmt12(int h, int m) {                        // shomoy lekha (12 ba 24 ghonta, setting onujayi)
  char b[10];
  if (use24) snprintf(b, sizeof(b), "%02d:%02d", h, m);
  else       snprintf(b, sizeof(b), "%d:%02d %s", (h % 12) ? h % 12 : 12, m, h < 12 ? "AM" : "PM");
  return String(b);
}

// ---------- ⏰ Alarm: 4 ta porjonto, je kono shomoy, protidin ba ekbar ----------
// EEPROM 256 = chinho (0xA4), 257.. protita 3 byte: ghonta, minute, flag (bit0 chalu, bit1 ekbar, bit7 ache)
#define AL_N 4
#define AL_ON   0x01
#define AL_ONCE 0x02
#define AL_USED 0x80
struct AlarmSlot { uint8_t h, m, fl; };
AlarmSlot alarms[AL_N];
int alDoneYday[AL_N] = { -1, -1, -1, -1 };
int alRingIdx = -1;

void loadAlarms() {
  if (EEPROM.read(256) == 0xA4) {
    for (int i = 0; i < AL_N; i++) { alarms[i].h = EEPROM.read(257 + i * 3); alarms[i].m = EEPROM.read(258 + i * 3); alarms[i].fl = EEPROM.read(259 + i * 3); }
    return;
  }
  for (int i = 0; i < AL_N; i++) alarms[i] = { 7, 0, 0 };
  if (alarmOn) alarms[0] = { alarmH, alarmM, (uint8_t)(AL_USED | AL_ON) };   // ager ekta alarm notun list e
  saveAlarms();
}

void saveAlarms() {
  EEPROM.write(256, 0xA4);
  for (int i = 0; i < AL_N; i++) { EEPROM.write(257 + i * 3, alarms[i].h); EEPROM.write(258 + i * 3, alarms[i].m); EEPROM.write(259 + i * 3, alarms[i].fl); }
  EEPROM.commit();
}

// Notun alarm (eki shomoy age thakle shetai chalu). ferot: slot, -1 = jayga nei
int alarmAdd(int h, int m, bool once) {
  int slot = -1;
  for (int i = 0; i < AL_N; i++) if ((alarms[i].fl & AL_USED) && alarms[i].h == h && alarms[i].m == m) slot = i;
  if (slot < 0) for (int i = 0; i < AL_N; i++) if (!(alarms[i].fl & AL_USED)) { slot = i; break; }
  if (slot < 0) return -1;
  alarms[slot] = { (uint8_t)h, (uint8_t)m, (uint8_t)(AL_USED | AL_ON | (once ? AL_ONCE : 0)) };
  alDoneYday[slot] = -1;
  saveAlarms();
  return slot;
}

String alarmLine(int i) {                              // "7:00 AM (protidin)"
  String s = fmt12(alarms[i].h, alarms[i].m);
  s += (alarms[i].fl & AL_ONCE) ? F(" (ekbar)") : F(" (protidin)");
  if (!(alarms[i].fl & AL_ON)) s += F(" — bondho");
  return s;
}

void wakeAlarmStart(int i) {
  if (gameMode) gameExit();
  bool morning = alarms[i].h >= 4 && alarms[i].h < 12;
  if (morning) startScene(FS_SUNRISE, EXCITED, BN_AL_WAKE, 120000UL);   // শুভ সকাল! ওঠো!
  else         startScene(FS_BELL,    EXCITED, BN_AL_RING, 120000UL);   // এলার্ম বাজছে!
  alarmRinging = true; alarmRingAt = now; alarmNextBeep = now; alRingIdx = i;
  String s = F("⏰ Alarm bajche! ("); s += fmt12(alarms[i].h, alarms[i].m); s += ')';
  if (morning) s += F(" Shubho shokal ☀️ Utho!");
  tgNotify(s);
  if (alarms[i].fl & AL_ONCE) { alarms[i].fl &= (uint8_t)(0xFF ^ AL_ON); saveAlarms(); }   // ekbar er alarm: bajar por bondho
  Serial.printf_P(PSTR("Alarm %d bajche\n"), i + 1);
}

// 🔔 Ghonta dulche, dui pashe "ring" er dheu
void drawBell() {
  float a = sinf((now - festSince) / 110.0f) * 0.35f;   // dolon
  int cx = 64, top = 22;
  int sx = (int)(sinf(a) * 14), bx = cx + (int)(sinf(a) * 4);
  display.fillCircle(cx, top - 2, 2, SSD1306_WHITE);                  // jhulano
  display.fillCircle(bx + sx / 3, top + 9, 9, SSD1306_WHITE);         // ghontar matha
  display.fillTriangle(bx + sx / 3 - 9, top + 9, bx + sx / 3 + 9, top + 9, bx + sx - 15, top + 30, SSD1306_WHITE);
  display.fillTriangle(bx + sx / 3 + 9, top + 9, bx + sx + 15, top + 30, bx + sx - 15, top + 30, SSD1306_WHITE);
  display.fillRoundRect(bx + sx - 17, top + 29, 34, 4, 2, SSD1306_WHITE);   // nicher dhar
  display.fillCircle(bx + sx + (int)(sinf(a * 2) * 3), top + 36, 3, SSD1306_WHITE);   // dolok
  if ((now / 150) % 2) {                                              // ring ring
    for (int k = 0; k < 3; k++) {
      display.drawCircleHelper(cx - 10, 40, 22 + k * 5, 0x8 | 0x1, SSD1306_WHITE);
      display.drawCircleHelper(cx + 10, 40, 22 + k * 5, 0x2 | 0x4, SSD1306_WHITE);
    }
  }
}


// "7:30", "7.30pm", "730", "11 pm", "19:30" -> h, m. pmGuess: am/pm na likhle 6-11 ke PM dhoro (ghumer shomoy)
// ferot: koto akkhor porlo (0 = bujhini)
int parseClock(const String& s, int& h, int& m, bool pmGuess) {
  String x = s; x.toLowerCase(); x.trim();
  unsigned int i = 0;
  long a = 0; int nd = 0;
  while (i < x.length() && isdigit((uint8_t)x[i])) { a = a * 10 + (x[i] - '0'); i++; nd++; }
  if (!nd) return 0;
  h = a; m = 0;
  if (i < x.length() && (x[i] == ':' || x[i] == '.')) {
    i++; int mm = 0, md = 0;
    while (i < x.length() && isdigit((uint8_t)x[i]) && md < 2) { mm = mm * 10 + (x[i] - '0'); i++; md++; }
    m = mm;
  } else if (nd >= 3) { h = a / 100; m = a % 100; }
  while (i < x.length() && x[i] == ' ') i++;
  bool am = x.substring(i).startsWith("am"), pm = x.substring(i).startsWith("pm");
  if (am || pm) i += 2;
  if (pm && h < 12) h += 12;
  if (am && h == 12) h = 0;
  if (!am && !pm && pmGuess && h >= 6 && h <= 11) h += 12;
  if (h > 23 || m > 59) return 0;
  return i;
}

// Notun reminder (ferot: kon slot, -1 = jayga nei)
int addReminder(uint32_t due, const String& text) {
  int slot = -1;
  for (int i = 0; i < REM_N; i++) if (!rems[i].on) { slot = i; break; }
  if (slot < 0) {                                        // shob bhora: shobcheye purono ta bodlao
    slot = 0;
    for (int i = 1; i < REM_N; i++) if (rems[i].due < rems[slot].due) slot = i;
  }
  rems[slot].on = true;
  rems[slot].due = due;
  strncpy(rems[slot].text, text.c_str(), REM_LEN - 1);
  rems[slot].text[REM_LEN - 1] = 0;
  int L = strlen(rems[slot].text);                     // Bangla (UTF-8) akkhor majhe kata porle pura akkhor bad
  if (L == REM_LEN - 1) {
    int k = L;
    while (k > 0 && ((uint8_t)rems[slot].text[k - 1] & 0xC0) == 0x80) k--;   // shesh akkhor er shuru khujo
    if (k > 0) {
      uint8_t lead = rems[slot].text[k - 1];
      int need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
      if (L - (k - 1) < need) rems[slot].text[k - 1] = 0;   // adhkhana akkhor
    }
  }
  saveSched();
  return slot;
}

// Shomoy ta aj periye gele kal
uint32_t nextAt(int h, int m) {
  time_t t = time(nullptr);
  struct tm tmv = *localtime(&t);
  tmv.tm_hour = h; tmv.tm_min = m; tmv.tm_sec = 0;
  time_t d = mktime(&tmv);
  if (d <= t) d += 86400;
  return (uint32_t)d;
}

String remWhen(uint32_t due) {                         // "aj 9:30 PM" / "kal 7:00 AM"
  time_t d = due, t = time(nullptr);
  struct tm a = *localtime(&d), b = *localtime(&t);
  String s = (a.tm_yday == b.tm_yday && a.tm_year == b.tm_year) ? F("aj ") : F("kal ");
  s += fmt12(a.tm_hour, a.tm_min);
  return s;
}

void wakeAlarmStop() {
  alarmRinging = false; alRingIdx = -1;
  festUntil = now;
  if (bandBnOverride == sceneBand) { bandBnOverride = nullptr; sceneBand = nullptr; }   // "এলার্ম বাজছে!" muche
  noTone(PIN_BUZZER); sndNotes = nullptr;
  setMood(HAPPY, 2500);
  Serial.println(F("Alarm bondho"));
}

void bedtimeStart() {
  if (gameMode) gameExit();
  startScene(FS_MOON, SLEEP, BN_AL_BED, 12000);       // SLEEP: "keu khele na" golpo eta bodlay na
  PLAY(S_YAWN);
  goSleepAt = now + 12000;
  tgNotify(F("🌙 Ghumanor shomoy hoyeche! Shubho ratri 😴"));
}

void fireReminder(int i) {
  String txt = rems[i].text;
  rems[i].on = false;
  saveSched();
  if (gameMode) gameExit();
  clockMode = false;
  bool ascii = false;                                  // screen e shudhu English akkhor dekha jay
  for (unsigned int k = 0; k < txt.length(); k++) if (isalnum((uint8_t)txt[k]) && (uint8_t)txt[k] < 128) ascii = true;
  showMessage(ascii ? txt : String(F("Reminder!")));
  msgBand = &BN_REMIND;                                // holud e: মনে করিয়ে দিচ্ছি!
  String s = F("⏰ Mone koriye dicchi: "); s += txt;
  tgNotify(s);
}

void schedLoop() {
  static unsigned long last = 0;
  if (alarmRinging) {                                  // alarm baje: touch na kora porjonto (2 min)
    if (!festActive() || now - alarmRingAt > 120000UL) { alarmRinging = false; if (!festActive()) noTone(PIN_BUZZER); }
    else if (!sndNotes && (long)(now - alarmNextBeep) >= 0) {
      unsigned long el = now - alarmRingAt;
      if (el > 40000UL)      PLAY(S_ALM_3);                       // 40 s por: ek-tana siren, kono fank nei
      else if (el > 15000UL) { if ((el / 3000) % 2) PLAY(S_ALM_2); else PLAY(S_ALM_1); }   // 15 s por: bip + siren pala kore
      else                   PLAY(S_ALM_1);                       // shuru: jore bip-bip-bip-bip
      alarmNextBeep = now + 100;                                   // shur shesh hole prai shathe shathe abar
    }
  }
  if (goSleepAt && (long)(now - goSleepAt) >= 0) {     // ghumer shomoy er por nijei ghum
    goSleepAt = 0;
    if (!gameMode && !clockMode && !messageActive() && (mood == SLEEP || mood == SLEEPY || mood == NORMAL)) setMood(SLEEP, 0);
  }
  if (now - last < 1000) return;
  last = now;
  if (!timeValid()) return;
  time_t t = time(nullptr);
  struct tm* tm = localtime(&t);
  int nm = tm->tm_hour * 60 + tm->tm_min;
  for (int i = 0; i < AL_N; i++) {                    // ⏰ protita alarm
    if ((alarms[i].fl & (AL_USED | AL_ON)) != (AL_USED | AL_ON) || tm->tm_yday == alDoneYday[i]) continue;
    int am = alarms[i].h * 60 + alarms[i].m;
    if (nm >= am && nm - am <= 2 && !alarmRinging) { alDoneYday[i] = tm->tm_yday; wakeAlarmStart(i); break; }
  }
  if (bedOn && tm->tm_yday != bedDoneYday) {
    int bm = bedH * 60 + bedM;
    if (nm >= bm && nm - bm <= 2) { bedDoneYday = tm->tm_yday; bedtimeStart(); }
  }
  for (int i = 0; i < REM_N; i++)
    if (rems[i].on && (uint32_t)t >= rems[i].due && !messageActive()) { fireReminder(i); break; }
}

// 🎂 Cake: duita tola, 3 ta mombati (alo kanpe), upore rongin kagoj pore
void drawCake() {
  unsigned long el = now - festSince;
  display.fillRoundRect(30, 46, 68, 17, 3, SSD1306_WHITE);           // nicher tola
  display.fillRoundRect(40, 34, 48, 13, 3, SSD1306_WHITE);           // upor er tola
  for (int x = 33; x < 96; x += 6) display.fillCircle(x, 46, 2, SSD1306_BLACK);   // cream er dheu
  for (int x = 43; x < 86; x += 6) display.fillCircle(x, 34, 2, SSD1306_BLACK);
  for (int x = 36; x < 94; x += 8) display.drawPixel(x, 55, SSD1306_BLACK);       // noksha
  const int cx[3] = { 52, 64, 76 };
  for (int i = 0; i < 3; i++) {
    display.fillRect(cx[i] - 1, 25, 3, 9, SSD1306_WHITE);            // mombati
    display.drawPixel(cx[i], 28, SSD1306_BLACK); display.drawPixel(cx[i], 31, SSD1306_BLACK);
    int fl = ((now / 90) + i) % 3;                                   // alo kanpe
    display.fillCircle(cx[i], 21, 2, SSD1306_WHITE);
    display.drawPixel(cx[i] + (fl == 0 ? -1 : fl == 1 ? 0 : 1), 18, SSD1306_WHITE);
  }
  for (int k = 0; k < 8; k++) {                                      // confetti
    unsigned long tt = (el + k * 377) % 2400;
    int px = (k * 41 + 9 + (int)(sinf(tt / 250.0f + k) * 3)) % SCREEN_W;
    if (px > 26 && px < 102) continue;
    int py = BAND_H + (int)(tt * (SCREEN_H - BAND_H) / 2400);
    if (k % 2) display.fillRect(px, py, 2, 2, SSD1306_WHITE); else drawSparkle(px, py, 2);
  }
}

// 🌅 Shurjo uthche: digonto, dhire dhire upore uthe, rodh ghore, pakhi ure
void drawSunrise() {
  unsigned long el = now - festSince;
  float k = min(1.0f, el / 3000.0f);
  int sy = 64 - (int)(k * 24);
  display.fillCircle(64, sy, 11, SSD1306_WHITE);
  float rot = el / 900.0f;
  for (int i = 0; i < 10; i++) {
    float a = rot + i * 0.628f;
    int x0 = 64 + (int)(cosf(a) * 15), y0 = sy + (int)(sinf(a) * 15);
    int x1 = 64 + (int)(cosf(a) * 20), y1 = sy + (int)(sinf(a) * 20);
    if (y0 < 56) display.drawLine(x0, y0, x1, y1, SSD1306_WHITE);
  }
  display.fillRect(0, 56, SCREEN_W, 8, SSD1306_BLACK);               // matir niche shurjo lukano
  display.drawFastHLine(0, 56, SCREEN_W, SSD1306_WHITE);
  for (int x = 4; x < SCREEN_W; x += 12) display.drawPixel(x, 59 + (x / 12) % 3, SSD1306_WHITE);   // ghash
  for (int b = 0; b < 2; b++) {                                       // pakhi
    int bx = (int)((el / 30 + b * 60) % 150) - 10, by = 24 + b * 8;
    int w = ((now / 200) + b) % 2 ? 2 : 0;
    display.drawLine(bx - 4, by - w, bx, by, SSD1306_WHITE);
    display.drawLine(bx, by, bx + 4, by - w, SSD1306_WHITE);
  }
}

// ---------- Telegram ----------
String tgArgRaw(const String& raw) {                   // "/alarm@Bot 7:30 am" -> "7:30 am"
  int sp = raw.indexOf(' ');
  if (sp < 0) return "";
  String r = raw.substring(sp + 1); r.trim();
  return r;
}

String schedListText() {
  String s = F("⏰ Alarm:");
  bool anyA = false;
  for (int i = 0; i < AL_N; i++) if (alarms[i].fl & AL_USED) { anyA = true; s += F("\n  "); s += i + 1; s += F(". "); s += alarmLine(i); }
  if (!anyA) s += F(" nei (/alarm 7:00)");
  s += F("\n🌙 Ghumer shomoy: ");
  s += bedOn ? fmt12(bedH, bedM) : String(F("bondho"));
  s += F("\n🎂 Jonmodin: ");
  if (bdayDay) { s += bdayDay; s += ' '; s += MON_FULL[bdayMon - 1]; if (bdayYear) { s += ' '; s += bdayYear; } }
  else s += F("set kora nei (/birthday 15-03)");
  s += F("\n📝 Reminder:");
  bool any = false;
  for (int i = 0; i < REM_N; i++) if (rems[i].on) { any = true; s += F("\n  • "); s += remWhen(rems[i].due); s += F(" — "); s += rems[i].text; }
  if (!any) s += F(" nei");
  return s;
}

String tgSchedCommand(const String& cmd, const String& raw) {
  String a = tgArgRaw(raw);
  String al = a; al.toLowerCase();
  int h, m;
  if (cmd == "alarm") {
    if (!a.length()) return schedListText();
    // alarm off = shob bondho, alarm clear = shob muche, alarm del 2 / off 2 / on 2 = ekta
    int sp = al.indexOf(' ');
    String w1 = sp > 0 ? al.substring(0, sp) : al;
    int n = sp > 0 ? al.substring(sp + 1).toInt() : 0;
    if ((w1 == "off" || w1 == "bondho") && !n) { for (int i = 0; i < AL_N; i++) alarms[i].fl &= (uint8_t)(0xFF ^ AL_ON); saveAlarms(); return F("🔕 Shob alarm bondho"); }
    if (w1 == "clear") { for (int i = 0; i < AL_N; i++) alarms[i].fl = 0; saveAlarms(); return F("🗑️ Shob alarm muche dilam"); }
    if ((w1 == "del" || w1 == "delete" || w1 == "off" || w1 == "on" || w1 == "bondho" || w1 == "chalu") && n) {
      if (n < 1 || n > AL_N || !(alarms[n - 1].fl & AL_USED)) return F("🤔 Oi number e alarm nei. /alarm dile list dekhabe");
      if (w1 == "del" || w1 == "delete") alarms[n - 1].fl = 0;
      else if (w1 == "on" || w1 == "chalu") { alarms[n - 1].fl |= AL_ON; alDoneYday[n - 1] = -1; }
      else alarms[n - 1].fl &= (uint8_t)(0xFF ^ AL_ON);
      saveAlarms();
      return schedListText();
    }
    if (!parseClock(a, h, m, false)) return F("🤔 Shomoy bujhini. Lekho: /alarm 7:00, /alarm 3:30pm, /alarm 22:15 ekbar");
    bool once = al.indexOf("once") >= 0 || al.indexOf("ekbar") >= 0;
    int slot = alarmAdd(h, m, once);
    if (slot < 0) return F("😅 4 ta alarm already ache. Age ekta muchho: /alarm del 1");
    String s = F("⏰ Alarm "); s += slot + 1; s += F(" set: "); s += alarmLine(slot);
    s += F("\nBajle robot e touch korle thambe.\nBondho: /alarm off "); s += slot + 1; s += F("  |  Muchte: /alarm del "); s += slot + 1;
    return s;
  }
  if (cmd == "bedtime") {
    if (!a.length()) return schedListText();
    if (al == "off" || al == "bondho") { bedOn = false; saveSched(); return F("🌙 Ghumer reminder bondho"); }
    if (!parseClock(a, h, m, true)) return F("🤔 Shomoy bujhini. Lekho: /bedtime 11pm ba /bedtime 23:00");
    bedH = h; bedM = m; bedOn = true; bedDoneYday = -1; saveSched();
    String s = F("🌙 Protidin "); s += fmt12(h, m); s += F(" e mone koriye debo ghumate jete, tarpor ami-o ghumabo 😴");
    return s;
  }
  if (cmd == "birthday" || cmd == "jonmodin") {
    if (!a.length()) return schedListText();
    if (al == "off") { bdayDay = 0; saveSched(); return F("🎂 Jonmodin muche dilam"); }
    int n[3] = { 0, 0, 0 }, c = 0; bool inNum = false;
    for (unsigned int i = 0; i < a.length() && c < 3; i++) {
      if (isdigit((uint8_t)a[i])) { n[c] = n[c] * 10 + (a[i] - '0'); inNum = true; }
      else if (inNum) { c++; inNum = false; }
    }
    if (inNum) c++;
    if (c < 2 || n[0] < 1 || n[0] > 31 || n[1] < 1 || n[1] > 12) return F("🤔 Lekho: /birthday 15-03 ba /birthday 15-03-2000");
    bdayDay = n[0]; bdayMon = n[1]; bdayYear = (c >= 3 && n[2] > 1900 && n[2] < 2100) ? n[2] : 0;
    saveSched();
    String s = F("🎂 Jonmodin save holo: "); s += bdayDay; s += ' '; s += MON_FULL[bdayMon - 1];
    if (bdayYear) { s += ' '; s += bdayYear; }
    s += F("\nOi din ami tomake surprise debo! 🎉");
    int days = 0, nx = nextSpecial(days);
    if (nx == BDAY_IDX) { s += F("\n⏳ Ar "); s += days; s += F(" din baki!"); }
    return s;
  }
  // remind
  if (!a.length()) return schedListText();
  if (al == "clear" || al == "off") { for (int i = 0; i < REM_N; i++) rems[i].on = false; saveSched(); return F("🗑️ Shob reminder muche dilam"); }
  uint32_t due = 0; int used = 0;
  {                                                    // "30m" / "2h" / "1h30m"
    String x = al; long v = 0; long mins = 0; unsigned int i = 0; bool ok = false;
    while (i < x.length()) {
      if (isdigit((uint8_t)x[i])) { v = v * 10 + (x[i] - '0'); i++; continue; }
      if (x[i] == 'h') { mins += v * 60; v = 0; ok = true; i++; continue; }
      if (x[i] == 'm' && (i + 1 >= x.length() || x[i + 1] == ' ' || isdigit((uint8_t)x[i + 1]))) { mins += v; v = 0; ok = true; i++; continue; }
      break;
    }
    if (ok && v == 0 && mins > 0 && (i >= x.length() || x[i] == ' ')) { due = (uint32_t)time(nullptr) + mins * 60; used = i; }
  }
  if (!due) {
    used = parseClock(a, h, m, false);
    if (!used) return F("🤔 Lekho: /remind 30m Pani khao  ba  /remind 9:30pm Medicine");
    due = nextAt(h, m);
  }
  String txt = a.substring(used); txt.trim();
  if (!txt.length()) txt = F("Reminder!");
  addReminder(due, txt);
  String s = F("📝 Thik ache! "); s += remWhen(due); s += F(" e mone koriye debo:\n\""); s += txt; s += '"';
  return s;
}

// ---------- Phone page (/api/sched) ----------
String webSchedJson() {
  char b[16];
  String j = F("{\"alarms\":[");
  bool fa = true;
  for (int i = 0; i < AL_N; i++) {
    if (!(alarms[i].fl & AL_USED)) continue;
    if (!fa) j += ',';
    fa = false;
    snprintf(b, sizeof(b), "%02d:%02d", alarms[i].h, alarms[i].m);
    j += F("{\"i\":"); j += i; j += F(",\"t\":\""); j += b; j += F("\",\"on\":"); j += (alarms[i].fl & AL_ON) ? 1 : 0;
    j += F(",\"once\":"); j += (alarms[i].fl & AL_ONCE) ? 1 : 0; j += '}';
  }
  j += F("],\"h24\":"); j += use24 ? 1 : 0;
  j += F(",\"bed\":\""); snprintf(b, sizeof(b), "%02d:%02d", bedH, bedM); j += b;
  j += F("\",\"bedOn\":"); j += bedOn ? 1 : 0;
  j += F(",\"bday\":\"");
  if (bdayDay) { snprintf(b, sizeof(b), "%04d-%02d-%02d", bdayYear ? bdayYear : 2000, bdayMon, bdayDay); j += b; }
  j += F("\",\"rem\":[");
  bool first = true;
  for (int i = 0; i < REM_N; i++) {
    if (!rems[i].on) continue;
    if (!first) j += ',';
    first = false;
    j += F("{\"i\":"); j += i; j += F(",\"when\":\""); j += remWhen(rems[i].due);
    j += F("\",\"t\":\""); j += jsonEsc(String(rems[i].text)); j += F("\"}");
  }
  j += F("]}");
  return j;
}

void webSched() {
  int h, m;
  if (web.hasArg("aadd") && parseClock(web.arg("aadd"), h, m, false)) alarmAdd(h, m, web.arg("once") == "1");
  if (web.hasArg("aon"))  { int i = web.arg("aon").toInt();  if (i >= 0 && i < AL_N) { if (web.arg("v") == "1") { alarms[i].fl |= AL_ON; alDoneYday[i] = -1; } else alarms[i].fl &= (uint8_t)(0xFF ^ AL_ON); saveAlarms(); } }
  if (web.hasArg("adel")) { int i = web.arg("adel").toInt(); if (i >= 0 && i < AL_N) { alarms[i].fl = 0; saveAlarms(); } }
  if (web.hasArg("aset") && parseClock(web.arg("t"), h, m, false)) {
    int i = web.arg("aset").toInt();
    if (i >= 0 && i < AL_N && (alarms[i].fl & AL_USED)) { alarms[i].h = h; alarms[i].m = m; alDoneYday[i] = -1; saveAlarms(); }
  }
  if (web.hasArg("bed")) {
    if (parseClock(web.arg("bed"), h, m, false)) { bedH = h; bedM = m; }
    bedOn = web.arg("on") != "0"; bedDoneYday = -1; saveSched();
  }
  if (web.hasArg("bday")) {
    String d = web.arg("bday");
    if (d == "off" || d.length() < 10) bdayDay = 0;
    else {
      int y = d.substring(0, 4).toInt(), mo = d.substring(5, 7).toInt(), da = d.substring(8, 10).toInt();
      if (mo >= 1 && mo <= 12 && da >= 1 && da <= 31) { bdayDay = da; bdayMon = mo; bdayYear = (y > 1900 && y < 2100) ? y : 0; }
    }
    saveSched();
  }
  String txt = web.arg("t"); txt.trim();
  if (!txt.length()) txt = F("Reminder!");
  if (web.hasArg("rem") && parseClock(web.arg("rem"), h, m, false)) addReminder(nextAt(h, m), txt);
  if (web.hasArg("remin")) { long mn = web.arg("remin").toInt(); if (mn > 0) addReminder((uint32_t)time(nullptr) + mn * 60, txt); }
  if (web.hasArg("del")) { int i = web.arg("del").toInt(); if (i >= 0 && i < REM_N) { rems[i].on = false; saveSched(); } }
  web.send(200, "application/json; charset=utf-8", webSchedJson());
}
// ===========================================================
//  📡 OTA: WiFi diye notun code (tools\upload.ps1 USB na pele nijei WiFi te pathay)
//  Password er MD5 hash shudhu (asol password: tools\ota_password.txt, code e na)
// ===========================================================
// OTA_PASS_MD5 ashe secrets.h theke (tools/set_ota_password.ps1 banay). Na thakle WiFi upload bondho.

void drawOtaScreen(int pct, bool failed) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  drawBandBitmap(BN_U0);                               // সিস্টেম আপডেট হচ্ছে
  if (failed) {
    drawBn(BN_CHECK_WIFI, 24, 0);                      // WiFi চেক করো
  } else {
    drawBn(pct >= 100 ? BN_U2 : BN_U1, 17, 0);         // নতুন কিছু আসছে! / একটু অপেক্ষা করো
    display.drawRoundRect(12, 41, 104, 10, 3, SSD1306_WHITE);
    display.fillRoundRect(14, 43, pct, 6, 2, SSD1306_WHITE);
    char b[8]; snprintf(b, sizeof(b), "%d%%", pct);
    display.setTextSize(1);
    display.setCursor((SCREEN_W - (int)strlen(b) * 6) / 2, 55);
    display.print(b);
  }
  display.display();
}

void otaSetup() {
#ifndef OTA_PASS_MD5
  Serial.println(F("OTA bondho: secrets.h nei (tools/set_ota_password.ps1 chalao)"));
  return;
#else
  ArduinoOTA.setHostname(MDNS_NAME);
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setPasswordHash(OTA_PASS_MD5);
  ArduinoOTA.onStart([]() {
    Serial.println(F("OTA: notun code ashche (WiFi)"));
    tgCli.stop();                                      // RAM khali kori
    noTone(PIN_BUZZER); sndNotes = nullptr;
    for (int i = 0; i < 2; i++) { servoArm[i].detach(); armAttached[i] = false; }
    display.invertDisplay(false);
    drawOtaScreen(0, false);
    if (soundOn) { tone(PIN_BUZZER, 1047, 90); delay(110); tone(PIN_BUZZER, 1568, 160); }
  });
  ArduinoOTA.onProgress([](unsigned int done, unsigned int total) {
    static int last = -1;
    int pct = total ? (int)((uint64_t)done * 100 / total) : 0;
    if (pct != last) { last = pct; drawOtaScreen(pct, false); }
  });
  ArduinoOTA.onEnd([]() {
    drawOtaScreen(100, false);
    Serial.println(F("OTA: shesh, restart"));
    if (soundOn) { tone(PIN_BUZZER, 1319, 90); delay(110); tone(PIN_BUZZER, 2093, 200); delay(220); }
  });
  ArduinoOTA.onError([](ota_error_t e) {
    Serial.printf_P(PSTR("OTA bhul: %u\n"), e);
    drawOtaScreen(0, true);
    delay(1500);
  });
  ArduinoOTA.begin(false);                             // mDNS age thekei chalu (petbot.local)
  MDNS.enableArduino(8266, true);
  Serial.println(F("OTA ready: " MDNS_NAME ".local:8266"));
#endif
}
// ===========================================================
//  ✨ Chalu howar shuru te: PETBOT / Designed & Developed by Al-Mamun / A Personal Project • 2026
//  🐙 7 tap e GitHub er QR code (tools/make_qr.py banay qr_github.h)
// ===========================================================
void centerText(const GFXfont* f, const GFXglyph* g, int baseY, const char* s) {
  printFont(f, (SCREEN_W - advanceOf(g, s)) / 2, baseY, s);
}

// Web page er favicon robot (neel mukh, dui chokh, hasi, holud antenna) - OLED e: uzzol mukh, kalo chokh/hasi
// (x, y) = upor-bam kona, n = mapa (n x n)
void drawLogo(int x, int y, int n) {
  float k = n / 64.0f;
  auto X = [&](float v) { return x + (int)roundf(v * k); };
  auto Y = [&](float v) { return y + (int)roundf(v * k); };
  int r = max(1, (int)roundf(4 * k));
  display.drawLine(X(32), Y(4), X(32), Y(13), SSD1306_WHITE);           // antenna
  display.fillCircle(X(32), Y(5), r, SSD1306_WHITE);
  display.fillRoundRect(X(4), Y(12), (int)roundf(56 * k), (int)roundf(46 * k), max(2, (int)roundf(15 * k)), SSD1306_WHITE);
  int ew = max(2, (int)roundf(15 * k)), eh = max(2, (int)roundf(17 * k)), er = max(1, (int)roundf(6 * k));
  display.fillRoundRect(X(13), Y(24), ew, eh, er, SSD1306_BLACK);     // chokh
  display.fillRoundRect(X(36), Y(24), ew, eh, er, SSD1306_BLACK);
  int th = n >= 24 ? 2 : 1;                                           // hasi
  for (int d = 0; d < th; d++) {
    display.drawLine(X(23), Y(47) + d, X(32), Y(52) + d, SSD1306_BLACK);
    display.drawLine(X(32), Y(52) + d, X(41), Y(47) + d, SSD1306_BLACK);
  }
}

// Shuru er boro naam: [robot] PetBot  (n = koto akkhor dekhabe, line = underline 0..6)
void drawSplashTitle(int n, int line) {
  const char* T = "PetBot";
  char buf[8];
  memcpy(buf, T, n); buf[n] = 0;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  const int L = 32;                                                   // logo er mapa
  int tw = advanceOf(FreeSansBold12pt7bGlyphs, T);
  int x0 = (SCREEN_W - (L + 7 + tw)) / 2;
  drawLogo(x0, 20, L);
  printFont(&FreeSansBold12pt7b, x0 + L + 7, 45, buf);
  if (line > 0) display.fillRect(x0 + L + 7, 50, tw * line / 6, 2, SSD1306_WHITE);
  int s = ((millis() / 220) % 2) ? 3 : 2;                             // holud e jhikimiki
  drawSparkle(20, 8, s); drawSparkle(64, 5, 5 - s); drawSparkle(108, 9, s);
  display.display();
}

void drawCredits(int reveal) {                         // reveal: koto line dekhabe (0-3)
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  // holud: [chhoto robot] PetBot
  int tw = advanceOf(FreeSansBold9pt7bGlyphs, "PetBot");
  int x0 = (SCREEN_W - (15 + 5 + tw)) / 2;
  drawLogo(x0, 0, 15);
  printFont(&FreeSansBold9pt7b, x0 + 20, 13, "PetBot");
  if (reveal >= 1) {
    display.setTextSize(1);
    const char* l1 = CREDIT_LINE1;
    display.setCursor((SCREEN_W - (int)strlen(l1) * 6) / 2, 20);
    display.print(l1);
  }
  if (reveal >= 2) centerText(&FreeSansBold9pt7b, FreeSansBold9pt7bGlyphs, 44, CREDIT_BY);
  if (reveal >= 3) {                                   // "A Personal Project • 2026" (chhoto font, majhe golla)
    const char* a = CREDIT_PROJECT;
    const char* b = CREDIT_YEAR;
    int wa = advanceOf(TomThumbGlyphs, a), wb = advanceOf(TomThumbGlyphs, b);
    int x = (SCREEN_W - (wa + 9 + wb)) / 2;
    printFont(&TomThumb, x, 60, a);
    display.fillCircle(x + wa + 4, 57, 1, SSD1306_WHITE);
    printFont(&TomThumb, x + wa + 9, 60, b);
  }
  display.display();
}

void showSplash() {
  // 1) Robot logo + "PetBot": akkhor ekta ekta kore, protita te choto "tik"
  for (int n = 0; n <= 6; n++) {
    drawSplashTitle(n, 0);
    if (n && soundOn) tone(PIN_BUZZER, 1200 + n * 150, 35);
    delay(n ? 130 : 250);
  }
  for (int k = 1; k <= 6; k++) { drawSplashTitle(6, k); delay(40); }   // underline
  if (soundOn) { tone(PIN_BUZZER, 1568, 90); delay(110); tone(PIN_BUZZER, 2093, 180); }
  unsigned long t0 = millis();
  while (millis() - t0 < 700) { drawSplashTitle(6, 6); delay(60); }
  // 2) Credits: line gulo ek ek kore
  for (int r = 0; r <= 3; r++) {
    t0 = millis();
    while (millis() - t0 < (r == 3 ? 1800UL : 380UL)) { drawCredits(r); delay(60); }
  }
  // 3) GitHub QR + "Details: 7 Tap"
  if (soundOn) tone(PIN_BUZZER, 2093, 60);
  t0 = millis();
  while (millis() - t0 < 3000) { drawSplashQR(); delay(60); }
  noTone(PIN_BUZZER);
}

// ---------- 🐙 GitHub QR ----------
static const Note S_TADA_QR[] PROGMEM = {{1047,90},{1319,90},{1568,90},{2093,220}};

#define PETBOT_VERSION "v1.0"
unsigned long qrShownAt = 0;

void showQR() {
  if (gameMode) gameExit();
  clockMode = false;
  msgUntil = 0;
  qrShownAt = millis();
  qrUntil = qrShownAt + 40000UL;
  lastActivity = now;
  PLAY(S_TADA_QR);
  Serial.println(F("About + GitHub QR (7 tap)"));
}

// Bam e QR: shada jaminer upor kalo ghor (2 px), charpashe 3 px faka
void drawQRCode() {
  display.fillRect(0, 0, 64, 64, SSD1306_WHITE);
  for (int r = 0; r < QR_SIZE; r++) {
    uint32_t row = pgm_read_dword(&QR_ROWS[r]);
    for (int c = 0; c < QR_SIZE; c++)
      if (row & (1UL << (QR_SIZE - 1 - c))) display.fillRect(3 + c * 2, 3 + r * 2, 2, 2, SSD1306_BLACK);
  }
}

void drawPageDots(int page, int count, int x, int y) {
  for (int i = 0; i < count; i++) {
    if (i == page) display.fillCircle(x + i * 7, y, 2, SSD1306_WHITE);
    else           display.drawCircle(x + i * 7, y, 2, SSD1306_WHITE);
  }
}

// Shuru te (credits er por): QR + "Details: 7 Tap"
void drawSplashQR() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  drawQRCode();
  printFont(&FreeSansBold9pt7b, 68, 13, "GitHub");
  printFont(&TomThumb, 68, 26, "@" CREDIT_GITHUB);
  display.setTextSize(1);
  display.setCursor(68, 33); display.print("Details:");
  printFont(&FreeSansBold9pt7b, 68, 56, "7 Tap");
  int uw = (int)((millis() / 60) % 40);                // nicher dag: tap er moto bar bar bhore
  display.fillRect(68, 60, min(uw, 46), 2, SSD1306_WHITE);
  display.display();
}

// 7 tap: "About PetBot" - 2 ta page nijei ghore (5 s), tap = bondho
void drawQR() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  int page = (int)(((millis() - qrShownAt) / 5000) % 2);
  if (page == 0) {                                     // ---- QR ----
    drawQRCode();
    printFont(&FreeSansBold9pt7b, 68, 13, "GitHub");
    printFont(&TomThumb, 68, 26, "@" CREDIT_GITHUB);
    display.setTextSize(1);
    display.setCursor(71, 33); display.print("Scan me!");
    int dx = (int)((millis() / 120) % 6);              // tir: QR er dike
    display.fillTriangle(74 - dx, 48, 82 - dx, 43, 82 - dx, 53, SSD1306_WHITE);
    display.fillRect(82 - dx, 46, 14, 4, SSD1306_WHITE);
    drawPageDots(0, 2, 110, 60);
  } else {                                             // ---- Details ----
    int tw = advanceOf(FreeSansBold9pt7bGlyphs, "PetBot");
    drawLogo(2, 0, 15);
    printFont(&FreeSansBold9pt7b, 21, 13, "PetBot");
    printFont(&TomThumb, 25 + tw, 12, PETBOT_VERSION);
    drawPageDots(1, 2, 113, 7);
    display.setTextSize(1);                            // shob lekha eki (6x8) mapay
    const char* l1 = CREDIT_LINE1;
    const char* l2 = CREDIT_BY;
    display.setCursor((SCREEN_W - (int)strlen(l1) * 6) / 2, 18); display.print(l1);
    display.setCursor((SCREEN_W - (int)strlen(l2) * 6) / 2, 28); display.print(l2);
    for (int x = 10; x < SCREEN_W - 10; x += 3) display.drawPixel(x, 39, SSD1306_WHITE);   // bindu-rekha
    display.setCursor(13, 44); display.print("Web: petbot.local");
    display.setCursor(13, 55); display.print("Git: " CREDIT_GITHUB);
  }
  display.display();
}

// ===========================================================
//  🔋 Battery (3.7V LiPo): LiPo + (B+) -> 150k -> A0. NodeMCU er bhetore 220k/100k ache,
//  tai A0 te 1023 = ~4.7V. /battery full ba /battery cal 3.95 diye thik kora jay (EEPROM 270-274)
// ===========================================================
float batScale = 4.70f;                               // ADC 1023 = koto volt
unsigned long batNextWarn = 0;
int batLastNotify = 101;
static const Note S_BAT_LOW[] PROGMEM = {{1047,140},{784,140},{523,320}};

void loadBat() {
  if (EEPROM.read(274) != 0xB4) return;
  float f; uint8_t* p = (uint8_t*)&f;
  for (int i = 0; i < 4; i++) p[i] = EEPROM.read(270 + i);
  if (f > 3.0f && f < 12.0f) batScale = f;
}
void saveBat() {
  uint8_t* p = (uint8_t*)&batScale;
  for (int i = 0; i < 4; i++) EEPROM.write(270 + i, p[i]);
  EEPROM.write(274, 0xB4);
  EEPROM.commit();
}

int lipoPct(float v) {                                // LiPo voltage -> % (bishram obosthar curve)
  static const float V[] = { 3.30f, 3.61f, 3.69f, 3.71f, 3.73f, 3.75f, 3.77f, 3.79f, 3.80f, 3.82f, 3.84f,
                             3.85f, 3.87f, 3.91f, 3.95f, 3.98f, 4.02f, 4.08f, 4.11f, 4.15f, 4.20f };
  if (v <= V[0]) return 0;
  if (v >= V[20]) return 100;
  for (int i = 1; i <= 20; i++)
    if (v < V[i]) return (int)roundf((i - 1) * 5 + 5 * (v - V[i - 1]) / (V[i] - V[i - 1]));
  return 100;
}

void batteryLoop() {
  static unsigned long last = 0;
  static bool first = true;
  if (now - last < 1000) return;                     // 1 second e ekbar (beshi porle WiFi te shomossa)
  last = now;
  float v = analogRead(A0) / 1023.0f * batScale;
  if (first) { batV = v; first = false; } else batV += (v - batV) * 0.15f;   // shanto kora
  batOk = batV > 2.8f;                                // tar na thakle A0 prai 0
  batPct = batOk ? lipoPct(batV) : -1;
  if (!batOk) return;
  // Telegram: 20% ar 10% e ekbar kore
  if (batPct <= 20 && batLastNotify > 20) { tgNotify(F("🔋 PetBot er battery 20%! Charge diye dao 🔌")); batLastNotify = 20; }
  if (batPct <= 10 && batLastNotify > 10) { tgNotify(F("🪫 Battery matro 10%! Ekhoni charge dao, na hole ghumiye porbo 😴")); batLastNotify = 10; }
  if (batPct >= 40) batLastNotify = 101;
  // 15% er niche: 10 min por por "চার্জ দাও!"
  if (batPct <= 15 && (long)(now - batNextWarn) >= 0 && mood == NORMAL && !gameMode && !clockMode && !messageActive()) {
    batNextWarn = now + 600000UL;
    startScene(FS_BATTERY, SAD, BN_BAT_LOW, 6000);
    PLAY(S_BAT_LOW);
  }
}

void showBattery() {
  if (gameMode) gameExit();
  clockMode = false; msgUntil = 0; qrUntil = 0;
  batShowUntil = millis() + 10000UL;
  lastActivity = now;
  PLAY(S_TICK);
  Serial.printf_P(PSTR("Battery: %d%% (%.2f V)\n"), batPct, batV);
}

// Battery chhobi: (x,y) theke w x h, pct 0-100 (-1 = jana nei)
void drawBatteryIcon(int x, int y, int w, int h, int pct, bool blink) {
  display.drawRoundRect(x, y, w, h, 3, SSD1306_WHITE);
  display.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 2, SSD1306_WHITE);
  display.fillRect(x + w, y + h / 3, 4, h / 3, SSD1306_WHITE);           // matha
  if (pct < 0) {
    display.setTextSize(2); display.setCursor(x + w / 2 - 5, y + h / 2 - 7); display.print('?'); display.setTextSize(1);
    return;
  }
  int fw = (w - 8) * pct / 100;
  if (!blink || (millis() / 400) % 2) display.fillRect(x + 4, y + 4, max(fw, pct > 0 ? 2 : 0), h - 8, SSD1306_WHITE);
  for (int k = 1; k < 4; k++) display.drawFastVLine(x + 4 + (w - 8) * k / 4, y + 4, h - 8, SSD1306_BLACK);   // bhag
}

void drawBatteryScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  if (!batOk) {
    drawBandBitmap(BN_BAT_NONE);                       // তার লাগানো নেই
    drawBatteryIcon(6, 24, 56, 30, -1, false);
    display.setTextSize(1);
    display.setCursor(72, 26); display.print("LiPo B+");
    display.setCursor(72, 37); display.print("-> 150k");
    display.setCursor(72, 48); display.print("-> A0");
    display.display();
    return;
  }
  bool low = batPct <= 15;
  drawBandBitmap(low ? BN_BAT_LOW : BN_BAT);           // ব্যাটারি / চার্জ দাও!
  drawBatteryIcon(4, 24, 54, 30, batPct, low);
  char b[8];
  snprintf(b, sizeof(b), "%d", batPct);
  bool big = batPct < 100;                             // "100" boro font e aNte na
  const GFXfont* bf = big ? &FreeSansBold18pt7b : &FreeSansBold12pt7b;
  int w = advanceOf(big ? FreeSansBold18pt7bGlyphs : FreeSansBold12pt7bGlyphs, b);
  int x = 66 + (60 - (w + 12)) / 2;
  printFont(bf, x, 47, b);
  printFont(&FreeSansBold9pt7b, x + w + 1, 47, "%");
  snprintf(b, sizeof(b), "%.2fV", batV);
  display.setTextSize(1);
  display.setCursor(70 + (52 - (int)strlen(b) * 6) / 2, 55);
  display.print(b);
  display.display();
}

// Low battery drishyo (face er bodole)
void drawBatteryScene() {
  drawBatteryIcon(30, 24, 60, 30, max(batPct, 0), true);
  char b[6]; snprintf(b, sizeof(b), "%d%%", max(batPct, 0));
  display.setTextSize(1);
  display.setCursor(64 - (int)strlen(b) * 3, 57);
  display.print(b);
}

String tgBatteryText(const String& raw) {
  String a = tgArgRaw(raw); a.toLowerCase();
  if (a == "full") {                                  // charge full (4.20V): ekhon er pora = 4.20
    float raw0 = batV / batScale;
    if (raw0 < 0.3f) return F("🤔 A0 te tar pai ni. LiPo B+ -> 150k -> A0 lagao");
    batScale = 4.20f / raw0; saveBat();
    return String(F("✅ Thik kora holo: ekhon full = 4.20V (scale ")) + String(batScale, 2) + ')';
  }
  if (a.startsWith("cal")) {                          // /battery cal 3.95 (multimeter er man)
    float want = a.substring(3).toFloat();
    float raw0 = batV / batScale;
    if (want < 2.5f || want > 5.0f || raw0 < 0.3f) return F("🤔 Lekho: /battery cal 3.95 (multimeter e battery te ja dekhay)");
    batScale = want / raw0; saveBat();
    return String(F("✅ Thik kora holo: ")) + String(want, 2) + F("V");
  }
  if (!batOk) return F("🔋 Battery er tar lagano nei.\nLiPo er + (B+) theke 150kΩ resistor diye NodeMCU er A0 te lagao. Tarpor /battery");
  String s = batPct <= 15 ? F("🪫 Battery: ") : F("🔋 Battery: ");
  s += batPct; s += F("% ("); s += String(batV, 2); s += F(" V)\n");
  for (int i = 0; i < 10; i++) s += (i < (batPct + 5) / 10) ? "🟩" : "⬜";
  if (batPct <= 15) s += F("\n⚠️ Charge diye dao!");
  s += F("\n\nMultimeter er sathe na mille: /battery cal 3.95 ba full charge e /battery full");
  return s;
}

// ===========================================================
//  ©️ Original credit pahara (GPL-3.0 + NOTICE er shorto)
//  credits.h er lekha ba GitHub QR bodlale, chalu howar shomoy original credit dekhabe.
// ===========================================================
#define CREDIT_SIG 0x97D039FAUL
static const uint8_t CREDIT_ORIG[] PROGMEM = {
  0x0A, 0x3F, 0x2E, 0x18, 0x35, 0x2E, 0x7A, 0x38, 0x23, 0x7A, 0x1B, 0x36, 0x77, 0x17, 0x3B, 0x37, 0x2F, 0x34, 0x26, 0x3D,
  0x33, 0x2E, 0x32, 0x2F, 0x38, 0x74, 0x39, 0x35, 0x37, 0x75, 0x63, 0x63, 0x63, 0x37, 0x3B, 0x37, 0x2F, 0x34, 0x63, 0x63, 0x63 };
bool creditsOk = true;

uint32_t fnv1a(uint32_t h, const char* s) {
  while (*s) { h ^= (uint8_t)*s++; h *= 16777619UL; }
  return h;
}

void checkCredits() {
  uint32_t h = 2166136261UL;
  static const char* const parts[] = { CREDIT_TITLE, CREDIT_LINE1, CREDIT_BY, CREDIT_AUTHOR,
                                       CREDIT_PROJECT, CREDIT_YEAR, CREDIT_GITHUB, CREDIT_URL };
  for (const char* p : parts) h = fnv1a(h, p);
  for (int r = 0; r < QR_SIZE; r++) {
    uint32_t v = pgm_read_dword(&QR_ROWS[r]);
    for (int k = 0; k < 4; k++) { h ^= (v >> (8 * k)) & 0xFF; h *= 16777619UL; }
  }
  h ^= QR_SIZE; h *= 16777619UL;
  creditsOk = (h == CREDIT_SIG);
  if (creditsOk) return;

  char buf[48];
  int n = sizeof(CREDIT_ORIG);
  for (int i = 0; i < n; i++) buf[i] = pgm_read_byte(&CREDIT_ORIG[i]) ^ 0x5A;
  buf[n] = 0;
  char* line2 = strchr(buf, '|');
  if (line2) *line2++ = 0;
  Serial.print(F("Credits modified! Original: ")); Serial.print(buf); Serial.print(F(" - ")); Serial.println(line2 ? line2 : "");
  for (int k = 0; k < 60; k++) {                        // 6 second
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    const char* t = "CREDITS MODIFIED";
    display.setCursor((SCREEN_W - (int)strlen(t) * 6) / 2, 4); display.print(t);
    display.setCursor(4, 22); display.print("Original:");
    display.setCursor(4, 34); display.print(buf);
    if (line2) { display.setCursor(4, 46); display.print(line2); }
    if ((k / 5) % 2) display.drawRect(0, 18, SCREEN_W, 46, SSD1306_WHITE);
    display.display();
    delay(100);
  }
}

// ===========================================================
//  📱 Phone page er shahajjo (mode, clock screen, timer, khela, info)
// ===========================================================
String webModeName() { return gameMode ? F("game") : (clockMode ? F("clock") : F("normal")); }

String webSetMode(const String& m) {
  closeOverlays();
  if (m == "game") { if (!gameMode) gameEnter(); return F("🎮 Game mode"); }
  if (gameMode) gameExit();
  if (m == "clock") { if (!clockMode) setClockMode(true); return F("⏰ Clock mode"); }
  if (clockMode) setClockMode(false);
  cmdMood(HAPPY);
  return F("😊 Normal mode");
}

String webClockView(int v) {
  if (v < 0 || v >= CV_COUNT) return F("bhul screen");
  closeOverlays();
  if (gameMode) gameExit();
  if (!clockMode) setClockMode(true);
  clockView = v; viewChangedAt = now; timerAlarm = false;
  PLAY(S_TICK);
  return String(F("⏰ ")) + (v == 0 ? "HOME" : CV_TITLE[v]);
}

String webTimerAction(const String& a) {
  if (a == "toggle") { timerAlarm = false; timerToggle(); return timerRunning ? F("▶️ cholche") : F("⏸️ thamlo"); }
  if (a == "reset")  { timerAlarm = false; timerReset(); return F("⏹️ reset"); }
  if (a == "add")    {
    if (gameMode) gameExit();
    if (!clockMode) setClockMode(true);
    clockView = CV_TIMER; viewChangedAt = now; timerAddMinute();
    return F("+1 min");
  }
  return F("?");
}

String webGameStart(int id) {
  if (id < 1 || id >= G_EXIT) return F("bhul khela");
  if (!gameMode) gameEnter();
  gameStart(id);
  gLastInput = now;
  return F("🎮 robot e khela shuru!");
}

String webToggleAzan() {
  prayerFlags ^= 1;
  savePrayerCfg();
  return (prayerFlags & 1) ? F("🔔 Telegram namaz alert chalu") : F("🔕 Telegram namaz alert bondho");
}

String webStatusExtra() {
  String j = F(",\"mode\":\""); j += webModeName();
  j += F("\",\"view\":"); j += clockMode ? (int)clockView : -1;
  j += F(",\"game\":"); j += gameMode ? (int)gameId : -1;
  unsigned long left = timerRemaining();
  j += F(",\"tm\":"); j += (left + 999) / 1000;
  j += F(",\"tmRun\":"); j += timerRunning ? 1 : 0;
  j += F(",\"tmAlarm\":"); j += timerAlarm ? 1 : 0;
  j += F(",\"h24\":"); j += use24 ? 1 : 0;
  j += F(",\"bat\":"); j += batPct;
  j += F(",\"batV\":"); j += String(batV, 2);
  return j;
}

String webInfoJson() {
  String j = F("{\"ip\":\""); j += WiFi.localIP().toString();
  j += F("\",\"ssid\":\""); j += jsonEsc(WiFi.SSID());
  j += F("\",\"rssi\":"); j += WiFi.RSSI();
  j += F(",\"up\":"); j += millis() / 60000UL;
  j += F(",\"heap\":"); j += ESP.getFreeHeap();
  j += F(",\"city\":\"" CITY_NAME_BN "\",\"credits\":"); j += creditsOk ? 1 : 0; j += F(",\"azan\":"); j += (prayerFlags & 1) ? 1 : 0;
  j += F(",\"rec\":{\"react\":"); j += reactBest;
  j += F(",\"catch\":"); j += catchBest;
  j += F(",\"flap\":"); j += flapBest;
  j += F(",\"simon\":"); j += simBest;
  j += F(",\"hole\":"); j += holeBest;
  j += F(",\"rps\":\""); j += rpsScoreYou; j += '-'; j += rpsScoreMe; j += F("\"}");
  if (timeValid()) {
    time_t t = time(nullptr);
    struct tm tmv = *localtime(&t);
    int d, m, y, hd, hm, hy;
    banglaDate(&tmv, d, m, y);
    hijriDate(&tmv, hd, hm, hy);
    j += F(",\"bn\":\""); j += bnDigitsUtf(d); j += ' '; j += pgmField(BM_UTF, m); j += ' '; j += bnDigitsUtf(y); j += F(" বঙ্গাব্দ");
    j += F("\",\"sea\":\""); j += pgmField(SEA_UTF, m / 2);
    j += F("\",\"hj\":\""); j += bnDigitsUtf(hd); j += ' '; j += pgmField(HM_UTF, hm - 1); j += ' '; j += bnDigitsUtf(hy); j += F(" হিজরি\"");
    int sp = todaySpecial();
    j += F(",\"sp\":\""); if (sp >= 0) j += SPDAYS[sp].nm; j += '"';
    int days = 0, nx = nextSpecial(days);
    j += F(",\"next\":\""); if (nx >= 0) j += SPDAYS[nx].nm; j += F("\",\"nextDays\":"); j += days;
    if (prEnsure()) {
      j += F(",\"ram\":"); j += prRamadan ? 1 : 0;
      j += F(",\"pr\":[");
      for (int k = 0; k < PR_N; k++) {
        if (k) j += ',';
        j += F("[\""); j += pgmField(PR_UTF, (k == PR_DHUHR && prFriday) ? 8 : k);
        j += F("\",\""); j += bnTimeUtf(prMin[k]); j += F("\","); j += prMin[k]; j += ']';
      }
      j += F("],\"sehri\":\""); j += bnTimeUtf(prMin[PR_FAJR] - 2);
      j += F("\",\"iftar\":\""); j += bnTimeUtf(prMin[PR_MAGH]);
      j += F("\",\"nowMin\":"); j += nowMinute();
    }
  }
  j += '}';
  return j;
}

void drawClock() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  // Notun view dan dik theke slide kore ashe
  int xoff = 0;
  unsigned long since = now - viewChangedAt;
  if (since < 250) {
    float k = since / 250.0f;
    k = 1 - (1 - k) * (1 - k);
    xoff = (int)(SCREEN_W * (1 - k));
  }

  if (timerAlarm) {                                    // timer shesh: screen jhilik dey
    drawTimerAlarm();
    display.display();
    return;
  }

  bool needTime = clockView == CV_HOME || clockView == CV_DATE || clockView == CV_DAY ||
                  clockView == CV_BANGLA || clockView == CV_HIJRI || clockView == CV_PRAYER;
  if (needTime && !timeValid()) {
    drawLabel(clockView == CV_HOME ? "CLOCK" : CV_TITLE[clockView], xoff);
    drawSmallMsg(&BN_NO_TIME, &BN_CHECK_WIFI, xoff);
    display.display();
    return;
  }

  time_t t = time(nullptr);
  struct tm* tm = localtime(&t);

  if (clockView != CV_HOME && clockView != CV_BANGLA && clockView != CV_HIJRI && clockView != CV_PRAYER)
    drawLabel(CV_TITLE[clockView], xoff);                              // holud: title

  switch (clockView) {
    case CV_HOME:    drawHome(tm, t, xoff);     break;
    case CV_DATE:    drawViewDate(tm, xoff);    break;
    case CV_BANGLA:  drawViewBangla(tm, xoff);  break;
    case CV_HIJRI:   drawViewHijri(tm, xoff);   break;
    case CV_PRAYER:  drawViewPrayer(tm, xoff);  break;
    case CV_DAY:     drawViewDay(tm, xoff);     break;
    case CV_WEATHER: drawViewWeather(xoff);     break;
    case CV_ROOM:    drawViewRoom(xoff);        break;
    case CV_TIMER:   drawViewTimer(xoff);       break;
  }

  display.display();
}

// ---------- Weather effect ----------
void drawRain(int count, int speed) {                  // brishtir fota pore
  for (int i = 0; i < count; i++) {
    int x = (i * 37 + 11) % SCREEN_W;
    int y = BAND_H + (int)((now / speed + i * 23) % (SCREEN_H - BAND_H));
    display.drawLine(x, y, x - 2, y + 4, SSD1306_WHITE);
  }
}

void drawSnow(int count) {                             // borof / thanda bhap
  for (int i = 0; i < count; i++) {
    int x = (i * 29 + 7 + (int)(sinf(now / 400.0f + i) * 3) + SCREEN_W) % SCREEN_W;
    int y = BAND_H + (int)((now / 45 + i * 17) % (SCREEN_H - BAND_H));
    display.fillRect(x, y, 2, 2, SSD1306_WHITE);
  }
}

void drawSunglasses(int lx, int rx, int cy) {          // 😎
  const int w = 36, h = 20;
  display.fillRoundRect(lx - w / 2, cy - h / 2, w, h, 6, SSD1306_WHITE);
  display.fillRoundRect(rx - w / 2, cy - h / 2, w, h, 6, SSD1306_WHITE);
  display.fillRect(lx + w / 2 - 1, cy - h / 2 + 2, rx - lx - w + 2, 3, SSD1306_WHITE);   // bridge
  display.drawLine(lx - w / 2, cy - h / 2 + 3, lx - w / 2 - 6, cy - h / 2, SSD1306_WHITE);
  display.drawLine(rx + w / 2, cy - h / 2 + 3, rx + w / 2 + 6, cy - h / 2, SSD1306_WHITE);
  int g = (int)((now / 30) % 90) - 20;                 // kacher upor jhilik
  for (int k = 0; k < 3; k++) {
    display.drawLine(lx - w / 2 + g + k, cy + h / 2, lx - w / 2 + g + 8 + k, cy - h / 2, SSD1306_BLACK);
    display.drawLine(rx - w / 2 + g + k, cy + h / 2, rx - w / 2 + g + 8 + k, cy - h / 2, SSD1306_BLACK);
  }
}

void drawUmbrella(int cx, int cy) {
  display.fillCircle(cx, cy - 1, 6, SSD1306_WHITE);
  display.fillRect(cx - 7, cy - 1, 15, 8, SSD1306_BLACK);     // nicher ordhek kete chhata
  display.drawFastVLine(cx, cy - 1, 7, SSD1306_WHITE);        // daant
  display.drawPixel(cx - 1, cy + 6, SSD1306_WHITE);
  display.drawPixel(cx - 2, cy + 5, SSD1306_WHITE);
}

void drawSnowflake(int cx, int cy) {
  display.drawLine(cx - 5, cy, cx + 5, cy, SSD1306_WHITE);
  display.drawLine(cx - 3, cy - 4, cx + 3, cy + 4, SSD1306_WHITE);
  display.drawLine(cx - 3, cy + 4, cx + 3, cy - 4, SSD1306_WHITE);
}

void drawBolt(int cx, int cy) {
  display.fillTriangle(cx + 2, cy - 7, cx - 3, cy + 1, cx + 1, cy + 1, SSD1306_WHITE);
  display.fillTriangle(cx - 1, cy - 1, cx + 3, cy - 1, cx - 2, cy + 7, SSD1306_WHITE);
}

// Holud band: bame icon, dane reminder lekha scroll kore
// Weather mood er Bangla reminder (chhobi)
const BnText* reminderBn(Mood m) {
  switch (m) {
    case COLD:   return &BN_R_COLD;     // ঠান্ডা! জ্যাকেট পরে নাও
    case HOT:    return &BN_R_HOT;      // গরম! বেশি করে পানি খাও
    case RAINY:  return &BN_R_RAIN;     // বৃষ্টি! ছাতা নিয়ে যাও
    case STORMY: return &BN_R_STORM;    // ঝড়! বাইরে যেও না
    case SUNNY:  return &BN_R_SUNNY;    // রোদ! সানগ্লাস পরে নাও
    default:     return nullptr;
  }
}

void drawReminderBand(const BnText& bn) {
  // Shamne temperature (18°C), tarpor Bangla reminder - shob eksathe scroll kore
  char num[8] = "";
  int tw = 0, nw = 0;
  if (weatherOk) {
    snprintf(num, sizeof(num), "%d", (int)roundf(weatherTemp));
    nw = advanceOf(FreeSansBold9pt7bGlyphs, num);
    tw = nw + 6 + advanceOf(FreeSansBold9pt7bGlyphs, "C") + 8;
  }
  bool en = langEn && enStr(bn);
  int bw = en ? advanceOf(FreeSansBold9pt7bGlyphs, enStr(bn)) : bn.w;
  int w = tw + bw;
  int span = SCREEN_W - 20 + w;
  int x = SCREEN_W - (int)(((now - moodSince) / 25) % span);
  display.setTextColor(SSD1306_WHITE);
  if (tw) {
    printFont(&FreeSansBold9pt7b, x, 13, num);
    display.drawCircle(x + nw + 2, 3, 2, SSD1306_WHITE);     // degree
    printFont(&FreeSansBold9pt7b, x + nw + 6, 13, "C");
  }
  if (en) printFont(&FreeSansBold9pt7b, x + tw, 13, enStr(bn));
  else    display.drawBitmap(x + tw, (BAND_H - bn.h) / 2, bn.bmp, bn.w, bn.h, SSD1306_WHITE);

  display.fillRect(0, 0, 19, BAND_H, SSD1306_BLACK);  // icon er jayga
  switch (mood) {
    case COLD:   drawSnowflake(9, 8);     break;
    case HOT:    drawSun(9, 8, 3, 1);     break;
    case RAINY:  drawUmbrella(9, 8);      break;
    case STORMY: drawBolt(9, 8);          break;
    case SUNNY:  drawSun(9, 8, 3, 1);     break;
    default: break;
  }
}

// Bajj porle puro screen ek jhalok ulte jay
void updateLightning() {
  static bool inverted = false;
  bool flash = !clockMode && mood == STORMY && ((now - moodSince) % 2500) < 120;
  if (flash != inverted) {
    display.invertDisplay(flash);
    inverted = flash;
  }
}

// Holud e Bangla chhobi: dhorle majhe, na dhorle aste aste scroll
void drawBandBitmap(const BnText& b) {
  if (langEn && enStr(b)) {                                // English mode
    display.setTextColor(SSD1306_WHITE);
    int w = advanceOf(FreeSansBold9pt7bGlyphs, enStr(b));
    int len = strlen(enStr(b));
    if (w <= SCREEN_W - 2) {
      drawLabel(enStr(b), 0);
    } else if (len * 6 <= SCREEN_W) {
      display.setTextSize(1);
      display.setCursor((SCREEN_W - len * 6) / 2, 4);
      display.print(enStr(b));
    } else {
      long over = w - SCREEN_W + 4;
      long t = (long)(now - moodSince) - 800;
      if (t < 0) t = 0;
      long dx = (t * 40 / 1000) % (over * 2 + 40);
      if (dx > over) dx = over;
      printFont(&FreeSansBold9pt7b, 2 - (int)dx, 13, enStr(b));
    }
    return;
  }
  int x = (SCREEN_W - (int)b.w) / 2;
  if (b.w > SCREEN_W) {
    long over = b.w - SCREEN_W + 4;
    long t = (long)(now - moodSince) - 800;
    if (t < 0) t = 0;
    long dx = (t * 40 / 1000) % (over * 2 + 40);    // shesh porjonto giye abar shuru
    if (dx > over) dx = over;
    x = 2 - (int)dx;
  }
  display.drawBitmap(x, (BAND_H - b.h) / 2, b.bmp, b.w, b.h, SSD1306_WHITE);
}

// Lomba lyric: prothome shuru-ta dekhay, tarpor aste aste bame shore shesh porjonto
int lyricScrollX(int w) {
  long over = w - SCREEN_W + 4;
  long t = (long)(now - lyricSince) - 600;
  if (t < 0) t = 0;
  long dx = t * 45 / 1000;
  if (dx > over) dx = over;
  return 2 - (int)dx;
}

// Karaoke: holud e ekhoner lyric line (English font ba Bangla chhobi)
void drawLyric(const Lyric& L, int idx) {
  if (idx != lyricShown) { lyricShown = idx; lyricSince = now; }
  display.setTextColor(SSD1306_WHITE);
  if (L.bn) {
    const BnText& b = *L.bn;
    int x = (b.w <= SCREEN_W) ? (SCREEN_W - b.w) / 2 : lyricScrollX(b.w);
    display.drawBitmap(x, (BAND_H - b.h) / 2, b.bmp, b.w, b.h, SSD1306_WHITE);
    return;
  }
  int w = advanceOf(FreeSansBold9pt7bGlyphs, L.en);
  int len = strlen(L.en);
  if (w <= SCREEN_W - 2) {
    drawLabel(L.en, 0);                                // boro sundor font
  } else if (len * 6 <= SCREEN_W) {
    display.setTextSize(1);                            // ektu lomba: choto font e puro line
    display.setCursor((SCREEN_W - len * 6) / 2, 4);
    display.print(L.en);
  } else {
    printFont(&FreeSansBold9pt7b, lyricScrollX(w), 13, L.en);   // aro lomba: scroll
  }
}

// Holud band (upor er 16 pixel)
void drawBand() {
  display.fillRect(0, 0, SCREEN_W, BAND_H, SSD1306_BLACK);

  if (bandBnOverride && bandBnOverride == sceneBand && !festActive()) {   // drishyo thamche (touch): lekha-o bad
    bandBnOverride = nullptr; sceneBand = nullptr;
  }
  if (bandBnOverride) {                                // welcome er Bangla message
    if ((long)(now - bandBnUntil) < 0) { drawBandBitmap(*bandBnOverride); return; }
    bandBnOverride = nullptr;
  }

  if (bandOverride) {                                  // funny shobdo er lekha (wah wah..., TA-DA!)
    if ((long)(now - bandOverrideUntil) < 0) { drawLabel(bandOverride, 0); return; }
    bandOverride = nullptr;
  }

  switch (mood) {
    case SNEEZE: {
      unsigned long t = now - moodSince;
      drawLabel(t < 1300 ? "a... a..." : (t < 1800 ? "ACHOO!" : "hehe"), 0);
      break;
    }

    // Keu na khelle er golpo (Bangla)
    case PLEAD:   drawBandBitmap(BN_I_PLAY);  break;  // আমার সাথে খেলবে?
    case ANNOYED: drawBandBitmap(BN_I_UFF);   break;  // উফ্! কেউ খেলে না...
    case BORED:   drawBandBitmap(BN_I_BORED); break;  // একা একা বোর হচ্ছি...
    case ANGRY:
      if (idleAngry) { drawBandBitmap(BN_I_ARI); break; }   // আড়ি! কথা নেই!
      drawLabel(MOOD_LABEL[mood], 0);
      break;

    case MUSIC: {                                     // holud e lyrics (karaoke), na thakle gaan er naam
      const Song& sg = SONGS[songIdx < 0 ? 0 : songIdx];
      if (sg.lyr && sndNotes == sg.notes) {
        unsigned long el = now - songStartAt;
        int cur = -1;
        for (int i = 0; i < sg.lyrN; i++) {
          bool started = sg.byMs ? (el >= sg.lyr[i].at) : (sndIdx > sg.lyr[i].at);
          if (started) cur = i; else break;
        }
        // English mode e Bangla lyrics er bodole gaan er naam
        if (cur >= 0 && !(langEn && sg.lyr[cur].bn)) { drawLyric(sg.lyr[cur], cur); break; }
      }
      if (sg.bn && !langEn) {                         // Bangla naam (chhobi)
        const BnText& b = *sg.bn;
        int y = (BAND_H - b.h) / 2;
        if (b.w <= 104) {
          display.drawBitmap((SCREEN_W - b.w) / 2, y, b.bmp, b.w, b.h, SSD1306_WHITE);
          drawNoteIcon(4, 12);
          drawNoteIcon(119, 12);
        } else if (sg.anthem) {                       // jatiyo shongeet: majhe sthir
          display.drawBitmap((SCREEN_W - b.w) / 2, y, b.bmp, b.w, b.h, SSD1306_WHITE);
        } else {
          int x = SCREEN_W - (int)(((now - moodSince) / 30) % (SCREEN_W + b.w));
          display.drawBitmap(x, y, b.bmp, b.w, b.h, SSD1306_WHITE);
        }
        break;
      }
      const char* nm = sg.name;
      int w = advanceOf(FreeSansBold9pt7bGlyphs, nm);
      display.setTextColor(SSD1306_WHITE);
      if (w <= 104) {
        drawLabel(nm, 0);
        drawNoteIcon(4, 12);
        drawNoteIcon(119, 12);
      } else {                                        // lomba naam: scroll kore
        int x = SCREEN_W - (int)(((now - moodSince) / 30) % (SCREEN_W + w));
        printFont(&FreeSansBold9pt7b, x, 13, nm);
      }
      break;
    }

    case COLD: case HOT: case RAINY: case STORMY: case SUNNY:
      drawReminderBand(*reminderBn(mood));
      break;

    case LOVE: {                                      // ♥  Love you  ♥
      int beat = (int)((now / 300) % 2);
      drawLabel("Love you", 0);
      drawHeart(14, 7, 5 + beat);
      drawHeart(114, 7, 5 + beat);
      break;
    }

    case SLEEP:
      drawSleepBand();
      break;

    case CONFUSED:
      drawLabel(MOOD_LABEL[mood], (int)(sinf(now / 200.0f) * 20));
      break;

    case EXCITED: {
      int s = ((now / 150) % 2) ? 4 : 2;
      drawSparkle(12, 8, s);
      drawSparkle(116, 8, 6 - s);
      drawLabel(MOOD_LABEL[mood], 0);
      break;
    }

    default:
      drawLabel(MOOD_LABEL[mood], 0);
      break;
  }
}

// Jatiyo shongeet er shomoy: bataashe ure jawa Bangladesh er potaka (shobuj = jola, lal britto = kalo)
void drawAnthemScreen() {
  const int fx = 40, fy = 22, fw = 60, fh = 34;
  display.fillRect(fx - 3, fy - 3, 2, SCREEN_H - fy + 3, SSD1306_WHITE);   // dondo
  display.fillCircle(fx - 2, fy - 4, 2, SSD1306_WHITE);                    // dondo er matha
  for (int x = 0; x < fw; x++) {                                           // dheu kheye ure
    int off = (int)roundf(sinf(x * 0.15f - now / 220.0f) * 2.0f * x / fw);
    display.drawFastVLine(fx + x, fy + off, fh, SSD1306_WHITE);
  }
  int cx = fx + fw * 9 / 20;                                               // britto: 9/20 dur e
  int coff = (int)roundf(sinf((cx - fx) * 0.15f - now / 220.0f) * 2.0f * (cx - fx) / fw);
  display.fillCircle(cx, fy + fh / 2 + coff, fw / 5, SSD1306_BLACK);
}

void drawFrame() {
  display.clearDisplay();

  if (anthemPlaying()) {                               // 🇧🇩 jatiyo shongeet: shudhu potaka, shomman e
    drawAnthemScreen();
    drawBand();
    display.display();
    return;
  }
  if (festActive()) {                                  // 🎉 bishesh din er drishyo (potaka / minar / chand ...)
    drawFestScene();
    drawBand();
    display.display();
    return;
  }

  bool shiver = mood == SCARED || mood == COLD || mood == STORMY;
  int jx = shiver ? (int)random(-1, 2) : 0;
  int lx  = LEFT_CX  + (int)roundf(eyeL.x) + jx;    // protita chokh nijer jaygay
  int rx  = RIGHT_CX + (int)roundf(eyeR.x) + jx;
  int cyL = EYE_CY   + (int)roundf(eyeL.y);
  int cyR = EYE_CY   + (int)roundf(eyeR.y);
  int cy  = (cyL + cyR) / 2;
  float w = EYE_W * scale;
  float h = EYE_H * scale * openness * blinkK;

  // 3D: je dike takacche shei diker chokh boro, onnota choto
  float n  = constrain((eyeL.x + eyeR.x) / (2.0f * MAX_LX), -1.0f, 1.0f);
  float sL = 1.0f - 0.14f * n;
  float sR = 1.0f + 0.14f * n;

  // 0) Pechone weather effect
  if (mood == RAINY)  drawRain(10, 9);
  if (mood == STORMY) drawRain(16, 6);
  if (mood == COLD)   drawSnow(9);

  // 1) Mukh
  drawMouth();

  // 2) Chokh
  if (mood == SUNNY) {
    drawSunglasses(lx, rx, cy);
  } else if (mood == DIZZY) {
    float ph = now / 80.0f;
    drawSpiral(lx, cyL, ph);
    drawSpiral(rx, cyR, ph + PI);
  } else if (mood == SNEEZE && now - moodSince >= 1300 && now - moodSince < 1800) {
    int j = (int)random(-2, 3);                        // 🤧 ACHOO! chokh kuchke > <, jhakuni
    drawChevron(lx + j, cyL, 1);
    drawChevron(rx + j, cyR, -1);
    for (int i = 0; i < 7; i++)                        // hanchi r chita 💦
      display.drawPixel(SCREEN_W / 2 - 14 + (int)random(29), MOUTH_Y + 5 + (int)random(5), SSD1306_WHITE);
  } else if (mood == LAUGH) {
    drawChevron(lx, cyL, 1);
    drawChevron(rx, cyR, -1);
  } else if (mood == SLEEP && openness < 0.25f) {      // ◡ ◡ ghumonto chokh, nishash e upor-niche + Zzz
    int by = (int)(sleepBreath() * 1.5f) + 3;
    drawSleepEye(lx, cyL + by, (int)(EYE_W * 0.50f));
    drawSleepEye(rx, cyR + by, (int)(EYE_W * 0.50f));
    drawSleepZ(rx + (int)(EYE_W / 2) + 4, cyR + 4);
  } else {
    float wl = w * sL, wr = w * sR;
    float hl = h * sL, hr = h * sR;
    if (mood == CONFUSED) { hl *= 1.15f; hr *= 0.70f; }
    if (mood == WINK)     { hr = 3; }
    if (mood == SNEEZE && now - moodSince < 1300) {    // hanchi ashche: chokh aste aste kuchke jay
      float k = 1.0f - 0.75f * (now - moodSince) / 1300.0f;
      hl *= k; hr *= k;
    }
    drawEye(lx, cyL, wl, hl, -1);
    drawEye(rx, cyR, wr, hr, 1);
    if (mood == CRY)    drawTears(lx, rx, cy, w, h);
    if (mood == LOVE) drawFloatingHearts();            // upore uthe jawa heart
    if (mood == WHISTLE) {                             // 😏 bhuru: bam ta nache, dan ta tola (wink er upore)
      int mx = SCREEN_W / 2 + (int)(lookX * 0.6f), my = MOUTH_Y + (int)lookY;
      for (int i = 0; i < 2; i++) {                    // 😗 mukh theke ♪ note pashe bhese jay
        float t = ((now + i * 600UL) % 1200UL) / 1200.0f;
        drawNoteIcon(mx + 12 + (int)(t * 34), my + 4 - (int)(t * 4));
      }
      int wig = (int)(fabsf(sinf(now / 120.0f)) * 3);
      int byL = cyL - (int)(hl / 2) - 3 - wig;
      int byR = cyR - (int)(EYE_H * scale * sR / 2) - 3;
      for (int t = 0; t < 2; t++) {
        display.drawLine(lx - 9, byL + 2 + t, lx + 9, byL - 1 + t, SSD1306_WHITE);
        display.drawLine(rx - 9, byR - 1 + t, rx + 9, byR + 1 + t, SSD1306_WHITE);
      }
    }
    if (mood == MUSIC || mood == HUM) drawFloatingNotes();   // ♪ ♫ bhese uthe
    if (mood == ANGRY) {                               // 💢 + matha theke dhoya
      drawAngerMark(rx + (int)(wr / 2) + 7, cyR - 12);
      drawSteam(cy);
    }
    if (mood == SCARED || mood == HOT)
      drawDrop(rx + (int)(wr / 2) + 6, cyR - 6 + (int)((now / 80) % 6));     // ghaam
    if (mood == HOT)
      drawDrop(lx - (int)(wl / 2) - 6, cyL - 2 + (int)((now / 95) % 6));
  }

  // Khushi / bhalobasa / raag - gal e lojjar dag "///" (raag o cute lage)
  bool blush = mood == HAPPY || mood == LAUGH || mood == EXCITED || mood == WINK ||
               mood == LOVE || mood == ANGRY || mood == MUSIC || mood == PLEAD || mood == WHISTLE;
  if (blush) {
    drawBlush(lx - (int)(EYE_W * sL / 2) + 6, cyL + (int)(EYE_H * scale * sL / 2) + 5);
    drawBlush(rx + (int)(EYE_W * sR / 2) - 6, cyR + (int)(EYE_H * scale * sR / 2) + 5);
  }

  // 3) Holud band
  drawBand();

  display.display();
}

// Upload er thik age: "সিস্টেম আপডেট হচ্ছে / নতুন কিছু আসছে! / একটু অপেক্ষা করো"
// Upload er shomoy ESP theme thake kintu OLED shesh chhobi dhore rakhe - tai ei lekha-i dekha jay.
void showUpdateScreen() {
  sndNotes = nullptr;
  noTone(PIN_BUZZER);
  for (int i = 0; i < 2; i++) { servoArm[i].detach(); armAttached[i] = false; }
  display.invertDisplay(false);

  display.clearDisplay();
  drawBandBitmap(BN_U0);                               // সিস্টেম আপডেট হচ্ছে / System Updating
  drawBn(BN_U1, 18, 0);                                // নতুন কিছু আসছে!
  drawBn(BN_U2, 37, 0);                                // একটু অপেক্ষা করো
  for (int i = 0; i < 5; i++) display.fillCircle(44 + i * 10, 60, 1 + (i == 2), SSD1306_WHITE);
  display.display();

  if (soundOn) {                                       // choto "notun kichu ashche" chime
    tone(PIN_BUZZER, 1047, 90);  delay(110);
    tone(PIN_BUZZER, 1319, 90);  delay(110);
    tone(PIN_BUZZER, 1568, 180); delay(220);
    noTone(PIN_BUZZER);
  }
  Serial.println(F("UPDATE MODE: upload er opekkha..."));
  unsigned long t0 = millis();
  while (millis() - t0 < 120000UL) delay(20);         // 2 min e upload na ele abar chalu
  ESP.restart();
}

// Chalu howar screen: holud e English title, neel e Bangla lekha (+ dorkar hole English naam)
void bootScreen(const char* top, const BnText* a, const BnText* b, const char* extra) {
  display.clearDisplay();
  drawLabel(top, 0);
  if (a) drawBn(*a, (b || extra) ? 18 : 28, 0);
  if (b) drawBn(*b, 38, 0);
  if (extra) {
    display.setTextSize(1);
    display.setCursor((SCREEN_W - (int)strlen(extra) * 6) / 2, 46);
    display.print(extra);
  }
  display.display();
}

// ===========================================================
//  Start screen: shomoy onujayi greeting + naam
// ===========================================================
// Shomoy onujayi Bangla greeting (chhobi) + Serial er jonno English naam
const BnText* greetingBn(const char** en) {
  if (!timeValid()) { *en = "Welcome"; return &BN_G_WELCOME; }      // স্বাগতম
  time_t t = time(nullptr);
  int h = localtime(&t)->tm_hour;
  if (h >= 5  && h < 12) { *en = "Good Morning";   return &BN_G_MORNING;   }   // শুভ সকাল
  if (h >= 12 && h < 15) { *en = "Good Noon";      return &BN_G_NOON;      }   // শুভ দুপুর
  if (h >= 15 && h < 18) { *en = "Good Afternoon"; return &BN_G_AFTERNOON; }   // শুভ বিকাল
  if (h >= 18 && h < 20) { *en = "Good Evening";   return &BN_G_EVENING;   }   // শুভ সন্ধ্যা
  *en = "Good Night";                              return &BN_G_NIGHT;         // শুভ রাত্রি
}

// reveal = naam er koto pixel dekhabe (bam theke dane khule jay)
void drawGreetingFrame(const BnText* greet, int reveal, int heartBeat) {
  display.clearDisplay();
  drawLabel("Hello!", 0);
  drawHeart(14, 8, 5 + heartBeat);                     // holud e duita heart
  drawHeart(114, 8, 5 + heartBeat);
  if (langEn) {                                        // English: Good Morning / Al-Mamun
    display.setTextColor(SSD1306_WHITE);
    int gw = advanceOf(FreeSans9pt7bGlyphs, enStr(*greet));
    printFont(&FreeSans9pt7b, (SCREEN_W - gw) / 2, 33, enStr(*greet));
    int nw = advanceOf(FreeSansBold12pt7bGlyphs, enStr(BN_NAME));
    int nx = (SCREEN_W - nw) / 2;
    printFont(&FreeSansBold12pt7b, nx, 57, enStr(BN_NAME));
    if (reveal < nw) display.fillRect(nx + reveal, 38, nw - reveal + 2, 26, SSD1306_BLACK);
    display.display();
    return;
  }
  drawBn(*greet, 17, 0);                               // শুভ সকাল

  int nx = (SCREEN_W - BN_NAME.w) / 2;                 // আল-মামুন
  int ny = 38 + (24 - (int)BN_NAME.h) / 2;
  display.drawBitmap(nx, ny, BN_NAME.bmp, BN_NAME.w, BN_NAME.h, SSD1306_WHITE);
  if (reveal < BN_NAME.w)
    display.fillRect(nx + reveal, ny, BN_NAME.w - reveal, BN_NAME.h, SSD1306_BLACK);
  display.display();
}

void showGreeting() {
  // WiFi thakle time pawar jonno 3 second porjonto opekkha
  unsigned long t0 = millis();
  while (WiFi.status() == WL_CONNECTED && !timeValid() && millis() - t0 < 3000) delay(100);

  const char* en = "";
  const BnText* greet = greetingBn(&en);
  int nameW = langEn ? advanceOf(FreeSansBold12pt7bGlyphs, enStr(BN_NAME)) : BN_NAME.w;

  // Naam bam theke dane ektu ektu kore lekha hoy, proti dhape choto "tik"
  const int STEP = 8;
  for (int r = 0, i = 0; r < nameW + STEP; r += STEP, i++) {
    drawGreetingFrame(greet, r, 0);
    armDirect(1, 115 + 30 * sinf(millis() / 140.0f));  // dan haat nere "hello" 👋
    if (r > 0 && soundOn) tone(PIN_BUZZER, 1600 + i * 90, 30);
    delay(90);
  }

  // Shesh e chalu hoyar shur + heart dhuk-dhuk
  static const Note tune[] = {{523,90},{659,90},{784,90},{1047,220}};
  for (unsigned int k = 0; k < sizeof(tune) / sizeof(tune[0]); k++) {
    if (soundOn) tone(PIN_BUZZER, tune[k].f, tune[k].d);
    drawGreetingFrame(greet, nameW, k % 2);
    armDirect(1, 115 + 30 * sinf(millis() / 140.0f));
    delay(tune[k].d + 20);
  }
  for (int k = 0; k < 12; k++) {
    drawGreetingFrame(greet, nameW, (k / 2) % 2);
    armDirect(1, 115 + 30 * sinf(millis() / 140.0f));
    delay(125);
  }
  noTone(PIN_BUZZER);
  Serial.print(en);
  Serial.print(F(", "));
  Serial.println(OWNER_NAME);
}

// ===========================================================
//  Setup / loop
// ===========================================================
void setup() {
  Serial.begin(115200);
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.setClock(400000);
  pinMode(PIN_TOUCH, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_TOUCH), touchISR, RISING);   // reaction game er nikhut shomoy
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  loadArmCalib();                                 // age save kora haat er calibrate thakle load
  armsInit();                                     // haat niche aram e (calibrate er age, jate kapuni na lage)
  randomSeed(analogRead(A0) ^ micros());

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println(F("OLED pawa jay ni! Wiring / address (0x3C/0x3D) check koro."));
    while (true) delay(1000);
  }
  display.setTextWrap(false);

  checkCredits();                                 // ©️ original credit thik ache kina
  showSplash();                                   // ✨ PetBot / Designed & Developed by ...
  bootScreen("PetBot", &BN_CALIB1, &BN_CALIB2, nullptr);        // নাড়িও না, ক্যালিব্রেট হচ্ছে...
  i2cScan();
  Serial.print(F("Touch pin (D5): "));
  Serial.println(digitalRead(PIN_TOUCH) == HIGH ? "HIGH (touch hocche?)" : "LOW (thik ache)");
  mpuOk = mpuBegin();
  if (mpuOk) {
    delay(300);
    calibrateGyro();
    Serial.println(F("MPU6050 OK"));
  } else {
    Serial.println(F("MPU6050 pawa jay ni - shudhu eye animation cholbe."));
    bootScreen("No MPU", &BN_NO_SENSOR, &BN_CHECK_WIRE, nullptr);  // সেন্সর পাওয়া যায়নি, তার চেক করো
    delay(1500);
  }

  bool forgetWifi = checkWifiResetGesture();       // touch chepe dhorle WiFi reset
  bootScreen("WiFi...", &BN_CONNECTING, nullptr, nullptr);     // কানেক্ট হচ্ছে...
  wifiBegin(forgetWifi);
  if (!wifiWasConnected) {
    bootScreen("WiFi set", &BN_PHONE_CONNECT, nullptr, AP_NAME);  // ফোনে কানেক্ট করো: PetBot-Setup
    delay(3000);
  } else {                                        // 📱 phone e kon thikana khulte hobe dekhao
    static char ipBuf[20];
    snprintf(ipBuf, sizeof(ipBuf), "%s", WiFi.localIP().toString().c_str());
    bootScreen("WiFi OK", &BN_PHONE, nullptr, ipBuf);              // ফোনে খোলো: 192.168.x.x
    delay(2500);
  }

  printHelp();

  now = millis();
  lastActivity = now;
  nextSaccade = now + 1000;
  showGreeting();                                   // "Good Evening, Al-Mamun"
  now = millis();
  lastActivity = now;
  if (demoMode) startDemo();
  else          startWelcome();                     // "অবশেষে তুমি এলে!" ...
}

void loop() {
  now = millis();
  handleSerial();
  if (shotRequested) sendScreenshot();           // je kono screen (face/clock/message) er shesh chhobi
  wifiLoop();
  if (otaWaiting()) {                             // 📡 WiFi update er opekkha: shudhu update screen
    static unsigned long lastOta = 0;
    if (now - lastOta > 500) { lastOta = now; drawOtaScreen(0, false); }
    return;
  }
  updateWeather();
  handleTouch();
  updateTimer();
  prayerAlerts();                                 // 🕌 namaz / sehri / iftar er shomoy
  schedLoop();                                    // ⏰ alarm, 🌙 ghum, 📝 reminder
  batteryLoop();                                  // 🔋 battery
  updateSound();
  updateLightning();
  updateArms();

  if (messageActive()) {                          // 📩 phone theke asha message
    drawMessage();
    return;
  }
  if (msgUntil) {                                 // message shesh: khushi
    msgUntil = 0;
    setMood(HAPPY, 2000);
  }

  if (screenHoldUntil) {                          // test er jonno screen dhore rakha
    if ((long)(millis() - screenHoldUntil) < 0) return;
    screenHoldUntil = 0;
  }

  if (batShowUntil) {                             // 🔋 battery (8 tap), 10 s ba touch porjonto
    if ((long)(millis() - batShowUntil) < 0 && !gameMode) { drawBatteryScreen(); return; }
    batShowUntil = 0;
  }

  if (qrUntil) {                                  // 🐙 GitHub QR (7 tap), 30 s ba touch porjonto
    if ((long)(millis() - qrUntil) < 0 && !gameMode) { drawQR(); return; }
    qrUntil = 0;
  }

  if (calibMode) {                                // haat calibrate er screen
    display.clearDisplay();
    drawLabel("Arm Calibrate", 0);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    char buf[12];                                 // boro kore: kon servo ar ekhon kon angle
    snprintf(buf, sizeof(buf), "%s  %d", ARM_NAME[calibSel], calibAngle[calibSel]);
    int w = advanceOf(FreeSansBold18pt7bGlyphs, buf);
    printFont(&FreeSansBold18pt7b, (SCREEN_W - w) / 2, 46, buf);
    display.setCursor(4, 55);
    display.printf("down %d   up %d", armDown[calibSel], armUp[calibSel]);
    display.display();
    return;
  }

  if (gameMode) {                                 // 🎮 khela
    gameLoop();
    return;
  }

  if (clockMode) {
    if (mpuOk) mpuRead();                         // ROOM TEMP er jonno
    drawClock();
    return;
  }

  if (demoMode) {
    updateDemo();
  } else {
    updateMoodTimeout();
    updateWelcome();                                   // chalu howar por welcome golpo
    if (mpuOk) {
      if (mpuRead() && !touching) updateBehaviour();   // touch er somoy dhakka dhora hobe na
    } else {
      randomMood();
    }
    weatherReminder();                                 // weather onujayi mood + reminder
    festReminder();                                    // bishesh din: majhe majhe utsob
  }

  updateBlink();
  updateLook();
  animate();
  drawFrame();
}

// Debug: OLED er puro chhobi (1024 byte) hex hishebe Serial e - PC te chhobi banano jay
void sendScreenshot() {
  shotRequested = false;
  const uint8_t* buf = display.getBuffer();
  Serial.print(F("SHOT:"));
  Serial.print(MOOD_NAME[mood]);
  Serial.print(F(":"));
  for (int i = 0; i < SCREEN_W * SCREEN_H / 8; i++) {
    if (buf[i] < 16) Serial.print('0');
    Serial.print(buf[i], HEX);
  }
  Serial.println();
}
