float humidity;

float temp_f;

float precipitation;

float Qin;

float Qout;

float Qinf;

// ===================== SECTION B — DATA FORMATTING =====================

String buildJSON(float humidity,

float temp_f,

float precipitation,

float Qin,

float Qout,

float Qinf)

{

String json = "{";

json += "\"humidity\":" + String(humidity, 2) + ",";

json += "\"temp_f\":" + String(temp_f, 2) + ",";

json += "\"precipitation\":" + String(precipitation, 4) + ",";

json += "\"inflow\":" + String(Qin, 6) + ",";

json += "\"outflow\":" + String(Qout, 6) + ",";

json += "\"downflow\":" + String(Qinf, 6);

json += "}";

return json;

}