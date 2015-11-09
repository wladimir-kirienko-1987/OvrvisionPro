// ovrvision_ar.cpp
//
//MIT License
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//THE SOFTWAR
//
// Oculus Rift : TM & Copyright Oculus VR, Inc. All Rights Reserved
// Unity : TM & Copyright Unity Technologies. All Rights Reserved

#ifndef __OVRVISION_METAIO_AR__

/////////// INCLUDE ///////////

#include "ovrvision_ar.h"

/////////// VARS AND DEFS ///////////

#define OV_RGB_DATASIZE		(3)	//24bit
#define OV_RGBA_DATASIZE	(4)	//32bit

/////////// CLASS ///////////

//OVR Group
namespace OVR {

//Constructor/Destructor
OvrvisionAR::OvrvisionAR(float markersize_meter, int w, int h, float focalPoint)
{
	//create alloc
	m_detector = new aruco::MarkerDetector();
	m_cameraParam = new aruco::CameraParameters();

	//Marker detector settings
	m_detector->setCornerRefinementMethod(aruco::MarkerDetector::LINES);
	m_detector->setThresholdMethod(aruco::MarkerDetector::ADPT_THRES);

	m_markerSize_Meter = markersize_meter;
	m_pMarkerData = NULL;
	m_markerDataSize = 0;

	//initialize
	m_width = w;
	m_height = h;
	m_isReady = true;

	m_pImageSrc = NULL;
	m_pImageOpenCVMat = NULL;

	//Default camera matrix
	cv::Mat cameramat(3,3,CV_32FC1);
	cameramat.at<float>(0) = focalPoint;	//f=3.1mm
	cameramat.at<float>(1) = 0.0f;
	cameramat.at<float>(2) = (float)(m_width / 2);
	cameramat.at<float>(3) = 0.0f;
	cameramat.at<float>(4) = focalPoint;
	cameramat.at<float>(5) = (float)(m_height / 2);
	cameramat.at<float>(6) = 0.0f;
	cameramat.at<float>(7) = 0.0f;
	cameramat.at<float>(8) = 1.0f;

	cv::Mat distorsionCoeff(4,1,CV_32FC1,0);
	m_cameraParam->setParams(cameramat,distorsionCoeff,cv::Size(m_width,m_height));	//set param
}

//OvrvisionAR::OvrvisionAR(int markersize){}

OvrvisionAR::~OvrvisionAR()
{
	delete m_detector;
	delete m_cameraParam;

	if(m_pMarkerData)
		delete m_pMarkerData;
}

//Private Methods
//Rotation Matrix to Quaternion
void OvrvisionAR::RotMatToQuaternion( OvVector4D* outQuat, const float* inMat )
{
	float s;
	float tr = inMat[0] + inMat[5] + inMat[10] + 1.0f;
	if( tr >= 1.0f ){
		s = 0.5f / sqrtf( tr );
		outQuat->w = 0.25f / s;
		outQuat->x = (inMat[6] - inMat[9]) * s;
		outQuat->y = (inMat[8] - inMat[2]) * s;
		outQuat->z = (inMat[1] - inMat[4]) * s;
		return;
	} else {
		float max;
		max = inMat[5] > inMat[10] ? inMat[5] : inMat[10];

		if( max < inMat[0] ){
			s = sqrtf( inMat[0] - inMat[5] - inMat[10] + 1.0f );
			float x = s * 0.5f;
			s = 0.5f / s;
			outQuat->x = x;
			outQuat->y = (inMat[1] + inMat[4]) * s;
			outQuat->z = (inMat[8] + inMat[2]) * s;
			outQuat->w = (inMat[6] - inMat[9]) * s;
			return;
		} else if ( max == inMat[5] ) {
			s = sqrtf( -inMat[0] + inMat[5] - inMat[10] + 1.0f );
			float y = s * 0.5f;
			s = 0.5f / s;
			outQuat->x = (inMat[1] + inMat[4]) * s;
			outQuat->y = y;
			outQuat->z = (inMat[6] + inMat[9]) * s;
			outQuat->w = (inMat[8] - inMat[2]) * s;
			return;
		} else {
			s = sqrtf( -inMat[0] - inMat[5] + inMat[10] + 1.0f );
			float z = s * 0.5f;
			s = 0.5f / s;
			outQuat->x = (inMat[8] + inMat[2]) * s;
			outQuat->y = (inMat[6] + inMat[9]) * s;
			outQuat->z = z;
			outQuat->w = (inMat[1] - inMat[4]) * s;
			return;
		}
	}
}

//Methods

//image set
void OvrvisionAR::SetImageRGB(unsigned char* pImage)
{
	m_pImageSrc = pImage;
	m_pImageOpenCVMat = NULL;
}

void OvrvisionAR::SetImageOpenCVImage(ovMat* pImageMat)
{
	m_pImageSrc = NULL;
	m_pImageOpenCVMat = pImageMat;
}

//Detectmarker
void OvrvisionAR::Render()
{
	//opencv var
	cv::Mat	pCamRGBImg;
	cv::Mat	pGrayImg;
	std::vector<aruco::Marker> markers;

	if(m_pImageSrc == NULL && m_pImageOpenCVMat == NULL)
		return;

	//create image
	pGrayImg = cv::Mat(cv::Size(m_width,m_height),CV_MAKETYPE(CV_8U,1));
	if(m_pImageSrc != NULL) {
		pCamRGBImg = cv::Mat(cv::Size(m_width,m_height),CV_MAKETYPE(CV_8U,OV_RGB_DATASIZE));
		//dataset
		memcpy(pCamRGBImg.data, m_pImageSrc, sizeof(unsigned char) * m_width * m_height * OV_RGB_DATASIZE);
		//convert color
		cv::cvtColor(pCamRGBImg, pGrayImg, CV_RGB2GRAY);
	} else {
		cv::cvtColor((*m_pImageOpenCVMat), pGrayImg, CV_RGB2GRAY);
	}
	
	//detect
	m_detector->detect(pGrayImg, markers, m_cameraParam->CameraMatrix, cv::Mat(), m_markerSize_Meter, true);

	//edit data
	m_markerDataSize = (int)markers.size();
	if(m_pMarkerData)
		delete m_pMarkerData;
	m_pMarkerData = new OVR::OvMarkerData[m_markerDataSize];

	//insert
	for(int i=0; i < m_markerDataSize; i++) {
		OvMarkerData* dt = &m_pMarkerData[i];
		float rotation_matrix[16];

		dt->id = markers[i].id;
		dt->centerPtOfImage.x = markers[i].getCenter().x;
		dt->centerPtOfImage.y = markers[i].getCenter().y;

		dt->translate.x = -markers[i].Tvec.at<float>(0,0) * 10.0f;	//X *10
		dt->translate.y = markers[i].Tvec.at<float>(1,0) * 10.0f;	//Y *10
		dt->translate.z = markers[i].Tvec.at<float>(2,0) * 10.0f;	//Z *10

		cv::Mat Rot(3,3,CV_32FC1);
		markers[i].Rvec.at<float>(0,0) = markers[i].Rvec.at<float>(0,0);
		markers[i].Rvec.at<float>(1,0) = -markers[i].Rvec.at<float>(1,0);
		markers[i].Rvec.at<float>(2,0) = -markers[i].Rvec.at<float>(2,0);
		cv::Rodrigues(markers[i].Rvec, Rot);

		rotation_matrix[0] = Rot.at<float>(0);
		rotation_matrix[1] = Rot.at<float>(1);
		rotation_matrix[2] = Rot.at<float>(2);
		rotation_matrix[3] = 0.0f;
		rotation_matrix[4] = Rot.at<float>(3);
		rotation_matrix[5] = Rot.at<float>(4);
		rotation_matrix[6] = Rot.at<float>(5);
		rotation_matrix[7] = 0.0f;
		rotation_matrix[8] = Rot.at<float>(6);
		rotation_matrix[9] = Rot.at<float>(7);
		rotation_matrix[10] = Rot.at<float>(8);
		rotation_matrix[11] = 0.0f;
		rotation_matrix[12] = 0.0f;
		rotation_matrix[13] = 0.0f;
		rotation_matrix[14] = 0.0f;
		rotation_matrix[15] = 1.0f;

		RotMatToQuaternion(&dt->quaternion, rotation_matrix);
		dt->quaternion.w = -dt->quaternion.w;
	}
}

//Get marker data
int	OvrvisionAR::GetMarkerDataSize()
{
	return m_markerDataSize;
}

OVR::OvMarkerData* OvrvisionAR::GetMarkerData()
{
	return m_pMarkerData;
}
OVR::OvMarkerData* OvrvisionAR::GetMarkerData(int idx)
{
	if(m_markerDataSize < idx)
		return NULL;

	return &m_pMarkerData[idx];
}

void OvrvisionAR::SetInstantTraking(bool value)
{
	//none
}

};

#endif __OVRVISION_METAIO_AR_