#include "MvCameraControl.h"
#include <cv_bridge/cv_bridge.h>
#include <image_transport/image_transport.h>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <pthread.h>
#include <chrono>
#include <ros/ros.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/mman.h>
struct time_stamp {
  int64_t high;
  int64_t low;
};

time_stamp *pointt;

using namespace std;
// 用的是这份代码
unsigned int g_nPayloadSize = 0;
bool is_undistorted = true;
bool exit_flag = false;
image_transport::Publisher pub;
std::string ExposureAutoStr[3] = {"Off", "Once", "Continues"};
std::string GammaSlectorStr[3] = {"User", "sRGB", "Off"};

std::string GainAutoStr[3] = {"Off", "Once", "Continues"};
std::string CameraName;
float resize_divider;
void setParams(void *handle, const std::string &params_file) {
  cv::FileStorage Params(params_file, cv::FileStorage::READ);
  if (!Params.isOpened()) {
    string msg = "Failed to open settings file at:" + params_file;
    ROS_ERROR_STREAM(msg.c_str());
    exit(-1);
  }
  resize_divider = Params["resize_divide"];
  if (resize_divider < 0.1) {
    resize_divider = 1;
  }
  int ExposureAuto = Params["ExposureAuto"];
  int ExposureTimeLower = Params["AutoExposureTimeLower"];
  int ExposureTimeUpper = Params["AutoExposureTimeUpper"];
  int ExposureTime = Params["ExposureTime"];
  int ExposureAutoMode = Params["ExposureAutoMode"];
  int GainAuto = Params["GainAuto"];
  float Gain = Params["Gain"];
  float Gamma = Params["Gamma"];
  int GammaSlector = Params["GammaSelector"];

  int CenterAlign = Params["CenterAlign"];


        int Binning = Params["Binning"];
        printf("Binning : %d .\n", Binning);

  int nRet;

        nRet = MV_CC_SetEnumValue(handle, "BinningHorizontal", Binning);
        if (MV_OK != nRet)
            ROS_WARN("set BinningHorizontal fail! nRet [%x]\n", nRet);
        else
            ROS_WARN("set BinningHorizontal ok! nRet [%x]\n", nRet);

        nRet = MV_CC_SetEnumValue(handle, "BinningVertical", Binning);
        if (MV_OK != nRet)
            ROS_WARN("set BinningVertical fail! nRet [%x]\n", nRet);
        else
            ROS_WARN("set BinningVertical ok! nRet [%x]\n", nRet);


  if (CenterAlign) { //这里开始 设置中心对齐
    // Strobe输出
    nRet = MV_CC_SetEnumValue(handle, "LineSelector", 2);
    if (MV_OK != nRet) {
      printf("LineSelector fail.\n");
    }

    // 0:Line0 1:Line1 2:Line2

    nRet = MV_CC_SetEnumValue(
        handle, "LineMode",
        8); //仅LineSelector为line2时需要特意设置，其他输出不需要
    if (MV_OK != nRet) {
      printf("LineMode fail.\n");
    }
    // 0:Input 1:Output 8:Strobe
    int DurationValue = 0, DelayValue = 0, PreDelayValue = 0; // us
    // nRet = MV_CC_SetIntValue(handle, "StrobeLineDuration", DurationValue);
    // if (MV_OK != nRet) {
    //   printf("StrobeLineDuration fail.\n");
    // }
    nRet = MV_CC_SetIntValue(handle, "LineSource", 0);
    if (MV_OK != nRet) {
      printf("Line source:exposure start activate fail.\n");
    }

    // strobe持续时间，设置为0，持续时间就是曝光时间，设置其他值，就是其他值时间
    nRet =
        MV_CC_SetIntValue(handle, "StrobeLineDelay",
                          DelayValue); // strobe延时，从曝光开始，延时多久输出
    if (MV_OK != nRet) {
      printf("StrobeLineDelay fail.\n");
    }

    nRet = MV_CC_SetIntValue(handle, "StrobeLinePreDelay",
                             PreDelayValue); // strobe提前输出，曝光延后开始
    if (MV_OK != nRet) {
      printf("StrobeLinePreDelay fail.\n");
    }

    nRet = MV_CC_SetBoolValue(handle, "StrobeEnable", 1);
    if (MV_OK != nRet) {
      printf("StrobeEnable fail.\n");
    }
    // Strobe输出使能，使能之后，上面配置参数生效，IO输出与曝光同步
    //这里结束
  }
  // 设置曝光模式
  nRet = MV_CC_SetExposureAutoMode(handle, ExposureAutoMode);
  if (MV_OK == nRet) {
    std::string msg =
        "Set ExposureAutoMode: " + std::to_string(ExposureAutoMode);
    ROS_INFO_STREAM(msg.c_str());
  } else {
    ROS_ERROR_STREAM("Fail to set ExposureAutoMode");
  }
  // 如果是自动曝光
  if (ExposureAutoMode == 2) {
    // nRet = MV_CC_SetFloatValue(handle, "ExposureAuto", ExposureAuto);
    nRet = MV_CC_SetEnumValue(handle, "ExposureAuto",
                              MV_EXPOSURE_AUTO_MODE_CONTINUOUS);
    if (MV_OK == nRet) {
      std::string msg = "Set Exposure Auto: " + ExposureAutoStr[ExposureAuto];
      ROS_INFO_STREAM(msg.c_str());
    } else {
      ROS_ERROR_STREAM("Fail to set Exposure auto mode");
    }
    nRet = MV_CC_SetAutoExposureTimeLower(handle, ExposureTimeLower);
    if (MV_OK == nRet) {
      std::string msg =
          "Set Exposure Time Lower: " + std::to_string(ExposureTimeLower) +
          "ms";
      ROS_INFO_STREAM(msg.c_str());
    } else {
      ROS_ERROR_STREAM("Fail to set Exposure Time Lower");
    }
    nRet = MV_CC_SetAutoExposureTimeUpper(handle, ExposureTimeUpper);
    if (MV_OK == nRet) {
      std::string msg =
          "Set Exposure Time Upper: " + std::to_string(ExposureTimeUpper) +
          "ms";
      ROS_INFO_STREAM(msg.c_str());
    } else {
      ROS_ERROR_STREAM("Fail to set Exposure Time Upper");
    }
  }
  // 如果是固定曝光
  if (ExposureAutoMode == 0) {
    nRet = MV_CC_SetExposureTime(handle, ExposureTime);
    if (MV_OK == nRet) {
      std::string msg =
          "Set Exposure Time: " + std::to_string(ExposureTime) + "ms";
      ROS_INFO_STREAM(msg.c_str());
    } else {
      ROS_ERROR_STREAM("Fail to set Exposure Time");
    }
  }

  nRet = MV_CC_SetEnumValue(handle, "GainAuto", GainAuto);

  if (MV_OK == nRet) {
    std::string msg = "Set Gain Auto: " + GainAutoStr[GainAuto];
    ROS_INFO_STREAM(msg.c_str());
  } else {
    ROS_ERROR_STREAM("Fail to set Gain auto mode");
  }

  if (GainAuto == 0) {
    nRet = MV_CC_SetGain(handle, Gain);
    if (MV_OK == nRet) {
      std::string msg = "Set Gain: " + std::to_string(Gain);
      ROS_INFO_STREAM(msg.c_str());
    } else {
      ROS_ERROR_STREAM("Fail to set Gain");
    }
  }

  nRet = MV_CC_SetGammaSelector(handle, GammaSlector);
  if (MV_OK == nRet) {
    std::string msg = "Set GammaSlector: " + GammaSlectorStr[GammaSlector];
    ROS_INFO_STREAM(msg.c_str());
  } else {
    ROS_ERROR_STREAM("Fail to set GammaSlector");
  }

  nRet = MV_CC_SetGamma(handle, Gamma);
  if (MV_OK == nRet) {
    std::string msg = "Set Gamma: " + std::to_string(Gamma);
    ROS_INFO_STREAM(msg.c_str());
  } else {
    ROS_ERROR_STREAM("Fail to set Gamma");
  }
}

void PressEnterToExit(void) {
  int c;
  while (ros::ok())
    ;
  exit_flag = true;
  sleep(1);
}

// 线程 获取

static void *WorkThread(void *pUser) {
  int nRet = MV_OK;

  // ch:获取数据包大小 | en:Get payload size
  MVCC_INTVALUE stParam;
  memset(&stParam, 0, sizeof(MVCC_INTVALUE));
  nRet = MV_CC_GetIntValue(pUser, "PayloadSize", &stParam);
  if (MV_OK != nRet) {
    printf("Get PayloadSize fail! nRet [0x%x]\n", nRet);
    return NULL;
  }

  MV_FRAME_OUT_INFO_EX stImageInfo = {0};
  memset(&stImageInfo, 0, sizeof(MV_FRAME_OUT_INFO_EX));
  unsigned char *pData =
      (unsigned char *)malloc(sizeof(unsigned char) * stParam.nCurValue);
  if (NULL == pData)
    return NULL;

  unsigned int nDataSize = stParam.nCurValue;

  while (ros::ok()) {
    auto st = ros::Time::now();
    nRet =
        MV_CC_GetOneFrameTimeout(pUser, pData, nDataSize, &stImageInfo, 100);
auto et = ros::Time::now();
ROS_WARN_ONCE("use time : %lf", (et - st).toSec() );

    if (nRet == MV_OK) {
      auto time_pc_clk = std::chrono::high_resolution_clock::now();
      // double time_pc =
      // uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(time_pc_clk.time_since_epoch()).count())
      // / 1000000000.0;

      // 时间戳 在这里读取
      // int64_t a = pointt->high;
      // int64_t b = pointt->low + 1e8;
      // bias 干掉之后不需要加0.1 s了

      int64_t b = pointt->low;
      // printf("b:%d\n");
      double time_pc = b / 1000000000.0;
      ros::Time rcv_time = ros::Time(time_pc);
      std::string debug_msg;
      debug_msg = CameraName + " GetOneFrame,nFrameNum[" +
                  std::to_string(stImageInfo.nFrameNum) + "], FrameTime:" +
                  std::to_string(rcv_time.toSec());
      ROS_WARN_ONCE(debug_msg.c_str());
      cv::Mat srcImage;
      srcImage =
          cv::Mat(stImageInfo.nHeight, stImageInfo.nWidth, CV_8UC3, pData);
      cv::resize(srcImage, srcImage,
                 cv::Size(resize_divider * srcImage.cols,
                          resize_divider * srcImage.rows),
                 CV_INTER_LINEAR);

      // printf("a = %ld b = %ld\n", , pointt->low);
      sensor_msgs::ImagePtr msg =
          cv_bridge::CvImage(std_msgs::Header(), "rgb8", srcImage).toImageMsg();
      msg->header.stamp = rcv_time;
      msg->header.frame_id = "camera";
      pub.publish(msg);
      // ROS_WARN("camera use time : %lf", msg->header.stamp.toSec());
    }

    if (exit_flag)
      break;
  }

  if (pData) {
    free(pData);
    pData = NULL;
  }

  return 0;
}

bool PrintDeviceInfo(MV_CC_DEVICE_INFO* pstMVDevInfo)
{
    if (NULL == pstMVDevInfo)
    {
        printf("The Pointer of pstMVDevInfo is NULL!\n");
        return false;
    }
    if (pstMVDevInfo->nTLayerType == MV_GIGE_DEVICE)
    {
        int nIp1 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0xff000000) >> 24);
        int nIp2 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x00ff0000) >> 16);
        int nIp3 = ((pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x0000ff00) >> 8);
        int nIp4 = (pstMVDevInfo->SpecialInfo.stGigEInfo.nCurrentIp & 0x000000ff);

        // ch:打印当前相机ip和用户自定义名字 | en:print current ip and user defined name
        printf("Device Model Name: %s\n", pstMVDevInfo->SpecialInfo.stGigEInfo.chModelName);
        printf("CurrentIp: %d.%d.%d.%d\n" , nIp1, nIp2, nIp3, nIp4);
        printf("UserDefinedName: %s\n\n" , pstMVDevInfo->SpecialInfo.stGigEInfo.chUserDefinedName);
    }
    else if (pstMVDevInfo->nTLayerType == MV_USB_DEVICE)
    {
        printf("Device Model Name: %s\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chModelName);
        printf("UserDefinedName: %s\n\n", pstMVDevInfo->SpecialInfo.stUsb3VInfo.chUserDefinedName);
    }
    else
    {
        printf("Not support.\n");
    }

    return true;
}

void signal_callback_handler(int signum)
{
  cout << "Caught signal, EXIT mvs_trigger . " << signum << endl;
  // Terminate program
  exit_flag = true;
  exit(signum);

}

int main(int argc, char **argv)
{
  signal(SIGINT, signal_callback_handler);

  ros::init(argc, argv, "mvs_trigger");
  std::string params_file = std::string(argv[1]);
  ros::NodeHandle nh;
  image_transport::ImageTransport it(nh);
  int nRet = MV_OK;
  void *handle = NULL;
  ros::Rate loop_rate(10);
  cv::FileStorage Params(params_file, cv::FileStorage::READ);
  if (!Params.isOpened()) {
    string msg = "Failed to open settings file at:" + params_file;
    ROS_ERROR_STREAM(msg.c_str());
    exit(-1);
  }
  std::string expect_serial_number = Params["SerialNumber"];
  std::string pub_topic = Params["TopicName"];
  std::string camera_name = Params["CameraName"];
  // std::string path_for_time_stamp = "/home/aa/test4";
  // nh.getParam("path_for_time_stamp", path_for_time_stamp);
  std::string path_for_time_stamp = Params["path_for_time_stamp"];
  // path_for_time_stamp = Params["path_for_time_stamp"];
  CameraName = camera_name;
  pub = it.advertise(pub_topic, 1);
  const char *shared_file_name = path_for_time_stamp.c_str();

  int fd = open(shared_file_name, O_RDWR);

  pointt = (time_stamp *)mmap(NULL, sizeof(time_stamp), PROT_READ | PROT_WRITE,
                              MAP_SHARED, fd, 0);
  while (ros::ok()) {
    MV_CC_DEVICE_INFO_LIST stDeviceList;
    memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    // 枚举检测到的相机数量
    nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_USB_DEVICE, &stDeviceList);
    if (MV_OK != nRet) {
      printf("Enum Devices fail!");
      break;
    }

    bool find_expect_camera = false;
    int expect_camera_index = 0;
    if (stDeviceList.nDeviceNum == 0) {
      ROS_ERROR_STREAM("No Camera.\n");
      break;
    } else {
      // 根据serial number启动指定相机
      for (int i = 0; i < stDeviceList.nDeviceNum; i++) {
        PrintDeviceInfo(stDeviceList.pDeviceInfo[i]);

        std::string serial_number =
            std::string((char *)stDeviceList.pDeviceInfo[i]
                            ->SpecialInfo.stUsb3VInfo.chSerialNumber);
ROS_WARN("serial_number: %s.\n", serial_number.c_str());

       // if (expect_serial_number == serial_number) {
          find_expect_camera = true;
          expect_camera_index = i;
         // break;
        //}
      }
    }
    if (!find_expect_camera) {
      std::string msg =
          "Can not find the camera with serial number " + expect_serial_number;
      ROS_ERROR_STREAM(msg.c_str());
      break;
    }

    nRet = MV_CC_CreateHandle(&handle,
                              stDeviceList.pDeviceInfo[expect_camera_index]);
    if (MV_OK != nRet) {
      ROS_ERROR_STREAM("Create Handle fail");
      break;
    }

    nRet = MV_CC_OpenDevice(handle);
    if (MV_OK != nRet) {
      printf("Open Device fail\n");
      break;
    }

    nRet = MV_CC_SetBoolValue(handle, "AcquisitionFrameRateEnable", false);
    if (MV_OK != nRet) {
      printf("set AcquisitionFrameRateEnable fail! nRet [%x]\n", nRet);
      break;
    }

    MVCC_INTVALUE stParam;
    memset(&stParam, 0, sizeof(MVCC_INTVALUE));
    nRet = MV_CC_GetIntValue(handle, "PayloadSize", &stParam);
    if (MV_OK != nRet) {
      printf("Get PayloadSize fail\n");
      break;
    }
    g_nPayloadSize = stParam.nCurValue;

    nRet = MV_CC_SetEnumValue(handle, "PixelFormat", 0x02180014);
    if (nRet != MV_OK) {
      printf("Pixel setting can't work.");
      break;
    }

    setParams(handle, params_file);

    // 设置触发模式为on
    // set trigger mode as on
    nRet = MV_CC_SetEnumValue(handle, "TriggerMode", 1);
    if (MV_OK != nRet) {
      printf("MV_CC_SetTriggerMode fail! nRet [%x]\n", nRet);
      break;
    }

    // 设置触发源
    // set trigger source
    nRet = MV_CC_SetEnumValue(handle, "TriggerSource", MV_TRIGGER_SOURCE_LINE0);
    if (MV_OK != nRet) {
      printf("MV_CC_SetTriggerSource fail! nRet [%x]\n", nRet);
      break;
    }

      ROS_INFO("Finish all params set! Start grabbing...");
      nRet = MV_CC_StartGrabbing(handle);
      if (MV_OK != nRet) {
        printf("Start Grabbing fail.\n");
        break;
      }

      pthread_t nThreadID;
      nRet = pthread_create(&nThreadID, NULL, WorkThread, handle);
      if (nRet != 0) {
        printf("thread create failed.ret = %d\n", nRet);
        break;
      }

      PressEnterToExit();

      nRet = MV_CC_StopGrabbing(handle);
      if (MV_OK != nRet) {
        printf("MV_CC_StopGrabbing fail! nRet [%x]\n", nRet);
        break;
      }

      nRet = MV_CC_CloseDevice(handle);
      if (MV_OK != nRet) {
        printf("MV_CC_CloseDevice fail! nRet [%x]\n", nRet);
        break;
      }

      nRet = MV_CC_DestroyHandle(handle);
      if (MV_OK != nRet) {
        printf("MV_CC_DestroyHandle fail! nRet [%x]\n", nRet);
        break;
      }

      break;
    }
    munmap(pointt, sizeof(time_stamp) * 5);
    return 0;
}
