#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <chrono>
#include <deque> // عشان نخزن تاريخ الإشارة للرسم البياني

using namespace cv;
using namespace std;
using namespace std::chrono;

// ================== الألوان (Theme: Cyberpunk Cyan) ==================
const Scalar COL_BG(10, 10, 10);           // خلفية سوداء تقريباً
const Scalar COL_CYAN(255, 255, 0);        // سماوي (BGR)
const Scalar COL_CYAN_DIM(100, 100, 0);    // سماوي غامق
const Scalar COL_RED(50, 50, 255);         // أحمر للتحذير
const Scalar COL_WHITE(220, 220, 220);     // نص أبيض
const Scalar COL_GLASS(20, 20, 20);        // لون الزجاج الخلفي

// مقاسات الشاشة
const int W = 1280;
const int H = 720;

map<string, char> morseMap = {
    {".-", 'A'}, {"-...", 'B'}, {"-.-.", 'C'}, {"-..", 'D'}, {".", 'E'},
    {"..-.", 'F'}, {"--.", 'G'}, {"....", 'H'}, {"..", 'I'}, {".---", 'J'},
    {"-.-", 'K'}, {".-..", 'L'}, {"--", 'M'}, {"-.", 'N'}, {"---", 'O'},
    {".--.", 'P'}, {"--.-", 'Q'}, {".-.", 'R'}, {"...", 'S'}, {"-", 'T'},
    {"..-", 'U'}, {"...-", 'V'}, {".--", 'W'}, {"-..-", 'X'}, {"-.--", 'Y'},
    {"--..", 'Z'}, {".----", '1'}, {"..---", '2'}, {"...--", '3'}, {"----.", '9'}, {"-----", '0'}
};

// دالة لرسم مستطيل زجاجي شفاف (Glass Effect)
void drawGlassRect(Mat& img, Rect r, Scalar color, double alpha) {
    Mat overlay;
    img.copyTo(overlay);
    rectangle(overlay, r, color, -1);
    addWeighted(overlay, alpha, img, 1.0 - alpha, 0, img);
    rectangle(img, r, COL_CYAN, 1); // إطار رفيع
}

// دالة لرسم الشبكة الخلفية (Grid)
void drawGrid(Mat& img) {
    for(int i=0; i<W; i+=50) line(img, Point(i, 0), Point(i, H), Scalar(30, 30, 30), 1);
    for(int i=0; i<H; i+=50) line(img, Point(0, i), Point(W, i), Scalar(30, 30, 30), 1);
}

int main() {
    VideoCapture cam(1); // جرب 0 لو 1 مشتغلتش
    if (!cam.isOpened()) cam.open(0);
    cam.set(CAP_PROP_EXPOSURE, -6);

    Mat frame, gray, thresh;
    Mat canvas(H, W, CV_8UC3, COL_BG);

    int thresholdVal = 230;
    string winName = "Spy System";
    namedWindow(winName, WINDOW_AUTOSIZE);
    createTrackbar("Sens.", winName, &thresholdVal, 255);

    // متغيرات المنطق
    auto lightStart = high_resolution_clock::now();
    bool isLightOn = false;
    string currentSymbol = "";
    string decodedMessage = "";
    auto lastSymbolTime = high_resolution_clock::now();
    
    // متغيرات الرسم
    int scanLineY = 100; // مكان خط المسح
    int scanDir = 2;     // سرعة خط المسح
    deque<int> signalHistory(300, 0); // لتخزين آخر 300 قراءة للرسم البياني

    while (true) {
        cam >> frame;
        if (frame.empty()) break;
        flip(frame, frame, 1);

        // --- المعالجة ---
        cvtColor(frame, gray, COLOR_BGR2GRAY);
        threshold(gray, thresh, thresholdVal, 255, THRESH_BINARY);
        erode(thresh, thresh, Mat(), Point(-1, -1), 2);
        dilate(thresh, thresh, Mat(), Point(-1, -1), 2);

        // كشف الضوء
        double maxArea = 0;
        vector<vector<Point>> contours;
        findContours(thresh, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE);
        for (auto& c : contours) {
            double a = contourArea(c);
            if (a > maxArea) maxArea = a;
        }
        
        bool lightDetectedNow = (maxArea > 20);

        // تحديث الرسم البياني (Signal Graph)
        signalHistory.pop_front();
        signalHistory.push_back(lightDetectedNow ? 100 : 0);

        // منطق مورس
        auto now = high_resolution_clock::now();
        if (lightDetectedNow) {
            if (!isLightOn) { lightStart = now; isLightOn = true; }
        } else {
            if (isLightOn) {
                isLightOn = false;
                auto duration = duration_cast<milliseconds>(now - lightStart).count();
                if (duration < 400) currentSymbol += "."; else currentSymbol += "-";
                lastSymbolTime = now;
            }
        }

        auto timeSinceLast = duration_cast<milliseconds>(now - lastSymbolTime).count();
        if (!currentSymbol.empty() && timeSinceLast > 1200 && !isLightOn) {
            if (morseMap.count(currentSymbol)) decodedMessage += morseMap[currentSymbol];
            else decodedMessage += "?";
            currentSymbol = "";
        }

        // ================== (New GUI) ==================
        canvas.setTo(COL_BG);
        drawGrid(canvas); // الخلفية الشبكية

        // 1. الفيديو الرئيسي (Main Feed) - مع إطار مشطو
        Mat mainView;
        resize(frame, mainView, Size(800, 600));
        
        // تأثير خط المسح (Scanner Line)
        scanLineY += scanDir;
        if (scanLineY > 580 || scanLineY < 20) scanDir *= -1;
        line(mainView, Point(0, scanLineY), Point(800, scanLineY), COL_CYAN, 2);
        
        // دمج الفيديو في اللوحة
        Rect mainRect(30, 80, 800, 600);
        mainView.copyTo(canvas(mainRect));
        rectangle(canvas, mainRect, COL_CYAN, 2);
        // زينة الزوايا
        line(canvas, Point(30, 80), Point(100, 80), COL_CYAN, 5);
        line(canvas, Point(30, 80), Point(30, 150), COL_CYAN, 5);
        
        putText(canvas, "OPTICAL SENSOR FEED [LIVE]", Point(30, 70), FONT_HERSHEY_SIMPLEX, 0.7, COL_CYAN, 1);

        // 2. الرسم البياني للموجة (Signal Waveform) - يمين الشاشة
        Rect graphRect(850, 80, 400, 150);
        drawGlassRect(canvas, graphRect, COL_GLASS, 0.7);
        putText(canvas, "SIGNAL FREQUENCY", Point(855, 70), FONT_HERSHEY_SIMPLEX, 0.6, COL_CYAN, 1);
        
        // رسم الخطوط
        for (size_t i = 1; i < signalHistory.size(); i++) {
            int y1 = graphRect.y + 120 - signalHistory[i-1];
            int y2 = graphRect.y + 120 - signalHistory[i];
            int x1 = graphRect.x + i;
            int x2 = graphRect.x + i + 1;
            line(canvas, Point(x1, y1), Point(x2, y2), COL_CYAN, 2);
        }

        // 3. منطقة الكود والترجمة (يمين تحت)
        Rect textRect(850, 250, 400, 430);
        drawGlassRect(canvas, textRect, COL_GLASS, 0.5);
        putText(canvas, "DECODER LOG", Point(855, 240), FONT_HERSHEY_SIMPLEX, 0.6, COL_CYAN, 1);
        
        putText(canvas, "INCOMING:", Point(860, 300), FONT_HERSHEY_PLAIN, 1.2, COL_WHITE, 1);
        putText(canvas, currentSymbol, Point(860, 350), FONT_HERSHEY_SIMPLEX, 2, COL_CYAN, 2);
        
        putText(canvas, "TRANSLATED:", Point(860, 420), FONT_HERSHEY_PLAIN, 1.2, COL_WHITE, 1);
        putText(canvas, decodedMessage, Point(860, 480), FONT_HERSHEY_SIMPLEX, 1.5, COL_RED, 2); // رسالة حمراء

        // 4. مؤشر الحالة (Status Indicator)
        if (lightDetectedNow) {
            putText(canvas, "SIGNAL LOCKED", Point(860, 650), FONT_HERSHEY_SIMPLEX, 1, COL_CYAN, 2);
            circle(canvas, Point(1200, 640), 10, COL_CYAN, -1);
        } else {
            putText(canvas, "WAITING...", Point(860, 650), FONT_HERSHEY_SIMPLEX, 1, Scalar(100, 100, 100), 1);
        }

        imshow(winName, canvas);
        char key = waitKey(1);
        if (key == 27) break;
        if (key == 'c') { decodedMessage = ""; currentSymbol = ""; }
    }
    return 0;

}
