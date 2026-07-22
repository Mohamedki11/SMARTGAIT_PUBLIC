#include "SharedState.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
using namespace std;




//thread 2 : ssd save 
void ssd_thread_worker(string bin_path)
{
    cout <<"RADAR: SSD Saver thread started"<<endl;

    ofstream bin_file(bin_path, ios::out | ios::binary);
    if(!bin_file)
    {
        cerr <<"[RADAR ERROR] could not open .bin file!"<< endl;
        return;
    }
    vector<uint8_t> incoming_packet;
    vector<uint8_t> leftover;
    bool is_sync = false;
    size_t fail_idx;
    //keeps going if there is data or the flag is on
    while(radar_record)
    {
        if(radar_queue.try_dequeue(incoming_packet))
        {
            //if they are byte from the last odd packet , we put them in the new packet
            if(!leftover.empty())
            {
                incoming_packet.insert(incoming_packet.begin(), leftover.begin(),leftover.end());
                leftover.clear();
            }

            if(incoming_packet.size() % 2 != 0)
            {
                leftover.push_back(incoming_packet.back());
                incoming_packet.pop_back();
            }


            if(!is_sync)

            {
                uint16_t* words = reinterpret_cast<uint16_t*>(incoming_packet.data());
                size_t num_words = incoming_packet.size() / 2;

                int sync_idx = -1;

                for(size_t i = 0; i < num_words; i++) {
                    if(words[i] >= HEADER_VAL_TX1) { // Found the TX1 header!
                        // Look ahead 4096 words (8192 bytes) to prove it's a real header
                        if(i + WORDS_PER_SWEEP < num_words) {
                            if(words[i + WORDS_PER_SWEEP] >= HEADER_VAL_TX1) {
                                sync_idx = i;
                                break;
                            }
                        } else {
                            sync_idx = i; // Too close to the end, assume it's good
                            break;
                        }
                    }
                }

                if(sync_idx != -1) {
                    is_sync = true;
                    // Because we searched by words, we MUST multiply by 2 to get bytes!
                    size_t byte_offset = sync_idx * 2; 
                    size_t bytes_to_write = incoming_packet.size() - byte_offset;

                    // Write to SSD
                    bin_file.write(reinterpret_cast<const char*>(incoming_packet.data() + byte_offset), bytes_to_write);
                }
                if(sync_idx != -1)
                {
                    size_t fail_idx = 0;
                    is_sync = true;
                    size_t byte_offset = sync_idx * 2;
                    size_t bytes = incoming_packet.size() - byte_offset;

                    //prevent odd byte writes ( for future analysing)
                    if(bytes % 2 !=0)
                    {
                        leftover.push_back(incoming_packet.back());
                        bytes -=1;
                    }
                    bin_file.write(reinterpret_cast<const char*>(incoming_packet.data() + byte_offset), bytes);
                }
                else{
                    fail_idx ++;
                    cerr <<"[RADAR] : SEARCHING FOR TRUE ALIGNEMENT"<<endl;
                }
            }
            else{
                size_t bytes = incoming_packet.size();
                //prevent odd byte writes ( for future analysing)
                    if(bytes % 2 !=0)
                    {
                        leftover.push_back(incoming_packet.back());
                        bytes -=1;
                    }
                 bin_file.write(reinterpret_cast<const char*>(incoming_packet.data()), bytes);
                 
            }
            
        }
        else{
            this_thread::sleep_for(chrono::milliseconds(1));
        }
    }

    while(radar_queue.try_dequeue(incoming_packet))
    {
        if(is_sync)
        {
            if(!leftover.empty())
            {
                incoming_packet.insert(incoming_packet.begin(), leftover.begin(),leftover.end());
                leftover.clear();
            }
            size_t bytes = incoming_packet.size();
                //prevent odd byte writes ( for future analysing)
                    if(bytes % 2 !=0)
                    {
                        leftover.push_back(incoming_packet.back());
                        bytes -=1;
                    }
                 bin_file.write(reinterpret_cast<const char*>(incoming_packet.data()), bytes);

        }

    }
    bin_file.flush(); //immediatly saves the last file from the RAM
    bin_file.close();
    cout <<"RADAR: saved"<<endl;
}
