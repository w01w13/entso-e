#include "entso-e.h"
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <ctime>
#include <stdio.h>

WiFiClientSecure httpsClient; // Declare object of class WiFiClient
double* prices = NULL;
int size = 0;
time_t updated;
int updateStatus;
void format_timestamp(struct tm* startTime, char* str)
{
    unsigned int year = startTime->tm_year + 1900;
    unsigned int month = startTime->tm_mon + 1;
    unsigned int day = startTime->tm_mday;
    // Format is 202211072300 -> yyyyMMddHHmm
    char timeStamp[DATE_LEN];
    snprintf(timeStamp, 9, "%04d%02d%02d", year, month, day);
    strcpy(str, timeStamp);
}

void get_api(struct tm* startTime, char* str, const char* token)
{
    char startDate[10];
    char endDate[10];
    format_timestamp(startTime, startDate);
    startTime->tm_mday += 1;
    time_t next = mktime(startTime);
    format_timestamp(localtime(&next), endDate);

    char api[244];
    sprintf(api, "/api?securityToken=%s&documentType=%s&in_Domain=%s&out_Domain=%s&periodStart=%s0000&periodEnd=%s0000",
        token, API_DOCUMENT, API_DOMAIN, API_DOMAIN, startDate, endDate);
    strcpy(str, api);
}
void read_cache(double** priceData, int** len)
{
    *priceData = (double*)malloc(size * sizeof(double));
    memcpy(*priceData, prices, size * sizeof(double));
    *len = (int*)malloc(sizeof(int));
    **len = size;
}
int get_offset(struct tm* time)
{
    int offset = 3;
    if (time->tm_isdst) {
        offset = 2;
    }
    return offset;
}
int get_position(struct tm* startTime, char* str)
{
    unsigned int hour = startTime->tm_hour % 24;
    // There's no hour 0 in XML, only 1-24
    if (hour == 0) {
        hour = 24;
    }
    sprintf(str, "<position>%i</position>", hour);
    return hour;
}
int connect()
{
    Serial.printf("HTTPS: Connecting to API at %s\n", API_HOST);
    httpsClient.setTimeout(5000);
    httpsClient.setBufferSizes(1000, 1000);
    httpsClient.setInsecure();
    delay(1000);
    Serial.printf("HTTPS: Connecting");
    int httpsStatus = httpsClient.connect(API_HOST, 443);
    if (httpsStatus != 1 && !httpsClient.connected()) {
        Serial.printf("HTTPS: Connection failed: %d\n", httpsStatus);
        return 599;
    } else {
        Serial.printf("HTTPS: Connected\n");
        return 0;
    }
}

time_t adjustForTimezone(time_t utcTime, int timezoneOffset) { return utcTime + (timezoneOffset * 3600); }

time_t get_time()
{
    time_t rawtime;
    rawtime = time(NULL);
    int offset = get_offset(localtime(&rawtime));
    return adjustForTimezone(rawtime, offset);
}
void http_get(const char* token, struct tm* startTime)
{
    char api[255];
    get_api(startTime, api, token);
    char endpoint[512];
    sprintf(endpoint, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", api, API_HOST);
    Serial.printf("HTTPS:Constructed Request:\n------------------------\n\n%s------------------------\n", endpoint);
    httpsClient.printf("%s", endpoint);
}
void rotateCachedPrices(bool shouldUpdate)
{
    // Only rotate cached prices if necessary (every hour in case the refresh fails)
    if (shouldUpdate) {
        int positionsToShift = size - 1;
        // Rotate the prices by one to left
        for (int i = 0; i < positionsToShift; i++) {
            prices[i] = prices[i + 1];
        }
        prices = (double*)realloc(prices, positionsToShift * sizeof(double));
    }
}
int read_response(struct tm* startTime)
{
    // Check the current hour we need to retrieve
    char position[24];
    // Bypass 24 hour for now, as it's problematic. We can still get the values from cache
    int hour = get_position(startTime, position);
    if (hour == 24) {
        return 0;
    }
    int status = 404;
    String line;
    String code = "";
    while (httpsClient.connected() || httpsClient.available()) {
        line = httpsClient.readStringUntil('\n');
        line.trim();
        // Serial.println(line);
        // Shoddy parsing to save memory, we just need prices
        if (status == 0 && line.startsWith("<price.amount>") && line.endsWith("</price.amount>")) {
            String price = line.substring(line.indexOf(">") + 1, line.lastIndexOf('<'));
            size++;
            prices = (double*)realloc(prices, size * sizeof(double));
            prices[size - 1] = price.toDouble();
        } else if (line.startsWith(position) && status != 0) {
            status = 0;
            free(prices);
            size = 0;
            prices = NULL;
        } else if (line.startsWith("<code>")) {
            code = line.substring(line.indexOf(">") + 1, line.lastIndexOf('<'));
        } else if (line.startsWith("<text>") && code != "") {
            String text = line.substring(line.indexOf(">") + 1, line.lastIndexOf('<'));
            Serial.print("Got error code\u0020");
            Serial.print(code);
            Serial.print(":\u0020");
            Serial.print(text);
            Serial.println("\n");
        }
    }
    return status;
}
int read_headers()
{
    int status = 0;
    while (httpsClient.connected() || httpsClient.available()) {
        String line = httpsClient.readStringUntil('\n');
        // Serial.println(line);
        if (line == "\r") {
            Serial.println("headers received");
            break;
        } else if (line.startsWith("HTTP/1.1 401 Unauthorized")) {
            status = 401;
            break;
        }
    }
    return status;
}
int get_data(const char* token, struct tm* startTime)
{
    int status = connect();
    if (status == 0) {
        // Construct HTTP GET
        Serial.println("Sending request");
        http_get(token, startTime);
        // Only read if the headers were ok
        status = read_headers();
        if (status == 0) {
            status = read_response(startTime);
        }
        httpsClient.stop();
    }
    return status;
}

int entso_e_refresh(const char* token, double** priceData, int** len)
{
    time_t now = get_time();
    double diff = difftime(now, updated);
    bool shouldUpdate = diff >= 3600.0;
    if (!updated || shouldUpdate || updateStatus != 0) {
        struct tm* startTime = localtime(&now);
        startTime->tm_min = 0;
        startTime->tm_sec = 0;
        Serial.print("Update time: ");
        Serial.println(asctime(startTime));
        rotateCachedPrices(shouldUpdate);
        updated = mktime(startTime);
        updateStatus = get_data(token, startTime);
    } else {
        Serial.printf("Reading cached values, hourstamp was %f seconds ago\n", diff);
    }
    read_cache(priceData, len);
    return updateStatus;
}