/*
 * mmWaveQuickConfig.cpp
 *
 * This file will load a .txt file specified at the command line
 * and parse it, sending each line to the mmWaveCLI service as a command.
 * Examples configurations are provided under the cfg folder.
 *
 * Copyright (C) 2017 Texas Instruments Incorporated - http://www.ti.com/
 *
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *    Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 *    Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "ros/ros.h"
#include "ti_mmwave_rospkg/mmWaveCLI.h"
#include <cstdlib>
#include <fstream>
#include <regex>
#include <stdio.h>

int main(int argc, char **argv) {
  ros::init(argc, argv, "mmWaveQuickConfig");
  if (argc != 2) {
    ROS_INFO("mmWaveQuickConfig: usage: mmWaveQuickConfig "
             "/file_directory/params.cfg");
    return 1;
  } else {
    ROS_INFO(
        "mmWaveQuickConfig: Configuring mmWave device using config file: %s",
        argv[1]);
  }

  ros::NodeHandle n;
  ros::ServiceClient client =
      n.serviceClient<ti_mmwave_rospkg::mmWaveCLI>("/mmWaveCommSrv/mmWaveCLI");
  ti_mmwave_rospkg::mmWaveCLI srv;
  std::ifstream myParams;

  // wait for service to become available
  ros::service::waitForService("/mmWaveCommSrv/mmWaveCLI", 100000);

  int txAntennas = 0;
  myParams.open(argv[1]);

  if (myParams.is_open()) {
    while (std::getline(myParams, srv.request.comm)) {
      // Remove Windows carriage-return if present
      srv.request.comm.erase(
          std::remove(srv.request.comm.begin(), srv.request.comm.end(), '\r'),
          srv.request.comm.end());

      // Ignore comment lines (first non-space char is '%') or blank lines
      if (std::regex_match(srv.request.comm, std::regex("^\\s*%.*")) ||
          std::regex_match(srv.request.comm, std::regex("^\\s*"))) {
        ROS_INFO("mmWaveQuickConfig: Ignored blank or comment line: '%s'",
                 srv.request.comm.c_str());
      }

      // Send commands to mmWave sensor
      else {
        ROS_INFO("mmWaveQuickConfig: Sending command: '%s'",
                 srv.request.comm.c_str());
        int numTries = 0;

        // Try each command twice if first time fails (in case serial port
        // connection had initial error)
        while (numTries < 2) {
          if (client.call(srv)) {
            if (std::regex_search(srv.response.resp, std::regex("Done"))) {
              ROS_INFO("mmWaveQuickConfig: Command successful (mmWave sensor "
                       "responded with 'Done')");

              size_t pos = 0;
              int i = 0;

              std::string cmd;
              std::string token;
              while ((pos = srv.request.comm.find(" ")) != std::string::npos) {
                token = srv.request.comm.substr(0, pos);
                if (!token.compare("chirpCfg")) {
                  txAntennas++;
                }

                if (i == 0) {
                  cmd = token;
                } else if (!cmd.compare("frameCfg")) {
                  if (i == 1) {
                    n.setParam("/mmWave_Manager/chirpStartIdx",
                               std::stoi(token));
                  } else if (i == 2) {
                    n.setParam("/mmWave_Manager/chirpEndIdx", std::stoi(token));
                  } else if (i == 3) {
                    n.setParam("/mmWave_Manager/numLoops", std::stoi(token));
                  } else if (i == 4) {
                    n.setParam("/mmWave_Manager/numFrames", std::stoi(token));
                  } else if (i == 5) {
                    n.setParam("/mmWave_Manager/framePeriodicity",
                               std::stof(token));
                  }
                } else if (!cmd.compare("profileCfg")) {
                  if (i == 2) {
                    n.setParam("/mmWave_Manager/startFreq", std::stof(token));
                  } else if (i == 3) {
                    n.setParam("/mmWave_Manager/idleTime", std::stof(token));
                  } else if (i == 5) {
                    n.setParam("/mmWave_Manager/rampEndTime", std::stof(token));
                  } else if (i == 8) {
                    n.setParam("/mmWave_Manager/freqSlopeConst",
                               std::stof(token));
                  } else if (i == 10) {
                    n.setParam("/mmWave_Manager/numAdcSamples",
                               std::stoi(token));
                  } else if (i == 11) {
                    n.setParam("/mmWave_Manager/digOutSampleRate",
                               std::stof(token));
                  }
                } else if (!cmd.compare("compRangeBiasAndRxChanPhase")) {
                  if (i == 1) {
                    n.setParam("/mmWave_Manager/rangeBias", std::stof(token));
                  }
                } else if (!cmd.compare("channelCfg")) {
                  if (i == 1) {
                    int binary = std::stoi(token);
                    int value = ((binary << 0) & 1) + ((binary >> 1) & 1);
                    n.setParam("/mmWave_Manager/numTxAzimAnt", value);
                  } else if (i == 2) {
                    int binary = std::stoi(token);
                    int value = ((binary << 0) & 1) + ((binary >> 1) & 1) +
                                ((binary >> 2) & 1) + ((binary >> 3) & 1);
                    n.setParam("/mmWave_Manager/numRxAnt", value);
                  }
                }
                srv.request.comm.erase(0, pos + 1);
                i++;
              }
              break;
            } else if (numTries == 0) {
              ROS_INFO("mmWaveQuickConfig: Command failed (mmWave sensor did "
                       "not respond with 'Done')");
              ROS_INFO("mmWaveQuickConfig: Response: '%s'",
                       srv.response.resp.c_str());
            } else {
              ROS_ERROR("mmWaveQuickConfig: Command failed (mmWave sensor did "
                        "not respond with 'Done')");
              ROS_ERROR("mmWaveQuickConfig: Response: '%s'",
                        srv.response.resp.c_str());
              return 1;
            }
          } else {
            ROS_ERROR("mmWaveQuickConfig: Failed to call service mmWaveCLI");
            ROS_ERROR("%s", srv.request.comm.c_str());
            return 1;
          }
          numTries++;
        }
      }
    }

    n.setParam("/mmWave_Manager/numTxAnt", txAntennas);
    myParams.close();
  } else {
    ROS_ERROR("mmWaveQuickConfig: Failed to open File %s", argv[1]);
    return 1;
  }

  ROS_INFO("mmWaveQuickConfig: mmWaveQuickConfig will now terminate. Done "
           "configuring mmWave device using config file: %s",
           argv[1]);
  return 0;
}
