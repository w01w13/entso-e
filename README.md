# entso-e

Simple ESP8266 lib to read electricity prices from entso-e with ESP8266. This is work in progress.

1. Register to [Transparency Platform RESTful API](https://transparency.entsoe.eu/content/static_content/Static%20content/web%20api/Guide.html#_authentication_and_authorisation)
2. Once registered and created API token, just call something along lines:

```code
#include "entso-e.h"
#define NUM_REGS 49
double priceData[NUM_REGS];
String token = "YOUR_TOKEN_FROM_ENTSO";
boolean read = false;
void loop()
{
    if (!read) {
        int status;
        status = entso_e_refresh(token.c_str(), priceData);
        if (status != 0) {
            Serial.println("Invalid read status");
        } else {
            Serial.println("Valid read status");
        }
    }
}
```
