// ###############################################################################
// #                                                                             #
// #   PROJEKT: APRS MONITOR (ESP8266 / OLED)                                    #
// #   AUTOR:   Marcin "Skrętka" (SQ7UTP)                                        #
// #   PODZIĘKOWANIA : Jacek (SP7EZD)                                            #
// #                                                                             #
// #   OPIS PROJEKTU:                                                            #
// #   To jest projekt hobbystyczny typu Open Source.                            #
// #   Został stworzony z myślą o kolegach krótkofalowcach.                      #
// #   Pozwalam na instalację, dowolne przeróbki, modyfikacje i zabawę kodem.    #
// #                                                                             #
// #   PROŚBA AUTORA:                                                            #
// #   Jeśli udostępniasz ten projekt dalej lub wykorzystujesz jego fragmenty,   #
// #   bardzo proszę - zostaw ten nagłówek w nienaruszonym stanie.               #
// #   Szanujmy swoją pracę i czas poświęcony na ten projekt :)                  #
// #                                                                             #
// #   Miłej zabawy i dalekich łączności! 73!                                    #
// #                                                            sq7utp@gmail.com #
// ###############################################################################

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <U8g2lib.h>
#include <time.h>
#include <math.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h> 
#include <WiFiManager.h> 
#include <FS.h>         
#include <LittleFS.h>   
#include <ArduinoJson.h> 

// ===================================
// 0. GRAFIKA EKRANU STARTOWEGO
// ===================================

static const unsigned char image_aprsfi_logo_mid_tr_bits[] = {0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x06,0x00,0x00,0xe0,0x3f,0x00,0x03,0x00,0x00,0xf8,0xff,0x00,0x01,0x00,0x00,0xfc,0xff,0x81,0x00,0x00,0x00,0xfe,0xff,0xc1,0x00,0x00,0x00,0xfe,0xff,0xc3,0x00,0x00,0x00,0xff,0xff,0xc3,0x00,0x00,0x00,0xff,0xff,0x47,0x00,0x00,0x80,0xff,0xff,0x67,0x00,0x00,0xc0,0xff,0xff,0x6f,0x00,0xe0,0xff,0xff,0xff,0xff,0x01,0xf8,0xff,0xff,0xff,0xff,0x03,0xfc,0xff,0xff,0xff,0xff,0x07,0xfe,0xff,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0xff,0xff,0x0f,0xff,0xff,0xff,0xff,0xff,0x1f,0xff,0xff,0xff,0xff,0xff,0x1f,0xff,0xff,0xff,0xff,0xff,0x1f,0xff,0xff,0xff,0xff,0xff,0x1f,0xff,0xff,0xff,0xff,0xff,0x0f,0xfe,0xff,0xff,0xff,0xff,0x0f,0xfc,0xff,0xff,0xff,0xff,0x07,0x00,0xff,0x00,0x00,0xff,0x00,0x00,0x7e,0x00,0x00,0x7e,0x00,0x00,0x38,0x00,0x00,0x3c,0x00};
static const unsigned char image_Layer_3_bits[] = {0x0f,0x00,0x00,0x00,0x00,0x09,0x00,0x00,0x00,0x00,0x0b,0x00,0x00,0x00,0x00,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0x00,0x80,0x0f,0xc0,0x30,0x00,0x60,0x30,0x60,0x60,0x00,0x30,0x60,0x00,0x00,0x00,0x20,0x00};


// ===================================
// 1. KONFIGURACJA SPRZĘTOWA I SIECIOWA
// ===================================

char my_callsign[12] = "'Znak'-X"; 
char aprs_pass[8] = "--";
char my_lat_str[10] = "--";
char my_lon_str[10] = "--";
char beacon_comment[65] = "--";
char aprs_symbol[2] = "L"; 
char aprs_filter_range[5] = "--"; 
const char* APRS_SERVER = "lodz.aprs2.net";
const int APRS_PORT = 14580;
const unsigned long BEACON_INTERVAL = 600000; 
const unsigned long APRS_TIMEOUT = 120000; 
const int MAX_STATIONS = 10; 
const unsigned long SINGLE_STATION_DURATION = 12000; 
const unsigned long SINGLE_STATION_PHASE_DURATION = 4000;  
const int MAX_VISIBLE_ROWS = 4;    
const int STATION_ROW_HEIGHT = 10; 
const int SCROLL_SPEED_MS = 100;   
const int SCROLL_PIXEL_STEP = 1;   

WiFiClient client;
U8G2_SSD1309_128X64_NONAME0_F_4W_SW_SPI u8g2(U8G2_R0, /* clock=*/ D5, /* data=*/ D7, /* cs=*/ D8, /* dc=*/ D6, /* reset=*/ D0);
WiFiManager wifiManager;
ESP8266WebServer server(80); 

// ===================================
// 2. STRUKTURY I ZMIENNE GLOBALNE
// ===================================

bool shouldSaveConfig = false; 

struct APRSStation {
    String callsign;
    float distance; 
    float bearing;  
    int speed;      
    int heading;    
    String comment; 
    unsigned long timestamp;
};

APRSStation stations[MAX_STATIONS];
int lastStationsCount = 0; 

unsigned long lastBeaconMillis = 0;
unsigned long lastPacketMillis = 0; 

int displayMode = 1;
unsigned long singleStationTimer = 0; 
APRSStation currentSingleStation; 

unsigned long lastScrollMillis = 0;
int currentScrollY = 0; 

// ===================================
// 3. FUNKCJE POMOCNICZE WYSWIETLANIA
// ===================================

u8g2_uint_t u8g2_center_x(const char* s) {
  return (u8g2.getDisplayWidth() - u8g2.getStrWidth(s)) / 2;
}

void drawWifiBars(int x, int y, long rssi) {
  int strength = 0;
  if (rssi > -60) strength = 4;
  else if (rssi > -70) strength = 3;
  else if (rssi > -80) strength = 2;
  else if (rssi > -90) strength = 1;

  for (int i = 0; i < 4; i++) {
    if (i < strength) {
      int height = (i + 1) * 2;
      int x_pos = x - 11 + (i * 3); 
      int y_pos = y + (6 - height);
      u8g2.drawBox(x_pos, y_pos, 2, height);
    }
  }
}

void drawBearingArrow(float bearing) {
    const int cx = 102;  
    const int cy = 25;   
    const int radius = 14;  
    const int label_offset = 1; 
    
    u8g2.drawCircle(cx, cy, radius);

    float angleRad = (bearing - 90) * M_PI / 180.0;
    int x2 = cx + (int)(radius * cos(angleRad));
    int y2 = cy + (int)(radius * sin(angleRad));
    int line_end_x = cx + (int)((radius - 3) * cos(angleRad));
    int line_end_y = cy + (int)((radius - 3) * sin(angleRad));
    u8g2.drawLine(cx, cy, line_end_x, line_end_y);
    
    float angle_grot_a = (bearing + 120 - 90) * M_PI / 180.0;
    float angle_grot_b = (bearing - 120 - 90) * M_PI / 180.0;
    
    u8g2.drawTriangle(
        x2, y2, 
        cx + (int)((radius - 5) * cos(angle_grot_a)), 
        cy + (int)((radius - 5) * sin(angle_grot_a)),
        cx + (int)((radius - 5) * cos(angle_grot_b)), 
        cy + (int)((radius - 5) * sin(angle_grot_b))
    );
    
    u8g2.setFont(u8g2_font_5x7_tr); 
    u8g2.setCursor(cx - 3, cy - radius - label_offset); u8g2.print("N");
    u8g2.setCursor(cx - 3, cy + radius + 7); u8g2.print("S");
    u8g2.setCursor(cx - radius - 5, cy + 3); u8g2.print("W");
    u8g2.setCursor(cx + radius + 2, cy + 3); u8g2.print("E");
}

// ===================================
// 3b. FUNKCJA BEZPIECZNEJ KONWERSJI
// ===================================

float safeToFloat(const char* str) {
  String s = str;
  s.replace(',', '.'); 
  return s.toFloat();
}

// ===================================
// 4. FUNKCJE WYSWIETLANIA EKRANÓW
// ===================================

void displaySplashScreen() {
  u8g2.begin(); 
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setBitmapMode(1);
  u8g2.drawXBM(42, 0, 45, 27, image_aprsfi_logo_mid_tr_bits); 
  u8g2.setDrawColor(2); 
  u8g2.drawLine(55, 11, 78, 11);
  u8g2.drawXBM(44, 15, 39, 10, image_Layer_3_bits); 
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_profont17_tr);
  u8g2.drawStr(17, 40, "APRS");
  u8g2.drawStr(53, 51, "Monitor");
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.drawStr(42, 61, "By SQ7UTP");
  u8g2.sendBuffer();
  delay(3000); 
}

void displayProgressScreen(int step, const char* message) {
    u8g2.clearBuffer();
    
    u8g2.setFont(u8g2_font_ncenB10_tr); 
    u8g2.drawStr(u8g2_center_x("INICJALIZACJA"), 15, "INICJALIZACJA");
    
    u8g2.setFont(u8g2_font_ncenB08_tr); 
    u8g2.drawStr(u8g2_center_x(message), 35, message);
    
    int progress_width = 120;
    int progress_x = (u8g2.getDisplayWidth() - progress_width) / 2; 
    int progress_y = 50;
    int progress_height = 8;
    int radius = 3; 

    int fill_amount = (progress_width * step) / 4; 
    
    u8g2.drawRFrame(progress_x, progress_y, progress_width, progress_height, radius);
    
    if (fill_amount > 2) { 
        u8g2.drawRBox(progress_x + 1, progress_y + 1, fill_amount - 2, progress_height - 2, radius - 1);
    }
    
    u8g2.sendBuffer();
}

void displayDataListScreen() {
    u8g2.clearBuffer();
    
    u8g2.setFont(u8g2_font_6x12_tr); 
    
    String title = "APRS Monitor"; 
    u8g2_uint_t titleWidth = u8g2.getStrWidth(title.c_str());
    u8g2_uint_t startX = (u8g2.getDisplayWidth() - titleWidth) / 2;
    u8g2.setCursor(startX, 10);
    u8g2.print(title);
    u8g2.drawHLine(0, 12, 128);

    if (WiFi.status() == WL_CONNECTED) {
        drawWifiBars(126, 2, WiFi.RSSI());
    }

    u8g2.setFont(u8g2_font_6x12_tr); 
    u8g2.setClipWindow(0, 13, 128, 55);

    int maxScrollOffset = 0;
    int totalStationRows = lastStationsCount;
    
    if (totalStationRows > MAX_VISIBLE_ROWS) {
        maxScrollOffset = (totalStationRows - MAX_VISIBLE_ROWS) * STATION_ROW_HEIGHT;
    }

    if (maxScrollOffset > 0) {
        if (millis() - lastScrollMillis > SCROLL_SPEED_MS) {
            currentScrollY += SCROLL_PIXEL_STEP;
            
            if (currentScrollY > maxScrollOffset) {
                currentScrollY = 0; 
            }
            lastScrollMillis = millis();
        }
    } else {
        currentScrollY = 0; 
    }
    
    int y_start = 24 - currentScrollY; 
    
    for (int i = 0; i < totalStationRows; i++) {
        const APRSStation& currentStation = stations[i]; 
        
        String stationInfo = currentStation.callsign + " " + String(currentStation.distance, 1) + "km " + String((int)currentStation.bearing) + "deg";
        
        u8g2.setCursor(0, y_start + (i * STATION_ROW_HEIGHT));
        u8g2.print(stationInfo);
    }
    
    u8g2.setClipWindow(0, 0, 128, 64);
    
    u8g2.drawHLine(0, 56, 128); 
    
    struct tm timeinfo;
    u8g2.setFont(u8g2_font_5x7_tr); 
    
    if(!getLocalTime(&timeinfo)){
      u8g2.setCursor(0, 64); 
      u8g2.print("Brak czasu NTP");
    } else {
      char timeStringBuff[9]; 
      strftime(timeStringBuff, sizeof(timeStringBuff), "%H:%M:%S", &timeinfo); 
      u8g2.setCursor(0, 64); 
      u8g2.print(timeStringBuff);
    }
    
    if(WiFi.status() == WL_CONNECTED){
      String ip = WiFi.localIP().toString();
      u8g2_uint_t ipWidth = u8g2.getStrWidth(ip.c_str());
      u8g2.setCursor(u8g2.getDisplayWidth() - ipWidth, 64); 
      u8g2.print(ip);
    }
    
    u8g2.sendBuffer();
}

void displaySingleStationScreen() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.drawStr(0, 10, "NOWA STACJA:");
    u8g2.setCursor(0, 25);
    u8g2.setFont(u8g2_font_profont12_tr); 
    u8g2.print(currentSingleStation.callsign);

    drawBearingArrow(currentSingleStation.bearing); 
    
    u8g2.setFont(u8g2_font_5x7_tr); 
    unsigned long timeElapsed = millis() - singleStationTimer;
    
    int currentPhase = (timeElapsed / SINGLE_STATION_PHASE_DURATION) % 3; 

    if (currentPhase == 0) {
      u8g2.setCursor(0, 38); 
      u8g2.print("Dist: ");
      u8g2.print(String(currentSingleStation.distance, 1)); 
      u8g2.print(" km");
      u8g2.setCursor(0, 48); 
      u8g2.print("Kier: ");
      u8g2.print(String((int)currentSingleStation.bearing)); 
      u8g2.print(" deg");
      
    } 
    else if (currentPhase == 1) {
      u8g2.setCursor(0, 38);
      u8g2.print("SPD: ");
      int speed_knots = currentSingleStation.speed;
      int speed_kmh = (int)((float)speed_knots * 1.852); 
      u8g2.print(String(speed_kmh));
      u8g2.print(" km/h");
      u8g2.setCursor(0, 48); 
      u8g2.print("HDG: ");
      u8g2.print(String(currentSingleStation.heading));
      u8g2.print(" deg");
    }
    else {
      u8g2.setFont(u8g2_font_5x7_tr);
      u8g2.setCursor(0, 43); 
      u8g2.print("TXT:");
      u8g2.setFont(u8g2_font_4x6_tr);
      String comment = currentSingleStation.comment;
      int max_chars_per_line = 32; 
      
      u8g2.setCursor(0, 50); 
      if (comment.length() > 0) {
          u8g2.print(comment.substring(0, min((int)comment.length(), max_chars_per_line)));
      }

      u8g2.setCursor(0, 57); 
      if (comment.length() > max_chars_per_line) {
          int start = max_chars_per_line;
          u8g2.print(comment.substring(start, min((int)comment.length(), start + max_chars_per_line)));
      }

      u8g2.setCursor(0, 64); 
      if (comment.length() > max_chars_per_line * 2) {
          int start = max_chars_per_line * 2;
          u8g2.print(comment.substring(start, min((int)comment.length(), start + max_chars_per_line)));
      }
    }
    
    u8g2.sendBuffer();
}

// ========================================
// 5. ZAPIS/ODCZYT KONFIGURACJI Z LITTLEFS
// ========================================

void saveConfigCallback () {
  Serial.println("Wymagany zapis konfiguracji.");
  shouldSaveConfig = true;
}

void loadConfig() {
  if (LittleFS.exists("/config.json")) {
    File configFile = LittleFS.open("/config.json", "r");
    if (configFile) {
      size_t size = configFile.size();
      std::unique_ptr<char[]> buf(new char[size]);
      configFile.readBytes(buf.get(), size);
      
      DynamicJsonDocument json(1024);
      DeserializationError error = deserializeJson(json, buf.get());

      if (!error) {
        Serial.println("Wczytano konfiguracje z FS.");
        strcpy(my_callsign, json["callsign"] | my_callsign);
        strcpy(aprs_pass, json["aprspass"] | aprs_pass);
        strcpy(my_lat_str, json["mylat"] | my_lat_str);
        strcpy(my_lon_str, json["mylon"] | my_lon_str);
        strcpy(beacon_comment, json["comment"] | beacon_comment);
        strcpy(aprs_symbol, json["symbol"] | aprs_symbol);
        strcpy(aprs_filter_range, json["filter"] | aprs_filter_range);
      } else {
        Serial.println("Blad parsowania JSON, uzywam domyslnych wartosci.");
      }
      configFile.close();
    }
  }
}

void saveConfig() {
  Serial.println("Zapisywanie konfiguracji do FS...");
  DynamicJsonDocument json(1024);
  
  json["callsign"] = my_callsign;
  json["aprspass"] = aprs_pass;
  json["mylat"] = my_lat_str;
  json["mylon"] = my_lon_str;
  json["comment"] = beacon_comment;
  json["symbol"] = aprs_symbol;
  json["filter"] = aprs_filter_range;

  File configFile = LittleFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Blad otwarcia pliku do zapisu!");
    return;
  }

  if (serializeJson(json, configFile) == 0) {
      Serial.println("Blad zapisu JSON do pliku.");
  } else {
      Serial.println("Zapisano konfiguracje.");
  }
  configFile.close();
}

// ===================================
// 6. FUNKCJE SERWERA WWW
// ===================================

void handleRoot() {

  String style = F(
    "<style>"
      "body {"
        "background-color: #222;"    
        "color: #EEE;"               
        "font-family: Arial, sans-serif;"
        "margin: 0; padding: 15px;"
      "}"
      ".container {"
        "max-width: 500px;"         
        "margin: 0 auto;"
        "background-color: #333;"  
        "padding: 20px;"
        "border-radius: 8px;"
        "box-shadow: 0 0 10px rgba(0,0,0,0.5);" 
      "}"
      "h3 {"
        "color: #4CAF50;"            
        "text-align: center;"
        "border-bottom: 2px solid #4CAF50;"
        "padding-bottom: 10px;"
      "}"
      "p { color: #CCC; font-size: 0.9em; }"
      "span.ip {"
        "color: #57a9ff;"            
        "font-weight: bold;"
      "}"
      "label {"
        "display: block;"
        "margin-top: 15px;"
        "font-weight: bold;"
        "color: #BBB;"               
      "}"
      "input[type='text'], input[type='password'], input[type='number'] {"
        "width: calc(100% - 22px);"  
        "padding: 10px;"
        "margin-top: 5px;"
        "background-color: #444;"   
        "color: #FFF;"               
        "border: 1px solid #555;"
        "border-radius: 4px;"
      "}"
      "button {"
        "background-color: #4CAF50;"  
        "color: white;"
        "padding: 12px 20px;"
        "border: none;"
        "border-radius: 4px;"
        "margin-top: 20px;"
        "cursor: pointer;"
        "width: 100%;"               
        "font-size: 1.1em;"
      "}"
      "button:hover { background-color: #45a049; }" 
      "hr { border: 1px solid #444; }"
      ".logo-img { display: block; margin-left: auto; margin-right: auto; width: 80px; }" 
    "</style>"
  );

  
  String content = F("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1.0'>");
  content += F("<title>APRS Monitor Ustawienia</title>");
  content += style;
  content += F("</head><body><div class='container'>");
  content += F("<div style='text-align: center;'>");
  content += F("<img src='data:image/png;base64,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' alt='Test' />");
  content += F("</div>");
  content += F("<h3>APRS Monitor: Konfiguracja</h3>");
  content += F("<p>Wpisz nowe dane i kliknij Zapisz. <br>Aktualny adres IP: <span class='ip'>");
  content += WiFi.localIP().toString();
  content += F("</span></p><hr>");
  content += F("<form action='/save' method='get'>");
  content += F("<label for='callsign'>1. Znak Wolawczy (zalecany SSID -X):</label><input type='text' id='callsign' name='callsign' value='");
  content += String(my_callsign);
  content += F("' maxlength='12'>");
  content += F("<label for='aprspass'>2. APRS Passcode (Haslo APRS-IS):</label><input type='password' id='aprspass' name='aprspass' value='");
  content += String(aprs_pass);
  content += F("' maxlength='8'>");
  content += F("<label for='mylat'>3. Szerokosc Geo. (Lat, np. 51.7583):</label><input type='text' id='mylat' name='mylat' value='");
  content += String(my_lat_str);
  content += F("' maxlength='10'>");
  content += F("<label for='mylon'>4. Dlugosc Geo. (Lon, np. 19.4569):</label><input type='text' id='mylon' name='mylon' value='");
  content += String(my_lon_str);
  content += F("' maxlength='10'>");
  content += F("<label for='comment'>5. Komentarz Beacona:</label><input type='text' id='comment' name='comment' value='");
  content += String(beacon_comment);
  content += F("' maxlength='65'>");
  content += F("<label for='symbol'>6. Symbol APRS (np monitorek L):</label><input type='text' id='symbol' name='symbol' value='");
  content += String(aprs_symbol);
  content += F("' maxlength='2'>");
  content += F("<label for='filter'>7. Promien filtra (km):</label><input type='number' id='filter' name='filter' value='");
  content += String(aprs_filter_range);
  content += F("' maxlength='5'>");
  content += F("<button type='submit'>Zapisz i Restartuj Urzadzenie</button>");
  content += F("</form></div></body></html>");
  
  server.send(200, "text/html", content);
}

void handleSave() {

  if (server.hasArg("callsign")) strcpy(my_callsign, server.arg("callsign").c_str());
  if (server.hasArg("aprspass")) strcpy(aprs_pass, server.arg("aprspass").c_str());
  if (server.hasArg("mylat")) strcpy(my_lat_str, server.arg("mylat").c_str());
  if (server.hasArg("mylon")) strcpy(my_lon_str, server.arg("mylon").c_str());
  if (server.hasArg("comment")) strcpy(beacon_comment, server.arg("comment").c_str());
  if (server.hasArg("symbol")) strcpy(aprs_symbol, server.arg("symbol").c_str());
  if (server.hasArg("filter")) strcpy(aprs_filter_range, server.arg("filter").c_str());
  
  shouldSaveConfig = true; 

  String message = "Dane zapisane. Urzadzenie zrestartuje sie za 3 sekundy. Nie przerywaj.";
  server.send(200, "text/plain", message);
}

// ===================================
// 7. LOGIKA WIFI MANAGER
// ===================================

void setupWiFi() {

  const char* style_minified = "<style>"
    "body{background:#222;color:#eee;font-family:sans-serif}"
    "h1,h3{color:#4CAF50}"
    "button{background:#4CAF50;color:#fff;border:0;padding:10px;margin:5px 0;width:100%;border-radius:4px}"
    "input{background:#444;color:#fff;border:1px solid #555;padding:8px;border-radius:4px;width:100%}"
    "div,fieldset{background:#333;padding:10px;border:0;margin-top:10px;border-radius:4px}"
    "a{color:#57a9ff;text-decoration:none}"
    ".c{text-align:center}"
    "</style>";

  wifiManager.setCustomHeadElement(style_minified);

  const char* page_content = 
    "<div class='c'><img src='data:image/png;base64,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'></div>"
    "<h3 style='color:#4CAF50'>Konfiguracja APRS</h3>"
    "<p style='color:#ccc;font-size:0.9em'>Logowanie APRS-IS i koordynaty.</p>";

  WiFiManagerParameter custom_content(page_content);
  WiFiManagerParameter custom_callsign("callsign", "1. Znak + SSID ( Np. SQ7UTP-X)", my_callsign, 12);
  WiFiManagerParameter custom_aprspass("aprspass", "2. APRS Passcode (Haslo APRS-IS)", aprs_pass, 8, "type='password'");
  WiFiManagerParameter custom_lat("mylat", "3. Lat, np. 51.7583", my_lat_str, 10);
  WiFiManagerParameter custom_lon("mylon", "4. Lon, np. 19.4569", my_lon_str, 10);
  WiFiManagerParameter custom_symbol("symbol", "5. Symbol APRS (np monitorek L)", aprs_symbol, 2);
  WiFiManagerParameter custom_comment("comment", "6. Komentarz", beacon_comment, 65);
  WiFiManagerParameter custom_filter_range("filter", "7. Promien filtra (km)", aprs_filter_range, 5, "type='number'"); 

  wifiManager.addParameter(&custom_content);
  
  wifiManager.addParameter(&custom_callsign);
  wifiManager.addParameter(&custom_aprspass);
  wifiManager.addParameter(&custom_lat);
  wifiManager.addParameter(&custom_lon);
  wifiManager.addParameter(&custom_symbol);
  wifiManager.addParameter(&custom_comment);
  wifiManager.addParameter(&custom_filter_range);
  wifiManager.setAPCallback([](WiFiManager* wm) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x12_tr); 
    u8g2.drawStr(u8g2_center_x("KONFIGURACJA WiFi"), 10, "KONFIGURACJA WiFi");
    u8g2.drawHLine(0, 12, 128);
    u8g2.setFont(u8g2_font_profont12_tr);
    u8g2.drawStr(5, 35, "1. Polacz z siecia:");
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.setCursor(15, 48); 
    u8g2.print("AP: ");
    u8g2.print(wm->getConfigPortalSSID());
    u8g2.setCursor(15, 58);
    u8g2.print("IP: 192.168.4.1"); 
    u8g2.sendBuffer();
  });

  if (!wifiManager.autoConnect("APRS-SETUP")) {
    Serial.println("Nie udalo sie polaczyc/skonfigurowac. Restart.");
    delay(3000);
    ESP.restart();
  } 

  if(WiFi.status() == WL_CONNECTED){
     WiFi.setHostname("aprs-setup"); 
  }
  
  Serial.println("Polaczono z siecia WiFi!");
  
  strcpy(my_callsign, custom_callsign.getValue());
  strcpy(aprs_pass, custom_aprspass.getValue());
  strcpy(my_lat_str, custom_lat.getValue());
  strcpy(my_lon_str, custom_lon.getValue());
  strcpy(beacon_comment, custom_comment.getValue());
  strcpy(aprs_symbol, custom_symbol.getValue());
  strcpy(aprs_filter_range, custom_filter_range.getValue()); 

  server.on("/", handleRoot);
  server.on("/save", handleSave);
  server.begin();
  Serial.println("Serwer HTTP aktywny pod adresem: " + WiFi.localIP().toString());
}


// ===================================
// 8. FUNKCJE APRS I LOGIKI
// ===================================

float parseAPRSCoord(String coord) {
    if (coord.length() < 6) return 0.0;
    
    char direction = coord.charAt(coord.length() - 1);
    coord.remove(coord.length() - 1); 

    int dotIndex = coord.indexOf('.');
    if (dotIndex < 0) return 0.0; 
    
    int minutesStartIndex = dotIndex - 2; 
    if (minutesStartIndex < 0) return 0.0; 
    
    String minStr = coord.substring(minutesStartIndex);
    float minutes = minStr.toFloat();
    
    String degStr = coord.substring(0, minutesStartIndex);
    float degrees = degStr.toFloat();
    
    if (degrees > 180.0) {
        return 0.0; 
    }
    if ((direction == 'N' || direction == 'S') && degrees > 90.0) {
        return 0.0;
    }
    
    float decimalValue = degrees + (minutes / 60.0);
    
    if (direction == 'S' || direction == 'W') {
        decimalValue *= -1.0;
    }
    
    return decimalValue;
}

void calculateDistanceAndBearing(float targetLat, float targetLon, APRSStation& station) {

    float myLat = safeToFloat(my_lat_str);
    float myLon = safeToFloat(my_lon_str);
    
    const float R = 6371.0; 
    
    float lat1R = radians(myLat);
    float lon1R = radians(myLon);
    float lat2R = radians(targetLat);
    float lon2R = radians(targetLon);
    
    float dLat = lat2R - lat1R;
    float dLon = lon2R - lon1R;
    
    float a = sin(dLat / 2) * sin(dLat / 2) + 
              cos(lat1R) * cos(lat2R) * sin(dLon / 2) * sin(dLon / 2);
    float c = 2 * atan2(sqrt(a), sqrt(1 - a));
    station.distance = R * c; 

    float y = sin(dLon) * cos(lat2R);
    float x = cos(lat1R) * sin(lat2R) - 
              sin(lat1R) * cos(lat2R) * cos(dLon);
    
    float bearingRad = atan2(y, x);
    float bearingDeg = degrees(bearingRad);
    
    station.bearing = fmod(bearingDeg + 360, 360);
}

void addStationToBuffer(const APRSStation& newStation) {
    
    int shift_limit = min(MAX_STATIONS - 1, lastStationsCount);

    for (int i = shift_limit; i > 0; i--) {
        stations[i] = stations[i - 1];
    }
    
    if (lastStationsCount < MAX_STATIONS) {
        lastStationsCount++;
    }

    stations[0] = newStation; 
    
    currentSingleStation = newStation;
    singleStationTimer = millis();
    displayMode = 2; 
}

void connectToAPRS() {
  if (WiFi.status() != WL_CONNECTED) return;
  Serial.printf("Laczenie z APRS: %s:%d\n", APRS_SERVER, APRS_PORT);

  if (client.connect(APRS_SERVER, APRS_PORT)) {
    Serial.println("Polaczono z APRS");
    
    float myLat = safeToFloat(my_lat_str);
    float myLon = safeToFloat(my_lon_str);
    
    String filterRange = String(aprs_filter_range); 
    
    String login = String("user ") + String(my_callsign) + " pass " + String(aprs_pass) + " vers ESP8266APRSMonitor V1.2 filter r/" +
                   String(myLat, 2) + "/" + String(myLon, 2) + "/" + filterRange + "\r\n";
    client.print(login);
    client.flush();
  }
}

String formatPosition(float lat, float lon) {
  char ns = lat >= 0 ? 'N' : 'S';
  char ew = lon >= 0 ? 'E' : 'W';
  lat = fabs(lat);
  lon = fabs(lon);
  int latDeg = (int)lat;
  float latMin = (lat - latDeg) * 60.0;
  int lonDeg = (int)lon;
  float lonMin = (lon - lonDeg) * 60.0;
  char buf[20];
  snprintf(buf, sizeof(buf), "%02d%05.2f%c/%03d%05.2f%c%c", latDeg, latMin, ns, lonDeg, lonMin, ew, aprs_symbol[0]); 
  return String(buf);
}

void sendBeacon() {
  if (!client.connected()) {
    connectToAPRS();
    if (!client.connected()) return;
  }

  char timeStr[8] = "000000";
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    snprintf(timeStr, sizeof(timeStr), "%02d%02d%02dh", timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min);
  }

  float myLat = safeToFloat(my_lat_str);
  float myLon = safeToFloat(my_lon_str);

  String pos = formatPosition(myLat, myLon);
  
  String beacon = String(my_callsign) + ">APRS,WIDE1-1:@" + timeStr + pos + String(beacon_comment) + "\r\n";

  Serial.print("Wysylam beacon: ");
  Serial.print(beacon);

  client.print(beacon);
  client.flush();
}

void processAPRSpacket(String packet) {
    if (packet.length() == 0 || packet.startsWith("#") || packet.startsWith("user")) {
        return; 
    }

    int gt_index = packet.indexOf('>');
    if (gt_index < 0) return;
    int info_start = packet.indexOf(':');
    if (info_start < 0) return; 
    String callsign = packet.substring(0, gt_index);
    callsign.trim();
    char data_type = packet.charAt(info_start + 1);
    if (strchr("!@=/$;PTBptb", data_type) == NULL) {
        return; 
    }
    
    int pos_data_start = 0;
    if (data_type == '@' || data_type == '/') {
        pos_data_start = info_start + 9; 
    } else {
        pos_data_start = info_start + 2; 
    }

    if (pos_data_start >= packet.length()) return;

    String posData = packet.substring(pos_data_start);
    
    APRSStation newStation;
    newStation.callsign = callsign;
    newStation.timestamp = millis();
    newStation.comment = ""; 
    int lat_end = -1;
    for(int i = 0; i < posData.length(); i++) {
        char c = posData.charAt(i);
        if (c == 'N' || c == 'S') {
            lat_end = i;
            break;
        }
    }

    if (lat_end < 4) return;
    int lon_start = lat_end + 2;
    int lon_end = lon_start;
    while (lon_end < posData.length() && (isDigit(posData.charAt(lon_end)) || posData.charAt(lon_end) == '.' || posData.charAt(lon_end) == 'E' || posData.charAt(lon_end) == 'W')) {
        lon_end++;
    }
    
    if (lon_end <= lon_start) return;
    char ew_char = posData.charAt(lon_end - 1);
    if (ew_char != 'E' && ew_char != 'W') return;
    String latStr = posData.substring(0, lat_end + 1);
    String lonStr = posData.substring(lon_start, lon_end);
    float targetLat = parseAPRSCoord(latStr);
    float targetLon = parseAPRSCoord(lonStr);
    if (abs(targetLat) < 0.1 && abs(targetLon) < 0.1) return;
    calculateDistanceAndBearing(targetLat, targetLon, newStation);
    if (newStation.distance > 500.0) return;
    int data_end_pos = lon_end; 
    newStation.heading = 0;
    newStation.speed = 0;
    int comment_start = data_end_pos; 
    if (comment_start + 1 < posData.length()) {
        comment_start += 2;
    }

    if (comment_start + 3 < posData.length() && isDigit(posData.charAt(comment_start)) && posData.charAt(comment_start+3) == '/') {
    }

    if (comment_start + 7 < posData.length() && posData.charAt(comment_start) == '/') { 
        String speedHeadingStr = posData.substring(comment_start + 1, comment_start + 8); 
        int slash_index = speedHeadingStr.indexOf('/');
        if (slash_index > 0) {
            newStation.heading = speedHeadingStr.substring(0, slash_index).toInt();
            newStation.speed = speedHeadingStr.substring(slash_index + 1).toInt();
            comment_start += 8; 
        } 
    } 
    
    if (comment_start < posData.length()) {
        newStation.comment = posData.substring(comment_start); 
        newStation.comment.trim(); 
    }
    
    Serial.printf("Zapisano: %s (%.1f km) %s\n", newStation.callsign.c_str(), newStation.distance, newStation.comment.c_str());
    addStationToBuffer(newStation);
}


// ===================================
// 9. SETUP I LOOP
// ===================================

void setup() {
  Serial.begin(115200);
  Serial.println("\nStart ESP8266 APRS Monitor (Wersja 2.9)");

  displaySplashScreen();
  displayProgressScreen(1, "LittleFS...");
  
  if(!LittleFS.begin()){
    Serial.println("Blad montowania LittleFS.");
    Serial.println("Proba formatowania partycji. NIE PRZERYWAJ.");
    
    LittleFS.format(); 
    
    if(!LittleFS.begin()){
        Serial.println("Krytyczny blad LittleFS. Uruchomienie niemozliwe.");
        while(true){
            delay(1000); 
        }
    }
    Serial.println("LittleFS po formatowaniu: Sukces.");
  } else {
    Serial.println("LittleFS: Sukces. Trwa ladowanie konfiguracji.");
  }
  
  loadConfig(); 
  delay(100); 
  
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  displayProgressScreen(2, "WiFi Manager...");
  setupWiFi(); 
  
  displayProgressScreen(3, "Pobieranie czasu NTP...");
  Serial.println("Synchronizacja NTP...");
  configTime("CET-1CEST,M3.5.0,M10.5.0/3", "vega.cbk.poznan.pl");
  delay(3000);
  
  displayProgressScreen(4, "Laczenie z APRS...");
  connectToAPRS();
  delay(1000); 

  Serial.println("Inicjalizacja zakonczona. Gotowy do pracy.");
  displayProgressScreen(4, "Gotowy do pracy!"); 
  delay(1000);
  
  sendBeacon();
  lastBeaconMillis = millis();
}

void loop() {
  
  if (WiFi.status() == WL_CONNECTED) {
    server.handleClient(); 
  }

  if (shouldSaveConfig) {
    saveConfig();
    shouldSaveConfig = false;
    Serial.println("Zapis zakonczony. Restart w 3s...");
    delay(3000);
    ESP.restart(); 
  }
  
  if (millis() - lastBeaconMillis >= BEACON_INTERVAL) {
    Serial.println("Wysylam kolejny beacon...");
    sendBeacon();
    lastBeaconMillis = millis();
  }

  if (client.available()) {
    String line = client.readStringUntil('\n');
    processAPRSpacket(line); 
    lastPacketMillis = millis(); 
  }
  
  if (!client.connected() || (millis() - lastPacketMillis > APRS_TIMEOUT && lastPacketMillis != 0)) {
    if (WiFi.status() == WL_CONNECTED) {
        if (client.connected()) {
            Serial.println("Brak pakietow przez 2 minuty. Resetowanie polaczenia...");
            client.stop(); 
        } else {
            Serial.println("Polaczenie APRS zerwane, proba ponownego laczenia...");
        }
        connectToAPRS();
        lastPacketMillis = millis(); 
    } else {
        Serial.println("Brak WiFi. Restartuje WiFiManager w celu rekonfiguracji.");
        setupWiFi(); 
    }
  }

  if (displayMode == 2) {
    if (millis() - singleStationTimer >= SINGLE_STATION_DURATION) {
        displayMode = 1; 
        currentScrollY = 0; 
        lastScrollMillis = millis();
    }
  } 

  if (displayMode == 2) {
      displaySingleStationScreen(); 
  } else {
      displayDataListScreen(); 
  }
  
  delay(10); 
}