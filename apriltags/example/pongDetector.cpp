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


//g++ `pkg-config --cflags opencv` pongDetector.cpp `pkg-config --libs opencv` -o detect; ./detect 

const char* windowName = "Cup Detector";

class cupDetector {

  AprilTags::TagDetector* m_tagDetector;
  AprilTags::TagCodes m_tagCodes;

  bool m_draw; // draw image and April tag detections?
  bool m_arduino; // send tag detections to serial port?
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
    m_arduino(false),
    m_timing(false),

    m_width(640),
    m_height(480),
    m_tagSize(0.166), // TODO
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
      namedWindow(windowName, WINDOW_NORMAL);
    }

    // optional: prepare serial port for communication with Arduino
    if (m_arduino) {
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

  void TagExtraction(Mat& frame, Mat& frame_gray) {

    vector<AprilTags::TagDetection> detections = m_tagDetector->extractTags(frame_gray);
    cout << detections.size() << " tags detected:" << endl;

    // show the current frame including any detections
    if (m_draw) {
        for (int i=0; i<detections.size(); i++) {
          // also highlight in the frame
          detections[i].draw(frame);
        }
        imshow(windowName, frame); // OpenCV call
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



      imshow(windowName, frame);


     


      // End of video feed
      if (frame.empty()) break;

      // exit if any key is pressed
      if (waitKey(10) == 27) break;

      // wait 1 second before next frame capture
      //sleep(1); 
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