#pragma once

const char index_html[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>SpaceMouse ESP32</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 800px;
            margin: 0 auto;
            padding: 20px;
            background-color: #f0f0f0;
        }
        .container {
            background-color: white;
            padding: 20px;
            border-radius: 8px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
        h1 {
            color: #333;
        }
        .status {
            padding: 10px;
            margin: 10px 0;
            border-radius: 4px;
            font-weight: bold;
        }
        .status.connected {
            background-color: #4CAF50;
            color: white;
        }
        .status.disconnected {
            background-color: #f44336;
            color: white;
        }
        .status.no-mouse {
            background-color: #FF9800;
            color: white;
        }
        a {
            display: inline-block;
            margin: 15px 0;
            padding: 10px 20px;
            background-color: #2196F3;
            color: white;
            text-decoration: none;
            border-radius: 4px;
        }
        a:hover {
            background-color: #1976D2;
        }
        .data-grid {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 15px;
            margin: 20px 0;
        }
        .data-item {
            background-color: #f5f5f5;
            padding: 15px;
            border-radius: 4px;
        }
        .data-label {
            font-weight: bold;
            color: #666;
            font-size: 0.9em;
            margin-bottom: 5px;
        }
        .data-value {
            font-size: 1.2em;
            color: #333;
            font-family: 'Courier New', monospace;
        }
        .button-indicator {
            display: inline-block;
            width: 60px;
            height: 30px;
            line-height: 30px;
            text-align: center;
            margin: 5px;
            border-radius: 4px;
            background-color: #ddd;
            color: #666;
        }
        .button-indicator.pressed {
            background-color: #4CAF50;
            color: white;
        }
        .slider-container {
            width: 100%;
            height: 20px;
            background-color: #ddd;
            border-radius: 10px;
            position: relative;
            margin-top: 10px;
            overflow: hidden;
        }
        .slider-bar {
            height: 100%;
            background-color: #2196F3;
            position: absolute;
            left: 50%;
            width: 0;
            transition: width 0.1s, left 0.1s;
        }
        .slider-center {
            position: absolute;
            left: 50%;
            top: 0;
            bottom: 0;
            width: 2px;
            background-color: #666;
            transform: translateX(-1px);
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>SpaceMouse ESP32+Teensy</h1>
        <div id="status" class="status disconnected">Status: Disconnected</div>
        <a href="/wifi">WiFi Manager</a> $AUTH_INFO$
        
        <h2>Translation</h2>
        <div class="data-grid">
            <div class="data-item">
                <div class="data-label">X</div>
                <div class="data-value" id="tx">0</div>
                <div class="slider-container">
                    <div class="slider-center"></div>
                    <div class="slider-bar" id="slider-tx"></div>
                </div>
            </div>
            <div class="data-item">
                <div class="data-label">Y</div>
                <div class="data-value" id="ty">0</div>
                <div class="slider-container">
                    <div class="slider-center"></div>
                    <div class="slider-bar" id="slider-ty"></div>
                </div>
            </div>
            <div class="data-item">
                <div class="data-label">Z</div>
                <div class="data-value" id="tz">0</div>
                <div class="slider-container">
                    <div class="slider-center"></div>
                    <div class="slider-bar" id="slider-tz"></div>
                </div>
            </div>
        </div>
        
        <h2>Rotation</h2>
        <div class="data-grid">
            <div class="data-item">
                <div class="data-label">X</div>
                <div class="data-value" id="rx">0</div>
                <div class="slider-container">
                    <div class="slider-center"></div>
                    <div class="slider-bar" id="slider-rx"></div>
                </div>
            </div>
            <div class="data-item">
                <div class="data-label">Y</div>
                <div class="data-value" id="ry">0</div>
                <div class="slider-container">
                    <div class="slider-center"></div>
                    <div class="slider-bar" id="slider-ry"></div>
                </div>
            </div>
            <div class="data-item">
                <div class="data-label">Z</div>
                <div class="data-value" id="rz">0</div>
                <div class="slider-container">
                    <div class="slider-center"></div>
                    <div class="slider-bar" id="slider-rz"></div>
                </div>
            </div>
        </div>
        
        <h2>Buttons</h2>
        <div>
            <span id="btn0" class="button-indicator">Left</span>
            <span id="btn1" class="button-indicator">Right</span>
        </div>
    </div>
    
    <script>
        var ws;
        var reconnectInterval;
        
        function updateSlider(id, value) {
            var slider = document.getElementById('slider-' + id);
            var percent = (value / 350) * 50; // Convert to percentage (50% = full range one side)
            
            if (value >= 0) {
                slider.style.left = '50%';
                slider.style.width = Math.abs(percent) + '%';
            } else {
                slider.style.left = (50 + percent) + '%';
                slider.style.width = Math.abs(percent) + '%';
            }
        }
        
        function connect() {
            ws = new WebSocket('ws://' + window.location.hostname + '/ws');
            
            ws.onopen = function() {
                console.log('WebSocket connected');
                clearInterval(reconnectInterval);
            };
            
            ws.onmessage = function(event) {
                var data = JSON.parse(event.data);
                
                // Update connection status
                var statusDiv = document.getElementById('status');
                if (data.state === 2) { // CONNECTED
                    statusDiv.className = 'status connected';
                    statusDiv.textContent = 'Status: Connected';
                } else if (data.state === 1) { // NO_MOUSE
                    statusDiv.className = 'status no-mouse';
                    statusDiv.textContent = 'Status: No Mouse';
                } else { // DISCONNECTED
                    statusDiv.className = 'status disconnected';
                    statusDiv.textContent = 'Status: Disconnected';
                }
                
                // Update translation values
                document.getElementById('tx').textContent = data.x;
                document.getElementById('ty').textContent = data.y;
                document.getElementById('tz').textContent = data.z;
                updateSlider('tx', data.x);
                updateSlider('ty', data.y);
                updateSlider('tz', data.z);
                
                // Update rotation values
                document.getElementById('rx').textContent = data.rx;
                document.getElementById('ry').textContent = data.ry;
                document.getElementById('rz').textContent = data.rz;
                updateSlider('rx', data.rx);
                updateSlider('ry', data.ry);
                updateSlider('rz', data.rz);
                
                // Update button states
                var btn0 = document.getElementById('btn0');
                var btn1 = document.getElementById('btn1');
                
                if (data.buttons & 1) {
                    btn0.classList.add('pressed');
                } else {
                    btn0.classList.remove('pressed');
                }
                
                if (data.buttons & 2) {
                    btn1.classList.add('pressed');
                } else {
                    btn1.classList.remove('pressed');
                }
            };
            
            ws.onclose = function() {
                console.log('WebSocket disconnected');
                // Try to reconnect every 2 seconds
                reconnectInterval = setInterval(connect, 2000);
            };
            
            ws.onerror = function(error) {
                console.log('WebSocket error:', error);
            };
        }
        
        connect();
    </script>
</body>
</html>
)rawliteral";
