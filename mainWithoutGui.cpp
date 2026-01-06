#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <chrono>

using namespace cv;
using namespace std;
using namespace std::chrono;

// قاموس مورس
map<string, char> morseMap = {
    {".-", 'A'}, {"-...", 'B'}, {"-.-.", 'C'}, {"-..", 'D'}, {".", 'E'},
    {"..-.", 'F'}, {"--.", 'G'}, {"....", 'H'}, {"..", 'I'}, {".---", 'J'},
    {"-.-", 'K'}, {".-..", 'L'}, {"--", 'M'}, {"-.", 'N'}, {"---", 'O'},
    {".--.", 'P'}, {"--.-", 'Q'}, {".-.", 'R'}, {"...", 'S'}, {"-", 'T'},
    {"..-", 'U'}, {"...-", 'V'}, {".--", 'W'}, {"-..-", 'X'}, {"-.--", 'Y'},
    {"--..", 'Z'},
    {".----", '1'}, {"..---", '2'}, {"...--", '3'}, {"....-", '4'}, {".....", '5'},
    {"-....", '6'}, {"--...", '7'}, {"---..", '8'}, {"----.", '9'}, {"-----", '0'}
};

int main() {
    VideoCapture cap(1);
    if (!cap.isOpened()) return -1;

    // حاول تقلل الإضاءة قدر الإمكان (لو مش شغال، استخدم السلايدر تحت)
    cap.set(CAP_PROP_EXPOSURE, -6);

    Mat frame, gray, thresh;

    // متغير للتحكم في الحساسية (Threshold)
    int thresholdVal = 230; // قللناه شوية كبداية
namedWindow("Control Panel", WINDOW_AUTOSIZE);
createTrackbar("Sensitivity", "Control Panel", &thresholdVal, 255);
auto lightStart = high_resolution_clock::now();
bool isLightOn = false;
string currentSymbol = "";
string decodedMessage = "";
auto lastSymbolTime = high_resolution_clock::now();

    while (true) {
        cap >> frame;
        if (frame.empty()) break;
        flip(frame, frame, 1);

        cvtColor(frame, gray, COLOR_BGR2GRAY);

        // استخدام القيمة من السلايدر
        threshold(gray, thresh, thresholdVal, 255, THRESH_BINARY);

        erode(thresh, thresh, Mat(), Point(-1, -1), 2);
        dilate(thresh, thresh, Mat(), Point(-1, -1), 2);

        vector<vector<Point>> contours;
        findContours(thresh, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);

        bool lightDetectedNow = false;
        Point lightPos;

        // البحث عن أكبر مصدر ضوء (عشان نتجاهل الانعكاسات الصغيرة)
        double maxArea = 0;
        int maxIdx = -1;
        for (size_t i = 0; i < contours.size(); i++) {
            double area = contourArea(contours[i]);
            if (area > maxArea) {
                maxArea = area;
                maxIdx = i;
            }
        }

        // لو لقينا ضوء مساحته معقولة (أكبر من 20 بيكسل)
        if (maxIdx != -1 && maxArea > 20) {
            lightDetectedNow = true;
            Moments m = moments(contours[maxIdx]);
            if (m.m00 != 0) {
                lightPos = Point(m.m10 / m.m00, m.m01 / m.m00);
                circle(frame, lightPos, 20, Scalar(0, 255, 0), 2);
                putText(frame, "ON", Point(lightPos.x, lightPos.y - 25), FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 255, 0), 2);
            }
        }

        // --- نفس المنطق الزمني (Logic) ---
        auto now = high_resolution_clock::now();

        if (lightDetectedNow) {
            if (!isLightOn) {
                lightStart = now;
                isLightOn = true;
            }
        } else {
            if (isLightOn) {
                isLightOn = false;
                auto duration = duration_cast<milliseconds>(now - lightStart).count();

                // 200ms هو الحد الفاصل بين النقطة والشرطة (ممكن تعدله)
                if (duration < 500) {
                    currentSymbol += ".";
                } else {
                    currentSymbol += "-";
                }
                lastSymbolTime = now;
            }
        }

        auto timeSinceLast = duration_cast<milliseconds>(now - lastSymbolTime).count();
        if (!currentSymbol.empty() && timeSinceLast > 1200 && !isLightOn) {
            if (morseMap.count(currentSymbol)) {
                decodedMessage += morseMap[currentSymbol];
            } else {
                decodedMessage += "?";
            }
            currentSymbol = "";
        }

        // --- الرسم ---
        rectangle(frame, Point(0, frame.rows - 100), Point(frame.cols, frame.rows), Scalar(0, 0, 0), -1);
        putText(frame, "Input: " + currentSymbol, Point(20, frame.rows - 60), FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 255, 255), 2);
        putText(frame, "Msg: " + decodedMessage, Point(20, frame.rows - 20), FONT_HERSHEY_SIMPLEX, 1, Scalar(0, 255, 0), 2);

        // مؤشر التحميل (Bar)
        if (isLightOn) {
            auto duration = duration_cast<milliseconds>(now - lightStart).count();
            int barLength = min((int)duration, 400);
            Scalar barColor = (duration < 250) ? Scalar(0, 255, 255) : Scalar(0, 0, 255);
            rectangle(frame, Point(frame.cols - 40, frame.rows - barLength), Point(frame.cols - 10, frame.rows), barColor, -1);
        }

        imshow("Optical Morse Decoder", frame);
        imshow("Light Sensor View", thresh);

        if (waitKey(1) == 27) break;
    }
    return 0;
}


