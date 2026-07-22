#include "SharedState.h"
#include <iostream>

using namespace std;

//thread 1 that wakes up the camera and keep it active and the GUI active
void capture_worker() 
{
    string pipe = //"nvarguscamerasrc sensor-id=0 ! "
    "v4l2src device=/dev/video1 ! "
    //"video/x-raw(memory:NVMM), width=1280, height=720, format=NV12, framerate=30/1 ! "
    "image/jpeg, width=1280, height=720, framerate=30/1, format=MJPG ! "
    "nvv4l2decoder mjpeg=1 ! nvvidconv ! video/x-raw(memory:NVMM), format=NV12 ! "//USB CAM
    "tee name=t "
    //"t. ! queue ! nvvidconv ! nv3dsink window-x=0 window-y=0 window-width=640 window-height=360 "
    "t. ! queue ! nvvidconv ! video/x-raw,format=BGRx ! "
    "videoconvert ! video/x-raw, format=BGR ! appsink drop=true sync=false" ;

    cv::VideoCapture cap(pipe,cv::CAP_GSTREAMER);
    cv::Mat frame;
    if(!cap.isOpened())
    {
        cerr<<"ERROR"<<endl;
        run_flag = false;
        camera_error = true;
        return;
    }

    int empty_frames = 0;

    while (run_flag)
    {
        cap >> frame;

        if(frame.empty())
        {
            empty_frames ++;
            if(empty_frames > 60)
            {
                cerr <<"[CAMERA]: CSI CAMERA unplugged or frozen! Forcing System Reboot!" <<endl;
                camera_error = true;
                run_flag = false;
                break;
            }
            continue;
        }
        empty_frames = 0;

        if (is_recording)
        {
            //check if queue if FULL or not 
            //it keeps pushing 400 frames ( around 1 GB of RAM , before it stops)
            if(save_queue.size_approx() <= MAX_SAVE_QUEUE)
            {
                save_queue.enqueue(frame.clone()); // pass the stream to the save_queue so it saves to the SSD (used clone() to not lose the orginal frame if the saving or AI algorithm does corrupt the real frames)
                
            }
            else{
               cout <<"[CAMERA ENGINE]: WARNING!!! SSD LAGGING! Queue is Full. Frame Dropped"<<endl;
            }
        
        }
        if(is_ai_active)
        {
            //check if queue if FULL or not 
            if(ai_queue.size_approx() <= MAX_AI_QUEUE)
            {
                ai_queue.enqueue(frame.clone()); // pass the stream to the save_queue so it saves to the SSD (used clone() to not lose the orginal frame if the saving or AI algorithm does corrupt the real frames)
            }
            else{
                 cout <<"[CAMERA ENGINE]: WARNING!!! AI is LAGGING! Queue is Full. Frame Dropped"<<endl;   
        }
        }
        //webinterface video feed
            //check if queue if FULL or not 
            if(web_queue.size_approx() <= MAX_WEB_QUEUE)
            {
                cv::Mat small_frame;
                cv::resize(frame.clone(), small_frame, cv::Size(AI_WIDTH, AI_HEIGHT));
                web_queue.enqueue(small_frame); // pass the stream to the save_queue so it saves to the SSD (used clone() to not lose the orginal frame if the saving or AI algorithm does corrupt the real frames)
            }
            
    }
}
