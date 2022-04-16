#pragma once
#include <studio.h>

struct CStudioModel
{
	CStudioModel(const char* path);

	~CStudioModel();

	void Draw(Vector& pos, QAngle& ang);
	void Draw(matrix3x4_t& mat);

	Vector Center();

	CStudioHdr* m_studiohdr;
	studiohwdata_t* m_studiohwdata;
	int m_sequence;
	Vector* m_posepos;
	Quaternion* m_poseang;
	float* m_poseparameter;
	float* m_poseparameterProcessed;

	float m_time;
};