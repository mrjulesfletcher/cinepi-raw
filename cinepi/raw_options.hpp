/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2020, Raspberry Pi (Trading) Ltd.
 *
 * still_video.hpp - video capture program options
 */

#pragma once

#include <cstdio>

#include <string>

#include "core/video_options.hpp"

struct RawOptions : public VideoOptions
{   
        RawOptions() : VideoOptions()
        {
                using namespace boost::program_options;
                options_.add_options()
                        ("redis-snapshot-ms", value<uint32_t>(&snapshot_interval)->default_value(5000),
                         "Interval in milliseconds between Redis snapshots (0 to disable)");
        }

	std::optional<std::string> redis;

	uint32_t clip_number;
	std::string mediaDest;
	std::string folder;

	float wb;
	std::string sensor;
	std::string model;
	std::string make;
	std::string serial;

        float clipping;

        uint32_t snapshot_interval;

};
