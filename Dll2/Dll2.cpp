// Dll2.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include <opencv2\videoio.hpp>
#include <opencv2\objdetect.hpp>
#include <opencv2\imgproc.hpp>
#include <opencv2\imgcodecs.hpp>
#include <opencv2\core.hpp>
#include <opencv2\opencv.hpp>
#include <opencv2\highgui.hpp>

#include <dlib\opencv.h>
#include <dlib\image_processing\frontal_face_detector.h>
#include <dlib\image_processing.h>

#define _USE_MATH_DEFINES
#include <math.h>

#define DllExport   __declspec( dllexport )

struct MyImageStruct
{
	int width;
	int height;
	int channels;
	unsigned char *data;
	unsigned char *data2;
};
MyImageStruct myImage;
cv::Mat myMat(12, 12, CV_8UC1);
cv::Mat face3D;
cv::Mat rotation_vector;
cv::Mat translation_vector;
cv::VideoCapture cap;
cv::CascadeClassifier faceDetection;
std::vector<cv::Point3d> model_points;
// Load face detection and pose estimation models.
dlib::frontal_face_detector detector(dlib::get_frontal_face_detector());
dlib::shape_predictor pose_model;
bool faceLoaded(false);
bool faceLandmarkLoaded(false);
bool isCap;

double jawYVar(0.0);
double leftEyelidY(0.0);
double rightEyelidY(0.0);
double leftEyebrowY(0.0);
double rightEyebrowY(0.0);

double mouth_l(0.0);
double mouth_r(0.0);

void drawFaceLandmarkLines();

struct MyFacePointLandmarks
{
	int myFacePoint2DArray[68][2];
};

cv::Rect myFaceROI;

//std::ofstream out("C:\\Users\\Andy BadMan\\source\\repos\\Dll2\\output.txt");

struct MyFaceLinesInstruction
{
	const int chin[17] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
	const int eyeBrowLeft[5] = { 17, 18, 19, 20, 21 };
	const int eyeBrowRight[5] = { 22, 23, 24, 25, 26 };
	const int eyeLeft[6] = { 36, 37, 38, 39, 40, 41 };
	const int eyeRight[6] = { 42, 43, 44, 45, 46, 47 };
	const int noseBrige[5] = { 27, 28, 29, 30, 33 };
	const int noseTip[5] = { 31, 32, 33, 34, 35 };
	const int outerMouth[12] = { 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59 };
	const int inerMouth[8] = { 60, 61, 62, 63, 64, 65, 66, 67};

	const int chinSize= 17;
	const int eyeBrowLeftSize = 5;
	const int eyeBrowRightSize = 5;
	const int eyeLeftSize = 6;
	const int eyeRightSize = 6;
	const int noseBrigeSize = 5;
	const int noseTipSize = 5;
	const int outerMouthSize = 12;
	const int inerMouthSize = 8;
};

static const MyFaceLinesInstruction myFaceLinesInstruction;

MyFacePointLandmarks myFacePointArray;

void routateX(cv::Point3d &point, const cv::Point3d &centerPoint, const double &angle) {
	
	double s = sin(angle);
	double c = cos(angle);
	
	// translate point back to origin:
	point.y -= centerPoint.y;
	point.z -= centerPoint.z;

	// rotate point
	double ynew = point.y * c - point.z * s;
	double znew = point.y * s + point.z * c;

	// translate point back:
	point.y = ynew + centerPoint.y;
	point.z = znew + centerPoint.z;
}

void routateZ(cv::Point3d &point, const cv::Point3d &centerPoint, const double &angle) {
	
	double s = sin(angle);
	double c = cos(angle);

	// translate point back to origin:
	point.x -= centerPoint.x;
	point.y -= centerPoint.y;

	// rotate point
	double xnew = point.x * c - point.y * s;
	double ynew = point.x * s + point.y * c;

	// translate point back:
	point.x = xnew + centerPoint.x;
	point.y = ynew + centerPoint.y;
}

void routateY(cv::Point3d &point, const cv::Point3d &centerPoint, const double &angle) {

	double s = sin(angle);
	double c = cos(angle);

	// translate point back to origin:
	point.x -= centerPoint.x;
	point.z -= centerPoint.z;
	// rotate point
	double znew = point.z * c - point.x * s;
	double xnew = point.z * s + point.x * c;

	// translate point back:
	point.x = xnew + centerPoint.x;
	point.z = znew + centerPoint.z;
}

extern "C" double DllExport getX() {
	return rotation_vector.at<double>(0);
}
extern "C" double DllExport getY() {
	return rotation_vector.at<double>(1);
}

extern "C" double DllExport getZ() {
	return rotation_vector.at<double>(2);
}

extern "C" double DllExport getJawY() {
	return jawYVar;
}

extern "C" double DllExport getMouth_l() {
	return jawYVar;
}

extern "C" double DllExport getMouth_r() {
	return jawYVar;
}

extern "C" double DllExport getLeftEyelidY() {
	return leftEyelidY;
}

extern "C" double DllExport getRightEyelidY() {
	return rightEyelidY;
}

extern "C" double DllExport getLeftEyeBrowY() {
	return leftEyebrowY;
}

extern "C" double DllExport getRightEyeBrowY() {
	return rightEyebrowY;
}

cv::Point2d getCenterPoint() {
	cv::Point2d center;
	for(int i(0); i < 68; ++i)
	{
		center.x += myFacePointArray.myFacePoint2DArray[i][0];
		center.y += myFacePointArray.myFacePoint2DArray[i][1];
	}
	center.x /= 68;
	center.y /= 68;
	return center;
}

void debugFacePoints() {
	if (!isCap) {
		return;
	}
	cv::Point3d point3d;
	cv::Point2d point2d;
	const cv::Point3d centerPoint3d(translation_vector.at<double>(0), translation_vector.at<double>(1), translation_vector.at<double>(2));
	std::string textOut;
	textOut += "[ "; 
	textOut += std::to_string(centerPoint3d.x) + ", " + std::to_string(centerPoint3d.y) + ", " + std::to_string(centerPoint3d.z);
	textOut += "]";
	//out << centerPoint3d << '\n';
	//double xDegree((180.0 / M_PI) *getX());
	//double yDegree((180.0 / M_PI) *getY());
	//double zDegree((180.0 / M_PI) *getZ());
	
	double xDegree(-getX());
	double yDegree(-getY());
	double zDegree(-getZ());

	face3D = cv::Mat(myMat.rows, myMat.cols, CV_8UC4, cv::Scalar::all(0));
	cv::putText(face3D, textOut, cv::Point(10, 200), cv::FONT_HERSHEY_PLAIN, 2, cv::Scalar::all(255));
	//for (int i(0); i < 68; ++i) {
		//point3d.x = myFacePointArray.myFacePoint2DArray[i][0];
		//point3d.y = myFacePointArray.myFacePoint2DArray[i][1];
		//point3d.z = 0;

		//routateX(point3d, centerPoint3d, xDegree);
		//routateY(point3d, centerPoint3d, yDegree);
		//routateZ(point3d, centerPoint3d, zDegree);

		//point2d.x = point3d.x;
		//point2d.y = point3d.y;
		//cv::circle(face3D, point2d, 2, cv::Scalar(255, 0, 0));
	//}
	drawFaceLandmarkLines();
	cv::rectangle(face3D, myFaceROI, cv::Scalar::all(255));
	cv::circle(face3D, cv::Point2d(centerPoint3d.x, centerPoint3d.y), 4, cv::Scalar::all(255));
	xDegree = (180.0 / M_PI) * -xDegree;
	yDegree = (180.0 / M_PI) * -yDegree;
	zDegree = (180.0 / M_PI) * -zDegree;

	if(xDegree < 0)
		cv::putText(myMat, "X: " + std::to_string(xDegree), cv::Point(10, 20), cv::FONT_HERSHEY_PLAIN, 2, cv::Scalar(255, 0, 0));
	else
		cv::putText(myMat, "X: " + std::to_string(xDegree), cv::Point(10, 20), cv::FONT_HERSHEY_PLAIN, 2, cv::Scalar(0, 0, 255));
	cv::putText(myMat, "Y: " + std::to_string(yDegree), cv::Point(10, 40), cv::FONT_HERSHEY_PLAIN, 2, cv::Scalar(0, 255, 0));
	cv::putText(myMat, "Z: " + std::to_string(zDegree), cv::Point(10, 60), cv::FONT_HERSHEY_PLAIN, 2, cv::Scalar(0, 0, 255));

	myImage.data2 = face3D.data;
}

extern "C" MyImageStruct DllExport getMyImage() {
	
	//cv::cvtColor(myMat, myMat, cv::COLOR_GRAY2RGB);
	cv::cvtColor(myMat, myMat, cv::COLOR_BGR2RGB);
	myImage.width = myMat.cols;
	myImage.height = myMat.rows;
	myImage.channels = myMat.channels();
	myImage.data = myMat.data;

	//debugFacePoints();
	
	return myImage;
}

extern "C" MyFacePointLandmarks DllExport myfacePoints2D() {
	return myFacePointArray;
}

extern "C" void DllExport stopCapture(bool stop) {
	if (cap.isOpened() || stop) {
		cap.release();
		//cv::destroyAllWindows();
	}
	else if (!stop) {
		//cap.open(0);
		cap.open("F:\\face_animation\\eye_brows.mp4");
		//cap.open("C:\\Users\\Andy BadMan\\Pictures\\Camera Roll\\WIN_20200601_13_29_56_Pro.mp4");
	}		
}

double getDistance(dlib::point a, dlib::point b)
{
	// Calculating distance 
	return sqrt(pow(b.x() - a.x(), 2) + pow(b.y() - a.y(), 2) * 1.0);
}

extern "C" int DllExport FooPluginFunctionTemp() {
	
	faceLoaded = faceDetection.load("F:\\Unity\\Projects\\Test_camera\\haarcascade_frontalface_alt_tree.xml");
	if (!faceLoaded) {
		return -1;
	}
	try
	{
		dlib::deserialize("shape_predictor_68_face_landmarks.dat") >> pose_model;
		faceLandmarkLoaded = true;
	}
	catch (dlib::serialization_error& e) {
		return -5;
	}
	if (cap.isOpened()) {
		isCap = cap.read(myMat);
		if (isCap)
		{
			int frameNumnber = cap.get(cv::CAP_PROP_POS_FRAMES);
			cv::Mat gray;
			cv::cvtColor(myMat, gray, cv::COLOR_BGR2GRAY);
			dlib::cv_image<unsigned char> cimg(gray);
			std::vector<dlib::rectangle > faces = detector(cimg);
			cv::imwrite("F:\\face_animation\\eye_brows\\" + std::to_string(frameNumnber) + ".jpg", myMat);
			if (!faces.empty()) {
				dlib::rectangle myFace = *faces.begin();
				dlib::full_object_detection shape(pose_model(cimg, myFace));
				cv::Point cvp;
				for (int i(0), ii(shape.num_parts()); i <= ii; ++i, --ii)
				{
					dlib::point p = shape.part(i);
					cvp.x = p.x();
					cvp.y = p.y();
					cv::circle(myMat, cvp, 2, cv::Scalar(0, 0, 255));

					myFacePointArray.myFacePoint2DArray[i][0] = cvp.x;
					myFacePointArray.myFacePoint2DArray[i][1] = cvp.y;

					p = shape.part(ii);
					cvp.x = p.x();
					cvp.y = p.y();
					cv::circle(myMat, cvp, 2, cv::Scalar(0, 0, 255));

					myFacePointArray.myFacePoint2DArray[ii][0] = cvp.x;
					myFacePointArray.myFacePoint2DArray[ii][1] = cvp.y;
				}
				cv::imshow("Test", myMat);
			}
		}
	}
	return 0;
}


extern "C" int DllExport FooPluginFunction() {
	if (!faceLoaded) {
		faceLoaded = faceDetection.load("F:\\Unity\\Projects\\Test_camera\\haarcascade_frontalface_alt_tree.xml");
		try
		{
			model_points.clear();
			
			model_points.push_back(cv::Point3d(6.825897, 6.760612, 4.402142));     //#33 left brow left corner
			model_points.push_back(cv::Point3d(1.330353, 7.122144, 6.903745));     //#29 left brow right corner
			model_points.push_back(cv::Point3d(-1.330353, 7.122144, 6.903745));    //#34 right brow left corner
			model_points.push_back(cv::Point3d(-6.825897, 6.760612, 4.402142));    //#38 right brow right corner
			model_points.push_back(cv::Point3d(5.311432, 5.485328, 3.987654));     //#13 left eye left corner
			model_points.push_back(cv::Point3d(1.789930, 5.393625, 4.413414));     //#17 left eye right corner
			model_points.push_back(cv::Point3d(-1.789930, 5.393625, 4.413414));    //#25 right eye left corner
			model_points.push_back(cv::Point3d(-5.311432, 5.485328, 3.987654));    //#21 right eye right corner
			model_points.push_back(cv::Point3d(2.005628, 1.409845, 6.165652));     //#55 nose left corner
			model_points.push_back(cv::Point3d(-2.005628, 1.409845, 6.165652));    //#49 nose right corner
			model_points.push_back(cv::Point3d(2.774015, -2.080775, 5.048531));    //#43 mouth left corner
			model_points.push_back(cv::Point3d(-2.774015, -2.080775, 5.048531));   //#39 mouth right corner
			model_points.push_back(cv::Point3d(0.000000, -3.116408, 6.097667));    //#45 mouth central bottom corner
			model_points.push_back(cv::Point3d(0.000000, -7.415691, 4.070434));    //#6 chin corner
			
			//model_points.push_back(cv::Point3d(0.0f, 0.0f, 0.0f));               // Nose tip
			//model_points.push_back(cv::Point3d(0.0f, -330.0f, -65.0f));          // Chin
			//model_points.push_back(cv::Point3d(-225.0f, 170.0f, -135.0f));       // Left eye left corner
			//model_points.push_back(cv::Point3d(225.0f, 170.0f, -135.0f));        // Right eye right corner
			//model_points.push_back(cv::Point3d(-150.0f, -150.0f, -125.0f));      // Left Mouth corner
			//model_points.push_back(cv::Point3d(150.0f, -150.0f, -125.0f));       // Right mouth corner
			dlib::deserialize("shape_predictor_68_face_landmarks.dat") >> pose_model;
			faceLandmarkLoaded = true;
		}
		catch (dlib::serialization_error& e) {
			return -5;
		}
		return -1;
	}
	if (cap.isOpened()) {
		isCap = cap.read(myMat);
		if (isCap)
		{
			cv::resize(myMat, myMat, cv::Size(640, 480), 0, 0, cv::INTER_NEAREST);
			cv::cvtColor(myMat, myMat, cv::COLOR_BGR2GRAY);
			//std::vector<cv::Rect> faces;
			dlib::cv_image<unsigned char> cimg(myMat);
			std::vector<dlib::rectangle > faces = detector(cimg);
			//faceDetection.detectMultiScale(myMat, faces);
			if (!faces.empty()) {
				//cv::Rect myFace = *faces.begin();

				//dlib::rectangle rect(myFace.x, myFace.y, myFace.x + myFace.width, myFace.y + myFace.height);
				dlib::rectangle myFace = *faces.begin();
				
				myFaceROI.x = myFace.left();
				myFaceROI.y = myFace.top();
				myFaceROI.width = myFace.width();
				myFaceROI.height = myFace.height();

				dlib::full_object_detection shape(pose_model(cimg, myFace));
				cv::Point cvp;
				for (int i(0), ii(shape.num_parts()); i <= ii; ++i, --ii)
				{
					dlib::point p = shape.part(i);
					cvp.x = p.x();
					cvp.y = p.y();
					//cv::circle(myMat, cvp, 2, cv::Scalar::all(255));
					
					myFacePointArray.myFacePoint2DArray[i][0] = cvp.x;
					myFacePointArray.myFacePoint2DArray[i][1] = cvp.y;

					p = shape.part(ii);
					cvp.x = p.x();
					cvp.y = p.y();
					//cv::circle(myMat, cvp, 2, cv::Scalar::all(255));

					myFacePointArray.myFacePoint2DArray[ii][0] = cvp.x;
					myFacePointArray.myFacePoint2DArray[ii][1] = cvp.y;
				}

				// Camera internals
				double focal_length = myMat.cols; // Approximate focal length.
				cv::Point2d center = cv::Point2d(myMat.cols / 2, myMat.rows / 2);
				cv::Mat camera_matrix = (cv::Mat_<double>(3, 3) << focal_length, 0, center.x, 0, focal_length, center.y, 0, 0, 1);
				cv::Mat dist_coeffs = cv::Mat::zeros(4, 1, cv::DataType<double>::type); // Assuming no lens distortion
				//cout << "Camera Matrix " << endl << camera_matrix << endl;
				// Output rotation and translation
				//cv::Mat rotation_vector; // Rotation in axis-angle form
				// Solve for pose
				std::vector<cv::Point2d> image_points;

				dlib::point p = shape.part(17);
				image_points.push_back(cv::Point2d(p.x(), p.y()));
				p = shape.part(21);
				image_points.push_back(cv::Point2d(p.x(), p.y()));
				p = shape.part(22);
				image_points.push_back(cv::Point2d(p.x(), p.y()));
				p = shape.part(26);
				image_points.push_back(cv::Point2d(p.x(), p.y()));
				p = shape.part(36);
				image_points.push_back(cv::Point2d(p.x(), p.y()));
				p = shape.part(39);
				image_points.push_back(cv::Point2d(p.x(), p.y()));
				p = shape.part(42);
				image_points.push_back(cv::Point2d(p.x(), p.y()));
				p = shape.part(45);
				image_points.push_back(cv::Point2d(p.x(), p.y()));
				p = shape.part(31);
				image_points.push_back(cv::Point2d(p.x(), p.y()));
				p = shape.part(35);
				image_points.push_back(cv::Point2d(p.x(), p.y()));
				p = shape.part(48);
				image_points.push_back(cv::Point2d(p.x(), p.y()));
				p = shape.part(54);
				image_points.push_back(cv::Point2d(p.x(), p.y()));
				p = shape.part(66);
				image_points.push_back(cv::Point2d(p.x(), p.y()));
				p = shape.part(8);
				image_points.push_back(cv::Point2d(p.x(), p.y()));
				
				for (int i(0), ii(image_points.size()); i < ii; ++i, --ii)
				{
					cv::circle(myMat, image_points[i], 2, cv::Scalar::all(0));
				}
				cv::solvePnP(model_points, image_points, camera_matrix, dist_coeffs, rotation_vector, translation_vector);
				
				//jawYVar = getDistance(shape.part(62), shape.part(66));
				jawYVar = getDistance(shape.part(62), shape.part(66));
				double noseBridg(getDistance(shape.part(28), shape.part(29)));
				if (jawYVar < 0) {
					jawYVar = 0.0;
				}
				else if (jawYVar > noseBridg) 
				{
					jawYVar = 1.0;
				}
				else
				{
					double xDegree(-getX());
					xDegree = (180.0 / M_PI) * -xDegree;
					if(xDegree < 0)
						jawYVar = jawYVar / noseBridg;
					else
						jawYVar = noseBridg / jawYVar;
				}
				rightEyelidY = (getDistance(shape.part(43), shape.part(47)) + getDistance(shape.part(44), shape.part(46))) / 2.0;
				leftEyelidY = (getDistance(shape.part(37), shape.part(41)) + getDistance(shape.part(38), shape.part(40))) / 2.0;
				leftEyebrowY = getDistance(shape.part(21), shape.part(27));
				rightEyebrowY = getDistance(shape.part(22), shape.part(27));
				// Project a 3D point (0, 0, 1000.0) onto the image plane.
				// We use this to draw a line sticking out of the nose
				std::vector<cv::Point3d> nose_end_point3D;
				std::vector<cv::Point2d> nose_end_point2D;
				nose_end_point3D.push_back(cv::Point3d(0, 0, 1000.0));
				cv::projectPoints(nose_end_point3D, rotation_vector, translation_vector, camera_matrix, dist_coeffs, nose_end_point2D);
				for (int i = 0; i < image_points.size(); i++)
				{
					cv::circle(myMat, image_points[i], 3, cv::Scalar::all(128), -1);
				}
				//cv::line(myMat, image_points[image_points.size()-2], nose_end_point2D[0], cv::Scalar::all(255), 2);

				//cv::imshow("Dispaly", myMat);
				//cv::waitKey(1);
				return (((myFace.left() + myFace.width()) / 2.0) / myMat.cols) * 360;
			}
			//cv::imshow("Dispaly", myMat);
			//cv::waitKey(1);
			return -4;

		}
		//cv::imshow("Dispaly", myMat);
		//cv::waitKey(1);
		return -(isCap + 2);
	}
	return 0;
}

void drawFaceLandmarkLines() {
	const int x(0), y(1);
	int i(0), index(0);
	cv::Point p1, p2;
	while (i < myFaceLinesInstruction.chinSize - 1) {
		index = myFaceLinesInstruction.chin[i];
		p1.x = myFacePointArray.myFacePoint2DArray[index][x];
		p1.y = myFacePointArray.myFacePoint2DArray[index][y];

		++i;
		index = myFaceLinesInstruction.chin[i];
		p2.x = myFacePointArray.myFacePoint2DArray[index][x];
		p2.y = myFacePointArray.myFacePoint2DArray[index][y];
		cv::line(face3D, p1, p2, cv::Scalar::all(255));
	}
	i = 0;
	while (i < myFaceLinesInstruction.eyeBrowLeftSize - 1) {
		index = myFaceLinesInstruction.eyeBrowLeft[i];
		p1.x = myFacePointArray.myFacePoint2DArray[index][x];
		p1.y = myFacePointArray.myFacePoint2DArray[index][y];

		++i;
		index = myFaceLinesInstruction.eyeBrowLeft[i];
		p2.x = myFacePointArray.myFacePoint2DArray[index][x];
		p2.y = myFacePointArray.myFacePoint2DArray[index][y];
		cv::line(face3D, p1, p2, cv::Scalar::all(255));
	}
	i = 0;
	while (i < myFaceLinesInstruction.eyeBrowRightSize - 1) {
		index = myFaceLinesInstruction.eyeBrowRight[i];
		p1.x = myFacePointArray.myFacePoint2DArray[index][x];
		p1.y = myFacePointArray.myFacePoint2DArray[index][y];

		++i;
		index = myFaceLinesInstruction.eyeBrowRight[i];
		p2.x = myFacePointArray.myFacePoint2DArray[index][x];
		p2.y = myFacePointArray.myFacePoint2DArray[index][y];
		cv::line(face3D, p1, p2, cv::Scalar::all(255));
	}
	{
		index = myFaceLinesInstruction.eyeLeft[0];
		p1.x = myFacePointArray.myFacePoint2DArray[index][x];
		p1.y = myFacePointArray.myFacePoint2DArray[index][y];

		++i;
		index = myFaceLinesInstruction.eyeLeft[myFaceLinesInstruction.eyeLeftSize - 1];
		p2.x = myFacePointArray.myFacePoint2DArray[index][x];
		p2.y = myFacePointArray.myFacePoint2DArray[index][y];
		cv::line(face3D, p1, p2, cv::Scalar::all(255));
	}
	i = 0;
	while (i < myFaceLinesInstruction.eyeLeftSize - 1) {
		index = myFaceLinesInstruction.eyeLeft[i];
		p1.x = myFacePointArray.myFacePoint2DArray[index][x];
		p1.y = myFacePointArray.myFacePoint2DArray[index][y];

		++i;
		index = myFaceLinesInstruction.eyeLeft[i];
		p2.x = myFacePointArray.myFacePoint2DArray[index][x];
		p2.y = myFacePointArray.myFacePoint2DArray[index][y];
		cv::line(face3D, p1, p2, cv::Scalar::all(255));
	}
	{
		index = myFaceLinesInstruction.eyeRight[0];
		p1.x = myFacePointArray.myFacePoint2DArray[index][x];
		p1.y = myFacePointArray.myFacePoint2DArray[index][y];

		++i;
		index = myFaceLinesInstruction.eyeRight[myFaceLinesInstruction.eyeRightSize - 1];
		p2.x = myFacePointArray.myFacePoint2DArray[index][x];
		p2.y = myFacePointArray.myFacePoint2DArray[index][y];
		cv::line(face3D, p1, p2, cv::Scalar::all(255));
	}
	i = 0;
	while (i < myFaceLinesInstruction.eyeRightSize - 1) {
		index = myFaceLinesInstruction.eyeRight[i];
		p1.x = myFacePointArray.myFacePoint2DArray[index][x];
		p1.y = myFacePointArray.myFacePoint2DArray[index][y];

		++i;
		index = myFaceLinesInstruction.eyeRight[i];
		p2.x = myFacePointArray.myFacePoint2DArray[index][x];
		p2.y = myFacePointArray.myFacePoint2DArray[index][y];
		cv::line(face3D, p1, p2, cv::Scalar::all(255));
	}
	i = 0;
	while (i < myFaceLinesInstruction.noseBrigeSize - 1) {
		index = myFaceLinesInstruction.noseBrige[i];
		p1.x = myFacePointArray.myFacePoint2DArray[index][x];
		p1.y = myFacePointArray.myFacePoint2DArray[index][y];

		++i;
		index = myFaceLinesInstruction.noseBrige[i];
		p2.x = myFacePointArray.myFacePoint2DArray[index][x];
		p2.y = myFacePointArray.myFacePoint2DArray[index][y];
		cv::line(face3D, p1, p2, cv::Scalar::all(255));
	}
	i = 0;
	while (i < myFaceLinesInstruction.noseTipSize - 1) {
		index = myFaceLinesInstruction.noseTip[i];
		p1.x = myFacePointArray.myFacePoint2DArray[index][x];
		p1.y = myFacePointArray.myFacePoint2DArray[index][y];

		++i;
		index = myFaceLinesInstruction.noseTip[i];
		p2.x = myFacePointArray.myFacePoint2DArray[index][x];
		p2.y = myFacePointArray.myFacePoint2DArray[index][y];
		cv::line(face3D, p1, p2, cv::Scalar::all(255));
	}
	{
		index = myFaceLinesInstruction.outerMouth[0];
		p1.x = myFacePointArray.myFacePoint2DArray[index][x];
		p1.y = myFacePointArray.myFacePoint2DArray[index][y];

		++i;
		index = myFaceLinesInstruction.outerMouth[myFaceLinesInstruction.outerMouthSize - 1];
		p2.x = myFacePointArray.myFacePoint2DArray[index][x];
		p2.y = myFacePointArray.myFacePoint2DArray[index][y];
		cv::line(face3D, p1, p2, cv::Scalar::all(255));
	}
	i = 0;
	while (i < myFaceLinesInstruction.outerMouthSize - 1) {
		index = myFaceLinesInstruction.outerMouth[i];
		p1.x = myFacePointArray.myFacePoint2DArray[index][x];
		p1.y = myFacePointArray.myFacePoint2DArray[index][y];

		++i;
		index = myFaceLinesInstruction.outerMouth[i];
		p2.x = myFacePointArray.myFacePoint2DArray[index][x];
		p2.y = myFacePointArray.myFacePoint2DArray[index][y];
		cv::line(face3D, p1, p2, cv::Scalar::all(255));
	}
	{
		index = myFaceLinesInstruction.inerMouth[0];
		p1.x = myFacePointArray.myFacePoint2DArray[index][x];
		p1.y = myFacePointArray.myFacePoint2DArray[index][y];

		++i;
		index = myFaceLinesInstruction.inerMouth[myFaceLinesInstruction.inerMouthSize - 1];
		p2.x = myFacePointArray.myFacePoint2DArray[index][x];
		p2.y = myFacePointArray.myFacePoint2DArray[index][y];
		cv::line(face3D, p1, p2, cv::Scalar::all(255));
	}
	i = 0;
	while (i < myFaceLinesInstruction.inerMouthSize - 1) {
		index = myFaceLinesInstruction.inerMouth[i];
		p1.x = myFacePointArray.myFacePoint2DArray[index][x];
		p1.y = myFacePointArray.myFacePoint2DArray[index][y];

		++i;
		index = myFaceLinesInstruction.inerMouth[i];
		p2.x = myFacePointArray.myFacePoint2DArray[index][x];
		p2.y = myFacePointArray.myFacePoint2DArray[index][y];
		cv::line(face3D, p1, p2, cv::Scalar::all(255));
	}
}