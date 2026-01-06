# MHAB-GitHUB
The ground-side GUI is the operator interface for the comms link. It’s used to start/stop the radio connection, monitor live telemetry from the balloon, send commands safely, and provide debugging/logging tools during integration and flight testing.
## Features
### 1) Connection & Radio Control

Connect / Disconnect

Radio configuration: frequency/channel, SF/BW/CR, TX power

Payload size indicator (max 255 bytes)

Link status: Connected / Disconnected / Error

Last RX time / last TX time

### 2) Live Telemetry Dashboard

Latest packet summary: telemetry seq, decoded timestamp, payload length

System health cards (based on telemetry data): battery, temperatures, GPS, fault flags/codes

Link metrics: SNR / packet loss estimate

Telemetry update rate indicator

Time-series plots (battery/temp/altitude)

Map view (GPS track)

### 3) Command Panel (Uplink Control)

Predefined command buttons (recalibrate, reset stepper, shutdown...etc.)

Parameter inputs with range validation

Send button with double confirmation for critical commands like shutdown

Command history table: command name, cmd_seq, time sent, status (NEW/WAITING_ACK/DONE), result (OK/ERROR/TIMEOUT), retries used

### 4) Retry/Timeout Monitor 

Live view of pending commands (retry state machine)

Per-command details: retries_left, last_tx_time, timeout countdown

Controls: cancel command, resend now, clear DONE entries

### 5) Security / Key Management

Key loaded indicator + key file path

Load key file

Warnings for invalid key length or frequent decrypt/auth failures (possible key mismatch)

### 6) Alerts & Fault Handling

Alerts panel (telemetry timeout, ACK timeout, repeated auth failures, low battery...etc.)

Acknowledge/clear alerts
