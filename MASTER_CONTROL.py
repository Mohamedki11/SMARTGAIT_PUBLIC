import os
import sys
import time
import datetime
import threading
from flask import Flask, request, jsonify, Response
import logging




def system_telemetry_worker():
    pid = os.getpid()
    print(f"[TELEMETRY] Started monitoring PID: {pid}")
    while True:
        try:
            with open('/tmp/sg_heartbeat', 'w') as f:
                f.write("ALIVE")
            
            
            
            # 1. Count Open File Descriptors
            fds = len(os.listdir(f'/proc/{pid}/fd'))
            
            # 2. Count Active Threads
            threads = len(os.listdir(f'/proc/{pid}/task'))
            
            # 3. Read RAM Usage (Resident Set Size in MB)
            with open(f'/proc/{pid}/status') as f:
                for line in f:
                    if line.startswith("VmRSS:"):
                        ram_kb = int(line.split()[1])
                        ram_mb = ram_kb / 1024.0
                        break
            
            print(f"   ---> [HEALTH] RAM: {ram_mb:.1f} MB | Open Files (FDs): {fds} | Threads: {threads}")
            time.sleep(5)
            
        except Exception as e:
            print(f" [TELEMETRY ERROR]: {e}") # Failsafe so telemetry never crashes the main program
            time.sleep(5)








from Modules.CAMERA import CAM_CONTROL
from Modules.TRIGGER.TRIG import TriggerNode
from Modules.RADAR import RADAR_CONTROL
NUM_PIX = 2304.0 #number of pixels for 1% of the image
app = Flask(__name__)
log= logging.getLogger('werkzeug')
log.setLevel(logging.ERROR)
# Global parameters dictionary 
ai_params = {
    "duration": 10,
    "threshold": 3500.0,
    "cooldown": 1000,
    "system_active": False  
}
# Global state dictionary to feed the live Web UI
system_state = {
    "is_recording": False,
    "record_start_time": 0.0
}

# Standard Python string (No Jinja/Flask context needed)
HTML_TEMPLATE = """
<!DOCTYPE html>
<html>
<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>SmartGait Control</title>
    <style>
        body {{ font-family: Arial, sans-serif; margin: 20px; background-color: #f4f4f9; }}
        .card {{ background: white; padding: 20px; border-radius: 10px; box-shadow: 0px 4px 6px rgba(0,0,0,0.1); }}
        input[type=range] {{ width: 100%; }}
        button {{ background: #007BFF; color: white; border: none; padding: 10px 20px; border-radius: 5px; font-size: 16px; width: 100%; margin-top: 10px;}}

        .switch {{ position: relative; display: inline-block; width: 60px; height: 34px; }}
        .switch input {{ opacity: 0; width: 0; height: 0; }}
        .slider {{ position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; transition: .4s; border-radius: 34px; }}
        .slider:before {{ position: absolute; content: ""; height: 26px; width: 26px; left: 4px; bottom: 4px; background-color: white; transition: .4s; border-radius: 50%; }}
        input:checked + .slider {{ background-color: #28a745; }}
        input:checked + .slider:before {{ transform: translateX(26px); }}
        .status-text {{ font-size: 20px; font-weight: bold; vertical-align: super; margin-left: 10px; }}
    </style>
</head>
<body>
    <div class="card">
        <h2>SmartGait Console</h2>

        <label class="switch">
            <input type="checkbox" id="system_active" onchange="sendParams()" {system_active_checked}>
            <span class="slider"></span>
        </label>
        <span class="status-text" id="status_label" style="color: {status_color};">{system_status}</span>
        <hr><br>

        <!-- LIVE STATUS DASHBOARD -->
        <div class="live-status-box">
            <span id="rec_badge">🟢 IDLE</span>
            <div id="countdown_text"></div>
        </div>
        <hr><br>
        <div style="test-align: center; margin-bottom: 20px;">
        <img src="/video_feed" style="width: 100%; max-width: 640px; border-radius: 8px; border: 2px solid #ccc;">
        </div>

        <label>Recording Duration (Seconds): <span id="dur_val">{duration}</span></label>
        <input type="range" id="duration" min="2" max="600" value="{duration}" oninput="document.getElementById('dur_val').innerText=this.value" onchange="sendParams()">
        <br><br>

        <label>Motion Threshold (pixels): <span id="thresh_val">{threshold}</span> %</label>
        <input type="range" id="threshold" min="0.02" max="30.0" step="0.01" value="{threshold}" oninput="document.getElementById('thresh_val').innerText=this.value" onchange="sendParams()">
        <br><br>

        <label>Cooldown (Milliseconds): <span id="cool_val">{cooldown}</span></label>
        <input type="range" id="cooldown" min="500" max="120000" step="500" value="{cooldown}" oninput="document.getElementById('cool_val').innerText=this.value" onchange="sendParams()">
        <br><br>

        


        <p style="font-size: 12px; color: gray;">Changes save automatically when you release the slider.</p>
    </div>

    <script>
        function sendParams() {{
            let isActive = document.getElementById('system_active').checked;
            document.getElementById('status_label').innerText = isActive ? "SYSTEM ARMED" : "STANDBY";
            document.getElementById('status_label').style.color = isActive ? "#28a745" : "gray";

            let data = {{
                duration: document.getElementById('duration').value,
                threshold: document.getElementById('threshold').value,
                cooldown: document.getElementById('cooldown').value,
                system_active: isActive
            }};

            fetch('/update', {{
                method: 'POST',
                headers: {{'Content-Type': 'application/json'}},
                body: JSON.stringify(data)
            }});
        }}

        // AJAX POLLING FOR LIVE STATUS
        setInterval(function() {{
            fetch('/status')
            .then(response => response.json())
            .then(data => {{
                let badge = document.getElementById('rec_badge');
                let countdown = document.getElementById('countdown_text');

                if (data.is_recording) {{
                    badge.innerText = "🔴 RECORDING";
                    //badge.style.backgroundColor = "#dc3545"; // Red
                    countdown.innerText = "Time left: " + data.time_left + " s";
                }} else {{
                    badge.innerText = "🟢 IDLE";
                    //badge.style.backgroundColor = "#28a745"; // Green
                    countdown.innerText = "";
                }}
            }});
        }}, 500); // Ask the server every 500ms

    </script>
</body>
</html>
"""
@app.route('/')
def index():
    # We use safe standard Python formatting here
    checked_str = "checked" if ai_params["system_active"] else ""
    status_str = "SYSTEM ARMED" if ai_params["system_active"] else "STANDBY"
    color_str = "#28a745" if ai_params["system_active"] else "gray"
    #pixels to percentage
    current_percentage = round(ai_params["threshold"]/NUM_PIX, 2)
    return HTML_TEMPLATE.format(
        duration=ai_params["duration"],
        threshold=current_percentage,
        cooldown=ai_params["cooldown"],
        system_active_checked=checked_str,
        system_status=status_str,
        status_color=color_str
    )

@app.route('/update', methods=['POST'])
def update():
    # Use force=True to ensure it parses the JSON safely
    data = request.get_json(force=True)

    ai_params["duration"] = int(data['duration'])

    percent_val = float(data['threshold'])
    ai_params["threshold"] = percent_val*NUM_PIX
    ai_params["cooldown"] = int(data['cooldown'])
    ai_params["system_active"] = bool(data['system_active'])

    # Send the new parameters into your C++ Camera Engine
    CAM_CONTROL.SET_PARAMS(ai_params["duration"], ai_params["threshold"], ai_params["cooldown"])
    return jsonify({"status": "success"})
# API Endpoint to serve the live status to the frontend
@app.route('/status', methods=['GET'])
def status():
    time_left = 0
    if system_state["is_recording"]:
        elapsed = time.time() - system_state["record_start_time"]
        time_left = max(0, ai_params["duration"] - elapsed)

    return jsonify({
        "is_recording": system_state["is_recording"],
        "time_left": round(time_left, 1)
    })
def generate_frame():
    while True:
        frame_bytes = CAM_CONTROL.web_frame()
        if frame_bytes:
            yield (b'--frame\r\n'
                   b'Content-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n')
        time.sleep(0.05)
@app.route('/video_feed')
def video_feed():
    return Response(generate_frame(), mimetype='multipart/x-mixed-replace;boundary=frame')
def run_web_server():
    app.run(host='0.0.0.0', port=8080, debug=False, use_reloader=False)

##shared memory variable ( in RAM)
def set_rec (value: bool):
    shm_path = "/dev/shm/recording"
    with open(shm_path, "w") as f:
        f.write(str(value))
    ##give permession ( read/write)
    os.chmod(shm_path, 0o666)
    print(f"[MASTER]: Global variable 'recording' updated to : {value}")

save_dir = "/home/jetsonano-smartgaitlab/Desktop/Projet/Projet_ETIS_SMARTGAIT_DATA_LOGGER/__LOGS__"
if not os.path.exists(save_dir):
    os.makedirs(save_dir)

def main():
    print("      ************ SMART GAIT STARTING **************       ")
    
    print(f"SYSTEM READY... Saving DATA and Video to :{save_dir}")

    web_thread = threading.Thread(target=run_web_server, daemon=True)
    web_thread.start()


    # --- ADD THIS LINE ---
    telemetry_thread = threading.Thread(target=system_telemetry_worker, daemon=True)
    telemetry_thread.start()





    trigger = TriggerNode()
    cur_rec = False
    try:
        #hardware initilizing
        CAM_CONTROL.START()
       
        RADAR_CONTROL.START_REAL()
        time.sleep(4)
        RADAR_CONTROL.BOOT_RADAR()
        print("*********************WAITING TRIGGER******************")
        while True:
            if not CAM_CONTROL.CHECK_CAM_HEALTH():
                sys.exit(10)
            elif(not RADAR_CONTROL.CHECK_RADAR_HEALTH()):
                sys.exit(11)
        
            set_rec(cur_rec)
            was_pressed, led_state = trigger.wait_for_press()
            if led_state == 1 or ai_params["system_active"]: 
                CAM_CONTROL.AI()
                HUMAN = CAM_CONTROL.DETECT()
                if HUMAN == True and not cur_rec: 
                    cur_rec = True
                    #Update the global state for the Web UI!
                    system_state["is_recording"] = True
                    system_state["record_start_time"] = time.time()


                    print("\n[MASTER] : START RECORDING")
                    time_stamp = datetime.datetime.now().strftime("%Y_%m_%d_%H%M%S")
                    vid_file = os.path.join(save_dir, f"Video_{time_stamp}")
                    csv_file = os.path.join(save_dir, f"LOG_{time_stamp}")
                    in_file_path = os.path.join(save_dir, f"test_LOG_{time_stamp}.bin")
                    print("[MASTER]: RECORDING...")
                    RADAR_CONTROL.RADAR_REC(in_file_path)
                    
                    time.sleep(0.25)
                    CAM_CONTROL.REC(vid_file, csv_file)
                elif HUMAN == False and cur_rec:
                    RADAR_CONTROL.STOP_RADAR_REC()
                    
                    time.sleep(0.25)
                    CAM_CONTROL.STOP_REC()
                    cur_rec = False
                    system_state["is_recording"] = False
                    print("[MASTER]: SAVE COMPLETE ...")
                else:
                    pass
            else:
                CAM_CONTROL.STOP_AI()
                print("[MASTER]: FORCE STOP BY USER")
                RADAR_CONTROL.STOP_RADAR_REC()
                time.sleep(0.25)
                CAM_CONTROL.STOP_REC()
                
                cur_rec = False
                system_state["is_recording"] = False
                print("[MASTER]: STAND BY ...click the button or activate the system via the website...")
            time.sleep(0.5)
    except KeyboardInterrupt:
        print("\n[MASTER]: shutting down...")
    finally:
        CAM_CONTROL.STOP_ALL()
        RADAR_CONTROL.SHUTDOWN_RADAR()
        trigger.cleanup
        print("system OFF. GOODBYE")
if __name__ == '__main__':
    main()





        