#!/bin/bash

# ==============================================================
# CONFIGURATION VARIABLES (You can change these safely)
# ==============================================================
PROJECT_DIR="/home/jetsonano-smartgaitlab/Desktop/Projet/Projet_ETIS_SMARTGAIT_DATA_LOGGER"
LOG_DIR="$PROJECT_DIR/__LOGS__/System_Logs"

HOTSPOT_START_DELAY=2      # Seconds to wait after starting Hotspot
CAMERA_CHECK_DELAY=2       # Seconds between camera checks
RADAR_CHECK_DELAY=2        # Seconds between radar checks
RADAR_COOKING_TIME=15      # Seconds to wait after flashing radar firmware
NORMAL_RESTART_DELAY=5     # Seconds to wait before rebooting Python after a normal crash

# Create log directory if missing
mkdir -p "$LOG_DIR"

# ==============================================================
# HELPER FUNCTIONS
# ==============================================================
countdown() {
    local seconds=$1
    local message=$2
    for ((i=$seconds; i>0; i--)); do
        echo -ne "\r\033[K[WAITING] $message... $i seconds left"
        sleep 1
    done
    echo -e "\r\033[K[OK] $message... Done!"
}

# The Cancelable Countdown (Press any key to abort)
cancelable_countdown() {
    local seconds=$1
    local message=$2
    for ((i=$seconds; i>0; i--)); do
        echo -ne "\r\033[K🚨 [CRITICAL] $message... $i seconds left. PRESS ANY KEY TO CANCEL REBOOT!"
        # -t 1 means wait 1 second for input. -n 1 means 1 character.
        if read -t 1 -n 1 -s key; then
            echo -e "\n\r\033[K✅ [ABORT] Reboot cancelled by user! Resuming normal operation..."
            return 1 # Returns 1 if cancelled
        fi
    done
    echo -e "\n\r\033[K[OK] $message... Executing now!"
    return 0 # Returns 0 if countdown finished without interruption
}

# ==============================================================
# PART 1: THE EXTERNAL ASSASSIN (Freeze Detector)
# ==============================================================
(
    touch /tmp/sg_heartbeat
    while true; do
        sleep 10

        # A. 5:00 AM Daily Restart
        if [ "$(date +%H:%M)" == "05:00" ]; then
            pkill -9 -f MASTER_CONTROL.py
            sleep 65
            continue
        fi

        # B. Freeze Detector (30 seconds without a heartbeat)
        NOW=$(date +%s)
        FILE_TIME=$(stat -c %Y /tmp/sg_heartbeat 2>/dev/null || echo $NOW)
        if [ $((NOW - FILE_TIME)) -gt 30 ]; then
            pkill -9 -f MASTER_CONTROL.py
            touch /tmp/sg_heartbeat
        fi
    done
) &

# ==============================================================
# PART 2: HARDWARE BOOT
# ==============================================================
nmcli connection up MonHotspot > /dev/null 2>&1
sleep $HOTSPOT_START_DELAY

cd "$PROJECT_DIR"
FAIL_COUNT=0

while true; do

    # --- 1. CHECK CAMERA ---
    while ! ls /dev/video* 1> /dev/null 2>&1; do
        echo "$(date): [WAITING] No Camera (/dev/videoX) found! Please plug it in..."
        sleep $CAMERA_CHECK_DELAY
    done

    # --- 2. CHECK RADAR & FLASH ---
    while true; do
        RADAR_USB=$(lsusb | awk '/04b4:8613/ {print "/dev/bus/usb/"$2"/"substr($4,1,3)}')
        if [ -n "$RADAR_USB" ]; then
            cd "$PROJECT_DIR/Modules/RADAR/RADAR_HEX"
            fxload -v -t fx2 -I SDR_USB_FW.hex -D "$RADAR_USB" > /dev/null 2>&1
            echo ""
            countdown $RADAR_COOKING_TIME "Cooking Radar Hardware"
            break
        else
            echo "$(date): [WAITING] Cypress Radar not found! Please plug it in..."
            sleep $RADAR_CHECK_DELAY
        fi
    done

    # --- 3. CLEAN LOG ROTATION ---
    cd "$PROJECT_DIR"
    TIMESTAMP=$(date +"%y%m%d_%H%M")
    CURRENT_LOG="$LOG_DIR/SG_${TIMESTAMP}.log"
    find "$LOG_DIR" -name "SG_*.log" -type f -mtime +3 -exec rm -f {} \;

    echo "===================================================" > "$CURRENT_LOG"
    echo "$(date): [WATCHDOG] Launching MASTER_CONTROL.py..." >> "$CURRENT_LOG"
    touch /tmp/sg_heartbeat
    START_TIME=$(date +%s)

    # --- 4. RUN PYTHON ---
    python3 MASTER_CONTROL.py >> "$CURRENT_LOG" 2>&1
    EXIT_CODE=$?

    # --- 5. EXIT CODE DECODER ---
    RUN_DURATION=$(( $(date +%s) - START_TIME ))

    case $EXIT_CODE in
        10) ERROR_MSG="Camera physically disconnected." ;;
        11) ERROR_MSG="Radar USB disconnected." ;;
        137) ERROR_MSG="System FROZEN! Killed by Bash Assassin." ;;
        134|139) ERROR_MSG="C++ Core Dump / Segmentation Fault." ;;
        0) ERROR_MSG="Clean exit." ;;
        *) ERROR_MSG="Unknown Python crash." ;;
    esac

    echo "$(date): ❌ [WATCHDOG] Script exited with code $EXIT_CODE ($ERROR_MSG) after $RUN_DURATION seconds" >> "$CURRENT_LOG"
    echo "ERROR DETECTED: $ERROR_MSG (Code $EXIT_CODE)"

    rm -f /dev/shm/smartgait_radar_shm

    # --- 6. 3-STRIKE HARD REBOOT ---
    if [ $RUN_DURATION -gt 120 ]; then
        FAIL_COUNT=0
    else
        FAIL_COUNT=$((FAIL_COUNT + 1))
    fi

    if [ $FAIL_COUNT -ge 3 ]; then
        echo "$(date): 💀 [CRITICAL] 3 FAST CRASHES! INITIATING DEEP SLEEP..." >> "$CURRENT_LOG"

        # HARDCODED: 300 Seconds (5 Minutes). Press any key to cancel.
        if cancelable_countdown 300 "Deep Sleep (Rebooting Jetson)"; then
            echo "$(date): Forcing Full System Reboot..." >> "$CURRENT_LOG"
            sync
            sudo reboot
        else
            echo "$(date): Deep Sleep Cancelled by User." >> "$CURRENT_LOG"
            FAIL_COUNT=0 # Reset strikes so it can try running python again
        fi
    fi

    countdown $NORMAL_RESTART_DELAY "Restarting Python"
done