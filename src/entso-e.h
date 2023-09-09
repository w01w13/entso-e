#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <math.h>
#include <stdio.h>
#define DATE_LEN 8
#define PERION_END "periodEnd"
#define PERIOD_START "periodStart"
#define API_HOST "web-api.tp.entsoe.eu"
#define API_DOCUMENT "A44"
#define API_DOMAIN "10YFI-1--------U"

int entso_e_refresh(const char* token, double* priceData);
