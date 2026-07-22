#pragma once
#include <atomic>
#include <string>
#include <opencv2/opencv.hpp>

#include "readerwriterqueue.h"
using namespace std;

// Camera Parameters
constexpr int CAM_WIDTH = 1280;
constexpr int CAM_HEIGHT = 720;
constexpr int CAM_FPS = 30;

constexpr int AI_WIDTH = 640;
constexpr int AI_HEIGHT = 360;

//queue limits
constexpr size_t MAX_SAVE_QUEUE = 400; //approx maximum of 1GB of RAM to save to the SSD 
constexpr size_t MAX_AI_QUEUE = 10;
constexpr size_t MAX_WEB_QUEUE = 5;

//AI constants 
constexpr int BLUR_SIZE = 21;
constexpr double MIN_CONTOUR_AREA = 40.0;//the smallest threashhold possible before it actually starts the detection (to ignore small moving things)
constexpr double THREASHOLD_VAL = 25.0;
//global shared vars
extern atomic<bool> run_flag; // pour controler le camera a laide de python 
extern atomic<bool> is_recording;
extern atomic<bool> is_ai_active;

extern atomic<bool> camera_error;//detecting camera producer error

extern atomic<bool> motion_detected;

extern atomic<int> record_duration_sec;// record durations
extern atomic<int> cooldown_duration_ms;//delay before start of next recording
extern atomic<double> detection_theshold;//the generale threashhold value 


//the queues
extern moodycamel::ReaderWriterQueue<cv::Mat> save_queue; 
extern moodycamel::ReaderWriterQueue<cv::Mat> ai_queue;
extern moodycamel::ReaderWriterQueue<cv::Mat> web_queue;

