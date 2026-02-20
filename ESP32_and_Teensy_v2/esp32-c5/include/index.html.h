#pragma once

const char index_html[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>SpaceMouse ESP32</title>
    <link rel="stylesheet" href="/styles.css">
</head>
<body>
    <div class="container">
        <h1>SpaceMouse ESP32+Teensy</h1>
        <p>Select a page:</p>
        <div class="nav">
            <a href="/basic">Basic Dashboard</a>
            <a href="/3d">3D Pose</a>
            <a href="/wifi">WiFi Manager</a>
            <span class="auth-info">$AUTH_INFO$</span>
        </div>
    </div>
</body>
</html>
)rawliteral";
