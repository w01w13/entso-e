# entso-e

Simple ESP8266 lib to read electricity prices from entso-e with ESP8266. This is work in progress.

1. Register to [Transparency Platform RESTful API](https://transparency.entsoe.eu/content/static_content/Static%20content/web%20api/Guide.html#_authentication_and_authorisation)
2. Once registered and created API token, just call something along lines:

```code
#include "entso-e.h"
#define NUM_REGS 49
double* priceData;
int* priceLen;
String token = "YOUR_TOKEN_FROM_ENTSO";
boolean read = false;
void loop()
{
    if (!read) {
        int status;
        free(priceData);
        free(priceLen);
        status = entso_e_refresh(token.c_str(), &priceData, &priceLen);
        if (status != 0) {
            Serial.println("Invalid read status");
        } else {
            Serial.println("Valid read status");
        }
    }
}
```
NOTE: Currently the timezone values are hardcoded to finnish timezones. If you need other timezones, edit the

```
int get_offset(struct tm* time)
{
    int offset = 3;
    if (time->tm_isdst) {
        offset = 2;
    }
    return offset;
}
```
Same applies for the actual market price, this needs to be changed according to Entso-E documentation in entso-e.h:

```
#define API_DOMAIN "10YFI-1--------U"
```
