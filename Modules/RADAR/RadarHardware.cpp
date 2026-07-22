#include "SharedState.h"
#include <libusb-1.0/libusb.h>
#include <iostream>
#include <cmath>
#include <cstring>
using namespace std;


// 1. Send Parameters (1024 Bytes)
bool send_param_command(libusb_device_handle *handle, uint16_t base, uint8_t val) {
    uint16_t command = base + val;
    unsigned char buffer[1024];
    for (int i = 0; i < 512; ++i) { memcpy(buffer + i * 2, &command, 2); }
    int transferred = 0;
    return libusb_bulk_transfer(handle, 0x02, buffer, 1024, &transferred, 1000) == 0;
}

// 2. Send PLL Registers (512 Bytes)
bool send_pll_command(libusb_device_handle *handle, uint16_t base, uint8_t val) {
    uint16_t command = base + val;
    unsigned char buffer[512];
    for (int i = 0; i < 256; ++i) { memcpy(buffer + i * 2, &command, 2); }
    int transferred = 0;
    return libusb_bulk_transfer(handle, 0x02, buffer, 512, &transferred, 1000) == 0;
}

// 3. The 24GHz Boot Sequence
bool boot_radar_hardware(libusb_device_handle *handle) {
    cout << "[RADAR] Sending 24GHz Boot Sequence..." << endl;

    // A. Base Parameters: Sawtooth, 1ms Sweep, Max Samples, TX1, RX1
    send_param_command(handle, 0xE100, 0); 
    send_param_command(handle, 0xE200, 2); 
    send_param_command(handle, 0xE300, 1); 
    send_param_command(handle, 0xE400, 1); 
    send_param_command(handle, 0xE500, 1); 

    // B. The 24GHz PLL Math (Straight from the other team's working code)
    double freq_low = 24e9;
    double freq_high = 25e9;
    double sweep_time_s = 1e-3; // 1ms
    const double T_ref = 1.0 / 50e6;
    double sweepup_percent = 0.92; // For 1GHz bandwidth

    int MaxSweepover = 8192; // For 1ms
    int sweep_stop = static_cast<int>(std::ceil(MaxSweepover * (sweepup_percent + 0.01)));
    double T_sweepup = sweep_time_s * sweepup_percent;

    // Send SweepStop
    send_pll_command(handle, 0xD200, sweep_stop & 0xFF);
    send_pll_command(handle, 0xD100, (sweep_stop >> 8) & 0xFF);

    double F_start = freq_low / 16.0;
    double F_stop = freq_high / 16.0;
    double N_start = F_start / 50e6;
    double N_stop = F_stop / 50e6;

    int PLLReg03 = static_cast<int>(std::floor(N_start));
    double frac_start = N_start - PLLReg03;
    int PLLReg04 = static_cast<int>(std::round(frac_start * std::pow(2, 24)));

    send_pll_command(handle, 0xC300, PLLReg03 & 0xFF);
    send_pll_command(handle, 0xC200, (PLLReg03 >> 8) & 0xFF);
    send_pll_command(handle, 0xC100, (PLLReg03 >> 16) & 0xFF);
    send_pll_command(handle, 0xC600, PLLReg04 & 0xFF);
    send_pll_command(handle, 0xC500, (PLLReg04 >> 8) & 0xFF);
    send_pll_command(handle, 0xC400, (PLLReg04 >> 16) & 0xFF);

    double num_steps = T_sweepup / T_ref;
    double step_double = (N_stop - N_start) / num_steps;
    int PLLReg0A = static_cast<int>(std::round(step_double * std::pow(2, 24)));

    send_pll_command(handle, 0xC900, PLLReg0A & 0xFF);
    send_pll_command(handle, 0xC800, (PLLReg0A >> 8) & 0xFF);
    send_pll_command(handle, 0xC700, (PLLReg0A >> 16) & 0xFF);

    int rounded_steps = static_cast<int>(std::round((N_stop - N_start) / (PLLReg0A / std::pow(2, 24))));
    int num_50MHz = static_cast<int>(std::floor(rounded_steps * PLLReg0A / std::pow(2, 24)));
    int PLLReg0C = PLLReg03 + num_50MHz;
    int PLLReg0D = static_cast<int>((rounded_steps * PLLReg0A) % static_cast<int>(std::pow(2, 24))) + PLLReg04;

    if (PLLReg0D >= static_cast<int>(std::pow(2, 24))) {
        PLLReg0C += 1;
        PLLReg0D -= static_cast<int>(std::pow(2, 24));
    }

    send_pll_command(handle, 0xCC00, PLLReg0C & 0xFF);
    send_pll_command(handle, 0xCB00, (PLLReg0C >> 8) & 0xFF);
    send_pll_command(handle, 0xCA00, (PLLReg0C >> 16) & 0xFF);
    send_pll_command(handle, 0xCF00, PLLReg0D & 0xFF);
    send_pll_command(handle, 0xCE00, (PLLReg0D >> 8) & 0xFF);
    send_pll_command(handle, 0xCD00, (PLLReg0D >> 16) & 0xFF);
    
    cout << "[RADAR] 24GHz PLL Locked and Sweeping!" << endl;
    return true;
}


void reset_cypress_fifo(libusb_device_handle *handle)
{
    //refer to the documentation of the cypress 
    cout <<"[RADAR] : Forcing Hardware Reset CYPRESS FIFO..." <<endl;
    unsigned char data[1];
    data[0] = 0x80;  //NAKALL block usb transfers
    libusb_control_transfer(handle, 0x40, 0xA0, 0xE604, 0, data, 1, 1000);

    //reset endpoint 6
    data[0] = 0x86;
    libusb_control_transfer(handle, 0x40, 0xA0, 0xE604, 0, data, 1, 1000);

    data[0] = 0x00;
    libusb_control_transfer(handle, 0x40, 0xA0, 0xE604, 0, data, 1, 1000);

}

//ASYNCHRONUS USB CALLBACK ( instead of a synchronus solution we will implment a asynchronus solution where the libusb keeps reading and filling bufers we make )
static void LIBUSB_CALL usb_transfer_cb(struct libusb_transfer *xfr)
{
    if (xfr->status == LIBUSB_TRANSFER_COMPLETED)
    {
        if (xfr->actual_length > 0)
        {
            vector<uint8_t> packet;
            packet.reserve(xfr->actual_length);
            packet.assign(xfr->buffer, xfr->buffer + xfr->actual_length);

            if (radar_record && realtime_active)
            {
                vector<uint8_t> storage_vec = packet;
                radar_queue.enqueue(move(storage_vec));
                //real time queue ( but limited to prevent RAM eating)
                if(realtime_queue.size_approx() < 20)
                {
                    realtime_queue.enqueue(packet);
                }
            }
            else if(radar_record)
            {
                radar_queue.enqueue(move(packet));
            }
            else if(realtime_active)
            {
                if(realtime_queue.size_approx() < 20)
                {
                    realtime_queue.enqueue(move(packet));
                }
            }

        }
        
    }
    else if(xfr->status == LIBUSB_TRANSFER_CANCELLED)
    {
        return;//exit
    }
    else
    {
        cerr <<"[RADAR] : Async USB ERROR. STATUS ....:"<< xfr->status <<endl;
        cerr <<"[RADAR]: Restarting System..."<<endl;
        radar_error = true ; //Tracking Radar Producer Health
        radar_run_flag = false;
        
    }

    //instantly resubmit the transfer to the kernel
    if(radar_run_flag)
    {
        libusb_submit_transfer(xfr);
    }
}



//First thread for the USB
void usb_catcher_worker()
{
    pthread_t this_thread = pthread_self();
    struct sched_param params;
    params.sched_priority = sched_get_priority_max(SCHED_FIFO);
    pthread_setschedparam(this_thread, SCHED_FIFO, &params);
    
    cout <<"[RADAR] USB Catcher Thread Started :"<<endl;
    libusb_context *ctx = nullptr;
    libusb_init(&ctx);
    if(libusb_init(&ctx)<0)
    {
        cout <<"[RADAR]: FAILED TO initialize libusb"<<endl;
        radar_run_flag = false;
        return;
    }

    libusb_device_handle *dev_handle = libusb_open_device_with_vid_pid(ctx, 0x04b4, 0x8613);
    if(!dev_handle)
    {
        cerr <<"[RADAR FATAL] CANNOT OPEN RADAR"<<endl;
        libusb_exit(ctx);
        radar_run_flag = false;
        return;
    }

    

    //disconnect linux default drivers and claim the interface
    if(libusb_kernel_driver_active(dev_handle, 0) == 1)
    {
        libusb_detach_kernel_driver(dev_handle, 0);
    }

    if(libusb_claim_interface(dev_handle, 0) < 0)
    {
        cerr << "[RADAR  FATAL ]: CANNOT CLAIM USB INTERFACE"<<endl;
        libusb_close(dev_handle);
        libusb_exit(ctx);
        radar_run_flag = false;
        return;
    }
    libusb_set_interface_alt_setting(dev_handle, 0, 1);
   

    cout <<"[RADAR]: SUCCESS: listening to endpoint 6"<<endl;
    if(!boot_radar_hardware(dev_handle))
    {
        libusb_close(dev_handle);
        libusb_exit(ctx);
        radar_run_flag = false;
        exit(0);
    }
    
    
    
    vector<struct libusb_transfer*> transfers;
    vector<vector<uint8_t>> buffers(NUM_USB_TRANFERS, vector<uint8_t>(USB_BUFFER_SIZE));

    for (int i=0;i < NUM_USB_TRANFERS ; i++)
    {
        struct libusb_transfer *xfr = libusb_alloc_transfer(0);
        libusb_fill_bulk_transfer(xfr, dev_handle, 0x86, buffers[i].data(), USB_BUFFER_SIZE, usb_transfer_cb, nullptr, 1000);
        libusb_submit_transfer(xfr);
        transfers.push_back(xfr);
    }

    struct timeval tv = {0, 100000}; //timeout after 100ms for event loop



    while(radar_run_flag)
    {
        if(soft_restart)
        {
            cout<<"[RADAR]: executing soft restart before recording...."<<endl;
            

            //clean the C++ queue
            reset_cypress_fifo(dev_handle);
            
            libusb_clear_halt(dev_handle, 0x86);
            soft_restart = false;
        }
        // this process the callback we did 
        libusb_handle_events_timeout_completed(ctx, &tv, nullptr);

    }

    //cleanup ASYNC PIPE
    for(auto xfr: transfers) { libusb_cancel_transfer(xfr);}
    struct timeval cancel_tv = {0, 10000};//timeout
    for(int i =0 ;i<10;i++) { libusb_handle_events_timeout_completed(ctx, &cancel_tv, nullptr); }
    for (auto xfr : transfers) { libusb_free_transfer(xfr);}
    libusb_release_interface(dev_handle, 0);
    libusb_close(dev_handle);
    libusb_exit(ctx);
    cout <<"[RADAR]: usb catcher thread stopped"<< endl;

}