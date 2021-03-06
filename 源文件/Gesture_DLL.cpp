// Gesture_DLL.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "math.h"
#include <iostream>	
#include <string>   
#include <iomanip> 
#include <sstream>  
#include <opencv2/imgproc/imgproc.hpp>  
#include <opencv2/core/core.hpp>        
#include <opencv2/highgui/highgui.hpp>  
#include <windows.h>
using namespace cv;
using namespace std;
DWORD WINAPI myThread(char* _addr);

int fingerNum = 0; //手指个数
int gesture = -1;  //0 GO ; 1 JUMP
int over = 0; //判断是否结束

//返回识别参数
extern "C" _declspec(dllexport) void _stdcall GetCommand(int &_gesture, int &_over)
{
	_gesture = gesture;
	_over = over;
}

//打开摄像头
extern "C" _declspec(dllexport) void _stdcall StartDevice(char* addr)
{
	HANDLE myHandle = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)myThread, addr, 0, NULL);//创建线程    
}

//手势捕获
DWORD WINAPI myThread(char* addr)
{
	int delay = 1;
	char c;
	VideoCapture captRefrnc(0); //摄像头

	captRefrnc.set(CV_CAP_PROP_FRAME_WIDTH, 640);
	captRefrnc.set(CV_CAP_PROP_FRAME_HEIGHT, 480);

	if (!captRefrnc.isOpened())
	{
		return -1;
		cout << "Opening camera failed!";
	}

	Size refS = Size((int)captRefrnc.get(CV_CAP_PROP_FRAME_WIDTH),
		(int)captRefrnc.get(CV_CAP_PROP_FRAME_HEIGHT));

	Mat frame;	// 摄像头帧序列
	Mat frame_flip;  //镜面反转
	Mat frame_show; //摄像头画面显示
	Mat fetch_image; //捕获手势部分
	Mat frameHSV;	// hsv空间
	Mat mask(fetch_image.rows, fetch_image.cols, CV_8UC1);	// 2值掩膜
	Mat dst(fetch_image);	// 输出图像

	vector< vector<Point> > contours;	// 存储轮廓信息
	vector< vector<Point> > filterContours;	// 筛选后的轮廓
	vector< Vec4i > hierarchy;	// 轮廓的结构信息

	while (true) {

		captRefrnc >> frame;//读取摄像头
		if (frame.empty())
		{
			cout << " < < <  Game over!  > > > ";
			over = 1;
			break;
		}
		//镜面反转
		flip(frame, frame_flip, 1);
		//手势捕获部分
		fetch_image = frame_flip(Range(0, 275), Range(350, frame_flip.cols));

		Mat MYcontours(fetch_image.rows, fetch_image.cols, CV_8UC3, Scalar(0, 0, 0));

		// 运用中值滤波，去除椒盐噪声
		medianBlur(fetch_image, fetch_image, 5);
		//转换到HSV空间，便于后面的手部提取
		cvtColor(fetch_image, frameHSV, CV_BGR2HSV);

		Mat dstTemp1(fetch_image.rows, fetch_image.cols, CV_8UC1);
		Mat dstTemp2(fetch_image.rows, fetch_image.cols, CV_8UC1);
		// 对HSV空间进行量化，得到2值图像，亮的部分为手的形状
		inRange(frameHSV, Scalar(0, 30, 30), Scalar(40, 170, 256), dstTemp1);
		inRange(frameHSV, Scalar(156, 30, 30), Scalar(180, 170, 256), dstTemp2);
		bitwise_or(dstTemp1, dstTemp2, mask);

		// 形态学操作，去除噪声，并使手的边界更加清晰
		Mat element = getStructuringElement(MORPH_RECT, Size(3, 3));
		erode(mask, mask, element);
		morphologyEx(mask, mask, MORPH_OPEN, element);
		dilate(mask, mask, element);
		morphologyEx(mask, mask, MORPH_CLOSE, element);
		fetch_image.copyTo(dst, mask);
		contours.clear();
		hierarchy.clear();
		filterContours.clear();
		// 得到手的轮廓
		findContours(mask, contours, hierarchy, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

		// 去除伪轮廓
		for (size_t i = 0; i < contours.size(); i++)
		{
			if (fabs(contourArea(Mat(contours[i]))) > 3000)	//判断手进入区域的阈值
			{
				filterContours.push_back(contours[i]);
			}
		}
		// 画轮廓
		drawContours(dst, filterContours, -1, Scalar(0, 0, 255), 3/*, 8, hierarchy*/);

		int index = -1;
		Point2f center(0, 0); int num = 0;
		for (int i = 0; i < filterContours.size(); i++)
		{
			for (int j = 0; j < filterContours[i].size(); j++)
			{
				index = i;
				int xx = filterContours[i][j].x;
				int yy = filterContours[i][j].y;
				MYcontours.at<Vec3b>(yy, xx) = Vec3b(255, 255, 255);
				center.x += xx;
				center.y += yy;
				num = j;
			}
			center.x = center.x / num;
			center.y = center.y / num;

		}
		if (index == -1) continue;

		Point centerINT = Point(center.x, center.y);
		circle(MYcontours, centerINT, 15, Scalar(0, 0, 255), CV_FILLED);

		// 寻找指尖    
		vector<Point> couPoint = filterContours[index];
		vector<Point> fingerTips;
		vector<Point> palmCenter;
		Point tmp;
		int _max(0), count(0), notice(0);
		int fingerCount = -100; int fingerDistance = 0;
		for (int i = 0; i < couPoint.size(); i++)
		{
			tmp = couPoint[i];
			int dist = (tmp.x - center.x) * (tmp.x - center.x) + (tmp.y - center.y) * (tmp.y - center.y);
			if (dist > _max)
			{
				_max = dist;
				notice = i;
			}
			if (dist != _max)
			{
				count++;
				if (count > 30)
				{
					count = 0;
					_max = 0;
					bool flag = false;

					if (center.y + 40 < couPoint[notice].y)
						continue;

					if (sqrt(pow((couPoint[notice].x - center.x), 2) + pow((couPoint[notice].y - center.y), 2)) > 100)
					{
						fingerDistance = abs(notice - fingerCount);
						if (fingerDistance >= 40)
						{
							fingerTips.push_back(couPoint[notice]);
							circle(MYcontours, couPoint[notice], 6, Scalar(0, 255, 0), CV_FILLED);
							fingerCount = notice;
						}

					}
				}
			}
		}

		fingerNum = fingerTips.size();

		// 判断手型是否张开
		float angle;
		if (fingerNum == 0 || fingerNum == 1) {
			putText(MYcontours, "Jump ", Point(30, 100), FONT_HERSHEY_COMPLEX, 2, Scalar(0, 255, 255), 1);
			gesture = 0;
		}
		else {
			putText(MYcontours, "Go ", Point(30, 100), FONT_HERSHEY_COMPLEX, 2, Scalar(0, 255, 255), 1);
			gesture = 1;
		}


		//画手势捕获框
		Point start = Point(350, 0);
		Point mid = Point(350, 275);
		Point end = Point(frame_flip.cols, 275);
		line(frame_flip, start, mid, Scalar(0, 0, 255), 2);
		line(frame_flip, mid, end, Scalar(0, 0, 255), 2);

		//显示图像
		moveWindow("Source", 1500, 20);
		namedWindow("Source", 0);
		frame_show = frame_flip(Range(0, 350), Range(200, frame_flip.cols));
		moveWindow("frame", 900, 20);
		imshow("frame", frame_show);
		moveWindow("show_img", 1000, 420);
		imshow("show_img", MYcontours);

		dst.release();
		c = cvWaitKey(delay);
		if (c == 32) break;
	}

}

