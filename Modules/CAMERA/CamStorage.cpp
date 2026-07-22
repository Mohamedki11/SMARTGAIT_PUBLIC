#include "SharedState.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
// thread 2 ssd save thread

void save_ssd(string vid_path,string  csv_path)
{
    cv::VideoWriter writer;
    string writer_pipe = "appsrc ! video/x-raw, format=BGR ! queue ! videoconvert ! video/x-raw, format=I420 ! nvjpegenc quality=50 ! matroskamux ! filesink location="+ vid_path + ".mkv";
    writer.open(writer_pipe, cv::CAP_GSTREAMER, 0, CAM_FPS, cv::Size(CAM_WIDTH, CAM_HEIGHT));
    cv::Mat local_frame;// its not filled yet its an empty bucket for the incoming image coming from thread 1
    
    ofstream csv_file(csv_path);
    int frame_idx = 0;
    csv_file <<"Frame_index,Timestamp_nano_sec\n";
    while(is_recording)
    {
        if(save_queue.try_dequeue(local_frame))
        {
            writer.write(local_frame);
            auto now = chrono::steady_clock::now();
            auto timestamp_ns = chrono::duration_cast<chrono::nanoseconds>(now.time_since_epoch()).count();
            if(csv_file.is_open())
            {
                csv_file << frame_idx << "," << timestamp_ns << "\n";
            }

            frame_idx ++;
        }
        else
        {
            this_thread::sleep_for(chrono::milliseconds(1));
        }


    }
    csv_file.close();
    cout<<"(CLOSING THE CSV)"<<endl;
    while(save_queue.try_dequeue(local_frame))
    {
        writer.write(local_frame);
    } 
    writer.release();
}