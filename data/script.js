// XYRON TRACER - ESP DEAUTH PRO v5.0
// Web Interface JavaScript - Synchronized with C++ API

class XyronTracerUI {
    constructor() {
        this.config = {
            wsUrl: `ws://${window.location.host}/ws`,
            apiBase: `http://${window.location.host}/api`,
            reconnectInterval: 5000,
            statsInterval: 1000,
            configInterval: 30000
        };

        this.state = {
            connected: false,
            attackActive: false,
            attackPaused: false,
            ws: null,
            stats: {
                packets: 0,
                duration: 0,
                pps: 0,
                channel: 1,
                clients: 0,
                memory: 0
            },
            config: {
                attackMode: 0,
                deauthRate: 100,
                beaconRate: 20,
                hopInterval: 1000,
                macInterval: 30000,
                attackAll: true,
                beaconSpam: true,
                channelHop: true,
                randomMac: true,
                rogueAP: false,
                stealthLevel: 1,
                txPower: 2
            },
            networks: [],
            selectedNetworks: new Set(),
            logs: [],
            logPaused: false
        };

        this.lastUpdate = Date.now();
        this.reconnectAttempts = 0;
        this.maxReconnectAttempts = 10;

        this.initialize();
    }

    initialize() {
        // Hide loading screen after 2 seconds
        setTimeout(() => {
            document.getElementById('loading').style.display = 'none';
            document.getElementById('main-container').style.display = 'block';
            this.loadSystemInfo();
        }, 2000);

        this.bindEvents();
        this.connectWebSocket();
        this.startPolling();
        
        // Load saved config from localStorage
        this.loadSavedConfig();
    }

    bindEvents() {
        // Attack Controls
        document.getElementById('btn-attack-start').addEventListener('click', () => this.sendCommand('start'));
        document.getElementById('btn-attack-stop').addEventListener('click', () => this.sendCommand('stop'));
        document.getElementById('btn-channel-hop').addEventListener('click', () => this.sendCommand('hop'));
        document.getElementById('btn-rotate-mac').addEventListener('click', () => this.sendCommand('mac'));
        document.getElementById('btn-scan').addEventListener('click', () => this.scanNetworks());
        document.getElementById('btn-save').addEventListener('click', () => this.saveConfig());

        // Network Controls
        document.getElementById('btn-refresh-networks').addEventListener('click', () => this.scanNetworks());
        document.getElementById('btn-clear-targets').addEventListener('click', () => this.clearTargets());
        document.getElementById('btn-add-targets').addEventListener('click', () => this.addSelectedTargets());
        document.getElementById('select-all').addEventListener('change', (e) => this.selectAllNetworks(e.target.checked));

        // Log Controls
        document.getElementById('btn-clear-log').addEventListener('click', () => this.clearLog());
        document.getElementById('btn-pause-log').addEventListener('click', () => this.toggleLogPause());

        // System Controls
        document.getElementById('btn-reboot').addEventListener('click', () => this.rebootSystem());
        document.getElementById('btn-reset').addEventListener('click', () => this.factoryReset());
        document.getElementById('btn-reconnect').addEventListener('click', () => this.reconnect());
        document.getElementById('btn-fullscreen').addEventListener('click', () => this.toggleFullscreen());

        // Modal Controls
        document.getElementById('btn-manual-reconnect').addEventListener('click', () => {
            document.getElementById('connection-modal').style.display = 'none';
            this.reconnect();
        });

        // Configuration Changes
        document.querySelectorAll('input[name="attack-mode"]').forEach(radio => {
            radio.addEventListener('change', (e) => this.updateConfig('attackMode', parseInt(e.target.value)));
        });

        // Sliders
        document.getElementById('deauth-rate').addEventListener('input', (e) => {
            const value = parseInt(e.target.value);
            document.getElementById('deauth-rate-value').textContent = value;
            this.updateConfig('deauthRate', value);
        });

        document.getElementById('beacon-rate').addEventListener('input', (e) => {
            const value = parseInt(e.target.value);
            document.getElementById('beacon-rate-value').textContent = value;
            this.updateConfig('beaconRate', value);
        });

        document.getElementById('hop-interval').addEventListener('input', (e) => {
            const value = parseInt(e.target.value);
            document.getElementById('hop-interval-value').textContent = value + ' ms';
            this.updateConfig('hopInterval', value);
        });

        document.getElementById('mac-interval').addEventListener('input', (e) => {
            const value = parseInt(e.target.value);
            document.getElementById('mac-interval-value').textContent = Math.round(value / 1000) + ' sec';
            this.updateConfig('macInterval', value);
        });

        // Checkboxes
        document.getElementById('attack-all').addEventListener('change', (e) => this.updateConfig('attackAll', e.target.checked));
        document.getElementById('beacon-spam').addEventListener('change', (e) => this.updateConfig('beaconSpam', e.target.checked));
        document.getElementById('channel-hop').addEventListener('change', (e) => this.updateConfig('channelHop', e.target.checked));
        document.getElementById('random-mac').addEventListener('change', (e) => this.updateConfig('randomMac', e.target.checked));
        document.getElementById('rogue-ap').addEventListener('change', (e) => this.updateConfig('rogueAP', e.target.checked));

        // Advanced Settings
        document.getElementById('stealth-level-select').addEventListener('change', (e) => this.updateConfig('stealthLevel', parseInt(e.target.value)));
        document.getElementById('tx-power-select').addEventListener('change', (e) => this.updateConfig('txPower', parseInt(e.target.value)));
        document.getElementById('rogue-ssid').addEventListener('change', (e) => this.updateConfig('rogueSSID', e.target.value));

        // Keyboard Shortcuts
        document.addEventListener('keydown', (e) => this.handleKeyboardShortcut(e));

        // Auto-save config on changes
        setInterval(() => this.saveConfigToLocalStorage(), 10000);
    }

    connectWebSocket() {
        try {
            this.state.ws = new WebSocket(this.config.wsUrl);
            
            this.state.ws.onopen = () => {
                console.log('WebSocket connected');
                this.state.connected = true;
                this.updateConnectionStatus(true);
                this.reconnectAttempts = 0;
                this.addLog('WebSocket connected successfully', 'success');
            };

            this.state.ws.onmessage = (event) => {
                this.handleWebSocketMessage(event.data);
            };

            this.state.ws.onclose = () => {
                console.log('WebSocket disconnected');
                this.state.connected = false;
                this.updateConnectionStatus(false);
                this.attemptReconnect();
            };

            this.state.ws.onerror = (error) => {
                console.error('WebSocket error:', error);
                this.addLog('WebSocket error: ' + error.type, 'error');
            };

        } catch (error) {
            console.error('Failed to create WebSocket:', error);
            this.addLog('Failed to create WebSocket connection', 'error');
            this.attemptReconnect();
        }
    }

    handleWebSocketMessage(data) {
        try {
            const message = JSON.parse(data);
            
            switch (message.type) {
                case 'stats':
                    this.updateStats(message);
                    break;
                    
                case 'config':
                    this.updateUIConfig(message.data);
                    break;
                    
                case 'log':
                    this.addLog(message.message, this.getLogLevelText(message.level));
                    break;
                    
                case 'networks':
                    this.updateNetworkList(message.data);
                    break;
                    
                case 'welcome':
                    this.addLog('Connected to XYRON TRACER v' + message.version, 'success');
                    break;
            }
            
        } catch (error) {
            console.error('Failed to parse WebSocket message:', error);
        }
    }

    updateStats(data) {
        this.state.stats = {
            packets: data.packets_total || 0,
            duration: data.duration || 0,
            pps: data.pps || 0,
            channel: data.channel || 1,
            clients: data.clients || 0,
            memory: data.memory || 0
        };

        this.state.attackActive = data.active || false;
        this.state.attackPaused = data.paused || false;

        this.updateUIStats();
        this.updateAttackStatus();
    }

    updateUIStats() {
        const stats = this.state.stats;
        
        // Update stats panel
        document.getElementById('stat-packets').textContent = stats.packets.toLocaleString();
        document.getElementById('stat-duration').textContent = this.formatTime(stats.duration);
        document.getElementById('stat-pps').textContent = stats.pps.toFixed(1);
        document.getElementById('stat-channel').textContent = stats.channel;
        document.getElementById('stat-clients').textContent = stats.clients;
        document.getElementById('stat-memory').textContent = Math.round(stats.memory / 1024) + ' KB';

        // Update footer
        document.getElementById('update-time').textContent = 'LAST UPDATE: ' + 
            new Date().toLocaleTimeString([], {hour: '2-digit', minute:'2-digit', second:'2-digit'});
    }

    updateAttackStatus() {
        const attackBtn = document.getElementById('btn-attack-start');
        const stopBtn = document.getElementById('btn-attack-stop');
        const statusBadge = document.getElementById('attack-state');
        
        if (this.state.attackActive) {
            if (this.state.attackPaused) {
                statusBadge.className = 'status-badge paused';
                statusBadge.textContent = 'PAUSED';
                attackBtn.textContent = 'RESUME ATTACK';
                attackBtn.disabled = false;
                stopBtn.disabled = false;
            } else {
                statusBadge.className = 'status-badge running';
                statusBadge.textContent = 'RUNNING';
                attackBtn.textContent = 'ATTACK RUNNING';
                attackBtn.disabled = true;
                stopBtn.disabled = false;
            }
        } else {
            statusBadge.className = 'status-badge stopped';
            statusBadge.textContent = 'STOPPED';
            attackBtn.textContent = 'START ATTACK';
            attackBtn.disabled = false;
            stopBtn.disabled = true;
        }

        // Update attack mode display
        const modeMap = ['DEAUTH ONLY', 'BEACON SPAM', 'PROBE FLOOD', 'ROGUE AP', 'MIXED ATTACK'];
        document.getElementById('attack-mode').textContent = modeMap[this.state.config.attackMode] || 'UNKNOWN';

        // Update stealth level display
        const stealthMap = ['OFF', 'LOW', 'MEDIUM', 'HIGH', 'EXTREME'];
        document.getElementById('stealth-level').textContent = stealthMap[this.state.config.stealthLevel] || 'UNKNOWN';
    }

    updateUIConfig(config) {
        // Update attack mode radio
        document.querySelector(`input[name="attack-mode"][value="${config.attackMode}"]`).checked = true;

        // Update sliders
        document.getElementById('deauth-rate').value = config.deauthRate;
        document.getElementById('deauth-rate-value').textContent = config.deauthRate;
        
        document.getElementById('beacon-rate').value = config.beaconRate;
        document.getElementById('beacon-rate-value').textContent = config.beaconRate;
        
        document.getElementById('hop-interval').value = config.hopInterval;
        document.getElementById('hop-interval-value').textContent = config.hopInterval + ' ms';
        
        document.getElementById('mac-interval').value = config.macInterval;
        document.getElementById('mac-interval-value').textContent = Math.round(config.macInterval / 1000) + ' sec';

        // Update checkboxes
        document.getElementById('attack-all').checked = config.attackAll;
        document.getElementById('beacon-spam').checked = config.beaconSpam;
        document.getElementById('channel-hop').checked = config.channelHop;
        document.getElementById('random-mac').checked = config.randomMac;
        document.getElementById('rogue-ap').checked = config.rogueAP;

        // Update advanced settings
        document.getElementById('stealth-level-select').value = config.stealthLevel;
        document.getElementById('tx-power-select').value = config.txPower;
        document.getElementById('rogue-ssid').value = config.rogueSSID || 'Free_WiFi';

        // Update target count
        document.getElementById('target-count').textContent = config.targetCount || 0;

        // Save to state
        this.state.config = {...this.state.config, ...config};
    }

    updateNetworkList(networks) {
        this.state.networks = networks || [];
        this.renderNetworkTable();
    }

    renderNetworkTable() {
        const tbody = document.getElementById('network-list');
        const networks = this.state.networks;
        
        if (!networks.length) {
            tbody.innerHTML = '<tr class="empty-row"><td colspan="6">No networks found. Click SCAN to discover.</td></tr>';
            return;
        }

        let html = '';
        networks.forEach((network, index) => {
            const isSelected = this.state.selectedNetworks.has(index);
            const rssiClass = this.getRSSIClass(network.rssi);
            const encryptionText = this.getEncryptionText(network.encryption);
            
            html += `
                <tr class="network-row ${isSelected ? 'selected' : ''}">
                    <td><input type="checkbox" class="network-select" data-index="${index}" ${isSelected ? 'checked' : ''}></td>
                    <td>${this.escapeHtml(network.ssid || 'Hidden')}</td>
                    <td>${network.bssid || '00:00:00:00:00:00'}</td>
                    <td>${network.channel || '?'}</td>
                    <td class="${rssiClass}">${network.rssi || 0} dBm</td>
                    <td>${encryptionText}</td>
                </tr>
            `;
        });

        tbody.innerHTML = html;
        
        // Bind checkbox events
        document.querySelectorAll('.network-select').forEach(checkbox => {
            checkbox.addEventListener('change', (e) => {
                const index = parseInt(e.target.dataset.index);
                this.toggleNetworkSelection(index, e.target.checked);
            });
        });

        // Update selected count
        document.getElementById('selected-count').textContent = this.state.selectedNetworks.size;
    }

    toggleNetworkSelection(index, selected) {
        if (selected) {
            this.state.selectedNetworks.add(index);
        } else {
            this.state.selectedNetworks.delete(index);
        }
        document.getElementById('selected-count').textContent = this.state.selectedNetworks.size;
        
        // Update select all checkbox
        const selectAll = document.getElementById('select-all');
        selectAll.checked = this.state.selectedNetworks.size === this.state.networks.length;
        selectAll.indeterminate = this.state.selectedNetworks.size > 0 && 
                                 this.state.selectedNetworks.size < this.state.networks.length;
    }

    selectAllNetworks(selected) {
        if (selected) {
            for (let i = 0; i < this.state.networks.length; i++) {
                this.state.selectedNetworks.add(i);
            }
        } else {
            this.state.selectedNetworks.clear();
        }
        this.renderNetworkTable();
    }

    clearTargets() {
        this.state.selectedNetworks.clear();
        this.renderNetworkTable();
        this.addLog('All targets cleared', 'info');
    }

    async addSelectedTargets() {
        if (this.state.selectedNetworks.size === 0) {
            this.addLog('No networks selected to target', 'warning');
            return;
        }

        const targets = Array.from(this.state.selectedNetworks).map(index => this.state.networks[index]);
        
        try {
            const response = await fetch(this.config.apiBase + '/targets', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({targets: targets})
            });

            if (response.ok) {
                this.addLog(`Added ${targets.length} network(s) to targets`, 'success');
                this.state.selectedNetworks.clear();
                this.renderNetworkTable();
            } else {
                throw new Error('Server responded with ' + response.status);
            }
        } catch (error) {
            console.error('Failed to add targets:', error);
            this.addLog('Failed to add targets: ' + error.message, 'error');
        }
    }

    async sendCommand(command) {
        if (!this.state.connected) {
            this.addLog('Not connected to system', 'error');
            return;
        }

        try {
            const response = await fetch(this.config.apiBase + '/control', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({command: command})
            });

            if (!response.ok) {
                throw new Error('Server responded with ' + response.status);
            }

            const data = await response.json();
            this.addLog(`Command "${command}" sent successfully`, 'success');
            
        } catch (error) {
            console.error('Failed to send command:', error);
            this.addLog('Failed to send command: ' + error.message, 'error');
        }
    }

    async scanNetworks() {
        try {
            const response = await fetch(this.config.apiBase + '/scan');
            if (response.ok) {
                const data = await response.json();
                this.updateNetworkList(data.networks);
                this.addLog(`Scan completed: ${data.count} networks found`, 'success');
            }
        } catch (error) {
            console.error('Failed to scan networks:', error);
            this.addLog('Failed to scan networks', 'error');
        }
    }

    async saveConfig() {
        try {
            const response = await fetch(this.config.apiBase + '/config', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify(this.state.config)
            });

            if (response.ok) {
                this.addLog('Configuration saved successfully', 'success');
                this.saveConfigToLocalStorage();
            } else {
                throw new Error('Server responded with ' + response.status);
            }
        } catch (error) {
            console.error('Failed to save config:', error);
            this.addLog('Failed to save configuration', 'error');
        }
    }

    async rebootSystem() {
        if (!confirm('Are you sure you want to reboot the system? This will interrupt any ongoing attacks.')) {
            return;
        }

        try {
            const response = await fetch(this.config.apiBase + '/reboot', {method: 'POST'});
            if (response.ok) {
                this.addLog('System reboot initiated...', 'warning');
                setTimeout(() => {
                    this.addLog('System should be rebooting now. Page will refresh in 10 seconds.', 'info');
                    setTimeout(() => location.reload(), 10000);
                }, 1000);
            }
        } catch (error) {
            console.error('Failed to reboot:', error);
        }
    }

    async factoryReset() {
        if (!confirm('WARNING: This will reset ALL configuration to factory defaults! Are you sure?')) {
            return;
        }

        if (!confirm('LAST CHANCE: This cannot be undone! All settings will be lost.')) {
            return;
        }

        try {
            const response = await fetch(this.config.apiBase + '/reset', {method: 'POST'});
            if (response.ok) {
                this.addLog('Factory reset initiated...', 'warning');
                localStorage.clear();
                setTimeout(() => location.reload(), 3000);
            }
        } catch (error) {
            console.error('Failed to reset:', error);
        }
    }

    updateConfig(key, value) {
        this.state.config[key] = value;
        this.addLog(`Config updated: ${key} = ${value}`, 'info');
    }

    addLog(message, level = 'info') {
        if (this.state.logPaused) return;

        const time = new Date();
        const logEntry = {
            time: time.toLocaleTimeString([], {hour: '2-digit', minute:'2-digit', second:'2-digit'}),
            message: message,
            level: level
        };

        this.state.logs.push(logEntry);
        if (this.state.logs.length > 100) {
            this.state.logs.shift();
        }

        this.renderLogEntry(logEntry);
    }

    renderLogEntry(entry) {
        const container = document.getElementById('log-container');
        const div = document.createElement('div');
        div.className = `log-entry ${entry.level}`;
        div.innerHTML = `
            <span class="log-time">[${entry.time}]</span>
            <span class="log-text">${this.escapeHtml(entry.message)}</span>
        `;
        
        container.appendChild(div);
        container.scrollTop = container.scrollHeight;
    }

    clearLog() {
        this.state.logs = [];
        document.getElementById('log-container').innerHTML = '';
        this.addLog('Log cleared', 'info');
    }

    toggleLogPause() {
        this.state.logPaused = !this.state.logPaused;
        const btn = document.getElementById('btn-pause-log');
        btn.textContent = this.state.logPaused ? 'RESUME' : 'PAUSE';
        this.addLog(`Log ${this.state.logPaused ? 'paused' : 'resumed'}`, 'info');
    }

    updateConnectionStatus(connected) {
        const statusEl = document.getElementById('system-status');
        const wsStatusEl = document.getElementById('info-ws');
        const connStatusEl = document.getElementById('connection-status');
        
        if (connected) {
            statusEl.className = 'status-value online';
            statusEl.textContent = 'ONLINE';
            wsStatusEl.className = 'info-value connected';
            wsStatusEl.textContent = 'CONNECTED';
            connStatusEl.className = 'status-connected';
            connStatusEl.textContent = 'WEBSOCKET: CONNECTED';
            document.getElementById('connection-modal').style.display = 'none';
        } else {
            statusEl.className = 'status-value offline';
            statusEl.textContent = 'OFFLINE';
            wsStatusEl.className = 'info-value disconnected';
            wsStatusEl.textContent = 'DISCONNECTED';
            connStatusEl.className = 'status-disconnected';
            connStatusEl.textContent = 'WEBSOCKET: DISCONNECTED';
        }
    }

    attemptReconnect() {
        if (this.reconnectAttempts >= this.maxReconnectAttempts) {
            this.addLog('Max reconnection attempts reached', 'error');
            this.showConnectionModal();
            return;
        }

        this.reconnectAttempts++;
        const delay = Math.min(30000, this.config.reconnectInterval * Math.pow(1.5, this.reconnectAttempts - 1));
        
        this.addLog(`Attempting to reconnect... (Attempt ${this.reconnectAttempts}/${this.maxReconnectAttempts})`, 'warning');
        
        setTimeout(() => {
            if (!this.state.connected) {
                this.connectWebSocket();
            }
        }, delay);
    }

    showConnectionModal() {
        const modal = document.getElementById('connection-modal');
        modal.style.display = 'flex';
        
        let seconds = 30;
        const timerEl = document.getElementById('reconnect-timer');
        const progressEl = document.getElementById('reconnect-progress');
        
        const interval = setInterval(() => {
            seconds--;
            timerEl.textContent = seconds;
            progressEl.style.width = ((30 - seconds) / 30 * 100) + '%';
            
            if (seconds <= 0) {
                clearInterval(interval);
                modal.style.display = 'none';
                this.reconnectAttempts = 0;
                this.connectWebSocket();
            }
        }, 1000);
    }

    reconnect() {
        if (this.state.ws) {
            this.state.ws.close();
        }
        this.reconnectAttempts = 0;
        this.connectWebSocket();
    }

    startPolling() {
        // Poll for stats
        setInterval(async () => {
            if (this.state.connected) {
                try {
                    const response = await fetch(this.config.apiBase + '/stats');
                    if (response.ok) {
                        const data = await response.json();
                        this.updateStats(data);
                    }
                } catch (error) {
                    console.error('Failed to fetch stats:', error);
                }
            }
        }, this.config.statsInterval);

        // Poll for system info
        setInterval(() => this.loadSystemInfo(), this.config.configInterval);
    }

    async loadSystemInfo() {
        try {
            const response = await fetch(this.config.apiBase);
            if (response.ok) {
                const data = await response.json();
                
                document.getElementById('info-firmware').textContent = data.version || 'v5.0';
                document.getElementById('info-chipid').textContent = '0x' + (data.chip_id || '000000').toString(16).toUpperCase();
                document.getElementById('info-uptime').textContent = this.formatTime(data.uptime || 0);
                document.getElementById('info-ip').textContent = window.location.hostname;
                document.getElementById('info-mac').textContent = data.mac || '00:00:00:00:00:00';
            }
        } catch (error) {
            console.error('Failed to load system info:', error);
        }
    }

    loadSavedConfig() {
        try {
            const saved = localStorage.getItem('xyron_config');
            if (saved) {
                const config = JSON.parse(saved);
                this.state.config = {...this.state.config, ...config};
                this.updateUIConfig(config);
                this.addLog('Configuration loaded from local storage', 'info');
            }
        } catch (error) {
            console.error('Failed to load saved config:', error);
        }
    }

    saveConfigToLocalStorage() {
        try {
            localStorage.setItem('xyron_config', JSON.stringify(this.state.config));
        } catch (error) {
            console.error('Failed to save config to localStorage:', error);
        }
    }

    handleKeyboardShortcut(e) {
        // Only trigger if not typing in an input field
        if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;

        switch (e.key.toLowerCase()) {
            case ' ':
                e.preventDefault();
                if (this.state.attackActive) {
                    this.sendCommand('stop');
                } else {
                    this.sendCommand('start');
                }
                break;
                
            case 'h':
                this.sendCommand('hop');
                break;
                
            case 'm':
                this.sendCommand('mac');
                break;
                
            case 's':
                if (e.ctrlKey) {
                    e.preventDefault();
                    this.saveConfig();
                }
                break;
                
            case 'f5':
                e.preventDefault();
                this.scanNetworks();
                break;
                
            case 'escape':
                document.getElementById('connection-modal').style.display = 'none';
                break;
        }
    }

    toggleFullscreen() {
        if (!document.fullscreenElement) {
            document.documentElement.requestFullscreen().catch(err => {
                console.error('Failed to enter fullscreen:', err);
            });
        } else {
            document.exitFullscreen();
        }
    }

    // Utility Methods
    formatTime(seconds) {
        const hrs = Math.floor(seconds / 3600);
        const mins = Math.floor((seconds % 3600) / 60);
        const secs = Math.floor(seconds % 60);
        return `${hrs.toString().padStart(2, '0')}:${mins.toString().padStart(2, '0')}:${secs.toString().padStart(2, '0')}`;
    }

    getLogLevelText(level) {
        const levels = ['error', 'warning', 'info', 'debug', 'verbose'];
        return levels[level] || 'info';
    }

    getRSSIClass(rssi) {
        if (rssi >= -50) return 'rssi-excellent';
        if (rssi >= -60) return 'rssi-good';
        if (rssi >= -70) return 'rssi-fair';
        return 'rssi-poor';
    }

    getEncryptionText(encryption) {
        const types = ['Open', 'WEP', 'WPA', 'WPA2', 'WPA3'];
        return types[encryption] || 'Unknown';
    }

    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
}

// Initialize when page loads
window.addEventListener('load', () => {
    window.xyron = new XyronTracerUI();
    
    // Handle page visibility
    document.addEventListener('visibilitychange', () => {
        if (!document.hidden && !window.xyron.state.connected) {
            window.xyron.reconnect();
        }
    });
});