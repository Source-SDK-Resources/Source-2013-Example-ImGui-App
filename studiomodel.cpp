#include "studiomodel.h"

#include <istudiorender.h>
#include <bone_setup.h>
#include <bone_accessor.h>

extern IMDLCache* g_pMDLCache;

static float s_flexdescweight[MAXSTUDIOFLEXDESC];
static float s_flexdescweight2[MAXSTUDIOFLEXDESC];
static float s_flexweightsrc[MAXSTUDIOFLEXCTRL * 4];


CStudioModel::CStudioModel(const char* path)
{
	MDLHandle_t mdlHandle = g_pMDLCache->FindMDL(path);

	CStudioHdr* studiohdr = new CStudioHdr(g_pMDLCache->GetStudioHdr(mdlHandle), g_pMDLCache);
	studiohwdata_t* studiohwdata = g_pMDLCache->GetHardwareData(mdlHandle);
	if (studiohdr->GetRenderHdr()->version != STUDIO_VERSION)
	{
		Error("Bad model version on %s! Expected %d, got %d!\n", path, STUDIO_VERSION, studiohdr->GetRenderHdr()->version);
		return;
	}

	m_studiohdr = studiohdr;
	m_studiohwdata = studiohwdata;
	m_sequence = 0;
	m_posepos = new Vector[studiohdr->numbones()];
	m_poseang = new Quaternion[studiohdr->numbones()];
	m_poseparameter = new float[studiohdr->GetNumPoseParameters()];
	m_poseparameterProcessed = new float[studiohdr->GetNumPoseParameters()];

	for (int i = 0; i < studiohdr->numbones(); i++)
	{
		m_posepos[i].Init();
		m_poseang[i].Init();
	}

	for (int i = 0; i < studiohdr->GetNumPoseParameters(); i++)
	{
		m_poseparameter[i] = 0;
		m_poseparameterProcessed[i] = 0;
	}

}

CStudioModel::~CStudioModel()
{
	if (m_posepos)
		delete[] m_posepos;

	if (m_poseang)
		delete[] m_poseang;

	if(m_poseparameter)
		delete[] m_poseparameter;
	
	if(m_poseparameterProcessed)
		delete[] m_poseparameterProcessed;

}


void CStudioModel::Draw(Vector& pos, QAngle& ang)
{
	// Set the transform
	matrix3x4_t rootmatrix;
	AngleMatrix(ang, rootmatrix);
	MatrixSetColumn(pos, 3, rootmatrix);

	Draw(rootmatrix);
}

void CStudioModel::Draw(matrix3x4_t& rootmatrix)
{
	if (!m_studiohdr || !m_studiohwdata)
		return;

	// Set the info
	DrawModelInfo_t info;
	memset(&info, 0, sizeof(info));
	info.m_pStudioHdr = const_cast<studiohdr_t*>(m_studiohdr->GetRenderHdr());
	info.m_pHardwareData = m_studiohwdata;
	info.m_Lod = -1;


	// Draw it
	if (m_studiohdr->flags() & STUDIOHDR_FLAGS_STATIC_PROP)
		g_pStudioRender->DrawModelStaticProp(info, rootmatrix);
	else
	{
		memset(s_flexdescweight, 0, sizeof(s_flexdescweight));
		memset(s_flexdescweight2, 0, sizeof(s_flexdescweight2));

		int flexcount = m_studiohdr->numflexdesc();

		for (int i = 0; i < m_studiohdr->GetNumPoseParameters(); i++)
		{
			m_poseparameter[i] = Studio_SetPoseParameter(m_studiohdr, i, m_poseparameter[i], m_poseparameterProcessed[i]);
		}

		IBoneSetup boneSetup(m_studiohdr, BONE_USED_BY_ANYTHING, m_poseparameterProcessed);
		boneSetup.InitPose(m_posepos, m_poseang);


		float cycleRate = Studio_CPS(m_studiohdr, m_studiohdr->pSeqdesc(m_sequence), m_sequence, m_poseparameter);
		boneSetup.AccumulatePose(m_posepos, m_poseang, m_sequence, fmod(m_time * cycleRate, 1.0), 1.0, m_time, 0);

		matrix3x4_t* g_pBoneToWorld = g_pStudioRender->LockBoneMatrices(MAXSTUDIOBONES);

		for (int i = 0; i < m_studiohdr->numbones(); i++)
		{
			if (CalcProceduralBone(m_studiohdr, i, CBoneAccessor(g_pBoneToWorld)))
				continue;

			// Set the transform
			matrix3x4_t matrix;
			QuaternionMatrix(m_poseang[i], matrix);
			MatrixSetColumn(m_posepos[i], 3, matrix);

			mstudiobone_t* pBone = m_studiohdr->pBone(i);
			if (pBone->parent == -1)
				ConcatTransforms(rootmatrix, matrix, g_pBoneToWorld[i]);
			else
				ConcatTransforms(g_pBoneToWorld[pBone->parent], matrix, g_pBoneToWorld[i]);
		}

		for (int i = 0; i < flexcount; i++)
		{
			s_flexdescweight[i] = 0.0;
		}

		for (LocalFlexController_t i = (LocalFlexController_t)0; i < m_studiohdr->numflexcontrollers(); i++)
		{
			m_studiohdr->pFlexcontroller(i)->localToGlobal = i;
		}

		for (LocalFlexController_t i = (LocalFlexController_t)0; i < m_studiohdr->numflexcontrollers(); i++)
		{
			mstudioflexcontroller_t* pflex = m_studiohdr->pFlexcontroller(i);
			int j = m_studiohdr->pFlexcontroller(i)->localToGlobal;
			// remap m_flexweights to full dynamic range, global flexcontroller indexes
			s_flexweightsrc[j] = 0.5f * (pflex->max - pflex->min) + pflex->min;
		}

		m_studiohdr->RunFlexRules(s_flexweightsrc, s_flexdescweight);


		// Apparently, we have to hand these over to be allocated and then copy into them now.
		float* pFlexdescweight;
		float* pFlexdescweight2;
		g_pStudioRender->LockFlexWeights(flexcount, &pFlexdescweight, &pFlexdescweight2);
		for (int i = 0; i < m_studiohdr->numflexdesc(); i++)
		{
			s_flexdescweight2[i] = s_flexdescweight2[i] * 0/*weight ratio*/ + s_flexdescweight[i] * (1 - 0/*weight ratio*/);
			pFlexdescweight[i] = s_flexdescweight[i];
			pFlexdescweight2[i] = s_flexdescweight2[i];
		}
		g_pStudioRender->UnlockFlexWeights();

		g_pStudioRender->UnlockBoneMatrices();
		g_pStudioRender->DrawModel(0, info, g_pBoneToWorld, 0, 0, { rootmatrix.m_flMatVal[0][3], rootmatrix.m_flMatVal[1][3], rootmatrix.m_flMatVal[2][3]});
	}
}

Vector CStudioModel::Center()
{
	return (m_studiohdr->hull_max() + m_studiohdr->hull_min()) * 0.5f;
}


// Would just include studio_generic_io, but I'm not sure what nonsense is happening in there...

// This function would be useful, if it was static...
const studiohdr_t* studiohdr_t::FindModel(void** cache, char const* modelname) const
{
	MDLHandle_t handle = g_pMDLCache->FindMDL(modelname);
	*cache = (void*)handle;
	return g_pMDLCache->GetStudioHdr(handle);
}

virtualmodel_t* studiohdr_t::GetVirtualModel() const
{
	return g_pMDLCache->GetVirtualModel((MDLHandle_t)virtualModel);
}

byte* studiohdr_t::GetAnimBlock(int i) const
{
	return g_pMDLCache->GetAnimBlock((MDLHandle_t)virtualModel, i);
}

int studiohdr_t::GetAutoplayList(unsigned short** pOut) const
{
	return g_pMDLCache->GetAutoplayList((MDLHandle_t)virtualModel, pOut);
}

const studiohdr_t* virtualgroup_t::GetStudioHdr() const
{
	return g_pMDLCache->GetStudioHdr((MDLHandle_t)cache);
}