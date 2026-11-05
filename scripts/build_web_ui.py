import gzip
import os

def build():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    html_file = os.path.join(root, 'src', 'main', 'web_ui.html')
    header_file = os.path.join(root, 'src', 'main', 'web_ui.h')

    with open(html_file, 'rb') as f:
        raw_bytes = f.read()

    gz_bytes = gzip.compress(raw_bytes, compresslevel=9)

    print(f"Original HTML size: {len(raw_bytes)} bytes")
    print(f"Gzipped HTML size:  {len(gz_bytes)} bytes ({100 - (len(gz_bytes)*100//len(raw_bytes))}% reduction)")

    manifest_json = '''{
  "name": "SmartHome Hub",
  "short_name": "SmartHome",
  "start_url": "/",
  "display": "standalone",
  "background_color": "#0f172a",
  "theme_color": "#2563eb",
  "orientation": "portrait-primary",
  "icons": [
    {
      "src": "data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'%3E%3Crect width='100' height='100' rx='20' fill='%232563eb'/%3E%3Cpath d='M50 20 L20 48 L30 48 L30 78 L45 78 L45 60 L55 60 L55 78 L70 78 L70 48 L80 48 Z' fill='white'/%3E%3C/svg%3E",
      "sizes": "192x192 512x512",
      "type": "image/svg+xml"
    }
  ]
}'''

    hex_array = []
    for i, b in enumerate(gz_bytes):
        if i % 16 == 0:
            hex_array.append("\n  ")
        hex_array.append(f"0x{b:02x}, ")

    hex_str = "".join(hex_array).rstrip(", \n")

    out_content = f'''#ifndef SMARTHOME_WEB_UI_H
#define SMARTHOME_WEB_UI_H

#include <Arduino.h>

// Web App Manifest for Android PWA (Add to Home Screen)
const char MANIFEST_JSON[] PROGMEM = R"rawliteral({manifest_json})rawliteral";

// Embedded Modern Single Page Application (Gzipped to optimize ESP32 RAM & Flash)
// Source file: src/main/web_ui.html ({len(raw_bytes)} bytes -> {len(gz_bytes)} bytes)
const size_t INDEX_HTML_GZ_LEN = {len(gz_bytes)};
const uint8_t INDEX_HTML_GZ[] PROGMEM = {{{hex_str}
}};

#endif // SMARTHOME_WEB_UI_H
'''

    with open(header_file, 'w', encoding='utf-8') as f:
        f.write(out_content)

    print(f"Generated {header_file} successfully.")

if __name__ == '__main__':
    build()
