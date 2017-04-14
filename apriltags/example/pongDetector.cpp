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

class cup {
public:
  bool hit;
  bool new_hit;
  int id;

  cup(int id_in) : id(id_in), hit(0), new_hit(0) {}
};

bool compare_id(const cup& l, const cup& r) //(2)
  {
      return l.id < r.id;
  }

class cupDetector {

  AprilTags::TagDetector* m_tagDetector;
  AprilTags::TagCodes m_tagCodes;

  bool m_verbose; // output data on all detected
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

  vector<AprilTags::TagDetection> detections;

  vector<cup> game_cups;
  int cups_in_play;
  bool recent_miss;


public:

  // Default constructor
  cupDetector() :
    // default settings, most can be modified through command line options (see below)
    m_tagDetector(NULL),
    m_tagCodes(AprilTags::tagCodes36h11),
    game_cups(11, -1),
    cups_in_play(10),
    recent_miss(false),


    m_draw(true),
    m_verbose(false),
    m_usb(false),
    m_timing(false),

    m_width(640),
    m_height(480),
    m_tagSize(0.0448), // TODO
    m_fx(600),
    m_fy(600),
    m_px(m_width/2),
    m_py(m_height/2)
  {}

  // Init detection variables and settings
  void setup() {
    m_tagDetector = new AprilTags::TagDetector(m_tagCodes);

    for (int i = 1; i < 11; ++i) {
      game_cups[i] = cup(i);
    }

    // prepare window for drawing the camera images
    if (m_draw) {
      namedWindow(windowName, WINDOW_AUTOSIZE);
    }

    // optional: prepare serial port for communication with Arduino
    if (m_usb) {
      m_serial.open("/dev/ttyACM0");
    }
  }

  // Open video feed
  void setupVideo() {

    if(!m_cap.open(0)) {
        cout << "Error opening video" << endl;
        exit(1);
    }
    //sleep(0.5);
  }

  // Terminal output for 
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

  // Detect tags
  void TagExtraction(Mat& frame, Mat& frame_gray) {

    detections = m_tagDetector->extractTags(frame_gray);

    if (m_verbose) {
      // print out each detection
      cout << detections.size() << " tags detected:" << endl;
      for (int i=0; i<detections.size(); i++) {
        print_detection(detections[i]);
      }
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

  // Redeclare cups used in detection
  void resetCups() {

    Mat frame;
    Mat frame_gray;
    while (1) {

      cups_in_play = 0;


      if (!m_cap.grab()) {
        cout << "Error with grabbing video\n";
      }

      m_cap >> frame;
      imshow(windowName, frame); 

      //convert to grayscale
      cvtColor(frame, frame_gray, CV_RGB2GRAY);

      //Tag Extraction
      detections = m_tagDetector->extractTags(frame_gray);

      //recreate game_cups 
      cleanCups();

      //add valid detected tags to possible_cups list
      for (int i=0; i<detections.size(); i++) {
        game_cups[detections[i].id].id = detections[i].id;
      }

      cout << "Using " << detections.size() << " cups with ids: \n";
      for (int i = 0; i < game_cups.size(); ++i) {
        if (game_cups[i].id != -1) {
          cout << game_cups[i].id << " ";
          ++cups_in_play;
        }
      }
      cout << "\nPress spacebar to complete cup reset.\n";

      // Press spacebar to end program, have 1 seconds before next frame capture
      if (waitKey(100) == 32) {
        if (cups_in_play != 0) {
          cout << "Selected to play with " << cups_in_play << " cups.\n";
          return;
        }
      }

      //wait x seconds between captures
      //sleep(1);
    }
  }

  // Recreate game_cups with 10 cups
  void cleanCups() {
    game_cups.clear();
    for (int i = 0; i < 11; ++i) {
      game_cups.push_back(cup(-1));
    }
  }

  // Game complete == 1, game in progress == 0
  bool gameStatus() {

    int cups_left = 0;
    for (int i = 0; i < game_cups.size(); ++i) {
      if (game_cups[i].hit) {
        ++cups_left;
      }
    }
    return cups_in_play == cups_left;
  }
 
  // Determine which cups have been hit from known cups
  void hitDetection() {

    // mark all detected cups
    vector<bool> detected_cups(game_cups.size(), 0);
    vector<int> new_hit_cups;

    // check which cups are detected
    for (int i = 0; i < detections.size(); ++i) {
      if (game_cups[detections[i].id].hit == 0) {
        detected_cups[detections[i].id] = 1;
      }
    }

    // check for new missing cups, mark as hits
    for (int i = 0; i < detected_cups.size(); ++i) {
      if (detected_cups[i] == 0 && game_cups[i].id != -1) {
        if (!game_cups[i].hit) {
          // if cup is not detected, and hasn't been hit
          game_cups[i].hit = 1;
          game_cups[i].new_hit = 1;
          new_hit_cups.push_back(i);
        }
        else if (game_cups[i].new_hit) {
          //cup has already been marked as newly hit
          game_cups[i].new_hit = 0;
        }
      }
    }

    // inform user of the their new hits
    if (new_hit_cups.size() == 1) {
      cout << "Wow! You've just hit cup " << new_hit_cups[0] << ".\n";
      cout << "Keep it going!\n";
      //recent_miss = false;
    }
    else if (new_hit_cups.size() > 1) {
      cout << "Multi-hit! Nice job, you got these cups: ";
      for (int i = 0; i < new_hit_cups.size(); ++i) {
        cout << new_hit_cups[i] << " ";
      }
      cout << "\n";
      cout << "You're on fire!\n";
      //recent_miss = false;
    }
    else if (!recent_miss) {
      // cout << "Oh too bad! Turns out your beer pong skills translates to this robot!\n";
      // recent_miss = true;
      // cout << "Try another shot, but aim for the cups!\n";
    }
    //sleep(1);
  }

  // The processing loop where images are retrieved, tags detected,
  // and information about detections generated
  void loop() {

    Mat frame;
    Mat frame_gray;

    //int frame = 0;
    //double last_t = tic();

    resetCups();

    for (;;) {

      if (!m_cap.grab()) {
        cout << "Error with grabbing video\n";
      }
      // capture frame
      m_cap >> frame;

      //convert to grayscale
      cvtColor(frame, frame_gray, CV_RGB2GRAY);

      //Tag Extraction
      TagExtraction(frame, frame_gray);

      //Check & update hits
      hitDetection();

      //Empty detections
      detections.clear();
     
      // End of video feed
      if (frame.empty()) break;

      if (waitKey(10) == 32) resetCups();

      // exit if esc key is pressed
      if (waitKey(10) == 27) break;

      if(gameStatus()) {
        cout << "Great job! You got all the cups!\n";
        cout << "Game will restart in 5 seconds\n";
        sleep(5);
        resetCups();
      }

      // wait x seconds before next frame capture
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