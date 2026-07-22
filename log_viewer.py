import os
import glob
import subprocess

# Finds the newest log and follows it live
LOG_DIR = "/home/jetsonano-smartgaitlab/Desktop/Projet/Projet_ETIS_SMARTGAIT_DATA_LOGGER/__LOGS__/System_Logs"
list_of_files = glob.glob(f"{LOG_DIR}/SG_*.log")

if not list_of_files:
    print("No logs found!")
else:
    latest_file = max(list_of_files, key=os.path.getctime)
    print(f"Streaming live log: {latest_file}\n" + "="*50)
    # This runs the 'tail -f' command inside Python
    subprocess.run(["tail", "-f", latest_file])