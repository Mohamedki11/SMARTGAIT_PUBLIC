#include "SharedState.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
using namespace std;


void ai_worker()
{
    cv::Mat ai_frame, gray, baseline_frame;
    bool baseline_set = false;

    auto ai_start_warm = chrono::steady_clock::now();
    auto rec_start = chrono::steady_clock::now();
    auto cooldown_start = chrono::steady_clock::now();
    bool in_cooldown = false;

    while(is_ai_active)
    {
        if(ai_queue.try_dequeue(ai_frame))
        {
            auto now = chrono::steady_clock::now();
            cv::Mat small_frm;
            cv::resize(ai_frame, small_frm, cv::Size(AI_WIDTH, AI_HEIGHT));
            cv::cvtColor(small_frm, gray, cv::COLOR_BGR2GRAY); //cobvert to Gray and Blur to ignore tiny changes
            cv::GaussianBlur(gray, gray, cv::Size(BLUR_SIZE, BLUR_SIZE), 0);

            if(in_cooldown)
            {
                if(chrono::duration_cast<chrono::milliseconds>(now - cooldown_start).count() < cooldown_duration_ms.load())
                {
                   ///cv::addWeighted(baseline_frame, 0.95, gray, 0.05, 0, baseline_frame);
                    continue; 
                }
                else{
                    baseline_set = false;
                    in_cooldown = false; //cooldown finished
                    continue;
                }
                 
            }

            // empty hallway
            if(!baseline_set)
            {
                gray.copyTo(baseline_frame);
                ai_start_warm = now;
                cout <<"[C++ CAMERA]: LAUNCHING AI..."<<endl;
                baseline_set = true;
                continue;
            }



            // ignore the first two seconds of the AI
            if(chrono::duration_cast<chrono::seconds>(now - ai_start_warm).count() < 2)
            {  
                gray.copyTo(baseline_frame);
                continue;
            }

            //substract from the empty room ( detection)
            cv::Mat diff, thresh;
            cv::absdiff(baseline_frame, gray, diff);
            cv::threshold(diff, thresh, THREASHOLD_VAL, 255, cv::THRESH_BINARY);
            cv::dilate(thresh, thresh, cv::Mat(), cv::Point(-1, -1),2 );
            //find the contours
            vector<vector<cv::Point>> contours;
            cv::findContours(thresh, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

            double total_motion_detected = 0;

          
            for(size_t i=0;i < contours.size(); i++)
            {
                double area = cv::contourArea(contours[i]);
                //safety condition to keep going
                if(contours[i].empty())
                continue;
                //ignore tiny pixel noise
                if(area > MIN_CONTOUR_AREA)
                {
                    total_motion_detected +=area;
                }
            }


            //start recording
            if(!motion_detected && (total_motion_detected >= detection_theshold.load()))
            {
                cout <<"[C++ CAMERA ENGINE ]: AI : HUMAN DETECTED...RECORDING"<<endl;
                rec_start = now;
                motion_detected = true;
            }


            //stop recording
            if(motion_detected)
            {
                
                if(chrono::duration_cast<chrono::milliseconds>(now - rec_start).count() >= ((record_duration_sec.load() * 1000) + 300)) //300 ms just to account for other stuff happening 
                {
                    cout <<"[C++ CAMERA ENGINE ]: AI : STOPPING RECORDING...."<<endl;
                    motion_detected = false ;
                    cooldown_start = now;
                    in_cooldown = true;

                }
            }
            
            //adjusting the frames to outside ligtning ..etc
            cv::addWeighted(baseline_frame, 0.99, gray, 0.01, 0, baseline_frame);

            //----------------------------//
            //SHOWS THE ACTUAL AI INTERFACE ( becareful of using with the radar)
            ////////////////////cv::imshow("Live Motion detection", thresh);
            /////////////////////cv::waitKey(1);
            //----------------------------//
        }
        else 
        {
            this_thread::sleep_for(chrono::milliseconds(5));
        }
    }  
    cout<<"im hereeee (closing theeeeeee AI)"<<endl;
    //----------------------------//
    //////////////cv::destroyAllWindows();
    //----------------------------//
    while(ai_queue.try_dequeue(ai_frame))
        {
            //this_thread::sleep_for(chrono::milliseconds(5));
        }

}


