import sys 
import os
import time
# forcing python to add this folder to its internal search list ( because MASTER.py can(t see the .so file correctly))
cur_dir = os.path.dirname(os.path.abspath(__file__))
if cur_dir not in sys.path:
    sys.path.append(cur_dir)

started = False
AI_activated = False
Recording_Stopped = True

_save_choke_start = 0.0
_ai_choke_start = 0.0
_web_choke_start = 0.0

import CAM_engine

def START():
    global started
    if(started == False):
        print("[CAMERA NODE]: STARTING CAMERA ENGINE")
        CAM_engine.boot_cam()
        started = True


def AI():
    global AI_activated
    if(AI_activated == False):
        print("[CAMERA Node]: AI MODE ACTIVATED")
        CAM_engine.ai_active()
        time.sleep(1)
        AI_activated = True

def REC(vid_path, csv_path):
    global Recording_Stopped
    if (Recording_Stopped == True):
        print("[CAMERA NODE]: RECORDING ...")
        CAM_engine.start_recording(vid_path, csv_path)
        Recording_Stopped = False

def STOP_AI():
    global AI_activated
    if(AI_activated == True):
        print("[CAMERA NODE]: STOPPING AI")
        CAM_engine.stop_ai()
        AI_activated = False

def STOP_REC():
    global Recording_Stopped
    if(Recording_Stopped == False):
        print("[CAMERA NODE]: STOPPING RECORDING")
        CAM_engine.stop_record()
        Recording_Stopped = True

def STOP_ALL():
    global started
    if(started == True):
        print("[CAMERA NODE]: STOPPING CAMERA")
        CAM_engine.stop_camera()
        started = False
def DETECT():
    return CAM_engine.check_motion()
def web_frame():
    return CAM_engine.get_web_frame()
def SET_PARAMS(duration,threshold,cooldown):
    CAM_engine.set_ai_params(duration,threshold,cooldown)

#health checking
def CHECK_CAM_HEALTH():
    global _save_choke_start, _ai_choke_start , _web_choke_start
    _now = time.time()

    if(CAM_engine.get_cam_save_queue_size() >= CAM_engine.get_max_save_queue()):
        if(_save_choke_start == 0.0):
            _save_choke_start = _now
        elif _now - _save_choke_start > 10.0:
            print("[CAMERA]: ERROR DETECTED IN THE SAVE THREAD/QUEUE...EXITING")
            return False
    else:
        _save_choke_start = 0.0


    if(CAM_engine.get_cam_ai_queue_size() >= CAM_engine.get_max_ai_queue()):
        if(_ai_choke_start == 0.0):
            _ai_choke_start = _now
        elif _now - _ai_choke_start > 10.0:
            print("[CAMERA]: ERROR DETECTED IN THE AI THREAD/QUEUE...EXITING")
            return False
    else:
        _ai_choke_start = 0.0
    


   
    #if the camera error is false (then the camera is healthy thats why its inversed)
    if CAM_engine.get_camera_health():
        print("[CAMERA]: ERROR DETECTED IN THE PRODUCER(CAMERA) THREAD/QUEUE...CHECK THE CAMERA IS CORRECTLY WORKING ...EXITING")
        return False
    
    return True
    

