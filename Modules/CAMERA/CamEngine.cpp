#include "SharedState.h"
#include <thread>
#include <pybind11/pybind11.h>
#include <iostream>

namespace py = pybind11;
using namespace std;

//vars definitions
thread master_cam;
thread master_save;
thread master_ai;

atomic<bool> run_flag{false}; // pour controler le camera a laide de python 
atomic<bool> is_recording{false};
atomic<bool> is_ai_active{false};

atomic<bool> motion_detected{false};

atomic<bool> camera_error{false};

atomic<int> record_duration_sec{10};// record durations
atomic<int> cooldown_duration_ms{1000};//delay before start of next recording
atomic<double> detection_theshold{6000.0};//the general threashhold controlled elsewhere

moodycamel::ReaderWriterQueue<cv::Mat> save_queue; 
moodycamel::ReaderWriterQueue<cv::Mat> ai_queue;
moodycamel::ReaderWriterQueue<cv::Mat> web_queue;
//link external workers

extern void capture_worker();
extern void save_ssd(string vid_path, string csv_path);
extern void ai_worker();

//API functions

void boot_cam()
{
    if(!run_flag)
    {
    run_flag = true;
    master_cam = thread(capture_worker);
    }

}

void start_recording(string vid_path,string  csv_path)
{
    if(!is_recording)
    {
    is_recording = true;
    master_save = thread(save_ssd, vid_path, csv_path);
    }
    
}

void ai_active()
{
    if(!is_ai_active)
    {
        is_ai_active = true;
        master_ai = thread(ai_worker);
    }
}

void stop_record()
{   if(is_recording)
    {
        is_recording = false;
        cout <<"[CAM ENGINE]...STOPPING RECORDING..."<<endl;
        if(master_save.joinable())
        {
            master_save.join();
        }
    }
}
void stop_ai()
{
    if(is_ai_active)
    {

        is_ai_active = false;
        cout <<"CAM ENGINE]...STOPPING AI..."<<endl;
        if(master_ai.joinable())
        {
            master_ai.join();
        }
        //cleaning
        motion_detected = false;
        cv::Mat trash;
        while(ai_queue.try_dequeue(trash)){};


    }
}
void stop_camera()
{
    run_flag = false;
    stop_record();
    stop_ai();
    if(master_cam.joinable())
    {
        master_cam.join();
    }

}

bool check_motion()
{
    return motion_detected;
}

void set_ai_params(int duration_sec, double threshold, int cooldown_ms)
{
    record_duration_sec = duration_sec;// record durations
    cooldown_duration_ms = cooldown_ms;//delay before start of next recording
    detection_theshold = threshold;
    cout <<"[CAMERA] AI parameters updated -> Duration : "<<duration_sec<<"s \n->Threshold: "<<threshold<<"\n->Cooldown between recordings(minimum) : "<<cooldown_ms<<" ms."<<endl;

}


//lock free web frame fetcher
py::bytes get_web_frame()
{
    cv::Mat local_frame;
    cv::Mat newest_frame;

    while(web_queue.try_dequeue(local_frame))
    {
        newest_frame = local_frame;
    }
    if(newest_frame.empty())
    {
        return py::bytes("");
    }
    vector<uchar> buffer;
    cv::imencode(".jpg", newest_frame, buffer);
    return py::bytes(reinterpret_cast<const char*>(buffer.data()), buffer.size());
}


//functions to return queues Health
int get_cam_save_queue_size()   {return save_queue.size_approx();}
int get_cam_ai_queue_size()   {return ai_queue.size_approx();}
int get_cam_web_queue_size()   {return web_queue.size_approx();}
int get_max_save_queue() {return MAX_SAVE_QUEUE;}
int get_max_ai_queue() {return MAX_AI_QUEUE;}
int get_max_web_queue() {return MAX_WEB_QUEUE;}

bool get_camera_health() {return camera_error;}
//pybind export
PYBIND11_MODULE(CAM_engine, m)
{
    m.doc() = "CAMERA ENGINE GUI AND RECORDING";
    m.def("boot_cam", &boot_cam, py::call_guard<py::gil_scoped_release>());
    m.def("start_recording", &start_recording, py::arg("video_path"),py::arg("csv_path"),  py::call_guard<py::gil_scoped_release>());
    m.def("ai_active", &ai_active,  py::call_guard<py::gil_scoped_release>());
    m.def("stop_record", &stop_record,   py::call_guard<py::gil_scoped_release>());
    m.def("stop_ai", &stop_ai,   py::call_guard<py::gil_scoped_release>());
    m.def("stop_camera", &stop_camera,   py::call_guard<py::gil_scoped_release>());
    m.def("check_motion", &check_motion, py::call_guard<py::gil_scoped_release>());
    m.def("set_ai_params", &set_ai_params, "Dynamically update AI thresholds and timing", py::call_guard<py::gil_scoped_release>());
    m.def("get_web_frame", &get_web_frame);

    //health
    m.def("get_cam_save_queue_size", &get_cam_save_queue_size, py::call_guard<py::gil_scoped_release>());
    m.def("get_cam_ai_queue_size", &get_cam_ai_queue_size, py::call_guard<py::gil_scoped_release>());
    m.def("get_cam_web_queue_size", &get_cam_web_queue_size, py::call_guard<py::gil_scoped_release>());

    m.def("get_max_save_queue", &get_max_save_queue, py::call_guard<py::gil_scoped_release>());
    m.def("get_max_ai_queue", &get_max_ai_queue, py::call_guard<py::gil_scoped_release>());
    m.def("get_max_web_queue", &get_max_web_queue, py::call_guard<py::gil_scoped_release>());

    m.def ("get_camera_health", &get_camera_health, py::call_guard<py::gil_scoped_release>());

}

