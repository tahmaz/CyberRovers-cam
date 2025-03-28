#ifndef WEBPAGE_H
#define WEBPAGE_H

const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Camera Stream Controller</title>
    <style>
        body {
            margin: 0;
            padding: 20px;
            background: #333;
            color: #fff;
            font-family: Arial, sans-serif;
        }
        .main-wrapper {
            display: flex;
            flex-direction: column;
            min-height: 100vh; /* Ensures content fits or scrolls */
            justify-content: space-between; /* Distributes space */
        }
        .container {
            flex: 1; /* Allows it to grow but respects other elements */
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 20px;
        }
        .camera-view {
            max-width: 640px;
            max-height: 480px;
            transform: rotate(180deg);
        }
        .snapshot-image {
            max-width: 640px;
            max-height: 480px;
            transform: rotate(180deg);
            position: absolute;
            top: 50%;
            left: 50%;
            transform-origin: center;
            transform: translate(-50%, -50%) rotate(180deg);
        }
        .controls {
            display: flex;
            flex-direction: column;
            gap: 10px;
        }
        .label {
            font-size: 18px;
            text-align: center;
        }
        button {
            padding: 15px;
            font-size: 16px;
            background: #4CAF50;
            color: white;
            border: none;
            border-radius: 5px;
            cursor: pointer;
            width: 100px;
            transition: transform 0.1s ease, background 0.1s ease;
        }
        button:hover {
            background: #45a049;
        }
        button:active {
            transform: scale(0.95);
            background: #388E3C;
        }
        .emoji-button {
            background: #2196F3;
        }
        .emoji-button:hover {
            background: #1e88e5;
        }
        .emoji-button:active {
            background: #1976D2;
        }
        .snapshot-button {
            background: #9C27B0;
        }
        .snapshot-button:hover {
            background: #8E24AA;
        }
        .snapshot-button:active {
            background: #7B1FA2;
        }
        .refresh-button {
            background: #FF5722;
        }
        .refresh-button:hover {
            background: #E64A19;
        }
        .refresh-button:active {
            background: #D84315;
        }
        .circle-controls {
            position: relative;
            width: 120px;
            height: 120px;
            border-radius: 50%;
            background: #555;
            display: flex;
            justify-content: center;
            align-items: center;
        }
        .circle-button {
            position: absolute;
            width: 50px;
            height: 50px;
            background: #4CAF50;
            border: none;
            border-radius: 50%;
            color: white;
            font-size: 20px;
            cursor: pointer;
            display: flex;
            justify-content: center;
            align-items: center;
            transition: transform 0.1s ease, background 0.1s ease;
        }
        .circle-button:hover {
            background: #45a049;
        }
        .circle-button:active {
            transform: scale(0.9);
            background: #388E3C;
        }
        .up {
            top: 0;
            transform: translateY(-50%);
        }
        .up:active {
            transform: translateY(-50%) scale(0.9);
        }
        .down {
            bottom: 0;
            transform: translateY(50%);
        }
        .down:active {
            transform: translateY(50%) scale(0.9);
        }
        .left {
            left: 0;
            transform: translateX(-50%);
        }
        .left:active {
            transform: translateX(-50%) scale(0.9);
        }
        .right {
            right: 0;
            transform: translateX(50%);
        }
        .right:active {
            transform: translateX(50%) scale(0.9);
        }
        .bottom-section {
            margin-top: 20px;
            margin-bottom: 20px; /* Add some spacing at the bottom */
            display: flex;
            justify-content: center;
            align-items: center;
            gap: 10px;
        }
        input[type="text"] {
            padding: 10px;
            font-size: 16px;
            width: 200px;
            border: none;
            border-radius: 5px;
        }
        .text-input button {
            background: #FF9800;
            width: 120px;
        }
        .text-input button:hover {
            background: #F57C00;
        }
        .text-input button:active {
            background: #E64A19;
        }
    </style>
</head>
<body>
    <div class="main-wrapper">
        <div class="container">
            <div class="controls">
                <div class="label">Head</div>
                <button onclick="fetch('/up')">Up</button>
                <button onclick="fetch('/down')">Down</button>
                <div class="label">Emoji</div>
                <button class="emoji-button" onclick="fetch('/oled1')">E1</button>
                <button class="emoji-button" onclick="fetch('/oled2')">E2</button>
                <button class="snapshot-button" onclick="takeSnapshot()">Snapshot</button>
                <button class="refresh-button" onclick="location.reload()">Refresh</button>
            </div>
            <img src="/stream" class="camera-view" id="cameraFeed">
            <div class="circle-controls">
                <button class="circle-button up" onclick="fetch('/movef')">F</button>
                <button class="circle-button down" onclick="fetch('/moveb')">B</button>
                <button class="circle-button left" onclick="fetch('/movel')">L</button>
                <button class="circle-button right" onclick="fetch('/mover')">R</button>
            </div>
        </div>
        <div class="bottom-section">
            <input type="text" id="oledText" placeholder="Enter text">
            <div class="text-input">
                <button onclick="sendText()">Send Text</button>
            </div>
        </div>
    </div>
    <script>
        function sendText() {
            const text = document.getElementById('oledText').value;
            fetch('/sendText', {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: 'text=' + encodeURIComponent(text)
            }).then(response => response.text())
              .then(data => console.log(data));
        }
        function takeSnapshot() {
            fetch('/snapshot')
                .then(response => {
                    if (!response.ok) throw new Error('Snapshot failed');
                    return response.blob();
                })
                .then(blob => {
                    const url = URL.createObjectURL(blob);
                    const img = new Image();
                    img.src = url;
                    img.className = 'snapshot-image';
                    document.body.appendChild(img);
                    setTimeout(() => {
                        document.body.removeChild(img);
                        URL.revokeObjectURL(url);
                    }, 5000);
                })
                .catch(error => console.error('Snapshot error:', error));
        }
    </script>
</body>
</html>
)rawliteral";

#endif