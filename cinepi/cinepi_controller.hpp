#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <time.h>
#include <stdint.h>

#include "dng_encoder.hpp"
#include "preview/preview.hpp"
#include "core/logging.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>

#include <mutex>
#include <queue>
#include <thread>

#include "utils.hpp"

#include "cinepi_frameinfo.hpp"
#include "core/stream_info.hpp"

#include "cinepi_recorder.hpp"
#include "cinepi_state.hpp"
#include "raw_options.hpp"
#include <sw/redis++/redis++.h>

#define CHANNEL_CONTROLS "cp_controls"
#define CHANNEL_STATS "cp_stats"
#define CHANNEL_HISTOGRAM "cp_histogram"

#define REDIS_DEFAULT "redis://127.0.0.1:6379/0"

using namespace sw::redis;

class CinePIController : public CinePIState
{
    public:
        CinePIController(CinePIRecorder *app) : CinePIState(), app_(app), options_(app->GetOptions()), 
            folderOpen(false), abortThread_(false), cameraInit_(true), cameraRunning(false), triggerStill_(0) {};
        ~CinePIController() {
            abortThread_ = true;
            main_thread_.join();
            if(snapshot_thread_.joinable())
                snapshot_thread_.join();
        };

        void start(){
            redis_ = new Redis(options_->redis.value_or(REDIS_DEFAULT));
            LOG(2, redis_->ping());
            main_thread_ = std::thread(std::bind(&CinePIController::mainThread, this));
            snapshot_thread_ = std::thread(std::bind(&CinePIController::snapshotThread, this));
        }

        void sync();

        void process(CompletedRequestPtr &completed_request);
        void process_stream_info(libcamera::StreamConfiguration const &cfg){
            redis_->publish(CHANNEL_STATS, cfg.toString());
            redis_->set(CONTROL_KEY_WIDTH, std::to_string(cfg.size.width));
            redis_->set(CONTROL_KEY_HEIGHT, std::to_string(cfg.size.height));
        }

        bool folderOpen;
        bool cameraRunning;

        bool configChanged(){
            bool c = cameraInit_;
            cameraInit_ = false;
            return c;
        }

        int triggerRec(){
            if(!disk_mounted(const_cast<RawOptions *>(options_))){
                return 0;
            }
            int state = trigger_;
            if(state < 0){
                clip_number_++;
            }
            trigger_ = 0;
            return state;
        }

        int triggerStill(){
            int ts_ = triggerStill_;
            triggerStill_ = 0;
            return ts_;
        }

    protected:

    private:
        void mainThread();
        void pubThread();
        void snapshotThread();

        int trigger_;
        int triggerStill_;

        bool cameraInit_;

        CinePIRecorder *app_;
        RawOptions *options_;

        Redis *redis_;

        bool abortThread_;
        std::thread main_thread_;
        std::thread snapshot_thread_;
};
