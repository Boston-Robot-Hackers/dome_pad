#pragma once
// ROS tab: a topic monitor fed by rosbridge on the robot's Pi, over Wi-Fi.
// Wi-Fi is on only while the tab is open; the Drive tab never shares the radio with it.
// Wi-Fi credentials and the rosbridge address are in secrets.h (see secrets.h.sample).

void rosTabStart();               // the tab was opened: turn Wi-Fi on and start connecting
void rosTabStop();                // the tab was closed: drop rosbridge and turn Wi-Fi off
void rosTabLoop();                // call every loop while the tab is open
void rosTabDraw();                // draw the whole page
void rosTabTouch(int x, int y);   // a new press on the page: shows the next page of topics
