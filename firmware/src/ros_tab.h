#pragma once
// ROS tab: publish rates of a few topics (now /odom and /scan), from rosbridge on the remote
// computer (the VM), over Wi-Fi.
// Wi-Fi is on only while the tab is open; the Drive tab never shares the radio with it.
// Wi-Fi credentials and the rosbridge address are in secrets.h (see secrets.h.sample).

// Where the tab's connection stands, for the status bar's Wi-Fi indicator.
enum class RosLink {
  OFF,           // tab closed: Wi-Fi off
  JOINING,       // joining Wi-Fi
  CONNECTING,    // on Wi-Fi, connecting to rosbridge
  ONLINE,        // connected to rosbridge
  UNCONFIGURED,  // WIFI_SSID or ROSBRIDGE_HOST missing from secrets.h
};
RosLink rosTabLink();

void rosTabStart();               // the tab was opened: turn Wi-Fi on and start connecting
void rosTabStop();                // the tab was closed: drop rosbridge and turn Wi-Fi off
void rosTabLoop();                // call every loop while the tab is open
void rosTabDraw();                // draw the whole page
