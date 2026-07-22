#include "SharedState.h"
#include <pybind11/pybind11.h>
#include <thread>
#include <iostream>

namespace py = pybind11;
using namespace std;

//creation of the global flags
atomic<bool> radar_run_flag{false};
atomic<bool> radar_record{false};
atomic<bool> soft_restart{false}; 
atomic<bool> radar_error{false}; 

moodycamel::ReaderWriterQueue <vector<uint8_t>> radar_queue;

thread usb_thread;
thread ssd_thread;
thread realtime_thread;

//------------//
//------------//
//realtime doctor
atomic<bool> realtime_active{false};
moodycamel::ReaderWriterQueue <vector<uint8_t>> realtime_queue;
//------------//
//------------//
//link to the functions in the other files
extern void usb_catcher_worker();
extern void ssd_thread_worker(string bin_path);
extern void realtime_thread_worker();

//wrapper functions

void boot_radar()
{
    if(!radar_run_flag)
    {
        
        radar_run_flag = true;
        usb_thread = thread(usb_catcher_worker);

    }
}


//---------------------------------------------------//
//---------------------------------------------------//
void start_realtime()
{
    if(!realtime_active)
    {
        vector<uint8_t> trash;
        while(realtime_queue.try_dequeue(trash)){};

        realtime_active = true;
        realtime_thread = thread(realtime_thread_worker);
    }

}

void stop_realtime()
{
    if(realtime_active)
    {
        realtime_active = false;
        if(realtime_thread.joinable()) realtime_thread.join();
    }
}
//---------------------------------------------------//
//---------------------------------------------------//

void start_radar_rec(string bin_path)
{
    if(!radar_record)
    {

        soft_restart = true;
        vector<uint8_t> trash;
        while(soft_restart){this_thread::sleep_for(chrono::milliseconds(1));}
        while(radar_queue.try_dequeue(trash)){}
        radar_record = true;
        ssd_thread = thread(ssd_thread_worker, bin_path);
        //analyse_thread = thread(radar_calculations_worker);

    }
}

void stop_radar_rec()
{
    if (radar_record)
    {
        radar_record = false;
        if(ssd_thread.joinable()) ssd_thread.join();

    }
}

void stop_radar()
{
    
        radar_run_flag = false;
        stop_radar_rec();
        stop_realtime();
        if(usb_thread.joinable()) usb_thread.join();
        //if(analyse_thread.joinable()) analyse_thread.join();
}

//functions to return queues Health
int get_radar_save_queue_size()   {return radar_queue.size_approx();}
int get_radar_realtime_queue_size()   {return realtime_queue.size_approx();}
bool get_radar_health() {return radar_error;}
//pybind11 EXPORT

PYBIND11_MODULE(RADAR_engine, m)
{
    m.def("start_radar_rec", &start_radar_rec, py::arg("bin_path"), py::call_guard<py::gil_scoped_release>());
    m.def("stop_radar", &stop_radar, py::call_guard<py::gil_scoped_release>());
    m.def("stop_radar_rec", &stop_radar_rec, py::call_guard<py::gil_scoped_release>());
    m.def("boot_radar", &boot_radar, py::call_guard<py::gil_scoped_release>());
    m.def("start_realtime", &start_realtime, py::call_guard<py::gil_scoped_release>());
    m.def("stop_realtime", &stop_realtime, py::call_guard<py::gil_scoped_release>());

     //health
    m.def("get_radar_save_queue_size", &get_radar_save_queue_size, py::call_guard<py::gil_scoped_release>());
    m.def("get_radar_realtime_queue_size", &get_radar_realtime_queue_size, py::call_guard<py::gil_scoped_release>());

    m.def("get_radar_health", &get_radar_health, py::call_guard<py::gil_scoped_release>());
}