#include <Arduino.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

#include <ESP8266HTTPClient.h>
#include "JsonListener.h"
#include <time.h>
#include <sys/time.h>

// Bibliothèque pour l'écran OLED qui fonctionne
#include <U8g2lib.h>

#include "OpenWeatherMapCurrent.h"
#include "OpenWeatherMapForecast.h"

/******************** Begin Settings ********************/

/***************************
 * WIFI Settings
 **************************/
const char* WiFi_Name = "VOTRE-WIFI-ICI";
const char* WiFi_Password = "VOTRE-MOT-DE-PASSE-WIFI-ICI";
WiFiClient client;

/***************************
 * OpenWeatherMap Settings
 **************************/
const boolean IS_METRIC = true;
String OPEN_WEATHER_MAP_APP_ID = "VOTRE-CLE-API";
String OPEN_WEATHER_MAP_LOCATION_ID = "VOTRE-ID-DE-VILLE"; // ID pour Guipavas, France
String OPEN_WEATHER_MAP_LANGUAGE = "fr";
const uint8_t MAX_FORECASTS = 8; // On récupère plus de prévisions pour avoir le choix
OpenWeatherMapCurrentData   currentWeather;
OpenWeatherMapCurrent       currentWeatherClient;
OpenWeatherMapForecastData  forecasts[MAX_FORECASTS];
OpenWeatherMapForecast      forecastClient;


/***************************
 * 0.96 inch SSD1306 OLED Settings
 **************************/
U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, /* clock=*/ 12, /* data=*/ 14, /* reset=*/ U8X8_PIN_NONE);

/***************************
 * Interface Settings
 **************************/
const int TIME_HOME_PAGE_SECS = 10; // 10 secondes sur la page d'accueil
const int TIME_FORECAST_PAGE_SECS = 5; // 5 secondes par page de prévision
const int MAX_FORECAST_PAGES = 5; // Nombre de pages de prévisions à afficher
int currentPage = 0; // 0 = page d'accueil, 1,2,3... = pages de prévisions
unsigned long lastPageChangeTime = 0;

/***************************
 * Time Settings
 **************************/
time_t now;
const int UPDATE_INTERVAL_SECS = 20 * 60; // Mise à jour des données toutes les 20 minutes
long timeSinceLastWUpdate = 0;

// Déclarations des fonctions
void updateData();
void drawProgress(int percentage, String label);
void drawHomePage();
void drawForecastPage(int pageIndex);
const char* getWeatherIcon(String iconNameFromApi);


/******************** End Settings ********************/


void setup() {
  Serial.begin(115200);

  // Initialisation de l'écran OLED
  u8g2.begin();
  u8g2.enableUTF8Print(); // Important pour les caractères spéciaux (ex: °)
  Serial.println("Écran OLED initialisé !");

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(10, 35, "Connexion WiFi...");
  u8g2.sendBuffer();

  WiFi.begin(WiFi_Name, WiFi_Password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connecté !");

  // Configuration du fuseau horaire pour Paris (CET/CEST)
  configTime("CET-1CEST,M3.5.0,M10.5.0/3", "pool.ntp.org");

  // Mise à jour des données météo au démarrage
  updateData();
  timeSinceLastWUpdate = millis();
}

void loop() {
  unsigned long currentTime = millis();

  // Mise à jour périodique des données météo (toutes les 20 min)
  if (currentTime - timeSinceLastWUpdate > (1000L * UPDATE_INTERVAL_SECS)) {
    updateData();
    timeSinceLastWUpdate = currentTime;
  }

  // Logique de changement de page
  bool shouldChangePage = false;
  if (currentPage == 0) { // Si on est sur la page d'accueil
    if (currentTime - lastPageChangeTime > (TIME_HOME_PAGE_SECS * 1000L)) {
      shouldChangePage = true;
    }
  } else { // Si on est sur une page de prévision
    if (currentTime - lastPageChangeTime > (TIME_FORECAST_PAGE_SECS * 1000L)) {
      shouldChangePage = true;
    }
  }

  if (shouldChangePage) {
    currentPage++;
    if (currentPage > MAX_FORECAST_PAGES) {
      currentPage = 0; // Retour à la page d'accueil
    }
    lastPageChangeTime = currentTime;
  }
  
  // Affichage de la page courante
  u8g2.clearBuffer();
  if (currentPage == 0) {
    drawHomePage();
  } else {
    // pageIndex commence à 0, donc on fait -1
    drawForecastPage(currentPage - 1); 
  }
  u8g2.sendBuffer();

  delay(250); // Petite pause pour fluidifier et économiser les ressources
}

// Affiche la barre de progression
void drawProgress(int percentage, String label) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.setCursor(5, 15);
  u8g2.print(label);
  u8g2.drawRBox(10, 30, 108, 12, 3);
  u8g2.drawBox(12, 32, 104 * percentage / 100, 8);
  u8g2.sendBuffer();
}

// Récupère les données de temps et météo
void updateData() {
  drawProgress(25, "Maj. Météo Actuelle...");
  currentWeatherClient.setMetric(IS_METRIC);
  currentWeatherClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  currentWeatherClient.updateCurrentById(&currentWeather, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_ID);

  drawProgress(75, "Maj. Prévisions...");
  forecastClient.setMetric(IS_METRIC);
  forecastClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  // On ne filtre plus par heure pour avoir toutes les prévisions des prochaines 24h
  forecastClient.updateForecastsById(forecasts, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_ID, MAX_FORECASTS);

  drawProgress(100, "Terminé !");
  delay(1000);
}

// Dessine la page d'accueil
void drawHomePage() {
  char buff[32];
  now = time(nullptr);
  struct tm* timeInfo = localtime(&now);

  // Affichage de l'heure
  u8g2.setFont(u8g2_font_logisoso24_tr);
  sprintf(buff, "%02d:%02d", timeInfo->tm_hour, timeInfo->tm_min);
  u8g2.drawStr(32, 30, buff);

  // Affichage de la date
  u8g2.setFont(u8g2_font_ncenB08_tr);
  const String WDAY_NAMES[] = {"Dimanche", "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi"};
  const String MONTH_NAMES[] = {"Jan", "Fev", "Mar", "Avr", "Mai", "Juin", "Juil", "Aou", "Sep", "Oct", "Nov", "Dec"};
  sprintf(buff, "%s %d %s", WDAY_NAMES[timeInfo->tm_wday].c_str(), timeInfo->tm_mday, MONTH_NAMES[timeInfo->tm_mon].c_str());
  u8g2.drawStr(u8g2.getDisplayWidth()/2 - u8g2.getStrWidth(buff)/2, 48, buff);

  // Météo actuelle en petit
  String temp = String(currentWeather.temp, 0) + "°C";
  u8g2.setFont(u8g2_font_open_iconic_weather_2x_t);
  u8g2.drawStr(5, 62, getWeatherIcon(currentWeather.icon));
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(28, 62, temp.c_str());
  u8g2.drawStr(60, 62, currentWeather.description.c_str());
}

// Dessine une page de prévision
void drawForecastPage(int pageIndex) {
  if (pageIndex >= MAX_FORECASTS) return; // Sécurité

  time_t forecastTime = forecasts[pageIndex].observationTime;
  struct tm* timeInfo = localtime(&forecastTime);
  char buff[32];

  // Heure de la prévision
  u8g2.setFont(u8g2_font_logisoso18_tr);
  sprintf(buff, "%02dh", timeInfo->tm_hour);
  u8g2.drawStr(u8g2.getDisplayWidth()/2 - u8g2.getStrWidth(buff)/2, 16, buff);

  // *** MODIFIÉ : Utilisation d'une icône plus petite (2x) pour une meilleure compatibilité ***
  u8g2.setFont(u8g2_font_open_iconic_weather_2x_t);
  const char* icon = getWeatherIcon(forecasts[pageIndex].icon);
  u8g2.drawStr(u8g2.getDisplayWidth()/2 - u8g2.getStrWidth(icon)/2, 42, icon);

  // Température prévue
  String temp = String(forecasts[pageIndex].temp, 0) + "°C";
  u8g2.setFont(u8g2_font_ncenB10_tr);
  u8g2.drawStr(5, 15, temp.c_str());

  // Affichage de la description de la météo
  String description = forecasts[pageIndex].description;
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(u8g2.getDisplayWidth()/2 - u8g2.getStrWidth(description.c_str())/2, 60, description.c_str());


  // Indicateur de page
  for (int i=0; i < MAX_FORECAST_PAGES; i++) {
    if (i == pageIndex) {
      u8g2.drawDisc(u8g2.getDisplayWidth() - 15, 8 + i * 10, 3);
    } else {
      u8g2.drawCircle(u8g2.getDisplayWidth() - 15, 8 + i * 10, 3);
    }
  }
}

// Traduit l'icône de l'API en icône graphique U8g2
const char* getWeatherIcon(String iconNameFromApi) {
  // Mappage des codes d'icônes OpenWeatherMap vers le set d'icônes de U8G2
  // u8g2_font_open_iconic_weather_...
  // A = soleil, B = lune, C = nuage-soleil, D = nuage-lune, E = nuages
  // F = nuages gris, G = pluie, H = forte pluie, I = orage, J = neige, K = brouillard
  if (iconNameFromApi == "01d") return "A"; // clear sky day
  if (iconNameFromApi == "01n") return "B"; // clear sky night
  if (iconNameFromApi == "02d") return "C"; // few clouds day
  if (iconNameFromApi == "02n") return "D"; // few clouds night
  if (iconNameFromApi == "03d" || iconNameFromApi == "03n") return "E"; // scattered clouds
  if (iconNameFromApi == "04d" || iconNameFromApi == "04n") return "F"; // broken clouds
  if (iconNameFromApi == "09d" || iconNameFromApi == "09n") return "G"; // shower rain
  if (iconNameFromApi == "10d" || iconNameFromApi == "10n") return "H"; // rain
  if (iconNameFromApi == "11d" || iconNameFromApi == "11n") return "I"; // thunderstorm
  if (iconNameFromApi == "13d" || iconNameFromApi == "13n") return "J"; // snow
  if (iconNameFromApi == "50d" || iconNameFromApi == "50n") return "K"; // mist
  return "E"; // Icône par défaut
}
