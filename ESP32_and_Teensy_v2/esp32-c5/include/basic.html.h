#pragma once

const char basic_html[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>SpaceMouse ESP32 - Basic</title>
    <link rel="stylesheet" href="/styles.css">
</head>
<body>
    <div class="container">
        <h1>SpaceMouse ESP32+Teensy (Basic)</h1>
        <div class="nav">
            <a href="/">Home</a>
            <a href="/3d">3D Pose</a>
            <a href="/wifi">WiFi Manager</a>
            <span class="auth-info">$AUTH_INFO$</span>
        </div>
        <div id="status" class="status disconnected">Status: Disconnected</div>
        
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
            var percent = (value / 350) * 50;
            
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
                
                var statusDiv = document.getElementById('status');
                if (data.state === 2) {
                    statusDiv.className = 'status connected';
                    statusDiv.textContent = 'Status: Connected';
                } else if (data.state === 1) {
                    statusDiv.className = 'status no-mouse';
                    statusDiv.textContent = 'Status: No Mouse';
                } else {
                    statusDiv.className = 'status disconnected';
                    statusDiv.textContent = 'Status: Disconnected';
                }
                
                document.getElementById('tx').textContent = data.x;
                document.getElementById('ty').textContent = data.y;
                document.getElementById('tz').textContent = data.z;
                updateSlider('tx', data.x);
                updateSlider('ty', data.y);
                updateSlider('tz', data.z);
                
                document.getElementById('rx').textContent = data.rx;
                document.getElementById('ry').textContent = data.ry;
                document.getElementById('rz').textContent = data.rz;
                updateSlider('rx', data.rx);
                updateSlider('ry', data.ry);
                updateSlider('rz', data.rz);
                
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
