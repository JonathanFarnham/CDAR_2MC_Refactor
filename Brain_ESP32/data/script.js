document.addEventListener("DOMContentLoaded", () => {
    
    //Page Detection----------------------------------------------------------------
    const isHeatmapPage = document.getElementById('heatmapCanvas') !== null;
    const isManualPage = document.getElementById('manual-page') !== null;

    // LOGIC FOR HEATMAP------------------------------------------------------------
    if (isHeatmapPage) {
        const canvas = document.getElementById('heatmapCanvas');
        const ctx = canvas.getContext('2d');
        const tableBody = document.getElementById('table-body');
        const gridTitleInput = document.getElementById('grid-title');
        
        canvas.width = 800;
        canvas.height = 800;

        let selectedTurn = "left"; 
        let dataCount = 1;
        let isSimulating = false;
        let simInterval = null;
        let dataPoints = []; 

        // Helper Functions
        function getColorForVoltage(mv) {
            if (mv <= -600) return '#FFC0CB'; 
            if (mv <= -500) return "#FF0000"; 
            if (mv <= -400) return "#FFA500"; 
            if (mv <= -300) return "#FFFF00"; 
            if (mv <= -200) return "#008000"; 
            if (mv <= -100) return "#2ECC71"; 
            if (mv <= 0)    return "#008080"; 
            if (mv <= 100)  return "#0000FF"; 
            return "#00008B";                 
        }

        function renderFullHeatmap() {
            const gridW = parseFloat(document.getElementById('width').value) || 1;
            const gridH = parseFloat(document.getElementById('length').value) || 1;
            const cellSize = 10; 
            
            ctx.clearRect(0, 0, canvas.width, canvas.height);

            for (let x = 0; x < canvas.width; x += cellSize) {
                for (let y = 0; y < canvas.height; y += cellSize) {
                    let closestPoint = null;
                    let minDistance = Infinity;

                    dataPoints.forEach(pt => {
                        const px = pt.x * (canvas.width / gridW);
                        const py = pt.y * (canvas.height / gridH);
                        const dist = Math.sqrt((x - px) ** 2 + (y - py) ** 2);

                        if (dist < minDistance) {
                            minDistance = dist;
                            closestPoint = pt;
                        }
                    });

                    if (closestPoint && minDistance < 150) {
                        ctx.fillStyle = getColorForVoltage(closestPoint.mv);
                        ctx.globalAlpha = Math.max(0, 1 - (minDistance / 150));
                        ctx.fillRect(x, y, cellSize, cellSize);
                    }
                }
            }
            ctx.globalAlpha = 1.0;
        }

        function generateLegend() {
            const legendBar = document.getElementById('legend-bar');
            const legendLabels = document.getElementById('legend-labels');
            const steps = [
                { mv: -600, label: "≤ -600" }, { mv: -500, label: "-500" },
                { mv: -400, label: "-400" }, { mv: -300, label: "-300" },
                { mv: -200, label: "-200" }, { mv: -100, label: "-100" },
                { mv: 0, label: "0" }, { mv: 100, label: "100" }, { mv: 200, label: ">100" }
            ];
            legendBar.innerHTML = '';
            legendLabels.innerHTML = '';
            steps.forEach(step => {
                const block = document.createElement('div');
                block.style.flex = "1";
                block.style.backgroundColor = getColorForVoltage(step.mv);
                legendBar.appendChild(block);
                const label = document.createElement('span');
                label.innerText = step.label;
                legendLabels.appendChild(label);
            });
        }

        // Initialize
        generateLegend();

        // Data Handling
        window.onNewDataReceived = function(x, y, voltage) {
            const numX = parseFloat(x);
            const numY = parseFloat(y);

            if (tableBody.querySelector('.empty-msg')) tableBody.innerHTML = '';
            const row = document.createElement('tr');
            row.innerHTML = `
                <td>${dataCount++}</td>
                <td>${numX.toFixed(2)}</td>
                <td>${numY.toFixed(2)}</td>
                <td>${voltage} mV</td>
                <td><span class="status-pill status-success">Recorded</span></td>
            `;
            tableBody.appendChild(row);

            dataPoints.push({ x: numX, y: numY, mv: voltage });
            renderFullHeatmap();
        };

        const validateInputs = () => {
            const fieldIds = ['grid-title', 'length', 'width', 'passes'];
            let isValid = true;
            fieldIds.forEach(id => {
                const element = document.getElementById(id);
                if (!element.value.trim()) {
                    element.classList.remove('error-shake');
                    void element.offsetWidth; 
                    element.classList.add('error-shake');
                    isValid = false;
                    setTimeout(() => element.classList.remove('error-shake'), 600);
                }
            });
            if (!isValid) alert("Please fill in all highlighted fields.");
            return isValid;
        };

        // Event Listeners for Heatmap Page
        document.getElementById("export-menu-btn").onclick = (e) => {
            e.stopPropagation();
            document.getElementById("menu").classList.toggle("hidden");
        };
        document.addEventListener("click", () => document.getElementById("menu")?.classList.add("hidden"));

        document.getElementById('show-heatmap').onclick = function() {
            document.getElementById('heatmap-view').classList.remove('hidden');
            document.getElementById('table-view').classList.add('hidden');
            this.classList.add('active');
            document.getElementById('show-table').classList.remove('active');
        };
        document.getElementById('show-table').onclick = function() {
            document.getElementById('table-view').classList.remove('hidden');
            document.getElementById('heatmap-view').classList.add('hidden');
            this.classList.add('active');
            document.getElementById('show-heatmap').classList.remove('active');
        };

        document.querySelectorAll('.toggle-btn').forEach(btn => {
            btn.onclick = () => {
                document.querySelectorAll('.toggle-btn').forEach(b => b.classList.remove('active'));
                btn.classList.add('active');
                selectedTurn = btn.dataset.value;
            };
        });

        const fullscreenBtn = document.getElementById('fullscreen-btn');
        const heatmapArea = document.getElementById('heatmap-area');
        fullscreenBtn.onclick = () => {
            if (!document.fullscreenElement) {
                heatmapArea.requestFullscreen().catch(err => alert(err.message));
                fullscreenBtn.innerText = "✕ Exit Fullscreen";
            } else {
                document.exitFullscreen();
                fullscreenBtn.innerText = "⛶ Fullscreen View";
            }
        };

        document.getElementById('clear-data-btn').onclick = () => {
            if (confirm("Clear all data and reset grid?")) {
                if (simInterval) clearInterval(simInterval);
                isSimulating = false;
                ctx.clearRect(0, 0, canvas.width, canvas.height);
                tableBody.innerHTML = '<tr><td colspan="5" class="empty-msg">No data recorded.</td></tr>';
                dataPoints = [];
                dataCount = 1;
            }
        };

        document.getElementById('export-csv').onclick = () => {
            const title = gridTitleInput.value || "RobotData";
            let csv = "Point,X,Y,Voltage,Status\n";
            dataPoints.forEach((pt, i) => {
                csv += `${i+1},${pt.x},${pt.y},${pt.mv},Recorded\n`;
            });
            const blob = new Blob([csv], { type: 'text/csv' });
            const url = URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url; a.download = `${title}.csv`; a.click();
        };

        document.getElementById('export-png').onclick = () => {
            const a = document.createElement('a');
            a.href = canvas.toDataURL("image/png");
            a.download = `${gridTitleInput.value || 'Heatmap'}.png`;
            a.click();
        };

        document.getElementById('upload-btn').onclick = () => {
            if (!validateInputs()) return;
            const payload = {
                length: parseFloat(document.getElementById('length').value),
                width: parseFloat(document.getElementById('width').value),
                passes: parseInt(document.getElementById('passes').value),
                direction: document.querySelector('.toggle-btn.active').dataset.value
            };

            fetch('/api/robot/upload', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify(payload)
            }).then(() => alert("Uploaded to Robot!"));
        };

        document.getElementById('run-btn').onclick = () => {
            fetch('/api/robot/start', { method: 'POST' });
        
        };
    }
    // LOGIC FOR MANUAL CONTROL PAGE------------------------------------------------------------------------
    if (isManualPage) {
        const logDisplay = document.getElementById('command-log');
        const speedSlider = document.getElementById('speed-slider');
        const speedDisplay = document.getElementById('speed-display');

        
        //update the ui when slider moves
        if (speedSlider && speedDisplay)
        {
            speedSlider.addEventListener('input', (e) => {
                speedDisplay.innerText = e.target.value;
            });
        }
        
        const sendCommand = (cmd) => {
            const timestamp = new Date().toLocaleTimeString();
            const currentSpeed = speedSlider ? parseInt(speedSlider.value) : 1000;

            if(logDisplay) logDisplay.innerText = `[${timestamp}] Sending: ${cmd}`;
            // SEND DATA TO ESP32 with speed value
            fetch('/api/robot/move', { 
                method: 'POST', 
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ direction: cmd, speed: currentSpeed }) 
            })
            .then(response => {
                if (!response.ok) throw new Error("Robot did not acknowledge");
                return response.json();
            })
            .catch(err => {
                if(logDisplay) {
                    logDisplay.innerText += ` (Error: ${err.message})`;
                    logDisplay.style.color = "var(--danger)";
                }
            });

            // Visual feedback
            if(logDisplay) logDisplay.style.color = cmd === "STOP" ? "var(--danger)" : "var(--primary)";
        };

        document.querySelectorAll('.d-btn').forEach(btn => {
            // Mouse events
            btn.addEventListener('mousedown', () => {
                btn.classList.add('active-press');
                sendCommand(btn.dataset.cmd);
            });
            
            btn.addEventListener('mouseup', () => {
                btn.classList.remove('active-press');
                if (btn.dataset.cmd !== "STOP") {
                    sendCommand("STOP"); // Auto-stop on release
                }
            });

            // Touch events for mobile
            btn.addEventListener('touchstart', (e) => {
                e.preventDefault(); // Prevent scrolling
                btn.classList.add('active-press');
                sendCommand(btn.dataset.cmd);
            });

            btn.addEventListener('touchend', (e) => {
                e.preventDefault();
                btn.classList.remove('active-press');
                if (btn.dataset.cmd !== "STOP") sendCommand("STOP");
            });
        });

        // Keyboard support
        document.addEventListener('keydown', (e) => {
            let keyBtn = null;
            if (e.key === "ArrowUp") keyBtn = document.getElementById('btn-up');
            if (e.key === "ArrowDown") keyBtn = document.getElementById('btn-down');
            if (e.key === "ArrowLeft") keyBtn = document.getElementById('btn-left');
            if (e.key === "ArrowRight") keyBtn = document.getElementById('btn-right');
            if (e.key === " " || e.key === "Space") keyBtn = document.getElementById('btn-stop');

            if (keyBtn) {
                keyBtn.classList.add('active-press');
                sendCommand(keyBtn.dataset.cmd);
            }
        });

        document.addEventListener('keyup', (e) => {
            if (["ArrowUp", "ArrowDown", "ArrowLeft", "ArrowRight"].includes(e.key)) {
                document.querySelectorAll('.d-btn').forEach(b => b.classList.remove('active-press'));
                sendCommand("STOP");
            }
        });
    }
});