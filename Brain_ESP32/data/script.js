document.addEventListener("DOMContentLoaded", () => {

    // Page detection
    const isHeatmapPage = document.getElementById('heatmapCanvas') !== null;
    const isManualPage  = document.getElementById('manual-page')   !== null;

    /* ========================================================================
     * HEATMAP PAGE
     * ====================================================================== */
    if (isHeatmapPage) {
        const canvas       = document.getElementById('heatmapCanvas');
        const ctx          = canvas.getContext('2d');
        const tableBody    = document.getElementById('table-body');
        const titleInput   = document.getElementById('grid-title');
        const runStatus    = document.getElementById('run-status');

        canvas.width  = 800;
        canvas.height = 800;

        let selectedTurn = "left";
        let lastDataJSON = "";     // Cache — only re-render on actual changes
        let pollTimer    = null;
        let pollInterval = 0;
        let generator    = null;   // ContourMapGenerator instance

        /* ------------------------------------------------------------------
         * Build a ContourMapGenerator from the current UI settings
         * ------------------------------------------------------------------ */
        function makeGenerator() {
            const contourMin = parseFloat(document.getElementById('contour-min').value) || -350;
            const contourMax = parseFloat(document.getElementById('contour-max').value) || -50;
            return new ContourMapGenerator({ contourMin, contourMax });
        }

        /* ------------------------------------------------------------------
         * ASTM risk label (mirrors getASTMRiskLabel() in electrode_sensor.cpp)
         * ------------------------------------------------------------------ */
        function astmLabel(mv) {
            if (mv < -500)  return "Severe Corrosion";
            if (mv < -350)  return "High Probability of Active Corrosion (>90%)";
            if (mv <= -200) return "Intermediate Corrosion Risk";
            return "Low Probability of Corrosion (<10%)";
        }

        /* ------------------------------------------------------------------
         * Rebuild the legend bar using the generator's colorscale
         * ------------------------------------------------------------------ */
        function updateLegend() {
            generator = makeGenerator();
            generator.buildLegendHTML(
                document.getElementById('legend-bar'),
                document.getElementById('legend-labels')
            );
        }

        /* ------------------------------------------------------------------
         * Render the contour map and data table from a server response object
         * ------------------------------------------------------------------ */
        function renderResponse(response) {
            const data = response.data;
            if (!data || data.length === 0) return;

            generator = makeGenerator();

            // Contour map canvas
            generator.renderToCanvas(data, canvas, {
                title:          titleInput.value || 'Half-Cell Survey',
                showDataPoints: document.getElementById('show-data-pts').checked
            });

            // Data table — rebuild whenever data changes
            if (tableBody.querySelector('.empty-msg')) tableBody.innerHTML = '';

            // Only rebuild the table if the data actually changed (avoid flicker)
            const dataKey = JSON.stringify({ passes: response.passes, samples: response.samplesPerPass });
            if (dataKey !== lastDataJSON) {
                lastDataJSON = dataKey;
                tableBody.innerHTML = '';

                data.forEach((passRow, passIdx) => {
                    passRow.forEach((mv, sampleIdx) => {
                        const row = document.createElement('tr');
                        row.innerHTML = `
                            <td>${passIdx + 1}</td>
                            <td>${sampleIdx + 1}</td>
                            <td>${parseFloat(mv).toFixed(2)} mV</td>
                            <td>${astmLabel(mv)}</td>
                        `;
                        tableBody.appendChild(row);
                    });
                });

                const titleEl = document.getElementById('display-grid-title');
                if (titleEl) titleEl.textContent = `Grid: ${titleInput.value || 'Unnamed'} — ${response.passes} passes`;
            }

            // Status indicator
            if (runStatus) {
                runStatus.textContent   = response.isRunning ? '🟢 Running' : '⚪ Idle';
                runStatus.style.color   = response.isRunning ? '#16a34a' : '#64748b';
            }
        }

        /* ------------------------------------------------------------------
         * Poll /api/robot/data at the given interval (ms)
         * ------------------------------------------------------------------ */
        function startPolling(intervalMs) {
            stopPolling();
            poll(); // Immediate first fetch
            pollTimer = setInterval(poll, intervalMs);
        }

        function stopPolling() {
            if (pollTimer) { clearInterval(pollTimer); pollTimer = null; pollInterval = 0; }
        }

        function poll() {
            fetch('/api/robot/data')
                .then(r => { if (!r.ok) throw new Error(r.status); return r.json(); })
                .then(response => {
                    renderResponse(response);
                    // Speed up polling while the run is active
                    const target = response.isRunning ? 2000 : 5000;
                    if (pollTimer && getIntervalMs() !== target) startPolling(target);
                })
                .catch(() => {
                    if (runStatus) runStatus.textContent = '⚠️ No connection';
                });
        }

        // Track the current interval so we can avoid unnecessary restarts
        function getIntervalMs() { return pollTimer ? pollTimer._repeat : 0; }

        /* ------------------------------------------------------------------
         * Input validation
         * ------------------------------------------------------------------ */
        function validateInputs() {
            const fieldIds = ['grid-title', 'length', 'width', 'passes'];
            let isValid = true;
            fieldIds.forEach(id => {
                const el = document.getElementById(id);
                if (!el.value.trim()) {
                    el.classList.remove('error-shake');
                    void el.offsetWidth;
                    el.classList.add('error-shake');
                    isValid = false;
                    setTimeout(() => el.classList.remove('error-shake'), 600);
                }
            });
            if (!isValid) alert("Please fill in all highlighted fields.");
            return isValid;
        }

        /* ------------------------------------------------------------------
         * Event listeners — Heatmap page
         * ------------------------------------------------------------------ */

        // Export menu
        document.getElementById("export-menu-btn").onclick = (e) => {
            e.stopPropagation();
            document.getElementById("menu").classList.toggle("hidden");
        };
        document.addEventListener("click", () => document.getElementById("menu")?.classList.add("hidden"));

        // Tab switching
        document.getElementById('show-heatmap').onclick = function () {
            document.getElementById('heatmap-view').classList.remove('hidden');
            document.getElementById('table-view').classList.add('hidden');
            this.classList.add('active');
            document.getElementById('show-table').classList.remove('active');
        };
        document.getElementById('show-table').onclick = function () {
            document.getElementById('table-view').classList.remove('hidden');
            document.getElementById('heatmap-view').classList.add('hidden');
            this.classList.add('active');
            document.getElementById('show-heatmap').classList.remove('active');
        };

        // Direction toggle
        document.querySelectorAll('.toggle-btn').forEach(btn => {
            btn.onclick = () => {
                document.querySelectorAll('.toggle-btn').forEach(b => b.classList.remove('active'));
                btn.classList.add('active');
                selectedTurn = btn.dataset.value;
            };
        });

        // Fullscreen
        const fullscreenBtn  = document.getElementById('fullscreen-btn');
        const heatmapArea    = document.getElementById('heatmap-area');
        fullscreenBtn.onclick = () => {
            if (!document.fullscreenElement) {
                heatmapArea.requestFullscreen().catch(err => alert(err.message));
                fullscreenBtn.innerText = "✕ Exit Fullscreen";
            } else {
                document.exitFullscreen();
                fullscreenBtn.innerText = "⛶ Fullscreen View";
            }
        };

        // Contour settings — re-render immediately when changed
        ['contour-min', 'contour-max'].forEach(id => {
            document.getElementById(id).addEventListener('change', () => {
                updateLegend();
                poll(); // Force a re-render with new scale
            });
        });
        document.getElementById('show-data-pts').addEventListener('change', poll);

        // Clear data
        document.getElementById('clear-data-btn').onclick = () => {
            if (confirm("Clear all data and reset display?")) {
                ctx.clearRect(0, 0, canvas.width, canvas.height);
                tableBody.innerHTML = '<tr><td colspan="4" class="empty-msg">No data recorded.</td></tr>';
                lastDataJSON = "";
                if (runStatus) runStatus.textContent = 'Idle';
            }
        };

        // Export CSV
        document.getElementById('export-csv').onclick = () => {
            fetch('/api/robot/data')
                .then(r => r.json())
                .then(response => {
                    const title = titleInput.value || 'RobotData';
                    let csv = "Pass,Sample,Voltage_mV,ASTM_Assessment\n";
                    response.data.forEach((passRow, passIdx) => {
                        passRow.forEach((mv, sampleIdx) => {
                            csv += `${passIdx + 1},${sampleIdx + 1},${parseFloat(mv).toFixed(2)},"${astmLabel(mv)}"\n`;
                        });
                    });
                    const a    = document.createElement('a');
                    a.href     = URL.createObjectURL(new Blob([csv], { type: 'text/csv' }));
                    a.download = `${title}.csv`;
                    a.click();
                });
        };

        // Export PNG
        document.getElementById('export-png').onclick = () => {
            const a    = document.createElement('a');
            a.href     = canvas.toDataURL("image/png");
            a.download = `${titleInput.value || 'ContourMap'}.png`;
            a.click();
        };

        // Upload configuration to robot
        document.getElementById('upload-btn').onclick = () => {
            if (!validateInputs()) return;
            const payload = {
                length:    parseFloat(document.getElementById('length').value),
                width:     parseFloat(document.getElementById('width').value),
                passes:    parseInt(document.getElementById('passes').value),
                direction: document.querySelector('.toggle-btn.active').dataset.value
            };
            fetch('/api/robot/upload', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body:    JSON.stringify(payload)
            }).then(() => alert("Configuration uploaded to robot!"));
        };

        // Start grid run
        document.getElementById('run-btn').onclick = () => {
            fetch('/api/robot/start', { method: 'POST' })
                .then(() => startPolling(2000)); // Poll aggressively while running
        };

        // Emergency stop
        document.getElementById('kill-btn').onclick = () => {
            fetch('/api/robot/stop_grid', { method: 'POST' })
                .catch(err => console.error("Stop command failed:", err));
        };

        /* ------------------------------------------------------------------
         * Initialise
         * ------------------------------------------------------------------ */
        updateLegend();
        startPolling(5000); // Begin polling at idle rate; speeds up when run starts
    }

    /* ========================================================================
     * MANUAL CONTROL PAGE  (unchanged from original)
     * ====================================================================== */
    if (isManualPage) {
        const logDisplay  = document.getElementById('command-log');
        const speedSlider = document.getElementById('speed-slider');
        const speedDisplay = document.getElementById('speed-display');

        if (speedSlider && speedDisplay) {
            speedSlider.addEventListener('input', (e) => {
                speedDisplay.innerText = e.target.value;
            });
        }

        const sendCommand = (cmd) => {
            const timestamp    = new Date().toLocaleTimeString();
            const currentSpeed = speedSlider ? parseInt(speedSlider.value) : 1000;

            if (logDisplay) logDisplay.innerText = `[${timestamp}] Sending: ${cmd}`;

            fetch('/api/robot/move', {
                method:  'POST',
                headers: { 'Content-Type': 'application/json' },
                body:    JSON.stringify({ direction: cmd, speed: currentSpeed })
            })
            .then(response => {
                if (!response.ok) throw new Error("Robot did not acknowledge");
                return response.json();
            })
            .catch(err => {
                if (logDisplay) {
                    logDisplay.innerText += ` (Error: ${err.message})`;
                    logDisplay.style.color = "var(--danger)";
                }
            });

            if (logDisplay) logDisplay.style.color = cmd === "STOP" ? "var(--danger)" : "var(--primary)";
        };

        document.querySelectorAll('.d-btn').forEach(btn => {
            btn.addEventListener('mousedown', () => {
                btn.classList.add('active-press');
                sendCommand(btn.dataset.cmd);
            });
            btn.addEventListener('mouseup', () => {
                btn.classList.remove('active-press');
                if (btn.dataset.cmd !== "STOP") sendCommand("STOP");
            });
            btn.addEventListener('touchstart', (e) => {
                e.preventDefault();
                btn.classList.add('active-press');
                sendCommand(btn.dataset.cmd);
            });
            btn.addEventListener('touchend', (e) => {
                e.preventDefault();
                btn.classList.remove('active-press');
                if (btn.dataset.cmd !== "STOP") sendCommand("STOP");
            });
        });

        document.addEventListener('keydown', (e) => {
            const keyMap = {
                ArrowUp: 'btn-up', ArrowDown: 'btn-down',
                ArrowLeft: 'btn-left', ArrowRight: 'btn-right',
                ' ': 'btn-stop', Space: 'btn-stop'
            };
            const btnId = keyMap[e.key];
            if (btnId) {
                const keyBtn = document.getElementById(btnId);
                if (keyBtn) {
                    keyBtn.classList.add('active-press');
                    sendCommand(keyBtn.dataset.cmd);
                }
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