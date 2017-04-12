#include "opencv2/opencv.hpp"
#include "AprilTags/TagDetector.h"
#include "AprilTags/Tag36h11.h"
#include <stdlib.h>     //for using the function sleep
#include <iostream>
#include <cstring>
#include <vector>

// For Arduino: locally defined serial port access class
#include "Serial.h"

using namespace cv;
using namespace std;

#include <cmath>

#ifndef PI
const double PI = 3.14159265358979323846;
#endif
const double TWOPI = 2.0*PI;

/**
 * Normalize angle to be within the interval [-pi,pi].
 */
inline double standardRad(double t) {
  if (t >= 0.) {
    t = fmod(t+PI, TWOPI) - PI;
  } else {
    t = fmod(t-PI, -TWOPI) + PI;
  }
  return t;
}

/**
 * Convert rotation matrix to Euler angles
 */
void wRo_to_euler(const Eigen::Matrix3d& wRo, double& yaw, double& pitch, double& roll) {
    yaw = standardRad(atan2(wRo(1,0), wRo(0,0)));
    double c = cos(yaw);
    double s = sin(yaw);
    pitch = standardRad(atan2(-wRo(2,0), wRo(0,0)*c + wRo(1,0)*s));
    roll  = standardRad(atan2(wRo(0,2)*s - wRo(1,2)*c, -wRo(0,1)*s + wRo(1,1)*c));
}


//g++ `pkg-config --cflags opencv` pongDetector.cpp `pkg-config --libs opencv` -o detect; ./detect 

const char* windowName = "Cup Detector";

class cupDetector {

  AprilTags::TagDetector* m_tagDetector;
  AprilTags::TagCodes m_tagCodes;

  bool m_draw; // draw image and April tag detections?
  bool m_usb; // send tag detections to serial port?
  bool m_timing; // print timing information for each tag extraction call

  int m_width; // image size in pixels
  int m_height;
  double m_tagSize; // April tag side length in meters of square black frame
  double m_fx; // camera focal length in pixels
  double m_fy;
  double m_px; // camera principal point
  double m_py;

  VideoCapture m_cap;

  Serial m_serial;


public:

  // default constructor
  cupDetector() :
    // default settings, most can be modified through command line options (see below)
    m_tagDetector(NULL),
    m_tagCodes(AprilTags::tagCodes36h11),

    m_draw(true),
    m_usb(true),
    m_timing(false),

    m_width(640),
    m_height(480),
    m_tagSize(0.0448), // TODO
    m_fx(600),
    m_fy(600),
    m_px(m_width/2),
    m_py(m_height/2)
  {}

  //init detection variables and settings
  void setup() {
    m_tagDetector = new AprilTags::TagDetector(m_tagCodes);

    // prepare window for drawing the camera images
    if (m_draw) {
      namedWindow(windowName, WINDOW_AUTOSIZE);
    }

    // optional: prepare serial port for communication with Arduino
    if (m_usb) {
      m_serial.open("/dev/ttyACM0");
    }
  }

  //open video feed
  void setupVideo() {

    if(!m_cap.open(0)) {
        cout << "Error opening video" << endl;
        exit(1);
    }
  }

  void print_detection(AprilTags::TagDetection& detection) const {
    cout << "  Id: " << detection.id
         << " (Hamming: " << detection.hammingDistance << ")";

    // recovering the relative pose of a tag:

    // NOTE: for this to be accurate, it is necessary to use the
    // actual camera parameters here as well as the actual tag size
    // (m_fx, m_fy, m_px, m_py, m_tagSize)

    Eigen::Vector3d translation;
    Eigen::Matrix3d rotation;
    detection.getRelativeTranslationRotation(m_tagSize, m_fx, m_fy, m_px, m_py,
                                             translation, rotation);

    Eigen::Matrix3d F;
    F <<
      1, 0,  0,
      0,  -1,  0,
      0,  0,  1;
    Eigen::Matrix3d fixed_rot = F*rotation;
    double yaw, pitch, roll;
    wRo_to_euler(fixed_rot, yaw, pitch, roll);

    cout << "  distance=" << translation.norm()
         << "m, x=" << translation(0)
         << ", y=" << translation(1)
         << ", z=" << translation(2)
         << ", yaw=" << yaw
         << ", pitch=" << pitch
         << ", roll=" << roll
         << endl;

    // Also note that for SLAM/multi-view application it is better to
    // use reprojection error of corner points, because the noise in
    // this relative pose is very non-Gaussian; see iSAM source code
    // for suitable factors.
  }

  void TagExtraction(Mat& frame, Mat& frame_gray) {

    vector<AprilTags::TagDetection> detections = m_tagDetector->extractTags(frame_gray);

     // print out each detection
    cout << detections.size() << " tags detected:" << endl;
    for (int i=0; i<detections.size(); i++) {
      print_detection(detections[i]);
    }


    // show the current frame including any detections
    if (m_draw) {
        for (int i=0; i<detections.size(); i++) {
          // also highlight in the frame
          detections[i].draw(frame);
        }
        imshow(windowName, frame); // OpenCV call
      }

    if (m_usb) {
      for (int i = 0; i < detections.size(); ++i) {
        m_serial.print(detections[i].id);
        m_serial.print(",");
        m_serial.print("\n");
      } 
    }
}

  // The processing loop where images are retrieved, tags detected,
  // and information about detections generated
  void loop() {

    Mat frame;
    Mat frame_gray;

    //int frame = 0;
    //double last_t = tic();
    for (;;) {

      // capture frame
      m_cap >> frame;

      //convert to grayscale
      cvtColor(frame, frame_gray, CV_RGB2GRAY);

      //Tag Extraction
      TagExtraction(frame, frame_gray);
     
      // End of video feed
      if (frame.empty()) break;

      // exit if any key is pressed
      if (waitKey(10) == 27) break;

      // wait x seconds before next frame capture
      sleep(30); 
    }
  }
};


int main()
{
    cupDetector bpchamps;
    bpchamps.setup();
    bpchamps.setupVideo();
    bpchamps.loop();

    return 0;
}