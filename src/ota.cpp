#include "ota.h"
#include "display.h"
#include "config.h"

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

static ESP8266WebServer otaServer(80);

// Logging system: circular buffer (max 10 logs)
static const int MAX_LOGS = 10;
static String logs[MAX_LOGS];
static int logIndex = 0;

void logError(const String& msg) {
    String timestamp = "[ERROR] ";
    logs[logIndex] = timestamp + msg;
    logIndex = (logIndex + 1) % MAX_LOGS;
    Serial.println("[ERROR] " + msg);
}

void logInfo(const String& msg) {
    String timestamp = "[INFO] ";
    logs[logIndex] = timestamp + msg;
    logIndex = (logIndex + 1) % MAX_LOGS;
    Serial.println("[INFO] " + msg);
}

String getLastLogs() {
    JsonDocument doc;
    JsonArray logsArray = doc["logs"].to<JsonArray>();
    
    for (int i = 0; i < MAX_LOGS; i++) {
        if (logs[i].length() > 0) {
            logsArray.add(logs[i]);
        }
    }
    
    String json;
    serializeJson(doc, json);
    return json;
}

static const char OTA_PAGE[] PROGMEM = R"rawliteral(
<html><head><meta name='viewport' content='width=device-width,initial-scale=1'>
<style>
body{background:#222;color:#eee;font-family:sans-serif;max-width:500px;margin:0 auto;padding:16px}
h2{text-align:center;color:#fff}
h3{color:#aaa;border-bottom:1px solid #444;padding-bottom:6px;margin-top:24px}
input[type=text],input[type=range],input[type=number]{width:100%;box-sizing:border-box}
input[type=text],input[type=number]{background:#333;color:#fff;border:1px solid #555;padding:8px;border-radius:4px;margin:4px 0}
label{display:block;margin-top:10px;color:#ccc;font-size:14px}
.btn{background:#5865F2;color:#fff;border:none;padding:10px 20px;border-radius:4px;cursor:pointer;margin:8px 4px}
.btn-red{background:#d33}
.small{font-size:12px;color:#888}
hr{border:none;border-top:1px solid #444;margin:20px 0}
#fl a{color:#88f}
#logs{background:#1a1a1a;border:1px solid #444;border-radius:4px;padding:8px;max-height:200px;overflow-y:auto;font-family:monospace;font-size:12px;line-height:1.4}
.log-error{color:#f44}
.log-info{color:#4f4}
select{width:100%;background:#333;color:#fff;border:1px solid #555;padding:8px;border-radius:4px}
</style></head><body>
<h2>ImageDisplay</h2>

<h3>Brightness</h3>
<input type='range' id='brt' min='5' max='100'>
<span id='bv'></span>%

<h3>Theme</h3>
<select id='theme'>
<option value='0'>Dark</option>
<option value='1'>Light</option>
</select>

<h3>Settings</h3>
<label>Image URL (JPEG)</label>
<input type='text' id='url' placeholder='http://example.com/image.jpg'>
<label>Refresh interval (seconds, 1-3600)</label>
<input type='number' id='refresh' min='1' max='3600'>
<br><br>
<button class='btn' onclick='saveSettings()'>Save Settings</button>
<span id='ss' style='color:#5b5;font-size:14px'></span>
<button class='btn btn-red' onclick="if(confirm('Reboot?'))fetch('/reboot')">Reboot</button>

<h3>WiFi</h3>
<div id='wifi'>Connected</div>
<button class='btn btn-red' onclick="if(confirm('Reset WiFi and reboot?'))fetch('/resetwifi')">Reset WiFi</button>

<h3>Logs</h3>
<div id='logs'>Loading logs...</div>

<h3>Firmware Update</h3>
<form method='POST' action='/update' enctype='multipart/form-data'>
<input type='file' name='firmware' accept='.bin'><br>
<input type='submit' value='Upload Firmware' class='btn'>
</form>

<h3>File Upload (LittleFS)</h3>
<form method='POST' action='/upload' enctype='multipart/form-data'>
<input type='file' name='file' multiple><br>
<input type='submit' value='Upload Files' class='btn'>
</form>

<h3>Files</h3>
<div id='fl'>Loading...</div>

<script>
var bs=document.getElementById('brt'),bv=document.getElementById('bv');
fetch('/api/config').then(r=>r.json()).then(c=>{
  bs.value=c.brightness;bv.textContent=c.brightness;
  document.getElementById('url').value=c.url||'';
  document.getElementById('refresh').value=c.refreshInterval||300;
  document.getElementById('theme').value=c.theme||0;
  document.getElementById('wifi').innerHTML='SSID: '+c.ssid+'<br>IP: '+c.ip;
});
bs.oninput=function(){bv.textContent=bs.value;fetch('/brightness?v='+bs.value+'&save=0')};
bs.onchange=function(){fetch('/brightness?v='+bs.value+'&save=1')};
function saveSettings(){
  var d={url:document.getElementById('url').value,
    refreshInterval:parseInt(document.getElementById('refresh').value),
    theme:parseInt(document.getElementById('theme').value)};
  fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify(d)}).then(function(r){
    var s=document.getElementById('ss');
    if(r.ok){s.textContent='Saved!';s.style.color='#5b5'}else{s.textContent='Error!';s.style.color='#d33'}
    setTimeout(function(){s.textContent=''},2000)});
}
function updateLogs(){
  fetch('/api/logs').then(r=>r.json()).then(data=>{
    var html='';
    if(data.logs&&data.logs.length>0){
      data.logs.forEach(log=>{
        var cls=log.includes('[ERROR]')?'log-error':'log-info';
        html+='<div class="'+cls+'">'+log+'</div>';
      });
    }else{
      html='No logs yet';
    }
    document.getElementById('logs').innerHTML=html;
  });
}
updateLogs();
setInterval(updateLogs,1000);
fetch('/files').then(r=>r.text()).then(t=>document.getElementById('fl').innerHTML=t);
</script>
</body></html>
)rawliteral";

static File uploadFile;

void otaInit() {
    LittleFS.begin();

    otaServer.on("/", HTTP_GET, []() {
        otaServer.send_P(200, "text/html", OTA_PAGE);
    });

    otaServer.on("/api/config", HTTP_GET, []() {
        JsonDocument doc;
        doc["url"]             = getImageUrl();
        doc["refreshInterval"] = getRefreshInterval();
        doc["brightness"]      = getBrightness();
        doc["theme"]           = getTheme();
        doc["ssid"]            = WiFi.SSID();
        doc["ip"]              = WiFi.localIP().toString();
        String json;
        serializeJson(doc, json);
        otaServer.send(200, "application/json", json);
    });

    otaServer.on("/api/logs", HTTP_GET, []() {
        otaServer.send(200, "application/json", getLastLogs());
    });

    otaServer.on("/api/config", HTTP_POST, []() {
        JsonDocument doc;
        if (deserializeJson(doc, otaServer.arg("plain"))) {
            otaServer.send(400, "text/plain", "Invalid JSON");
            return;
        }

        if (doc["url"].is<const char*>())         setImageUrl(doc["url"].as<String>());
        if (doc["refreshInterval"].is<int>())      setRefreshInterval(doc["refreshInterval"]);
        if (doc["theme"].is<int>())                setTheme(doc["theme"]);
        saveConfig();

        otaServer.send(200, "text/plain", "Saved!");
    });

    otaServer.on("/brightness", HTTP_GET, []() {
        if (otaServer.hasArg("v")) {
            int val = otaServer.arg("v").toInt();
            setBrightness(val);
            displaySetBrightness(getBrightness());
            if (otaServer.arg("save") == "1") saveConfig();
        }
        otaServer.send(200, "text/plain", String(getBrightness()));
    });

    otaServer.on("/reboot", HTTP_GET, []() {
        otaServer.send(200, "text/plain", "Rebooting...");
        delay(500);
        ESP.restart();
    });

    otaServer.on("/resetwifi", HTTP_GET, []() {
        otaServer.send(200, "text/plain", "WiFi reset. Rebooting...");
        delay(500);
        WiFi.disconnect(true);
        ESP.restart();
    });

    otaServer.on("/update", HTTP_POST, []() {
        bool ok = !Update.hasError();
        if (ok) {
            otaServer.send(200, "text/html",
                "<html><body style='background:#222;color:#eee;font-family:sans-serif;text-align:center;padding-top:80px'>"
                "<h2>Firmware updated!</h2><p>Rebooting... will redirect when ready.</p>"
                "<script>setTimeout(function r(){fetch('/').then(()=>location.href='/').catch(()=>setTimeout(r,2000))},5000)</script>"
                "</body></html>");
        } else {
            otaServer.send(200, "text/html",
                "<html><body style='background:#222;color:#eee;font-family:sans-serif;text-align:center;padding-top:80px'>"
                "<h2 style='color:#d33'>Update failed!</h2><p><a href='/' style='color:#88f'>Back to settings</a></p>"
                "</body></html>");
        }
        delay(500);
        if (ok) ESP.restart();
    }, []() {
        HTTPUpload& upload = otaServer.upload();
        if (upload.status == UPLOAD_FILE_START) {
            uint32_t maxSize = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
            Update.begin(maxSize);
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            Update.write(upload.buf, upload.currentSize);
        } else if (upload.status == UPLOAD_FILE_END) {
            Update.end(true);
        }
    });

    otaServer.on("/upload", HTTP_POST, []() {
        otaServer.sendHeader("Location", "/");
        otaServer.send(303);
    }, []() {
        HTTPUpload& upload = otaServer.upload();
        if (upload.status == UPLOAD_FILE_START) {
            String path = "/" + upload.filename;
            uploadFile = LittleFS.open(path, "w");
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
        } else if (upload.status == UPLOAD_FILE_END) {
            if (uploadFile) uploadFile.close();
        }
    });

    otaServer.on("/files", HTTP_GET, []() {
        String html;
        Dir dir = LittleFS.openDir("/");
        while (dir.next()) {
            html += dir.fileName() + " (" + String(dir.fileSize()) + "b)";
            html += " <a href='/delete?f=/" + dir.fileName() + "'>del</a><br>";
        }
        if (html.isEmpty()) html = "No files";
        otaServer.send(200, "text/html", html);
    });

    otaServer.on("/delete", HTTP_GET, []() {
        String path = otaServer.arg("f");
        if (path.length() > 0 && LittleFS.exists(path)) {
            LittleFS.remove(path);
            otaServer.send(200, "text/plain", "Deleted " + path);
        } else {
            otaServer.send(404, "text/plain", "Not found");
        }
    });

    otaServer.begin();
    logInfo("OTA ready on port 80");
}

void otaHandle() {
    otaServer.handleClient();
}
