#include "entso-e.h"
#include <ctime>
WiFiClientSecure httpsClient; // Declare object of class WiFiClient
double prices[49];
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
void read_cache(double* priceData)
{
    for (unsigned int i = 0; i < sizeof(prices) / sizeof(double); i++) {
        priceData[i] = prices[i];
    }
}

void get_position(struct tm* startTime, char* str)
{
    int offset = 3;
    if (startTime->tm_isdst) {
        offset = 2;
    }
    unsigned int hour = startTime->tm_hour + offset;
    sprintf(str, "<position>%i</position>", hour);
}
int connect()
{
    Serial.printf("HTTPS: Connecting to API at %s\n", API_HOST);
    httpsClient.setTimeout(15000);
    httpsClient.setBufferSizes(1000, 1000);
    httpsClient.setInsecure();
    delay(1000);
    Serial.printf("HTTPS: Connecting");
    int r = 0; // retry counter
    while ((!httpsClient.connect(API_HOST, 443)) && (r < 30)) {
        delay(500);
        Serial.printf(".");
        r++;
    }
    Serial.printf("\n");
    if (r == 30) {
        Serial.printf("HTTPS: Connection failed\n");
        return 599;
    } else {
        Serial.printf("HTTPS: Connected %d\n", r);
        return 0;
    }
}
struct tm* get_time()
{
    time_t rawtime;
    time(&rawtime);
    return localtime(&rawtime);
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

int read_response(struct tm* startTime)
{
    // Check the current hour we need to retrieve
    char position[24];
    get_position(startTime, position);
    int index = 0;
    int status = 404;
    String line;
    while (httpsClient.connected() || httpsClient.available()) {
        line = httpsClient.readStringUntil('\n');
        line.trim();
        // Serial.println(line);
        // Shoddy parsing to save memory, we just need prices
        if (status == 0 && line.startsWith("<price.amount>") && line.endsWith("</price.amount>")) {
            String price = line.substring(line.indexOf(">") + 1, line.lastIndexOf('<'));
            prices[index++] = price.toDouble();
        } else if (line.startsWith(position)) {
            status = 0;
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
int get_data(const char* token)
{
    int status = connect();
    if (status == 0) {
        struct tm* startTime = get_time();
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

int entso_e_refresh(const char* token, double* priceData)
{
    if (!updated || difftime(time(0), updated) >= 3600.0 || updateStatus != 0) {
        struct tm* now = get_time();
        now->tm_min = 0;
        now->tm_sec = 0;
        updated = mktime(now);
        updateStatus = get_data(token);
    } else {
        Serial.printf("Reading cached values, hourstamp was %f seconds ago\n", difftime(time(0), updated));
    }
    read_cache(priceData);
    return updateStatus;
}