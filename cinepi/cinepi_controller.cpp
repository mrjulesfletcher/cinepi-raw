#include "cinepi_controller.hpp"

using namespace std;
using namespace std::chrono;

#define THREAD_SLEEP_MS 10

#define LV_KEY_ZOOM "lv_zoom"
#define LV_KEY_ENABLE "lv_en"

#define CP_DEF_WIDTH 1920
#define CP_DEF_HEIGHT 1080
#define CP_DEF_FRAMERATE 30
#define CP_DEF_ISO 400
#define CP_DEF_SHUTTER 60
#define CP_DEF_AWB 1
#define CP_DEF_COMPRESS 1

void CinePIController::sync(){
    auto pipe = redis_->pipeline();
    auto pipe_replies = pipe.get(CONTROL_KEY_WIDTH)
                            .get(CONTROL_KEY_HEIGHT)
                            .get(CONTROL_KEY_FRAMERATE)
                            .get(CONTROL_KEY_ISO)
                            .get(CONTROL_KEY_SHUTTER_SPEED)
                            .get(CONTROL_KEY_WB)
                            .get(CONTROL_KEY_COLORGAINS)
                            .get(CONTROL_KEY_COMPRESSION)
                            .exec();

    // width_ = stoi(pipe_replies.get<std::optional<std::string>>(0).value_or("1920"));

    auto width = pipe_replies.get<OptionalString>(0);
    if(width){
        width_ = stoi(*width);
    }else{
        width_ = CP_DEF_WIDTH;
        redis_->set(CONTROL_KEY_WIDTH, to_string(width_));
    }
        
    auto height = pipe_replies.get<OptionalString>(1);
    if(height){
        height_ = stoi(*height);
    }else{
        height_ = CP_DEF_HEIGHT;
        redis_->set(CONTROL_KEY_HEIGHT, to_string(height_)); 
    }

    auto framerate = pipe_replies.get<OptionalString>(2);
    if(framerate){
        framerate_ = stoi(*framerate);
    }else{
        framerate_ = CP_DEF_FRAMERATE;
        redis_->set(CONTROL_KEY_FRAMERATE, to_string(framerate_)); 
    }

    auto iso = pipe_replies.get<OptionalString>(3);
    if(iso){
        iso_ = stoi(*iso)/100;
    }else{
        iso_ = CP_DEF_ISO;
        redis_->set(CONTROL_KEY_ISO, to_string(iso_)); 
    }

    auto shutter_speed = pipe_replies.get<OptionalString>(4);
    if(shutter_speed){
        shutter_speed_ = stoi(*shutter_speed);
    }else{
        shutter_speed_ = CP_DEF_SHUTTER;
        redis_->set(CONTROL_KEY_SHUTTER_SPEED, to_string(shutter_speed_)); 
    }

    auto awb = pipe_replies.get<OptionalString>(5);
    if(awb){
        awb_ = stoi(*awb);
    }else{
        awb_ = CP_DEF_AWB;
        redis_->set(CONTROL_KEY_WB, to_string(awb_)); 
    }
    
    auto compress = pipe_replies.get<OptionalString>(7);
    if(compress){
        compression_ = stoi(*compress);
    }else{
        compression_ = CP_DEF_COMPRESS;
        redis_->set(CONTROL_KEY_COMPRESSION, to_string(compression_));
    }

    char *ptr = strtok(&(*pipe_replies.get<OptionalString>(6))[0], ",");
    uint8_t i = 0;
    while(ptr != NULL){
        cg_rb_[i] = (float)stof(ptr);
        i++;
        ptr = strtok(NULL, ",");  
    }

    options_->compression = compression_;
    options_->width = width_;
    options_->height = height_;
    options_->framerate = framerate_;
    options_->gain = iso_;

    options_->shutter = shutter_speed_ * 1e+6;

    options_->awbEn = awb_;
    if(awb_)
        options_->awb_index = 5; // daylight
    else{
        options_->awb_gain_r = cg_rb_[0];
        options_->awb_gain_b = cg_rb_[1];
    }
    
    options_->denoise = "off";
    options_->lores_width = 400;
    options_->lores_height = 200;
    options_->mode_string = "0:0:0:0";
}

void CinePIController::process(CompletedRequestPtr &completed_request){
    CinePIFrameInfo info(completed_request->metadata);

    redis_->publish(CHANNEL_STATS, to_string(completed_request->framerate));
    redis_->publish(CHANNEL_STATS, to_string(info.colorTemp));
    redis_->publish(CHANNEL_STATS, to_string(info.focus));
    redis_->publish(CHANNEL_STATS, to_string(app_->GetEncoder()->getFrameCount()));
    redis_->publish(CHANNEL_STATS, to_string(app_->GetEncoder()->bufferSize()));
    
    #ifdef LIBCAMERA_CINEPI_CONTROLS 
        redis_->publish(CHANNEL_STATS, info.histoString());
        redis_->publish(CHANNEL_STATS, to_string(info.trafficLight));
        redis_->publish(CHANNEL_HISTOGRAM, StringView(reinterpret_cast<const char *>(info.histogram), sizeof(info.histogram)));
    #endif
}


void CinePIController::mainThread(){

    time_point<system_clock> t = system_clock::now();

    LOG(1, "CINEPI_CONTROLLER THREAD STARTED");
    auto sub = redis_->subscriber();

    sub.on_message([this](std::string channel, std::string msg) {
        std::cout << msg << " from: " << channel << std::endl;

        if(msg.compare(CONTROL_TRIGGER_STILL) == 0){
            triggerStill_ = !triggerStill_;
            if(!triggerStill_){
                still_number_++;
            }
        }

        auto r = redis_->get(msg);
        if(r){
            if(msg.compare(CONTROL_KEY_RECORD) == 0){
                trigger_ = !is_recording_;
                is_recording_ = (bool)stoi(*r);
            } 
            else if (msg.compare(CONTROL_KEY_ISO) == 0){
                iso_ = (unsigned int)(stoi(*r)/100.0);
                libcamera::ControlList cl;
                cl.set(libcamera::controls::AnalogueGain, iso_);
                app_->SetControls(cl);
            }
            else if (msg.compare(CONTROL_KEY_WB) == 0){
                awb_ = (unsigned int)(stoi(*r));
                libcamera::ControlList cl;
                cl.set(libcamera::controls::AwbEnable, awb_);
                app_->SetControls(cl);
            } 
            else if (msg.compare(CONTROL_KEY_COLORGAINS) == 0){
                libcamera::ControlList cl;
                cl.set(libcamera::controls::AwbEnable, false);
                app_->SetControls(cl);
                std::string cg_rb_s = *r;
                char *ptr = strtok(&cg_rb_s[0], ",");
                uint8_t i = 0;
                while(ptr != NULL){
                    cg_rb_[i] = (float)stof(ptr);
                    i++;
                    ptr = strtok(NULL, ",");  
                }
                cl.set(libcamera::controls::ColourGains, libcamera::Span<const float, 2>({ cg_rb_[0], cg_rb_[1] }));
                app_->SetControls(cl);
            }
            else if (msg.compare(CONTROL_KEY_SHUTTER_ANGLE) == 0){
                // StreamInfo info = app_->GetStreamInfo(app->RawStream());
                shutter_angle_ = stof(*r);
                shutter_speed_ = 1.0 / ((framerate_ * 360.0) / shutter_angle_);
                uint64_t shutterTime = shutter_speed_ * 1e+6;
                libcamera::ControlList cl;
                cl.set(libcamera::controls::ExposureTime, shutterTime);
                app_->SetControls(cl);
            } 
            else if (msg.compare(CONTROL_KEY_SHUTTER_SPEED) == 0){
                shutter_speed_ = 1.0 / stof(*r);
                uint64_t shutterTime = shutter_speed_ * 1e+6;
                libcamera::ControlList cl;
                cl.set(libcamera::controls::ExposureTime, shutterTime);
                app_->SetControls(cl);
            }
            else if(msg.compare(CONTROL_KEY_WIDTH) == 0){
                width_ = (uint16_t)(stoi(*r));
                options_->width = width_;
            }
            else if(msg.compare(CONTROL_KEY_HEIGHT) == 0){
                height_ = (uint16_t)(stoi(*r));
                options_->height = height_;
            }
            else if(msg.compare(CONTROL_KEY_COMPRESSION) == 0){
                compression_ = stoi(*r);
                options_->compression = compression_;
                cameraInit_ = true;
            }
            else if(msg.compare(CONTROL_KEY_FRAMERATE) == 0){
                framerate_ = stof(*r);
                options_->framerate = framerate_;
                cameraInit_ = true;
            }
            else if(msg.compare(CONTROL_KEY_CAMERAINIT) == 0){
                cameraInit_ = true;
            } 
            else if(msg.compare(LV_KEY_ZOOM) == 0){
                float zoom = stof(*r);

                float roi_x = 1.0, roi_y = 1.0, roi_height = 1.0, roi_width = 1.0;

                roi_width = roi_width / zoom;
                roi_height = roi_height / zoom;
                roi_x = (roi_x - roi_width) / 2;
                roi_y = (roi_y - roi_height) / 2;

                libcamera::Rectangle sensor_area = *app_->GetCamera()->properties().get(properties::ScalerCropMaximum);
                int x = roi_x * sensor_area.width;
                int y = roi_y * sensor_area.height;
                int w = roi_width * sensor_area.width;
                int h = roi_height * sensor_area.height;
                libcamera::Rectangle crop(x, y, w, h);
                crop.translateBy(sensor_area.topLeft());

                libcamera::ControlList cl;
                cl.set(libcamera::controls::ScalerCrop, crop);
                app_->SetControls(cl);
            }

        }
    });

    sub.subscribe(CHANNEL_CONTROLS);

    while (true) {
        try {
            sub.consume();
        } catch (const Error &err) {
            // Handle exceptions.
        }

        t += milliseconds(THREAD_SLEEP_MS);
        this_thread::sleep_until(t);
    }
}

void CinePIController::snapshotThread(){
    using namespace std::chrono;
    while(!abortThread_){
        uint32_t interval = options_->snapshot_interval;
        try{
            auto r = redis_->get(CONTROL_KEY_BGSAVE_INTERVAL);
            if(r) interval = std::stoul(*r);
        }catch(const std::exception &e){ }

        if(interval == 0){
            std::this_thread::sleep_for(seconds(1));
            continue;
        }

        std::this_thread::sleep_for(milliseconds(interval));
        if(abortThread_) break;
        try{
            redis_->bgsave();
        }catch(const std::exception &e){ }
    }
}
