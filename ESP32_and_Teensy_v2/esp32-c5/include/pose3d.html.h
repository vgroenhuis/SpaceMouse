#pragma once

const char pose3d_html[] = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>SpaceMouse ESP32 - 3D Pose</title>
    <link rel="stylesheet" href="/styles.css">
</head>
<body>
    <div class="container">
        <h1>3D Pose Visualization</h1>
        <div class="nav">
            <a href="/">Home</a>
            <a href="/basic">Basic Dashboard</a>
            <a href="/wifi">WiFi Manager</a>
            <span class="auth-info">$AUTH_INFO$</span>
        </div>

        <div id="status" class="status disconnected">Status: Disconnected</div>

        <div class="row">
            <div class="panel">
                <canvas id="poseCanvas" width="600" height="420"></canvas>
            </div>
            <div class="panel">
                <div class="values" id="values">
                    T: (0, 0, 0)<br>
                    R: (0, 0, 0)<br>
                    Buttons: 0
                </div>
            </div>
        </div>
    </div>

    <script>
        var ws;
        var reconnectInterval;
        var latest = { state: 0, x: 0, y: 0, z: 0, rx: 0, ry: 0, rz: 0, buttons: 0 };

        var canvas = document.getElementById('poseCanvas');
        var ctx = canvas.getContext('2d');

        function clamp(v, min, max) {
            return Math.min(max, Math.max(min, v));
        }

        function toRad(v) {
            return (v / 350) * (Math.PI / 2);
        }

        function matMul(a, b) {
            return [
                a[0]*b[0] + a[1]*b[3] + a[2]*b[6],
                a[0]*b[1] + a[1]*b[4] + a[2]*b[7],
                a[0]*b[2] + a[1]*b[5] + a[2]*b[8],
                a[3]*b[0] + a[4]*b[3] + a[5]*b[6],
                a[3]*b[1] + a[4]*b[4] + a[5]*b[7],
                a[3]*b[2] + a[4]*b[5] + a[5]*b[8],
                a[6]*b[0] + a[7]*b[3] + a[8]*b[6],
                a[6]*b[1] + a[7]*b[4] + a[8]*b[7],
                a[6]*b[2] + a[7]*b[5] + a[8]*b[8]
            ];
        }

        function rotatePoint(p, r) {
            return {
                x: r[0] * p.x + r[1] * p.y + r[2] * p.z,
                y: r[3] * p.x + r[4] * p.y + r[5] * p.z,
                z: r[6] * p.x + r[7] * p.y + r[8] * p.z
            };
        }

        function project(p) {
            var cameraZ = 4.0;
            var z = p.z + cameraZ;
            var f = 220 / z;
            return {
                x: canvas.width * 0.5 + p.x * f,
                y: canvas.height * 0.5 + p.y * f
            };
        }

        function drawLine3D(a, b, color, width) {
            var pa = project(a);
            var pb = project(b);
            ctx.strokeStyle = color;
            ctx.lineWidth = width || 2;
            ctx.beginPath();
            ctx.moveTo(pa.x, pa.y);
            ctx.lineTo(pb.x, pb.y);
            ctx.stroke();
        }

        function draw() {
            ctx.clearRect(0, 0, canvas.width, canvas.height);

            var rx = toRad(latest.rx);
            var ry = toRad(latest.ry);
            var rz = toRad(latest.rz);

            var cx = Math.cos(rx), sx = Math.sin(rx);
            var cy = Math.cos(ry), sy = Math.sin(ry);
            var cz = Math.cos(rz), sz = Math.sin(rz);

            var rotX = [1,0,0, 0,cx,-sx, 0,sx,cx];
            var rotY = [cy,0,sy, 0,1,0, -sy,0,cy];
            var rotZ = [cz,-sz,0, sz,cz,0, 0,0,1];
            var rot = matMul(rotZ, matMul(rotY, rotX));

            var tx = clamp(latest.x / 350, -1, 1) * 1.2;
            var ty = clamp(latest.y / 350, -1, 1) * 1.2;
            var tz = clamp(latest.z / 350, -1, 1) * 1.2;

            var origin = { x: tx, y: ty, z: tz };

            drawLine3D({x: -1.8, y: 0, z: 0}, {x: 1.8, y: 0, z: 0}, '#dd4444', 1);
            drawLine3D({x: 0, y: -1.8, z: 0}, {x: 0, y: 1.8, z: 0}, '#44aa44', 1);
            drawLine3D({x: 0, y: 0, z: -1.8}, {x: 0, y: 0, z: 1.8}, '#4477dd', 1);

            var cube = [
                {x:-0.5,y:-0.5,z:-0.5}, {x:0.5,y:-0.5,z:-0.5},
                {x:0.5,y:0.5,z:-0.5}, {x:-0.5,y:0.5,z:-0.5},
                {x:-0.5,y:-0.5,z:0.5}, {x:0.5,y:-0.5,z:0.5},
                {x:0.5,y:0.5,z:0.5}, {x:-0.5,y:0.5,z:0.5}
            ];

            for (var i = 0; i < cube.length; i++) {
                var p = rotatePoint(cube[i], rot);
                cube[i] = { x: p.x + origin.x, y: p.y + origin.y, z: p.z + origin.z };
            }

            var edges = [
                [0,1],[1,2],[2,3],[3,0],
                [4,5],[5,6],[6,7],[7,4],
                [0,4],[1,5],[2,6],[3,7]
            ];
            for (var e = 0; e < edges.length; e++) {
                drawLine3D(cube[edges[e][0]], cube[edges[e][1]], '#222', 2);
            }

            var xAxis = rotatePoint({x:0.9,y:0,z:0}, rot);
            var yAxis = rotatePoint({x:0,y:0.9,z:0}, rot);
            var zAxis = rotatePoint({x:0,y:0,z:0.9}, rot);
            drawLine3D(origin, {x: origin.x + xAxis.x, y: origin.y + xAxis.y, z: origin.z + xAxis.z}, '#dd4444', 3);
            drawLine3D(origin, {x: origin.x + yAxis.x, y: origin.y + yAxis.y, z: origin.z + yAxis.z}, '#44aa44', 3);
            drawLine3D(origin, {x: origin.x + zAxis.x, y: origin.y + zAxis.y, z: origin.z + zAxis.z}, '#4477dd', 3);
        }

        function updateText() {
            document.getElementById('values').innerHTML =
                'T: (' + latest.x + ', ' + latest.y + ', ' + latest.z + ')<br>' +
                'R: (' + latest.rx + ', ' + latest.ry + ', ' + latest.rz + ')<br>' +
                'Buttons: ' + latest.buttons;
        }

        function updateStatus() {
            var statusDiv = document.getElementById('status');
            if (latest.state === 2) {
                statusDiv.className = 'status connected';
                statusDiv.textContent = 'Status: Connected';
            } else if (latest.state === 1) {
                statusDiv.className = 'status no-mouse';
                statusDiv.textContent = 'Status: No Mouse';
            } else {
                statusDiv.className = 'status disconnected';
                statusDiv.textContent = 'Status: Disconnected';
            }
        }

        function connect() {
            ws = new WebSocket('ws://' + window.location.hostname + '/ws');

            ws.onopen = function() {
                clearInterval(reconnectInterval);
            };

            ws.onmessage = function(event) {
                latest = JSON.parse(event.data);
                updateStatus();
                updateText();
                draw();
            };

            ws.onclose = function() {
                reconnectInterval = setInterval(connect, 2000);
            };
        }

        draw();
        connect();
    </script>
</body>
</html>
)rawliteral";
