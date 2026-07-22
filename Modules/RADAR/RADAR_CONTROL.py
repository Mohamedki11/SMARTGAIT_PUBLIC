import os
import sys
import time

# forcing python to add this folder to its internal search list ( because MASTER.py can(t see the .so file correctly))
cur_dir = os.path.dirname(os.path.abspath(__file__))
if cur_dir not in sys.path:
    sys.path.append(cur_dir)
    
import RADAR_engine

_save_choke_start = 0.0
_realtime_choke_start = 0.0

"""save_dir = "/home/jetsonano-smartgaitlab/Desktop/Projet/Projet_ETIS_SMARTGAIT_DATA_LOGGER/__LOGS__"
if not os.path.exists(save_dir):
    os.makedirs(save_dir)

bin_file_path = os.path.join(save_dir, "test_radar_20.bin")
"""
def RADAR_REC(bin_file_path):
    print("Starting radar...")
    RADAR_engine.start_radar_rec(bin_file_path)

def BOOT_RADAR():
    RADAR_engine.boot_radar()

def SHUTDOWN_RADAR():
    print("Stopping radar...")
    RADAR_engine.stop_radar()
def STOP_RADAR_REC():
    RADAR_engine.stop_radar_rec()

def START_REAL():
    RADAR_engine.start_realtime()
def STOP_REAL():
    RADAR_engine.stop_realtime()


#health checking
def CHECK_RADAR_HEALTH():
    global _save_choke_start, _realtime_choke_start 
    _now = time.time()

    if(RADAR_engine.get_radar_save_queue_size() >= 500.0):
        if(_save_choke_start == 0.0):
            _save_choke_start = _now
        elif _now - _save_choke_start > 10.0:
            print("[RADAR]: ERROR DETECTED IN THE SAVE THREAD/QUEUE...EXITING")
            return False
    else:
        _save_choke_start = 0.0


    if(RADAR_engine.get_radar_realtime_queue_size() >= 20.0):
        if(_realtime_choke_start == 0.0):
            _realtime_choke_start = _now
        elif _now - _realtime_choke_start > 10.0:
            print("[RADAR]: ERROR DETECTED IN THE REALTIME THREAD/QUEUE...EXITING")
            return False
    else:
        _realtime_choke_start = 0.0
    
    #if the camera error is false (then the camera is healthy thats why its inversed)
    if RADAR_engine.get_radar_health():
        print("[CAMERA]: ERROR DETECTED IN THE PRODUCER(RADAR) THREAD/QUEUE...CHECK THE RADAR IS CORRECTLY WORKING/PLUGGEd IN ...EXITING")
        return False
    
    return True
    
