/* The copyright in this software is being made available under the BSD
 * License, included below. This software may be subject to other third party
 * and contributor rights, including patent rights, and no such rights are
 * granted under this license.
 *
 * Copyright (c) 2010-2013, ITU/ISO/IEC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  * Neither the name of the ITU/ISO/IEC nor the names of its contributors may
 *    be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/** \file     TEncSearch.cpp
 \brief    encoder search class
 */

#include "TLibCommon/TypeDef.h"
#include "TLibCommon/TComRom.h"
#include "TLibCommon/TComMotionInfo.h"
#include "TEncSearch.h"
#include "primitives.h"
#include "common.h"
#include "PPA/ppa.h"
#include <math.h>
#include "RK_HEVC_ENC_MACRO.h"
#include "hardwareC/inter.h"
#include "CABAC.h"

using namespace x265;


DECLARE_CYCLE_COUNTER(ME);

//! \ingroup TLibEncoder
//! \{
void static logRefAndFencParam2File(FILE* fp_w, uint32_t initTrDepth, INTERFACE_INTRA rk_Interface_Intra)
{
	if (initTrDepth == 0)// 屏蔽 4x4 这一层
	{
		uint8_t flag_map_pel = 4;

		if (rk_Interface_Intra.cidx != 0)
		{
			flag_map_pel = 2;
		}

		// inf_intra_proc.reconEdgePixelY
		uint8_t* pRef = rk_Interface_Intra.reconEdgePixel;
		bool* bNeighbour = rk_Interface_Intra.bNeighborFlags;

		for (uint8_t i = 0; i < 2 * rk_Interface_Intra.size / flag_map_pel; i++)
		{
			if (*bNeighbour++ == 1)
			{
				for (uint8_t j = 0; j < flag_map_pel; j++)
				{
					RK_HEVC_FPRINT(fp_w, "0x%02x ", *pRef /* == 0xcd ? 0 : *pRef*/);
					pRef++;
				}
			}
			else
			{
				for (uint8_t j = 0; j < flag_map_pel; j++)
				{
					RK_HEVC_FPRINT(fp_w, "xxxx ");
					pRef++;
				}
			}
		}

		if (*bNeighbour++ == 1)
		{
			RK_HEVC_FPRINT(fp_w, "\n0x%02x \n", *pRef /* == 0xcd ? 0 : *pRef*/);
			pRef++;
		}
		else
		{
			RK_HEVC_FPRINT(fp_w, "\nxxxx \n");
			pRef++;
		}

		for (uint8_t i = 0; i < 2 * rk_Interface_Intra.size / flag_map_pel; i++)
		{
			if (*bNeighbour++ == 1)
			{
				for (uint8_t j = 0; j < flag_map_pel; j++)
				{
					RK_HEVC_FPRINT(fp_w, "0x%02x ", *pRef /* == 0xcd ? 0 : *pRef*/);
					pRef++;
				}
			}
			else
			{
				for (uint8_t j = 0; j < flag_map_pel; j++)
				{
					RK_HEVC_FPRINT(fp_w, "xxxx ");
					pRef++;
				}
			}
		}

		RK_HEVC_FPRINT(fp_w, "\n\n");

		// rk_Interface_Intra.fenc
		for (uint8_t i = 0; i < rk_Interface_Intra.size; i++)
		{
			for (uint8_t j = 0; j < rk_Interface_Intra.size; j++)
			{
				RK_HEVC_FPRINT(fp_w, "0x%02x ", rk_Interface_Intra.fenc[i * rk_Interface_Intra.size + j]);
			}
			RK_HEVC_FPRINT(fp_w, "\n");
		}
		RK_HEVC_FPRINT(fp_w, "\n\n");

		if (rk_Interface_Intra.cidx == 0)
		{
			RK_HEVC_FPRINT(fp_w, "Luma StrongFlag = %d\n\n", (rk_Interface_Intra.useStrongIntraSmoothing == true) ? 1 : 0);
		}
		else
		{
			RK_HEVC_FPRINT(fp_w, "chroma haven't StrongFlag\n\n");
		}
	}
}
TEncSearch::TEncSearch()
{

#ifdef X265_INTRA_DEBUG
	m_rkIntraPred = NULL;
#endif
#ifdef RK_INTRA_MODE_CHOOSE
	m_rkIntraPredFast = NULL;
#endif
	m_qtTempCoeffY = NULL;
	m_qtTempCoeffCb = NULL;
	m_qtTempCoeffCr = NULL;
	m_qtTempTrIdx = NULL;
	m_qtTempTComYuv = NULL;
	m_qtTempTUCoeffY = NULL;
	m_qtTempTUCoeffCb = NULL;
	m_qtTempTUCoeffCr = NULL;
	for (int i = 0; i < 3; i++)
	{
		m_sharedPredTransformSkip[i] = NULL;
		m_qtTempCbf[i] = NULL;
		m_qtTempTransformSkipFlag[i] = NULL;
	}

	m_cfg = NULL;
	m_rdCost = NULL;
	m_trQuant = NULL;
	m_tempPel = NULL;
	m_entropyCoder = NULL;
	m_rdSbacCoders = NULL;
	m_rdGoOnSbacCoder = NULL;
	setWpScalingDistParam(NULL, -1, REF_PIC_LIST_X);
}

TEncSearch::~TEncSearch()
{
	delete[] m_tempPel;

	if (m_cfg)
	{
		const uint32_t numLayersToAllocate = m_cfg->getQuadtreeTULog2MaxSize() - m_cfg->getQuadtreeTULog2MinSize() + 1;
		for (uint32_t i = 0; i < numLayersToAllocate; ++i)
		{
			delete[] m_qtTempCoeffY[i];
			delete[] m_qtTempCoeffCb[i];
			delete[] m_qtTempCoeffCr[i];
			m_qtTempTComYuv[i].destroy();
		}
	}
#ifdef X265_INTRA_DEBUG
	delete m_rkIntraPred;
#endif
#ifdef RK_INTRA_MODE_CHOOSE
	delete m_rkIntraPredFast;
#endif

	delete[] m_qtTempCoeffY;
	delete[] m_qtTempCoeffCb;
	delete[] m_qtTempCoeffCr;
	delete[] m_qtTempTrIdx;
	delete[] m_qtTempTComYuv;
	delete[] m_qtTempTUCoeffY;
	delete[] m_qtTempTUCoeffCb;
	delete[] m_qtTempTUCoeffCr;
	for (uint32_t i = 0; i < 3; ++i)
	{
		delete[] m_qtTempCbf[i];
		delete[] m_sharedPredTransformSkip[i];
		delete[] m_qtTempTransformSkipFlag[i];
	}

	m_qtTempTransformSkipTComYuv.destroy();
	m_tmpYuvPred.destroy();
}

void TEncSearch::init(TEncCfg* cfg, TComRdCost* rdCost, TComTrQuant* trQuant)
{
	m_cfg = cfg;
	m_trQuant = trQuant;
	m_rdCost = rdCost;

	m_me.setSearchMethod(cfg->param.searchMethod);
	m_me.setSubpelRefine(cfg->param.subpelRefine);

	/* When frame parallelism is active, only 'refLagPixels' of reference frames will be guaranteed
	 * available for motion reference.  See refLagRows in FrameEncoder::compressCTURows() */
	m_refLagPixels = cfg->param.frameNumThreads > 1 ? cfg->param.searchRange : cfg->param.sourceHeight;

	// default to no adaptive range
	for (int dir = 0; dir < 2; dir++)
	{
		for (int ref = 0; ref < 33; ref++)
		{
			m_adaptiveRange[dir][ref] = cfg->param.searchRange;
		}
	}

	// initialize motion cost
	for (int num = 0; num < AMVP_MAX_NUM_CANDS + 1; num++)
	{
		for (int idx = 0; idx < AMVP_MAX_NUM_CANDS; idx++)
		{
			if (idx < num)
				m_mvpIdxCost[idx][num] = xGetMvpIdxBits(idx, num);
			else
				m_mvpIdxCost[idx][num] = MAX_INT;
		}
	}

	initTempBuff(cfg->param.internalCsp);

	m_tempPel = new Pel[g_maxCUWidth * g_maxCUHeight];

	const uint32_t numLayersToAllocate = cfg->getQuadtreeTULog2MaxSize() - cfg->getQuadtreeTULog2MinSize() + 1;
	m_qtTempCoeffY = new TCoeff*[numLayersToAllocate];
	m_qtTempCoeffCb = new TCoeff*[numLayersToAllocate];
	m_qtTempCoeffCr = new TCoeff*[numLayersToAllocate];

	const uint32_t numPartitions = 1 << (g_maxCUDepth << 1);
	m_qtTempTrIdx = new UChar[numPartitions];
	m_qtTempCbf[0] = new UChar[numPartitions];
	m_qtTempCbf[1] = new UChar[numPartitions];
	m_qtTempCbf[2] = new UChar[numPartitions];
	m_qtTempTComYuv = new TShortYUV[numLayersToAllocate];

#ifdef X265_INTRA_DEBUG
	m_rkIntraPred = new Rk_IntraPred;
#endif
#ifdef RK_INTRA_MODE_CHOOSE
	m_rkIntraPredFast = new Rk_IntraPred;
#endif
	m_hChromaShift = CHROMA_H_SHIFT(cfg->param.internalCsp);
	m_vChromaShift = CHROMA_V_SHIFT(cfg->param.internalCsp);

	for (uint32_t i = 0; i < numLayersToAllocate; ++i)
	{
		m_qtTempCoeffY[i] = new TCoeff[g_maxCUWidth * g_maxCUHeight];

		m_qtTempCoeffCb[i] = new TCoeff[(g_maxCUWidth >> m_hChromaShift) * (g_maxCUHeight >> m_vChromaShift)];
		m_qtTempCoeffCr[i] = new TCoeff[(g_maxCUWidth >> m_hChromaShift) * (g_maxCUHeight >> m_vChromaShift)];
		m_qtTempTComYuv[i].create(MAX_CU_SIZE, MAX_CU_SIZE, cfg->param.internalCsp);
	}

	m_sharedPredTransformSkip[0] = new Pel[MAX_TS_WIDTH * MAX_TS_HEIGHT];
	m_sharedPredTransformSkip[1] = new Pel[MAX_TS_WIDTH * MAX_TS_HEIGHT];
	m_sharedPredTransformSkip[2] = new Pel[MAX_TS_WIDTH * MAX_TS_HEIGHT];
	m_qtTempTUCoeffY = new TCoeff[MAX_TS_WIDTH * MAX_TS_HEIGHT];
	m_qtTempTUCoeffCb = new TCoeff[MAX_TS_WIDTH * MAX_TS_HEIGHT];
	m_qtTempTUCoeffCr = new TCoeff[MAX_TS_WIDTH * MAX_TS_HEIGHT];

	m_qtTempTransformSkipTComYuv.create(g_maxCUWidth, g_maxCUHeight, cfg->param.internalCsp);

	m_qtTempTransformSkipFlag[0] = new UChar[numPartitions];
	m_qtTempTransformSkipFlag[1] = new UChar[numPartitions];
	m_qtTempTransformSkipFlag[2] = new UChar[numPartitions];
	m_tmpYuvPred.create(MAX_CU_SIZE, MAX_CU_SIZE, cfg->param.internalCsp);
}

void TEncSearch::setQPLambda(int QP, double lambdaLuma, double lambdaChroma)
{
	m_trQuant->setLambda(lambdaLuma, lambdaChroma);
	m_me.setQP(QP);
}

void TEncSearch::xEncSubdivCbfQT(TComDataCU* cu, uint32_t trDepth, uint32_t absPartIdx, bool bLuma, bool bChroma)
{
	uint32_t fullDepth = cu->getDepth(0) + trDepth;
	uint32_t trMode = cu->getTransformIdx(absPartIdx);
	uint32_t subdiv = (trMode > trDepth ? 1 : 0);
	uint32_t trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth()] + 2 - fullDepth;

	if (cu->getPredictionMode(0) == MODE_INTRA && cu->getPartitionSize(0) == SIZE_NxN && trDepth == 0)
	{
		assert(subdiv);
	}
	else if (trSizeLog2 > cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize())
	{
		assert(subdiv);
	}
	else if (trSizeLog2 == cu->getSlice()->getSPS()->getQuadtreeTULog2MinSize())
	{
		assert(!subdiv);
	}
	else if (trSizeLog2 == cu->getQuadtreeTULog2MinSizeInCU(absPartIdx))
	{
		assert(!subdiv);
	}
	else
	{
		assert(trSizeLog2 > cu->getQuadtreeTULog2MinSizeInCU(absPartIdx));
		if (bLuma)
		{
			m_entropyCoder->encodeTransformSubdivFlag(subdiv, 5 - trSizeLog2);
		}
	}

	if (bChroma)
	{
		if (trSizeLog2 > 2)
		{
			if (trDepth == 0 || cu->getCbf(absPartIdx, TEXT_CHROMA_U, trDepth - 1))
				m_entropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_CHROMA_U, trDepth);
			if (trDepth == 0 || cu->getCbf(absPartIdx, TEXT_CHROMA_V, trDepth - 1))
				m_entropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_CHROMA_V, trDepth);
		}
	}

	if (subdiv)
	{
		uint32_t qtPartNum = cu->getPic()->getNumPartInCU() >> ((fullDepth + 1) << 1);
		for (uint32_t part = 0; part < 4; part++)
		{
			xEncSubdivCbfQT(cu, trDepth + 1, absPartIdx + part * qtPartNum, bLuma, bChroma);
		}

		return;
	}

	//===== Cbfs =====
	if (bLuma)
	{
		m_entropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_LUMA, trMode);
	}
}

void TEncSearch::xEncCoeffQT(TComDataCU* cu, uint32_t trDepth, uint32_t absPartIdx, TextType ttype)
{
	uint32_t fullDepth = cu->getDepth(0) + trDepth;
	uint32_t trMode = cu->getTransformIdx(absPartIdx);
	uint32_t subdiv = (trMode > trDepth ? 1 : 0);
	uint32_t trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth()] + 2 - fullDepth;
	uint32_t chroma = (ttype != TEXT_LUMA ? 1 : 0);

	if (subdiv)
	{
		uint32_t qtPartNum = cu->getPic()->getNumPartInCU() >> ((fullDepth + 1) << 1);
		for (uint32_t part = 0; part < 4; part++)
		{
			xEncCoeffQT(cu, trDepth + 1, absPartIdx + part * qtPartNum, ttype);
		}

		return;
	}

	if (ttype != TEXT_LUMA && trSizeLog2 == 2)
	{
		assert(trDepth > 0);
		trDepth--;
		uint32_t qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + trDepth) << 1);
		bool bFirstQ = ((absPartIdx % qpdiv) == 0);
		if (!bFirstQ)
		{
			return;
		}
	}

	//===== coefficients =====
	uint32_t width = cu->getWidth(0) >> (trDepth + chroma);
	uint32_t height = cu->getHeight(0) >> (trDepth + chroma);
	uint32_t coeffOffset = (cu->getPic()->getMinCUWidth() * cu->getPic()->getMinCUHeight() * absPartIdx) >> (chroma << 1);
	uint32_t qtLayer = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() - trSizeLog2;
	TCoeff* coeff = 0;
	switch (ttype)
	{
	case TEXT_LUMA:     coeff = m_qtTempCoeffY[qtLayer];
		break;
	case TEXT_CHROMA_U: coeff = m_qtTempCoeffCb[qtLayer];
		break;
	case TEXT_CHROMA_V: coeff = m_qtTempCoeffCr[qtLayer];
		break;
	default: assert(0);
	}

	coeff += coeffOffset;

	m_entropyCoder->encodeCoeffNxN(cu, coeff, absPartIdx, width, height, fullDepth, ttype);
}

void TEncSearch::xEncIntraHeader(TComDataCU* cu, uint32_t trDepth, uint32_t absPartIdx, bool bLuma, bool bChroma)
{
	if (bLuma)
	{
		// CU header
		if (absPartIdx == 0)
		{
			if (!cu->getSlice()->isIntra())
			{
				if (cu->getSlice()->getPPS()->getTransquantBypassEnableFlag())
				{
					m_entropyCoder->encodeCUTransquantBypassFlag(cu, 0, true);
				}
				m_entropyCoder->encodeSkipFlag(cu, 0, true);
				m_entropyCoder->encodePredMode(cu, 0, true);
			}

			m_entropyCoder->encodePartSize(cu, 0, cu->getDepth(0), true);

			if (cu->isIntra(0) && cu->getPartitionSize(0) == SIZE_2Nx2N)
			{
				m_entropyCoder->encodeIPCMInfo(cu, 0, true);

				if (cu->getIPCMFlag(0))
				{
					return;
				}
			}
		}
		// luma prediction mode
		if (cu->getPartitionSize(0) == SIZE_2Nx2N)
		{
			if (absPartIdx == 0)
			{
				m_entropyCoder->encodeIntraDirModeLuma(cu, 0);
			}
		}
		else
		{
			uint32_t qtNumParts = cu->getTotalNumPart() >> 2;
			if (trDepth == 0)
			{
				assert(absPartIdx == 0);
				for (uint32_t part = 0; part < 4; part++)
				{
					m_entropyCoder->encodeIntraDirModeLuma(cu, part * qtNumParts);
				}
			}
			else if ((absPartIdx % qtNumParts) == 0)
			{
				m_entropyCoder->encodeIntraDirModeLuma(cu, absPartIdx);
			}
		}
	}
	if (bChroma)
	{
		// chroma prediction mode
		if (absPartIdx == 0)
		{
			m_entropyCoder->encodeIntraDirModeChroma(cu, 0, true);
		}
	}
}

uint32_t TEncSearch::xGetIntraBitsQT(TComDataCU* cu, uint32_t trDepth, uint32_t absPartIdx, bool bLuma, bool bChroma)
{
	m_entropyCoder->resetBits();
	xEncIntraHeader(cu, trDepth, absPartIdx, bLuma, bChroma);
	xEncSubdivCbfQT(cu, trDepth, absPartIdx, bLuma, bChroma);

	if (bLuma)
	{
		xEncCoeffQT(cu, trDepth, absPartIdx, TEXT_LUMA);
	}
	if (bChroma)
	{
		xEncCoeffQT(cu, trDepth, absPartIdx, TEXT_CHROMA_U);
		xEncCoeffQT(cu, trDepth, absPartIdx, TEXT_CHROMA_V);
	}
	return m_entropyCoder->getNumberOfWrittenBits();
}

uint32_t TEncSearch::xGetIntraBitsQTChroma(TComDataCU* cu, uint32_t trDepth, uint32_t absPartIdx, uint32_t chromaId)
{
	m_entropyCoder->resetBits();
	if (chromaId == TEXT_CHROMA_U)
	{
		xEncCoeffQT(cu, trDepth, absPartIdx, TEXT_CHROMA_U);
	}
	else if (chromaId == TEXT_CHROMA_V)
	{
		xEncCoeffQT(cu, trDepth, absPartIdx, TEXT_CHROMA_V);
	}
	return m_entropyCoder->getNumberOfWrittenBits();
}

void TEncSearch::xIntraCodingLumaBlk(TComDataCU* cu,
	uint32_t    trDepth,
	uint32_t    absPartIdx,
	TComYuv*    fencYuv,
	TComYuv*    predYuv,
	TShortYUV*  resiYuv,
	uint32_t&   outDist,
	int         default0Save1Load2)
{
	uint32_t lumaPredMode = cu->getLumaIntraDir(absPartIdx);
	uint32_t fullDepth = cu->getDepth(0) + trDepth;
	uint32_t width = cu->getWidth(0) >> trDepth;
	uint32_t height = cu->getHeight(0) >> trDepth;
	uint32_t stride = fencYuv->getStride();
	Pel*     fenc = fencYuv->getLumaAddr(absPartIdx);
	Pel*     pred = predYuv->getLumaAddr(absPartIdx);
	int16_t* residual = resiYuv->getLumaAddr(absPartIdx);
	Pel*     recon = predYuv->getLumaAddr(absPartIdx);
	int      part = partitionFromSizes(width, height);

	uint32_t trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> fullDepth] + 2;
	uint32_t qtLayer = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() - trSizeLog2;
	uint32_t numCoeffPerInc = cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight() >> (cu->getSlice()->getSPS()->getMaxCUDepth() << 1);
	TCoeff*  coeff = m_qtTempCoeffY[qtLayer] + numCoeffPerInc * absPartIdx;

	int16_t* reconQt = m_qtTempTComYuv[qtLayer].getLumaAddr(absPartIdx);
	assert(m_qtTempTComYuv[qtLayer].m_width == MAX_CU_SIZE);

	uint32_t zorder = cu->getZorderIdxInCU() + absPartIdx;
	Pel*     reconIPred = cu->getPic()->getPicYuvRec()->getLumaAddr(cu->getAddr(), zorder);
	uint32_t reconIPredStride = cu->getPic()->getPicYuvRec()->getStride();
	bool     useTransformSkip = cu->getTransformSkip(absPartIdx, TEXT_LUMA);

	//===== init availability pattern =====

	if (default0Save1Load2 != 2)
	{
		cu->getPattern()->initPattern(cu, trDepth, absPartIdx);
		cu->getPattern()->initAdiPattern(cu, absPartIdx, trDepth, m_predBuf, m_predBufStride, m_predBufHeight, refAbove, refLeft, refAboveFlt, refLeftFlt);
		// 验证FillRefSamples
#ifdef X265_INTRA_DEBUG

		Pel* roiOrigin;
		Pel* adiTemp;
		uint32_t cuWidth_t = cu->getWidth(0) >> trDepth;
		uint32_t cuHeight_t = cu->getHeight(0) >> trDepth;
		uint32_t cuWidth2_t  = cuWidth_t << 1;
		uint32_t cuHeight2_t = cuHeight_t << 1;
		uint32_t width_t;
		uint32_t height_t;
		int  picStride = cu->getPic()->getStride();
		int  unitSize = 0;
		int  numUnitsInCU = 0;
		int  totalUnits = 0;
		bool bNeighborFlags[4 * MAX_NUM_SPU_W + 1];
		int  numIntraNeighbor = 0;

		uint32_t partIdxLT, partIdxRT, partIdxLB;

		cu->deriveLeftRightTopIdxAdi(partIdxLT, partIdxRT, absPartIdx, trDepth);
		cu->deriveLeftBottomIdxAdi(partIdxLB,              absPartIdx, trDepth);

		unitSize      = g_maxCUWidth >> g_maxCUDepth;
		numUnitsInCU  = cuWidth_t / unitSize;
		totalUnits    = (numUnitsInCU << 2) + 1;

		bNeighborFlags[numUnitsInCU * 2] = cu->getPattern()->isAboveLeftAvailable(cu, partIdxLT);
		numIntraNeighbor  += (int)(bNeighborFlags[numUnitsInCU * 2]);
		numIntraNeighbor  += cu->getPattern()->isAboveAvailable(cu, partIdxLT, partIdxRT, bNeighborFlags + (numUnitsInCU * 2) + 1);
		numIntraNeighbor  += cu->getPattern()->isAboveRightAvailable(cu, partIdxLT, partIdxRT, bNeighborFlags + (numUnitsInCU * 3) + 1);
		numIntraNeighbor  += cu->getPattern()->isLeftAvailable(cu, partIdxLT, partIdxLB, bNeighborFlags + (numUnitsInCU * 2) - 1);
		numIntraNeighbor  += cu->getPattern()->isBelowLeftAvailable(cu, partIdxLT, partIdxLB, bNeighborFlags + numUnitsInCU   - 1);

		width_t = cuWidth2_t + 1;
		height_t = cuHeight2_t + 1;

		if (((width_t << 2) > m_predBufStride) || ((height_t << 2) > m_predBufHeight))
		{
			RK_HEVC_PRINT("%s bad case!\n",__FUNCTION__);
			return;
		}

		roiOrigin = cu->getPic()->getPicYuvRec()->getLumaAddr(cu->getAddr(), cu->getZorderIdxInCU() + absPartIdx);
		adiTemp   = m_predBuf;

		m_rkIntraPred->fillReferenceSamples(roiOrigin,
			adiTemp,
			bNeighborFlags,
			numIntraNeighbor,
			unitSize,
			numUnitsInCU,
			totalUnits,
			cuWidth_t,
			cuHeight_t,
			width_t,
			height_t,
			picStride,
			trDepth);

		assert(width == height);

		if ( cu->getWidth(0) != SIZE_64x64)
		{
			if ( trDepth == 0 )
			{
				::memcpy(rk_Interface_Intra.fenc,fenc,width*height);
			}
			else if ( trDepth == 1 )
			{
				assert(width == 4);
				Pel* pdst = rk_Interface_Intra.fenc;
				Pel* psrc = fenc;
				for (int i = 0 ; i < 4 ; i++ )
				{
					::memcpy(pdst, psrc, width);
					pdst += width; // intra_proc中是4x4连续存储的
					psrc += stride;
				}
			}
			else
			{
				RK_HEVC_PRINT("%s do not support for RK_HEVC encoder.\n",__FUNCTION__);
			}

			// 正常由上层给，这里从 x265中抽取
			Pel* pTmp = rk_Interface_Intra.reconEdgePixel;
			Pel* pReconRef = roiOrigin - 1;
			for (int i = 0 ; i < cuHeight2_t ; i++ )
			{
				pTmp[cuHeight2_t - i - 1] = pReconRef[i*picStride];
			}
			pTmp += cuHeight2_t;
			pReconRef = roiOrigin - picStride - 1;
			for (int i = 0 ; i < cuWidth2_t + 1; i++ )
			{
				pTmp[i] = pReconRef[i];
			}

			::memcpy(rk_Interface_Intra.bNeighborFlags,bNeighborFlags,4 * MAX_NUM_SPU_W + 1);
			rk_Interface_Intra.numintraNeighbor = numIntraNeighbor;
			rk_Interface_Intra.size = width;
			rk_Interface_Intra.useStrongIntraSmoothing = cu->getSlice()->getSPS()->getUseStrongIntraSmoothing();

		}

		// generate filtered intra prediction samples
		// left and left above border + above and above right border + top left corner = length of 3. filter buffer
		//int bufSize = cuHeight2_t + cuWidth2_t + 1;
		uint32_t wh = ADI_BUF_STRIDE * height_t;         // number of elements in one buffer

		Pel* filteredBuf1 = m_predBuf + wh;         // 1. filter buffer
		Pel* filteredBuf2 = filteredBuf1 + wh; // 2. filter buffer
		Pel* filterBuf = filteredBuf2 + wh;    // buffer for 2. filtering (sequential)

		int l = 0;
		// left border from bottom to top
		for (int i = 0; i < cuHeight2_t; i++)
		{
			m_rkIntraPred->rk_LineBufTmp[l++] = adiTemp[ADI_BUF_STRIDE * (cuHeight2_t - i)];
		}

		// top left corner
		m_rkIntraPred->rk_LineBufTmp[l++] = adiTemp[0];

		// above border from left to right
		memcpy(&m_rkIntraPred->rk_LineBufTmp[l], &adiTemp[1], cuWidth2_t * sizeof(*filterBuf));

		/* 函数接口 输出 rk_LineBuf */
		m_rkIntraPred->RkIntrafillRefSamples(roiOrigin,
			bNeighborFlags,
			numIntraNeighbor,
			unitSize,
			numUnitsInCU,
			totalUnits,
			width,
			height,
			picStride,
			m_rkIntraPred->rk_LineBuf);

		// 对比 最后的 L buffer
		m_rkIntraPred->RkIntraFillRefSamplesCheck(refAbove + width - 1,refLeft + height - 1,
			cuWidth_t, cuHeight_t);

#endif
		// 验证smoothing
#ifdef X265_INTRA_DEBUG
		//RK_HEVC_PRINT("TU width x height = %d x %d \r", width, height);
		// 指针 refAbove, refLeft, refAboveFlt, refLeftFlt
		// 分别存储了滤波前后的数据，注意左上角的点是复用了横纵2个方向
		// 存储了2次，且起始数据都是 左上角数据，一个往下，而不是往上(left)，一个往右(above)
		// 不是标准的 L 型，此外有效数据需要 地址偏移 width - 1，
		// 这么做的目的是后面角度预测需要扩展参考边数据
		::memcpy(&m_rkIntraPred->rk_IntraSmoothIn.rk_refAbove, refAbove + width - 1  ,  2*width + 1);
		::memcpy(&m_rkIntraPred->rk_IntraSmoothIn.rk_refLeft, refLeft + height - 1, 2*height + 1);
		::memcpy(&m_rkIntraPred->rk_IntraSmoothIn.rk_refAboveFiltered[X265_COMPENT], refAboveFlt  + width - 1,  2*width + 1);
		::memcpy(&m_rkIntraPred->rk_IntraSmoothIn.rk_refLeftFiltered[X265_COMPENT], refLeftFlt + height - 1, 2*height + 1);

		// do smoothing by youself
		m_rkIntraPred->rk_bUseStrongIntraSmoothing = cu->getSlice()->getSPS()->getUseStrongIntraSmoothing();
		m_rkIntraPred->rk_puWidth = width;
		m_rkIntraPred->rk_puHeight = height;
		Pel rk_refLeft[129],rk_refAbove[129],rk_refLeftFlt[129],rk_refAboveFlt[129];
		m_rkIntraPred->RkIntraSmoothing(rk_refLeft + width - 1,
			rk_refAbove + width - 1,
			rk_refLeftFlt + width - 1,
			rk_refAboveFlt + width - 1);
		// compare the filter result
		m_rkIntraPred->RkIntraSmoothingCheck();

#endif

		//===== get prediction signal =====
		predIntraLumaAng(lumaPredMode, pred, stride, width);

		// 验证35种预测模式
#ifdef X265_INTRA_DEBUG
		if ( cu->getWidth(0) != SIZE_64x64)
		{
			assert(width < 64);
			UChar RK_intraFilterThreshold[5] =
			{
				10, //4x4
				7,  //8x8
				1,  //16x16
				0,  //32x32
				10, //64x64
			};
			int log2BlkSize = g_convertToBit[width] + 2;

			int mode_idx;
			for ( mode_idx = 0 ; mode_idx < 35 ; mode_idx++ )
			{

				//int dirMode = lumaPredMode;
				int dirMode = mode_idx;
				int diff = std::min<int>(abs((int)dirMode - HOR_IDX), abs((int)dirMode - VER_IDX));
				UChar filterIdx = diff > RK_intraFilterThreshold[log2BlkSize - 2] ? 1 : 0;
				if (dirMode == DC_IDX)
				{
					filterIdx = 0; //no smoothing for DC or LM chroma
				}
				assert(filterIdx <= 1);

				Pel *refLft, *refAbv;
				refLft = refLeft + width - 1 ;
				refAbv = refAbove + width - 1;

				if (filterIdx)
				{
					refLft = refLeftFlt + width - 1;
					refAbv = refAboveFlt + width - 1;
				}

				//			RK_HEVC_PRINT("dirMode = %d \t", dirMode);
				//			RK_HEVC_PRINT("stride = %d, puSize  = %d x %d \n",stride, width,width);

				bool bFilter = width <= 16 && dirMode != PLANAR_IDX;

				primitives.intra_pred[log2BlkSize - 2][dirMode](
					m_rkIntraPred->rk_IntraPred_35.rk_predSampleOrg,
					stride,
					refLft,
					refAbv,
					dirMode,
					bFilter);

				// 采用RK作为输入
				refLft = rk_refLeft + width - 1 ;
				refAbv = rk_refAbove + width - 1;

				if (filterIdx)
				{
					refLft = rk_refLeftFlt + width - 1;
					refAbv = rk_refAboveFlt + width - 1;
				}

				m_rkIntraPred->RkIntraPredAll(m_rkIntraPred->rk_IntraPred_35.rk_predSample,
					refAbv ,
					refLft ,
					stride ,
					log2BlkSize,
					0, //(0=Y, 1=Cb, 2=Cr)
					dirMode);
				// 存储 35 种预测数据
				if ( trDepth == 0 )
				{
					::memcpy(m_rkIntraPred->rk_IntraPred_35.rk_predSampleTmp[dirMode],
						m_rkIntraPred->rk_IntraPred_35.rk_predSample,width*height);
				}
				else if ( trDepth == 1 )
				{
					assert(width == 4);
					Pel* pdst = m_rkIntraPred->rk_IntraPred_35.rk_predSampleTmp[dirMode];
					Pel* psrc = m_rkIntraPred->rk_IntraPred_35.rk_predSample;
					for (int i = 0 ; i < 4 ; i++ )
					{
						::memcpy(pdst, psrc, width);
						pdst += width; // intra_proc中是4x4连续存储的
						psrc += stride;
					}
				}
				else
				{
					RK_HEVC_PRINT("%s do not support for RK_HEVC encoder.\n",__FUNCTION__);
				}
#if 0
				// 计算 35 种 cost
				pixelcmp_t sa8d = primitives.sa8d[g_convertToBit[width]];
				//  获取 square的 SAD，
				// 			4x4 8x8 16x16 32x32 64x64
				//	org		0	 1	  2     3     4
				// 	correct	0    1    4     11    18
				int squareBlockIdx[5] = {0,1,4,11,18};
				pixelcmp_t sad = primitives.sad[squareBlockIdx[g_convertToBit[width]]];


				modeCosts[dirMode] 		= sa8d(fenc, stride,
					m_rkIntraPred->rk_IntraPred_35.rk_predSample, stride);

				modeCosts_SAD[dirMode] 	= sad(fenc, stride,
					m_rkIntraPred->rk_IntraPred_35.rk_predSample, stride);
#if 1
				uint32_t sad_value = modeCosts_SAD[dirMode];
#else
				uint32_t sad = modeCosts[dirMode];
#endif
				uint32_t bits = xModeBitsIntra(cu, dirMode, absPartIdx, cu->getDepth(0), trDepth);

				uint64_t cost = m_rdCost->calcRdSADCost(sad_value, bits);

				m_rkIntraPred->rk_modeCostsSadAndCabac[dirMode] = cost;
#endif
				//
				m_rkIntraPred->rk_puwidth_35 = width;
				m_rkIntraPred->rk_dirMode = dirMode;
				m_rkIntraPred->RkIntraPred_angularCheck();

			}

		}

#endif

		// save prediction
		if (default0Save1Load2 == 1)
		{
			primitives.luma_copy_pp[part](m_sharedPredTransformSkip[0], width, pred, stride);
		}
	}
	else
	{
		primitives.luma_copy_pp[part](pred, stride, m_sharedPredTransformSkip[0], width);
	}

	//===== get residual signal =====
	assert(!((uint32_t)(size_t)fenc & (width - 1)));
	assert(!((uint32_t)(size_t)pred & (width - 1)));
	assert(!((uint32_t)(size_t)residual & (width - 1)));
	primitives.calcresidual[(int)g_convertToBit[width]](fenc, pred, residual, stride);

#ifdef X265_INTRA_DEBUG
	if ( cu->getWidth(0) != SIZE_64x64)
	{
		//RK_HEVC_PRINT("0x%016x\n",(uint64_t)residual);
		//residual 在下面会被重构的更新，需要进行暂存
		// 拷贝32x32大小的块
		if ( trDepth == 0 )
		{
			::memcpy(m_rkIntraPred->rk_residual[X265_COMPENT],residual, 32*32*sizeof(uint16_t));
		}
		else if ( trDepth == 1 )
		{
			assert(width == 4);
			int16_t* pdst = m_rkIntraPred->rk_residual[X265_COMPENT];
			int16_t* psrc = residual;
			for (int i = 0 ; i < 4 ; i++ )
			{
				::memcpy(pdst, psrc, width*sizeof(int16_t));
				pdst += width; // intra_proc中是4x4连续存储的
				psrc += stride;
			}
		}
		else
		{
			RK_HEVC_PRINT("%s do not support for RK_HEVC encoder.\n",__FUNCTION__);
		}
	}
#endif
	//===== transform and quantization =====
	//--- init rate estimation arrays for RDOQ ---
	if (useTransformSkip ? m_cfg->bEnableRDOQTS : m_cfg->bEnableRDOQ)
	{
		m_entropyCoder->estimateBit(m_trQuant->m_estBitsSbac, width, width, TEXT_LUMA);
	}


	//--- transform and quantization ---
	uint32_t absSum = 0;
	int lastPos = -1;
	cu->setTrIdxSubParts(trDepth, absPartIdx, fullDepth);

	m_trQuant->setQPforQuant(cu->getQP(0), TEXT_LUMA, cu->getSlice()->getSPS()->getQpBDOffsetY(), 0);
	m_trQuant->selectLambda(TEXT_LUMA);
#if TQ_RUN_IN_X265_INTRA
	// get input
	if (cu->getSlice()->getSliceType() == B_SLICE) m_trQuant->m_hevcQT->m_infoFromX265->sliceType = 0; // B
	if (cu->getSlice()->getSliceType() == P_SLICE) m_trQuant->m_hevcQT->m_infoFromX265->sliceType = 1; // P
	if (cu->getSlice()->getSliceType() == I_SLICE) m_trQuant->m_hevcQT->m_infoFromX265->sliceType = 2; // I
	m_trQuant->m_hevcQT->m_infoFromX265->textType = 0; // Luma
	m_trQuant->m_hevcQT->m_infoFromX265->qpBdOffset = cu->getSlice()->getSPS()->getQpBDOffsetY();
	m_trQuant->m_hevcQT->m_infoFromX265->chromaQPOffset = cu->getSlice()->getSPS()->getQpBDOffsetC();
	m_trQuant->m_hevcQT->m_infoFromX265->size = (uint8_t)width;
	assert(width <= 32);
	m_trQuant->m_hevcQT->m_infoFromX265->qp = cu->getQP(0);
	m_trQuant->m_hevcQT->m_infoFromX265->transformSkip = useTransformSkip;

	// copy input residual, TU size in effect
	for (uint32_t k = 0; k < width; k++)
	{
		memcpy(&(m_trQuant->m_hevcQT->m_infoFromX265->inResi[k*CTU_SIZE]), &(residual[k*stride]), width*sizeof(int16_t));
	}
	if (cu->getPredictionMode(absPartIdx) == MODE_INTER)	 m_trQuant->m_hevcQT->m_infoFromX265->predMode = 1;
	if (cu->getPredictionMode(absPartIdx) == MODE_INTRA)	 m_trQuant->m_hevcQT->m_infoFromX265->predMode = 0;


	m_trQuant->m_hevcQT->m_infoFromX265->ctuWidth = cu->getWidth(0);
#endif

	absSum = m_trQuant->transformNxN(cu, residual, stride, coeff, width, height, TEXT_LUMA, absPartIdx, &lastPos, useTransformSkip);
#if TQ_RUN_IN_X265_INTRA
	// get output
	memcpy(m_trQuant->m_hevcQT->m_infoFromX265->coeffTQ, coeff, width*width*sizeof(int32_t));
#endif
	//--- set coded block flag ---
	cu->setCbfSubParts((absSum ? 1 : 0) << trDepth, TEXT_LUMA, absPartIdx, fullDepth);

	//--- inverse transform ---
	int size = g_convertToBit[width];
	if (absSum)
	{
		int scalingListType = 0 + g_eTTable[(int)TEXT_LUMA];
		assert(scalingListType < 6);
		m_trQuant->invtransformNxN(cu->getCUTransquantBypass(absPartIdx), cu->getLumaIntraDir(absPartIdx), residual, stride, coeff, width, height, scalingListType, useTransformSkip, lastPos);
	}
	else
	{
		int16_t* resiTmp = residual;
		memset(coeff, 0, sizeof(TCoeff)* width * height);
		primitives.blockfill_s[size](resiTmp, stride, 0);
	}

	//===== reconstruction =====
	assert(width <= 32);
	primitives.calcrecon[size](pred, residual, recon, reconQt, reconIPred, stride, MAX_CU_SIZE, reconIPredStride);

	//===== update distortion =====
	outDist += primitives.sse_pp[part](fenc, stride, recon, stride);

#if TQ_RUN_IN_X265_INTRA
	// get output
	// copy output residual, TU size in effect
	for (uint32_t k = 0; k < width; k++)
	{
		memcpy(&(m_trQuant->m_hevcQT->m_infoFromX265->outResi[k*CTU_SIZE]), &(residual[k*stride]), width*sizeof(int16_t));
	}

	m_trQuant->m_hevcQT->m_infoFromX265->absSum = absSum;
	m_trQuant->m_hevcQT->m_infoFromX265->lastPos = lastPos;

	assert(m_trQuant->m_hevcQT->m_infoFromX265->size <= 32);
	m_trQuant->m_hevcQT->proc(0); // QT proc for intra in x265
#endif

#ifdef X265_INTRA_DEBUG
	if ( cu->getWidth(0) != SIZE_64x64)
	{
		if ( trDepth == 0 )
		{
			::memcpy(m_rkIntraPred->rk_recon[X265_COMPENT],recon, 32*32*sizeof(uint8_t));
		}
		else if ( trDepth == 1 )
		{
			assert(width == 4);
			uint8_t* pdst = m_rkIntraPred->rk_recon[X265_COMPENT];
			uint8_t* psrc = recon;
			for (int i = 0 ; i < 4 ; i++ )
			{
				::memcpy(pdst, psrc, width*sizeof(uint8_t));
				pdst += width; // intra_proc中是4x4连续存储的
				psrc += stride;
			}

			// save 4x4 recon edge
			m_rkIntraPred->RK_store4x4Recon2Ref(&g_4x4_single_reconEdge[0][0], m_rkIntraPred->rk_recon[X265_COMPENT], absPartIdx);
			::memcpy(&g_4x4_total_reconEdge[cu->getZorderIdxInCU()/4][0][0],g_4x4_single_reconEdge,16);
		}
		else
		{
			RK_HEVC_PRINT("%s do not support for RK_HEVC encoder.\n",__FUNCTION__);
		}
	}
#endif

}

void TEncSearch::xIntraCodingChromaBlk(TComDataCU* cu,
	uint32_t    trDepth,
	uint32_t    absPartIdx,
	TComYuv*    fencYuv,
	TComYuv*    predYuv,
	TShortYUV*  resiYuv,
	uint32_t&   outDist,
	uint32_t    chromaId,
	int         default0Save1Load2)
{
	uint32_t origTrDepth = trDepth;
	uint32_t fullDepth = cu->getDepth(0) + trDepth;
	uint32_t trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> fullDepth] + 2;

	if (trSizeLog2 == 2)
	{
		assert(trDepth > 0);
		trDepth--;
		uint32_t qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + trDepth) << 1);
		bool bFirstQ = ((absPartIdx % qpdiv) == 0);
		if (!bFirstQ)
		{
			return;
		}
	}

	TextType ttype = (chromaId > 0 ? TEXT_CHROMA_V : TEXT_CHROMA_U);
	uint32_t chromaPredMode = cu->getChromaIntraDir(absPartIdx);
	uint32_t width = cu->getWidth(0) >> (trDepth + m_hChromaShift);
	uint32_t height = cu->getHeight(0) >> (trDepth + m_vChromaShift);
	uint32_t stride = fencYuv->getCStride();
	Pel*     fenc = (chromaId > 0 ? fencYuv->getCrAddr(absPartIdx) : fencYuv->getCbAddr(absPartIdx));
	Pel*     pred = (chromaId > 0 ? predYuv->getCrAddr(absPartIdx) : predYuv->getCbAddr(absPartIdx));
	int16_t* residual = (chromaId > 0 ? resiYuv->getCrAddr(absPartIdx) : resiYuv->getCbAddr(absPartIdx));
	Pel*     recon = (chromaId > 0 ? predYuv->getCrAddr(absPartIdx) : predYuv->getCbAddr(absPartIdx));

	uint32_t qtlayer = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() - trSizeLog2;
	uint32_t numCoeffPerInc = (cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight() >> (cu->getSlice()->getSPS()->getMaxCUDepth() << 1)) >> 2;
	TCoeff*  coeff = (chromaId > 0 ? m_qtTempCoeffCr[qtlayer] : m_qtTempCoeffCb[qtlayer]) + numCoeffPerInc * absPartIdx;
	int16_t* reconQt = (chromaId > 0 ? m_qtTempTComYuv[qtlayer].getCrAddr(absPartIdx) : m_qtTempTComYuv[qtlayer].getCbAddr(absPartIdx));
	assert(m_qtTempTComYuv[qtlayer].m_cwidth == MAX_CU_SIZE / 2);

	uint32_t zorder = cu->getZorderIdxInCU() + absPartIdx;
	Pel*     reconIPred = (chromaId > 0 ? cu->getPic()->getPicYuvRec()->getCrAddr(cu->getAddr(), zorder) : cu->getPic()->getPicYuvRec()->getCbAddr(cu->getAddr(), zorder));
	uint32_t reconIPredStride = cu->getPic()->getPicYuvRec()->getCStride();
	bool     useTransformSkipChroma = cu->getTransformSkip(absPartIdx, ttype);
	int      part = partitionFromSizes(width, height);

	//===== update chroma mode =====
	if (chromaPredMode == DM_CHROMA_IDX)
	{
		chromaPredMode = cu->getLumaIntraDir(0);
	}

	//===== init availability pattern =====
	if (default0Save1Load2 != 2)
	{
		cu->getPattern()->initPattern(cu, trDepth, absPartIdx);

		cu->getPattern()->initAdiPatternChroma(cu, absPartIdx, trDepth, m_predBuf, m_predBufStride, m_predBufHeight);
		Pel* chromaPred = (chromaId > 0 ? cu->getPattern()->getAdiCrBuf(width, height, m_predBuf) : cu->getPattern()->getAdiCbBuf(width, height, m_predBuf));

		// 验证FillRefSamples
#ifdef X265_INTRA_DEBUG

		Pel* roiOrigin;
		uint32_t cuWidth_t = cu->getWidth(0) >> trDepth;
		uint32_t cuHeight_t = cu->getHeight(0) >> trDepth;

		uint32_t width_t;
		uint32_t height_t;
		int  picStride = cu->getPic()->getCStride();
		int  unitSize = 0;
		int  numUnitsInCU = 0;
		int  totalUnits = 0;
		bool bNeighborFlags[4 * MAX_NUM_SPU_W + 1];
		int  numIntraNeighbor = 0;

		uint32_t partIdxLT, partIdxRT, partIdxLB;

		cu->deriveLeftRightTopIdxAdi(partIdxLT, partIdxRT, absPartIdx, trDepth);
		cu->deriveLeftBottomIdxAdi(partIdxLB,              absPartIdx, trDepth);

		unitSize      = (g_maxCUWidth >> g_maxCUDepth) >> cu->getHorzChromaShift(); // for chroma
		numUnitsInCU  = (cuWidth_t / unitSize) >> cu->getHorzChromaShift();
		totalUnits    = (numUnitsInCU << 2) + 1;

		bNeighborFlags[numUnitsInCU * 2] = cu->getPattern()->isAboveLeftAvailable(cu, partIdxLT);
		numIntraNeighbor  += (int)(bNeighborFlags[numUnitsInCU * 2]);
		numIntraNeighbor  += cu->getPattern()->isAboveAvailable(cu, partIdxLT, partIdxRT, bNeighborFlags + (numUnitsInCU * 2) + 1);
		numIntraNeighbor  += cu->getPattern()->isAboveRightAvailable(cu, partIdxLT, partIdxRT, bNeighborFlags + (numUnitsInCU * 3) + 1);
		numIntraNeighbor  += cu->getPattern()->isLeftAvailable(cu, partIdxLT, partIdxLB, bNeighborFlags + (numUnitsInCU * 2) - 1);
		numIntraNeighbor  += cu->getPattern()->isBelowLeftAvailable(cu, partIdxLT, partIdxLB, bNeighborFlags + numUnitsInCU   - 1);

		cuWidth_t = cuWidth_t >> cu->getHorzChromaShift(); // for chroma
		cuHeight_t = cuHeight_t >> cu->getVertChromaShift(); // for chroma


		width_t = cuWidth_t*2 + 1;
		height_t = cuHeight_t*2 + 1;
		if ((4 * width_t > m_predBufStride) || (4 * height > m_predBufStride))
		{
			RK_HEVC_PRINT("%s bad case!\n",__FUNCTION__);
			return;
		}

		// get Cb pattern
		roiOrigin = chromaId > 0 ?
			cu->getPic()->getPicYuvRec()->getCrAddr(cu->getAddr(), cu->getZorderIdxInCU() + absPartIdx) :
			cu->getPic()->getPicYuvRec()->getCbAddr(cu->getAddr(), cu->getZorderIdxInCU() + absPartIdx);

		if( cu->getWidth(0) != SIZE_64x64)
		{

			if ( trDepth == 0 )
			{
				uint8_t* pfenc = chromaId > 0 ? rk_Interface_IntraCr.fenc : rk_Interface_IntraCb.fenc;
				::memcpy(pfenc, fenc, width*height);
			}
			else
			{
				RK_HEVC_PRINT("%s do not support for RK_HEVC encoder.\n",__FUNCTION__);
			}

			// 正常由上层给，这里从 x265中抽取
			Pel* pTmp = chromaId > 0 ? rk_Interface_IntraCr.reconEdgePixel : rk_Interface_IntraCb.reconEdgePixel;
			Pel* pReconRef = roiOrigin - 1;
			for (int i = 0 ; i < cuHeight_t*2 ; i++ )
			{
				pTmp[cuHeight_t*2 - i - 1] = pReconRef[i*picStride];
			}
			pTmp += cuHeight_t*2;
			pReconRef = roiOrigin - picStride - 1;
			for (int i = 0 ; i < cuWidth_t*2 + 1; i++ )
			{
				pTmp[i] = pReconRef[i];
			}
			if ( chromaId > 0 )
			{
				::memcpy(rk_Interface_IntraCr.bNeighborFlags,bNeighborFlags,4 * MAX_NUM_SPU_W + 1);
				rk_Interface_IntraCr.numintraNeighbor = numIntraNeighbor;
				rk_Interface_IntraCr.size = width;
				rk_Interface_IntraCr.lumaDirMode = chromaPredMode;
			}
			else
			{
				::memcpy(rk_Interface_IntraCb.bNeighborFlags,bNeighborFlags,4 * MAX_NUM_SPU_W + 1);
				rk_Interface_IntraCb.numintraNeighbor = numIntraNeighbor;
				rk_Interface_IntraCb.size = width;
				rk_Interface_IntraCb.lumaDirMode = chromaPredMode;
			}

		}


		assert(width == height);

		// 从 chromaPred 中提取 refAbv 和 refLft
		// chromaPred 选择 Cb or Cr
		// Create the prediction
		Pel refAbv[3 * MAX_CU_SIZE];
		Pel refLft[3 * MAX_CU_SIZE];
		//int limit = (chromaPredMode <= 25 && chromaPredMode >= 11) ? (width + 1 + 1) : (2 * width + 1);
		int limit = 2 * width + 1;

		memcpy(refAbv + width - 1, chromaPred, (limit) * sizeof(Pel));
		for (int k = 0; k < limit; k++)
		{
			refLft[k + width - 1] = chromaPred[k * ADI_BUF_STRIDE];
		}

		Pel* pLineBuf = chromaId > 0 ? m_rkIntraPred->rk_LineBufCr : m_rkIntraPred->rk_LineBufCb;

		/* 函数接口 输出在 rk_LineBuf*/
		m_rkIntraPred->RkIntrafillRefSamples(roiOrigin,
			bNeighborFlags,
			numIntraNeighbor,
			unitSize,
			numUnitsInCU,
			totalUnits,
			width,
			height,
			picStride,
			pLineBuf);

		// 对比 最后的 L buffer
		m_rkIntraPred->RkIntraFillChromaCheck(pLineBuf,
			refAbv + width - 1,
			refLft + height - 1,
			cuWidth_t, cuHeight_t);



#endif


		//===== get prediction signal =====
		predIntraChromaAng(chromaPred, chromaPredMode, pred, stride, width);
#ifdef X265_INTRA_DEBUG
		// 存储 35 种预测数据
		if( cu->getWidth(0) != SIZE_64x64)
		{
			Pel* pPredSample = chromaId > 0 ? m_rkIntraPred->rk_IntraPred_35.rk_predSampleCr : m_rkIntraPred->rk_IntraPred_35.rk_predSampleCb;
			::memcpy(pPredSample, pred, width*height);
		}
#endif
		// save prediction
		if (default0Save1Load2 == 1)
		{
			Pel* predbuf = m_sharedPredTransformSkip[1 + chromaId];
			primitives.luma_copy_pp[part](predbuf, width, pred, stride);
		}
	}
	else
	{
		// load prediction
		Pel* predbuf = m_sharedPredTransformSkip[1 + chromaId];
		primitives.luma_copy_pp[part](pred, stride, predbuf, width);
	}

	//===== get residual signal =====
	assert(!((uint32_t)(size_t)fenc & (width - 1)));
	assert(!((uint32_t)(size_t)pred & (width - 1)));
	assert(!((uint32_t)(size_t)residual & (width - 1)));
	int size = g_convertToBit[width];
	primitives.calcresidual[size](fenc, pred, residual, stride);
#ifdef X265_INTRA_DEBUG
	if( cu->getWidth(0) != SIZE_64x64)
	{
		//RK_HEVC_PRINT("0x%016x\n",(uint64_t)residual);
		//residual 在下面会被重构的更新，需要进行暂存
		// 拷贝16x16大小的块
		int16_t* pResi = chromaId > 0 ? m_rkIntraPred->rk_residualCr[X265_COMPENT]: m_rkIntraPred->rk_residualCb[X265_COMPENT];
		if ( trDepth == 0 )
		{
			::memcpy(pResi,residual, 16*16*sizeof(uint16_t));
		}
		else
		{
			RK_HEVC_PRINT("%s do not support for RK_HEVC encoder.\n",__FUNCTION__);
		}
	}
#endif


	//===== transform and quantization =====
	{
		//--- init rate estimation arrays for RDOQ ---
		if (useTransformSkipChroma ? m_cfg->bEnableRDOQTS : m_cfg->bEnableRDOQ)
		{
			m_entropyCoder->estimateBit(m_trQuant->m_estBitsSbac, width, width, ttype);
		}
		//--- transform and quantization ---
		uint32_t absSum = 0;
		int lastPos = -1;

		int curChromaQpOffset;
		if (ttype == TEXT_CHROMA_U)
		{
			curChromaQpOffset = cu->getSlice()->getPPS()->getChromaCbQpOffset() + cu->getSlice()->getSliceQpDeltaCb();
		}
		else
		{
			curChromaQpOffset = cu->getSlice()->getPPS()->getChromaCrQpOffset() + cu->getSlice()->getSliceQpDeltaCr();
		}
		m_trQuant->setQPforQuant(cu->getQP(0), TEXT_CHROMA, cu->getSlice()->getSPS()->getQpBDOffsetC(), curChromaQpOffset);

		m_trQuant->selectLambda(TEXT_CHROMA);

#if TQ_RUN_IN_X265_INTRA
		// get input
		if (cu->getSlice()->getSliceType() == B_SLICE) m_trQuant->m_hevcQT->m_infoFromX265->sliceType = 0; // B
		if (cu->getSlice()->getSliceType() == P_SLICE) m_trQuant->m_hevcQT->m_infoFromX265->sliceType = 1; // P
		if (cu->getSlice()->getSliceType() == I_SLICE) m_trQuant->m_hevcQT->m_infoFromX265->sliceType = 2; // I
		m_trQuant->m_hevcQT->m_infoFromX265->textType = 1; //Chroma
		m_trQuant->m_hevcQT->m_infoFromX265->qpBdOffset = cu->getSlice()->getSPS()->getQpBDOffsetY();
		m_trQuant->m_hevcQT->m_infoFromX265->chromaQPOffset = cu->getSlice()->getSPS()->getQpBDOffsetC();
		m_trQuant->m_hevcQT->m_infoFromX265->size = (uint8_t)width;
		m_trQuant->m_hevcQT->m_infoFromX265->qp = cu->getQP(0);
		m_trQuant->m_hevcQT->m_infoFromX265->transformSkip = useTransformSkipChroma;

		// copy input residual, TU size in effect
		for (uint32_t k = 0; k < width; k++)
		{
			memcpy(&(m_trQuant->m_hevcQT->m_infoFromX265->inResi[k*CTU_SIZE]), &(residual[k*stride]), width*sizeof(int16_t));
		}
		if (cu->getPredictionMode(absPartIdx) == MODE_INTER)	 m_trQuant->m_hevcQT->m_infoFromX265->predMode = 1;
		if (cu->getPredictionMode(absPartIdx) == MODE_INTRA)	 m_trQuant->m_hevcQT->m_infoFromX265->predMode = 0;

		m_trQuant->m_hevcQT->m_infoFromX265->ctuWidth = cu->getWidth(0);
#endif

		absSum = m_trQuant->transformNxN(cu, residual, stride, coeff, width, height, ttype, absPartIdx, &lastPos, useTransformSkipChroma);
#if TQ_RUN_IN_X265_INTRA
		memcpy(m_trQuant->m_hevcQT->m_infoFromX265->coeffTQ, coeff, width*width*sizeof(int32_t));
#endif
		//--- set coded block flag ---
		cu->setCbfSubParts((absSum ? 1 : 0) << origTrDepth, ttype, absPartIdx, cu->getDepth(0) + trDepth);

		//--- inverse transform ---
		if (absSum)
		{
			int scalingListType = 0 + g_eTTable[(int)ttype];
			assert(scalingListType < 6);
			m_trQuant->invtransformNxN(cu->getCUTransquantBypass(absPartIdx), REG_DCT, residual, stride, coeff, width, height, scalingListType, useTransformSkipChroma, lastPos);
		}
		else
		{
			int16_t* resiTmp = residual;
			memset(coeff, 0, sizeof(TCoeff)* width * height);
			primitives.blockfill_s[size](resiTmp, stride, 0);
		}
#if TQ_RUN_IN_X265_INTRA
		// get output
		// copy output residual, TU size in effect
		for (uint32_t k = 0; k < width; k++)
		{
			memcpy(&(m_trQuant->m_hevcQT->m_infoFromX265->outResi[k*CTU_SIZE]), &(residual[k*stride]), width*sizeof(int16_t));
		}

		m_trQuant->m_hevcQT->m_infoFromX265->absSum = absSum;
		m_trQuant->m_hevcQT->m_infoFromX265->lastPos = lastPos;

		m_trQuant->m_hevcQT->proc(0);// QT proc for intra in x265
#endif
	}

	//===== reconstruction =====
	assert(((uint32_t)(size_t)residual & (width - 1)) == 0);
	assert(width <= 32);
	primitives.calcrecon[size](pred, residual, recon, reconQt, reconIPred, stride, MAX_CU_SIZE / 2, reconIPredStride);

	//===== update distortion =====
	uint32_t dist = primitives.sse_pp[part](fenc, stride, recon, stride);
	if (ttype == TEXT_CHROMA_U)
	{
		outDist += m_rdCost->scaleChromaDistCb(dist);
	}
	else if (ttype == TEXT_CHROMA_V)
	{
		outDist += m_rdCost->scaleChromaDistCr(dist);
	}
	else
	{
		outDist += dist;
	}
}

void TEncSearch::xRecurIntraCodingQT(TComDataCU* cu,
	uint32_t    trDepth,
	uint32_t    absPartIdx,
	bool        bLumaOnly,
	TComYuv*    fencYuv,
	TComYuv*    predYuv,
	TShortYUV*  resiYuv,
	uint32_t&   outDistY,
	uint32_t&   outDistC,
	bool        bCheckFirst,
	uint64_t&   rdCost)
{
	uint32_t fullDepth = cu->getDepth(0) + trDepth;
	uint32_t trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> fullDepth] + 2;
	bool     bCheckFull = (trSizeLog2 <= cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize());
	bool     bCheckSplit = (trSizeLog2 > cu->getQuadtreeTULog2MinSizeInCU(absPartIdx));

	int maxTuSize = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize();
	int isIntraSlice = (cu->getSlice()->getSliceType() == I_SLICE);

	// don't check split if TU size is less or equal to max TU size
	bool noSplitIntraMaxTuSize = bCheckFull;

	if (m_cfg->param.rdPenalty && !isIntraSlice)
	{
		// in addition don't check split if TU size is less or equal to 16x16 TU size for non-intra slice
		noSplitIntraMaxTuSize = (trSizeLog2 <= X265_MIN(maxTuSize, 4));

		// if maximum RD-penalty don't check TU size 32x32
		if (m_cfg->param.rdPenalty == 2)
		{
			bCheckFull = (trSizeLog2 <= X265_MIN(maxTuSize, 4));
		}
	}
	if (bCheckFirst && noSplitIntraMaxTuSize)
	{
		bCheckSplit = false;
	}

	if ((cu->getWidth(0) >> trDepth == 4) && (trDepth == 1))
	{
		assert(bCheckSplit == false);
	}

	uint64_t singleCost = MAX_INT64;
	uint32_t singleDistY = 0;
	uint32_t singleDistC = 0;
	uint32_t singleCbfY = 0;
	uint32_t singleCbfU = 0;
	uint32_t singleCbfV = 0;
	bool   checkTransformSkip = cu->getSlice()->getPPS()->getUseTransformSkip();
	uint32_t widthTransformSkip = cu->getWidth(0) >> trDepth;
	uint32_t heightTransformSkip = cu->getHeight(0) >> trDepth;
	int    bestModeId = 0;
	int    bestModeIdUV[2] = { 0, 0 };

	checkTransformSkip &= (widthTransformSkip == 4 && heightTransformSkip == 4);
	checkTransformSkip &= (!cu->getCUTransquantBypass(0));
	checkTransformSkip &= (!((cu->getQP(0) == 0) && (cu->getSlice()->getSPS()->getUseLossless())));
	if (m_cfg->param.bEnableTSkipFast)
	{
		checkTransformSkip &= (cu->getPartitionSize(absPartIdx) == SIZE_NxN);
	}

	if (bCheckFull)
	{
		if (checkTransformSkip == true)
		{
			//----- store original entropy coding status -----
			m_rdGoOnSbacCoder->store(m_rdSbacCoders[fullDepth][CI_QT_TRAFO_ROOT]);

			uint32_t singleDistYTmp = 0;
			uint32_t singleDistCTmp = 0;
			uint32_t singleCbfYTmp = 0;
			uint32_t singleCbfUTmp = 0;
			uint32_t singleCbfVTmp = 0;
			uint64_t singleCostTmp = 0;
			int    default0Save1Load2 = 0;
			int    firstCheckId = 0;

			uint32_t qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + (trDepth - 1)) << 1);
			bool   bFirstQ = ((absPartIdx % qpdiv) == 0);

			for (int modeId = firstCheckId; modeId < 2; modeId++)
			{
				singleDistYTmp = 0;
				singleDistCTmp = 0;
				cu->setTransformSkipSubParts(modeId, TEXT_LUMA, absPartIdx, fullDepth);
				if (modeId == firstCheckId)
				{
					default0Save1Load2 = 1;
				}
				else
				{
					default0Save1Load2 = 2;
				}
				//----- code luma block with given intra prediction mode and store Cbf-----
				xIntraCodingLumaBlk(cu, trDepth, absPartIdx, fencYuv, predYuv, resiYuv, singleDistYTmp, default0Save1Load2);
				singleCbfYTmp = cu->getCbf(absPartIdx, TEXT_LUMA, trDepth);
				//----- code chroma blocks with given intra prediction mode and store Cbf-----
				if (!bLumaOnly)
				{
					if (bFirstQ)
					{
						cu->setTransformSkipSubParts(modeId, TEXT_CHROMA_U, absPartIdx, fullDepth);
						cu->setTransformSkipSubParts(modeId, TEXT_CHROMA_V, absPartIdx, fullDepth);
					}
					xIntraCodingChromaBlk(cu, trDepth, absPartIdx, fencYuv, predYuv, resiYuv, singleDistCTmp, 0, default0Save1Load2);
					xIntraCodingChromaBlk(cu, trDepth, absPartIdx, fencYuv, predYuv, resiYuv, singleDistCTmp, 1, default0Save1Load2);
					singleCbfUTmp = cu->getCbf(absPartIdx, TEXT_CHROMA_U, trDepth);
					singleCbfVTmp = cu->getCbf(absPartIdx, TEXT_CHROMA_V, trDepth);
				}
				//----- determine rate and r-d cost -----
				if (modeId == 1 && singleCbfYTmp == 0)
				{
					//In order not to code TS flag when cbf is zero, the case for TS with cbf being zero is forbidden.
					singleCostTmp = MAX_INT64;
				}
				else
				{
					uint32_t singleBits = xGetIntraBitsQT(cu, trDepth, absPartIdx, true, !bLumaOnly);
					singleCostTmp = m_rdCost->calcRdCost(singleDistYTmp + singleDistCTmp, singleBits);
				}

				if (singleCostTmp < singleCost)
				{
					singleCost = singleCostTmp;
					singleDistY = singleDistYTmp;
					singleDistC = singleDistCTmp;
					singleCbfY = singleCbfYTmp;
					singleCbfU = singleCbfUTmp;
					singleCbfV = singleCbfVTmp;
					bestModeId = modeId;
					if (bestModeId == firstCheckId)
					{
						xStoreIntraResultQT(cu, trDepth, absPartIdx, bLumaOnly);
						m_rdGoOnSbacCoder->store(m_rdSbacCoders[fullDepth][CI_TEMP_BEST]);
					}
				}
				if (modeId == firstCheckId)
				{
					m_rdGoOnSbacCoder->load(m_rdSbacCoders[fullDepth][CI_QT_TRAFO_ROOT]);
				}
			}

			cu->setTransformSkipSubParts(bestModeId, TEXT_LUMA, absPartIdx, fullDepth);

			if (bestModeId == firstCheckId)
			{
				xLoadIntraResultQT(cu, trDepth, absPartIdx, bLumaOnly);
				cu->setCbfSubParts(singleCbfY << trDepth, TEXT_LUMA, absPartIdx, fullDepth);
				if (!bLumaOnly)
				{
					if (bFirstQ)
					{
						cu->setCbfSubParts(singleCbfU << trDepth, TEXT_CHROMA_U, absPartIdx, cu->getDepth(0) + trDepth - 1);
						cu->setCbfSubParts(singleCbfV << trDepth, TEXT_CHROMA_V, absPartIdx, cu->getDepth(0) + trDepth - 1);
					}
				}
				m_rdGoOnSbacCoder->load(m_rdSbacCoders[fullDepth][CI_TEMP_BEST]);
			}

			if (!bLumaOnly)
			{
				bestModeIdUV[0] = bestModeIdUV[1] = bestModeId;
				if (bFirstQ && bestModeId == 1)
				{
					//In order not to code TS flag when cbf is zero, the case for TS with cbf being zero is forbidden.
					if (singleCbfU == 0)
					{
						cu->setTransformSkipSubParts(0, TEXT_CHROMA_U, absPartIdx, fullDepth);
						bestModeIdUV[0] = 0;
					}
					if (singleCbfV == 0)
					{
						cu->setTransformSkipSubParts(0, TEXT_CHROMA_V, absPartIdx, fullDepth);
						bestModeIdUV[1] = 0;
					}
				}
			}
		}
		else
		{
			cu->setTransformSkipSubParts(0, TEXT_LUMA, absPartIdx, fullDepth);

			//----- store original entropy coding status -----
			m_rdGoOnSbacCoder->store(m_rdSbacCoders[fullDepth][CI_QT_TRAFO_ROOT]);

			//----- code luma block with given intra prediction mode and store Cbf-----
			singleCost = 0;
			xIntraCodingLumaBlk(cu, trDepth, absPartIdx, fencYuv, predYuv, resiYuv, singleDistY);
			if (bCheckSplit)
			{
				singleCbfY = cu->getCbf(absPartIdx, TEXT_LUMA, trDepth);
			}
			//----- code chroma blocks with given intra prediction mode and store Cbf-----
			if (!bLumaOnly)
			{
				cu->setTransformSkipSubParts(0, TEXT_CHROMA_U, absPartIdx, fullDepth);
				cu->setTransformSkipSubParts(0, TEXT_CHROMA_V, absPartIdx, fullDepth);
				xIntraCodingChromaBlk(cu, trDepth, absPartIdx, fencYuv, predYuv, resiYuv, singleDistC, 0);
				xIntraCodingChromaBlk(cu, trDepth, absPartIdx, fencYuv, predYuv, resiYuv, singleDistC, 1);
				if (bCheckSplit)
				{
					singleCbfU = cu->getCbf(absPartIdx, TEXT_CHROMA_U, trDepth);
					singleCbfV = cu->getCbf(absPartIdx, TEXT_CHROMA_V, trDepth);
				}
			}
			//----- determine rate and r-d cost -----
			uint32_t singleBits = xGetIntraBitsQT(cu, trDepth, absPartIdx, true, !bLumaOnly);
			if (m_cfg->param.rdPenalty && (trSizeLog2 == 5) && !isIntraSlice)
			{
				singleBits = singleBits * 4;
			}
			singleCost = m_rdCost->calcRdCost(singleDistY + singleDistC, singleBits);
		}
	}

	if (bCheckSplit)
	{
		//----- store full entropy coding status, load original entropy coding status -----
		if (bCheckFull)
		{
			m_rdGoOnSbacCoder->store(m_rdSbacCoders[fullDepth][CI_QT_TRAFO_TEST]);
			m_rdGoOnSbacCoder->load(m_rdSbacCoders[fullDepth][CI_QT_TRAFO_ROOT]);
		}
		else
		{
			m_rdGoOnSbacCoder->store(m_rdSbacCoders[fullDepth][CI_QT_TRAFO_ROOT]);
		}

		//----- code splitted block -----
		uint64_t splitCost = 0;
		uint32_t splitDistY = 0;
		uint32_t splitDistC = 0;
		uint32_t qPartsDiv = cu->getPic()->getNumPartInCU() >> ((fullDepth + 1) << 1);
		uint32_t absPartIdxSub = absPartIdx;

		uint32_t splitCbfY = 0;
		uint32_t splitCbfU = 0;
		uint32_t splitCbfV = 0;

		for (uint32_t part = 0; part < 4; part++, absPartIdxSub += qPartsDiv)
		{
			xRecurIntraCodingQT(cu, trDepth + 1, absPartIdxSub, bLumaOnly, fencYuv, predYuv, resiYuv, splitDistY, splitDistC, bCheckFirst, splitCost);

			splitCbfY |= cu->getCbf(absPartIdxSub, TEXT_LUMA, trDepth + 1);
			if (!bLumaOnly)
			{
				splitCbfU |= cu->getCbf(absPartIdxSub, TEXT_CHROMA_U, trDepth + 1);
				splitCbfV |= cu->getCbf(absPartIdxSub, TEXT_CHROMA_V, trDepth + 1);
			}
		}

		for (uint32_t offs = 0; offs < 4 * qPartsDiv; offs++)
		{
			cu->getCbf(TEXT_LUMA)[absPartIdx + offs] |= (splitCbfY << trDepth);
		}

		if (!bLumaOnly)
		{
			for (uint32_t offs = 0; offs < 4 * qPartsDiv; offs++)
			{
				cu->getCbf(TEXT_CHROMA_U)[absPartIdx + offs] |= (splitCbfU << trDepth);
				cu->getCbf(TEXT_CHROMA_V)[absPartIdx + offs] |= (splitCbfV << trDepth);
			}
		}
		//----- restore context states -----
		m_rdGoOnSbacCoder->load(m_rdSbacCoders[fullDepth][CI_QT_TRAFO_ROOT]);

		//----- determine rate and r-d cost -----
		uint32_t splitBits = xGetIntraBitsQT(cu, trDepth, absPartIdx, true, !bLumaOnly);
		splitCost = m_rdCost->calcRdCost(splitDistY + splitDistC, splitBits);

		//===== compare and set best =====
		if (splitCost < singleCost)
		{
			//--- update cost ---
			outDistY += splitDistY;
			outDistC += splitDistC;
			rdCost += splitCost;
			return;
		}

		//----- set entropy coding status -----
		m_rdGoOnSbacCoder->load(m_rdSbacCoders[fullDepth][CI_QT_TRAFO_TEST]);

		//--- set transform index and Cbf values ---
		cu->setTrIdxSubParts(trDepth, absPartIdx, fullDepth);
		cu->setCbfSubParts(singleCbfY << trDepth, TEXT_LUMA, absPartIdx, fullDepth);
		cu->setTransformSkipSubParts(bestModeId, TEXT_LUMA, absPartIdx, fullDepth);
		if (!bLumaOnly)
		{
			cu->setCbfSubParts(singleCbfU << trDepth, TEXT_CHROMA_U, absPartIdx, fullDepth);
			cu->setCbfSubParts(singleCbfV << trDepth, TEXT_CHROMA_V, absPartIdx, fullDepth);
			cu->setTransformSkipSubParts(bestModeIdUV[0], TEXT_CHROMA_U, absPartIdx, fullDepth);
			cu->setTransformSkipSubParts(bestModeIdUV[1], TEXT_CHROMA_V, absPartIdx, fullDepth);
		}

		//--- set reconstruction for next intra prediction blocks ---
		uint32_t width = cu->getWidth(0) >> trDepth;
		uint32_t height = cu->getHeight(0) >> trDepth;
		uint32_t qtLayer = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() - trSizeLog2;
		uint32_t zorder = cu->getZorderIdxInCU() + absPartIdx;
		int16_t* src = m_qtTempTComYuv[qtLayer].getLumaAddr(absPartIdx);
		assert(m_qtTempTComYuv[qtLayer].m_width == MAX_CU_SIZE);
		Pel*     dst = cu->getPic()->getPicYuvRec()->getLumaAddr(cu->getAddr(), zorder);
		uint32_t dststride = cu->getPic()->getPicYuvRec()->getStride();
		primitives.blockcpy_ps(width, height, dst, dststride, src, MAX_CU_SIZE);

		if (!bLumaOnly)
		{
			width >>= 1;
			height >>= 1;
			src = m_qtTempTComYuv[qtLayer].getCbAddr(absPartIdx);
			assert(m_qtTempTComYuv[qtLayer].m_cwidth == MAX_CU_SIZE / 2);
			dst = cu->getPic()->getPicYuvRec()->getCbAddr(cu->getAddr(), zorder);
			dststride = cu->getPic()->getPicYuvRec()->getCStride();
			primitives.blockcpy_ps(width, height, dst, dststride, src, MAX_CU_SIZE / 2);

			src = m_qtTempTComYuv[qtLayer].getCrAddr(absPartIdx);
			dst = cu->getPic()->getPicYuvRec()->getCrAddr(cu->getAddr(), zorder);
			primitives.blockcpy_ps(width, height, dst, dststride, src, MAX_CU_SIZE / 2);
		}
	}

	outDistY += singleDistY;
	outDistC += singleDistC;
	rdCost += singleCost;
}

void TEncSearch::residualTransformQuantIntra(TComDataCU* cu,
	uint32_t    trDepth,
	uint32_t    absPartIdx,
	bool        bLumaOnly,
	TComYuv*    fencYuv,
	TComYuv*    predYuv,
	TShortYUV*  resiYuv,
	TComYuv*    reconYuv)
{
	uint32_t fullDepth = cu->getDepth(0) + trDepth;
	uint32_t trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> fullDepth] + 2;
	bool     bCheckFull = (trSizeLog2 <= cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize());
	bool     bCheckSplit = (trSizeLog2 > cu->getQuadtreeTULog2MinSizeInCU(absPartIdx));

	int maxTuSize = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize();
	int isIntraSlice = (cu->getSlice()->getSliceType() == I_SLICE);

	if (m_cfg->param.rdPenalty && !isIntraSlice)
	{
		// if maximum RD-penalty don't check TU size 32x32
		if (m_cfg->param.rdPenalty == 2)
		{
			bCheckFull = (trSizeLog2 <= X265_MIN(maxTuSize, 4));
		}
	}
	if (bCheckFull)
	{
		cu->setTransformSkipSubParts(0, TEXT_LUMA, absPartIdx, fullDepth);

		//----- code luma block with given intra prediction mode and store Cbf-----
		uint32_t lumaPredMode = cu->getLumaIntraDir(absPartIdx);
		uint32_t width = cu->getWidth(0) >> trDepth;
		uint32_t height = cu->getHeight(0) >> trDepth;
		uint32_t stride = fencYuv->getStride();
		Pel*     fenc = fencYuv->getLumaAddr(absPartIdx);
		Pel*     pred = predYuv->getLumaAddr(absPartIdx);
		int16_t* residual = resiYuv->getLumaAddr(absPartIdx);
		Pel*     recon = reconYuv->getLumaAddr(absPartIdx);

		uint32_t numCoeffPerInc = cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight() >> (cu->getSlice()->getSPS()->getMaxCUDepth() << 1);
		TCoeff*  coeff = cu->getCoeffY() + numCoeffPerInc * absPartIdx;

		uint32_t zorder = cu->getZorderIdxInCU() + absPartIdx;
		Pel*     reconIPred = cu->getPic()->getPicYuvRec()->getLumaAddr(cu->getAddr(), zorder);
		uint32_t reconIPredStride = cu->getPic()->getPicYuvRec()->getStride();

		bool     useTransformSkip = cu->getTransformSkip(absPartIdx, TEXT_LUMA);

		//===== init availability pattern =====

		cu->getPattern()->initPattern(cu, trDepth, absPartIdx);
		cu->getPattern()->initAdiPattern(cu, absPartIdx, trDepth, m_predBuf, m_predBufStride, m_predBufHeight, refAbove, refLeft, refAboveFlt, refLeftFlt);
		//===== get prediction signal =====
		predIntraLumaAng(lumaPredMode, pred, stride, width);

		//===== get residual signal =====
		assert(!((uint32_t)(size_t)fenc & (width - 1)));
		assert(!((uint32_t)(size_t)pred & (width - 1)));
		assert(!((uint32_t)(size_t)residual & (width - 1)));
		primitives.calcresidual[(int)g_convertToBit[width]](fenc, pred, residual, stride);

		//===== transform and quantization =====
		uint32_t absSum = 0;
		int lastPos = -1;
		cu->setTrIdxSubParts(trDepth, absPartIdx, fullDepth);

		m_trQuant->setQPforQuant(cu->getQP(0), TEXT_LUMA, cu->getSlice()->getSPS()->getQpBDOffsetY(), 0);
		m_trQuant->selectLambda(TEXT_LUMA);
		absSum = m_trQuant->transformNxN(cu, residual, stride, coeff, width, height, TEXT_LUMA, absPartIdx, &lastPos, useTransformSkip);

		//--- set coded block flag ---
		cu->setCbfSubParts((absSum ? 1 : 0) << trDepth, TEXT_LUMA, absPartIdx, fullDepth);

		//--- inverse transform ---
		int size = g_convertToBit[width];
		if (absSum)
		{
			int scalingListType = 0 + g_eTTable[(int)TEXT_LUMA];
			assert(scalingListType < 6);
			m_trQuant->invtransformNxN(cu->getCUTransquantBypass(absPartIdx), cu->getLumaIntraDir(absPartIdx), residual, stride, coeff, width, height, scalingListType, useTransformSkip, lastPos);
		}
		else
		{
			int16_t* resiTmp = residual;
			memset(coeff, 0, sizeof(TCoeff)* width * height);
			primitives.blockfill_s[size](resiTmp, stride, 0);
		}

		//Generate Recon
		assert(width <= 32);
		int part = partitionFromSizes(width, height);
		primitives.luma_add_ps[part](recon, stride, pred, residual, stride, stride);
		primitives.blockcpy_pp(width, height, reconIPred, reconIPredStride, recon, stride);
	}

	if (bCheckSplit && !bCheckFull)
	{
		//----- code splitted block -----

		uint32_t qPartsDiv = cu->getPic()->getNumPartInCU() >> ((fullDepth + 1) << 1);
		uint32_t absPartIdxSub = absPartIdx;
		uint32_t splitCbfY = 0;

		for (uint32_t part = 0; part < 4; part++, absPartIdxSub += qPartsDiv)
		{
			residualTransformQuantIntra(cu, trDepth + 1, absPartIdxSub, bLumaOnly, fencYuv, predYuv, resiYuv, reconYuv);
			splitCbfY |= cu->getCbf(absPartIdxSub, TEXT_LUMA, trDepth + 1);
		}

		for (uint32_t offs = 0; offs < 4 * qPartsDiv; offs++)
		{
			cu->getCbf(TEXT_LUMA)[absPartIdx + offs] |= (splitCbfY << trDepth);
		}

		return;
	}
}

void TEncSearch::xSetIntraResultQT(TComDataCU* cu, uint32_t trDepth, uint32_t absPartIdx, bool bLumaOnly, TComYuv* reconYuv)
{
	uint32_t fullDepth = cu->getDepth(0) + trDepth;
	uint32_t trMode = cu->getTransformIdx(absPartIdx);

	if (trMode == trDepth)
	{
		uint32_t trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> fullDepth] + 2;
		uint32_t qtlayer = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() - trSizeLog2;

		bool bSkipChroma = false;
		bool bChromaSame = false;
		if (!bLumaOnly && trSizeLog2 == 2)
		{
			assert(trDepth > 0);
			uint32_t qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + trDepth - 1) << 1);
			bSkipChroma = ((absPartIdx % qpdiv) != 0);
			bChromaSame = true;
		}

		//===== copy transform coefficients =====
		uint32_t numCoeffY = (cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight()) >> (fullDepth << 1);
		uint32_t numCoeffIncY = (cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight()) >> (cu->getSlice()->getSPS()->getMaxCUDepth() << 1);
		TCoeff* coeffSrcY = m_qtTempCoeffY[qtlayer] + (numCoeffIncY * absPartIdx);
		TCoeff* coeffDestY = cu->getCoeffY() + (numCoeffIncY * absPartIdx);
		::memcpy(coeffDestY, coeffSrcY, sizeof(TCoeff)* numCoeffY);

		if (!bLumaOnly && !bSkipChroma)
		{
			uint32_t numCoeffC = (bChromaSame ? numCoeffY : numCoeffY >> 2);
			uint32_t numCoeffIncC = numCoeffIncY >> 2;
			TCoeff* coeffSrcU = m_qtTempCoeffCb[qtlayer] + (numCoeffIncC * absPartIdx);
			TCoeff* coeffSrcV = m_qtTempCoeffCr[qtlayer] + (numCoeffIncC * absPartIdx);
			TCoeff* coeffDstU = cu->getCoeffCb() + (numCoeffIncC * absPartIdx);
			TCoeff* coeffDstV = cu->getCoeffCr() + (numCoeffIncC * absPartIdx);
			::memcpy(coeffDstU, coeffSrcU, sizeof(TCoeff)* numCoeffC);
			::memcpy(coeffDstV, coeffSrcV, sizeof(TCoeff)* numCoeffC);
		}

		//===== copy reconstruction =====
		m_qtTempTComYuv[qtlayer].copyPartToPartLuma(reconYuv, absPartIdx, 1 << trSizeLog2, 1 << trSizeLog2);
		if (!bLumaOnly && !bSkipChroma)
		{
			uint32_t trSizeCLog2 = (bChromaSame ? trSizeLog2 : trSizeLog2 - 1);
			m_qtTempTComYuv[qtlayer].copyPartToPartChroma(reconYuv, absPartIdx, 1 << trSizeCLog2, 1 << trSizeCLog2);
		}
	}
	else
	{
		uint32_t numQPart = cu->getPic()->getNumPartInCU() >> ((fullDepth + 1) << 1);
		for (uint32_t part = 0; part < 4; part++)
		{
			xSetIntraResultQT(cu, trDepth + 1, absPartIdx + part * numQPart, bLumaOnly, reconYuv);
		}
	}
}

void TEncSearch::xStoreIntraResultQT(TComDataCU* cu, uint32_t trDepth, uint32_t absPartIdx, bool bLumaOnly)
{
	uint32_t fullMode = cu->getDepth(0) + trDepth;

	uint32_t trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> fullMode] + 2;
	uint32_t qtlayer = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() - trSizeLog2;

	bool bSkipChroma = false;
	bool bChromaSame = false;

	if (!bLumaOnly && trSizeLog2 == 2)
	{
		assert(trDepth > 0);
		uint32_t qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + trDepth - 1) << 1);
		bSkipChroma = ((absPartIdx % qpdiv) != 0);
		bChromaSame = true;
	}

	//===== copy transform coefficients =====
	uint32_t numCoeffY = (cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight()) >> (fullMode << 1);
	uint32_t numCoeffIncY = (cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight()) >> (cu->getSlice()->getSPS()->getMaxCUDepth() << 1);
	TCoeff* coeffSrcY = m_qtTempCoeffY[qtlayer] + (numCoeffIncY * absPartIdx);
	TCoeff* coeffDstY = m_qtTempTUCoeffY;
	::memcpy(coeffDstY, coeffSrcY, sizeof(TCoeff)* numCoeffY);

	if (!bLumaOnly && !bSkipChroma)
	{
		uint32_t numCoeffC = (bChromaSame ? numCoeffY : numCoeffY >> 2);
		uint32_t numCoeffIncC = numCoeffIncY >> 2;
		TCoeff* coeffSrcU = m_qtTempCoeffCb[qtlayer] + (numCoeffIncC * absPartIdx);
		TCoeff* coeffSrcV = m_qtTempCoeffCr[qtlayer] + (numCoeffIncC * absPartIdx);
		TCoeff* coeffDstU = m_qtTempTUCoeffCb;
		TCoeff* coeffDstV = m_qtTempTUCoeffCr;
		::memcpy(coeffDstU, coeffSrcU, sizeof(TCoeff)* numCoeffC);
		::memcpy(coeffDstV, coeffSrcV, sizeof(TCoeff)* numCoeffC);
	}

	//===== copy reconstruction =====
	m_qtTempTComYuv[qtlayer].copyPartToPartLuma(&m_qtTempTransformSkipTComYuv, absPartIdx, 1 << trSizeLog2, 1 << trSizeLog2);

	if (!bLumaOnly && !bSkipChroma)
	{
		uint32_t trSizeCLog2 = (bChromaSame ? trSizeLog2 : trSizeLog2 - 1);
		m_qtTempTComYuv[qtlayer].copyPartToPartChroma(&m_qtTempTransformSkipTComYuv, absPartIdx, 1 << trSizeCLog2, 1 << trSizeCLog2);
	}
}

void TEncSearch::xLoadIntraResultQT(TComDataCU* cu, uint32_t trDepth, uint32_t absPartIdx, bool bLumaOnly)
{
	uint32_t fullDepth = cu->getDepth(0) + trDepth;

	uint32_t trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> fullDepth] + 2;
	uint32_t qtlayer = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() - trSizeLog2;

	bool bSkipChroma = false;
	bool bChromaSame = false;

	if (!bLumaOnly && trSizeLog2 == 2)
	{
		assert(trDepth > 0);
		uint32_t qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + trDepth - 1) << 1);
		bSkipChroma = ((absPartIdx % qpdiv) != 0);
		bChromaSame = true;
	}

	//===== copy transform coefficients =====
	uint32_t numCoeffY = (cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight()) >> (fullDepth << 1);
	uint32_t numCoeffIncY = (cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight()) >> (cu->getSlice()->getSPS()->getMaxCUDepth() << 1);
	TCoeff* coeffDstY = m_qtTempCoeffY[qtlayer] + (numCoeffIncY * absPartIdx);
	TCoeff* coeffSrcY = m_qtTempTUCoeffY;
	::memcpy(coeffDstY, coeffSrcY, sizeof(TCoeff)* numCoeffY);

	if (!bLumaOnly && !bSkipChroma)
	{
		uint32_t numCoeffC = (bChromaSame ? numCoeffY : numCoeffY >> 2);
		uint32_t numCoeffIncC = numCoeffIncY >> 2;
		TCoeff* coeffDstU = m_qtTempCoeffCb[qtlayer] + (numCoeffIncC * absPartIdx);
		TCoeff* coeffDstV = m_qtTempCoeffCr[qtlayer] + (numCoeffIncC * absPartIdx);
		TCoeff* coeffSrcU = m_qtTempTUCoeffCb;
		TCoeff* coeffSrcV = m_qtTempTUCoeffCr;
		::memcpy(coeffDstU, coeffSrcU, sizeof(TCoeff)* numCoeffC);
		::memcpy(coeffDstV, coeffSrcV, sizeof(TCoeff)* numCoeffC);
	}

	int part = partitionFromSizes(1 << trSizeLog2, 1 << trSizeLog2);
	//===== copy reconstruction =====
	m_qtTempTransformSkipTComYuv.copyPartToPartLuma(&m_qtTempTComYuv[qtlayer], absPartIdx, part);

	if (!bLumaOnly && !bSkipChroma)
	{
		m_qtTempTransformSkipTComYuv.copyPartToPartChroma(&m_qtTempTComYuv[qtlayer], absPartIdx, part);
	}

	uint32_t   zOrder = cu->getZorderIdxInCU() + absPartIdx;
	Pel*   reconIPred = cu->getPic()->getPicYuvRec()->getLumaAddr(cu->getAddr(), zOrder);
	uint32_t   reconIPredStride = cu->getPic()->getPicYuvRec()->getStride();
	int16_t* reconQt = m_qtTempTComYuv[qtlayer].getLumaAddr(absPartIdx);
	assert(m_qtTempTComYuv[qtlayer].m_width == MAX_CU_SIZE);
	uint32_t   width = cu->getWidth(0) >> trDepth;
	uint32_t   height = cu->getHeight(0) >> trDepth;
	primitives.blockcpy_ps(width, height, reconIPred, reconIPredStride, reconQt, MAX_CU_SIZE);

	if (!bLumaOnly && !bSkipChroma)
	{
		width >>= 1;
		height >>= 1;
		reconIPred = cu->getPic()->getPicYuvRec()->getCbAddr(cu->getAddr(), zOrder);
		reconIPredStride = cu->getPic()->getPicYuvRec()->getCStride();
		reconQt = m_qtTempTComYuv[qtlayer].getCbAddr(absPartIdx);
		assert(m_qtTempTComYuv[qtlayer].m_cwidth == MAX_CU_SIZE / 2);
		primitives.blockcpy_ps(width, height, reconIPred, reconIPredStride, reconQt, MAX_CU_SIZE / 2);

		reconIPred = cu->getPic()->getPicYuvRec()->getCrAddr(cu->getAddr(), zOrder);
		reconQt = m_qtTempTComYuv[qtlayer].getCrAddr(absPartIdx);
		primitives.blockcpy_ps(width, height, reconIPred, reconIPredStride, reconQt, MAX_CU_SIZE / 2);
	}
}

void TEncSearch::xStoreIntraResultChromaQT(TComDataCU* cu, uint32_t trDepth, uint32_t absPartIdx, uint32_t stateU0V1Both2)
{
	uint32_t fullDepth = cu->getDepth(0) + trDepth;
	uint32_t trMode = cu->getTransformIdx(absPartIdx);

	if (trMode == trDepth)
	{
		uint32_t trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> fullDepth] + 2;
		uint32_t qtlayer = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() - trSizeLog2;

		bool bChromaSame = false;
		if (trSizeLog2 == 2)
		{
			assert(trDepth > 0);
			trDepth--;
			uint32_t qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + trDepth) << 1);
			if ((absPartIdx % qpdiv) != 0)
			{
				return;
			}
			bChromaSame = true;
		}

		//===== copy transform coefficients =====
		uint32_t numCoeffC = (cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight()) >> (fullDepth << 1);
		if (!bChromaSame)
		{
			numCoeffC >>= 2;
		}
		uint32_t numCoeffIncC = (cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight()) >> ((cu->getSlice()->getSPS()->getMaxCUDepth() << 1) + 2);
		if (stateU0V1Both2 == 0 || stateU0V1Both2 == 2)
		{
			TCoeff* coeffSrcU = m_qtTempCoeffCb[qtlayer] + (numCoeffIncC * absPartIdx);
			TCoeff* coeffDstU = m_qtTempTUCoeffCb;
			::memcpy(coeffDstU, coeffSrcU, sizeof(TCoeff)* numCoeffC);
		}
		if (stateU0V1Both2 == 1 || stateU0V1Both2 == 2)
		{
			TCoeff* coeffSrcV = m_qtTempCoeffCr[qtlayer] + (numCoeffIncC * absPartIdx);
			TCoeff* coeffDstV = m_qtTempTUCoeffCr;
			::memcpy(coeffDstV, coeffSrcV, sizeof(TCoeff)* numCoeffC);
		}

		//===== copy reconstruction =====
		uint32_t trSizeCLog2 = (bChromaSame ? trSizeLog2 : trSizeLog2 - 1);
		m_qtTempTComYuv[qtlayer].copyPartToPartChroma(&m_qtTempTransformSkipTComYuv, absPartIdx, 1 << trSizeCLog2, 1 << trSizeCLog2, stateU0V1Both2);
	}
}

void TEncSearch::xLoadIntraResultChromaQT(TComDataCU* cu, uint32_t trDepth, uint32_t absPartIdx, uint32_t stateU0V1Both2)
{
	uint32_t fullDepth = cu->getDepth(0) + trDepth;
	uint32_t trMode = cu->getTransformIdx(absPartIdx);

	if (trMode == trDepth)
	{
		uint32_t trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> fullDepth] + 2;
		uint32_t qtlayer = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() - trSizeLog2;

		bool bChromaSame = false;
		if (trSizeLog2 == 2)
		{
			assert(trDepth > 0);
			trDepth--;
			uint32_t qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + trDepth) << 1);
			if ((absPartIdx % qpdiv) != 0)
			{
				return;
			}
			bChromaSame = true;
		}

		//===== copy transform coefficients =====
		uint32_t numCoeffC = (cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight()) >> (fullDepth << 1);
		if (!bChromaSame)
		{
			numCoeffC >>= 2;
		}
		uint32_t numCoeffIncC = (cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight()) >> ((cu->getSlice()->getSPS()->getMaxCUDepth() << 1) + 2);

		if (stateU0V1Both2 == 0 || stateU0V1Both2 == 2)
		{
			TCoeff* coeffDstU = m_qtTempCoeffCb[qtlayer] + (numCoeffIncC * absPartIdx);
			TCoeff* coeffSrcU = m_qtTempTUCoeffCb;
			::memcpy(coeffDstU, coeffSrcU, sizeof(TCoeff)* numCoeffC);
		}
		if (stateU0V1Both2 == 1 || stateU0V1Both2 == 2)
		{
			TCoeff* coeffDstV = m_qtTempCoeffCr[qtlayer] + (numCoeffIncC * absPartIdx);
			TCoeff* coeffSrcV = m_qtTempTUCoeffCr;
			::memcpy(coeffDstV, coeffSrcV, sizeof(TCoeff)* numCoeffC);
		}

		//===== copy reconstruction =====
		uint32_t trSizeCLog2 = (bChromaSame ? trSizeLog2 : trSizeLog2 - 1);
		m_qtTempTransformSkipTComYuv.copyPartToPartChroma(&m_qtTempTComYuv[qtlayer], absPartIdx, 1 << trSizeCLog2, 1 << trSizeCLog2, stateU0V1Both2);

		uint32_t zorder = cu->getZorderIdxInCU() + absPartIdx;
		uint32_t width = cu->getWidth(0) >> (trDepth + 1);
		uint32_t height = cu->getHeight(0) >> (trDepth + 1);
		assert(m_qtTempTComYuv[qtlayer].m_cwidth == MAX_CU_SIZE / 2);
		uint32_t reconIPredStride = cu->getPic()->getPicYuvRec()->getCStride();

		if (stateU0V1Both2 == 0 || stateU0V1Both2 == 2)
		{
			Pel* reconIPred = cu->getPic()->getPicYuvRec()->getCbAddr(cu->getAddr(), zorder);
			int16_t* reconQt = m_qtTempTComYuv[qtlayer].getCbAddr(absPartIdx);
			primitives.blockcpy_ps(width, height, reconIPred, reconIPredStride, reconQt, MAX_CU_SIZE / 2);
		}
		if (stateU0V1Both2 == 1 || stateU0V1Both2 == 2)
		{
			Pel* reconIPred = cu->getPic()->getPicYuvRec()->getCrAddr(cu->getAddr(), zorder);
			int16_t* reconQt = m_qtTempTComYuv[qtlayer].getCrAddr(absPartIdx);
			primitives.blockcpy_ps(width, height, reconIPred, reconIPredStride, reconQt, MAX_CU_SIZE / 2);
		}
	}
}

void TEncSearch::xRecurIntraChromaCodingQT(TComDataCU* cu,
	uint32_t    trDepth,
	uint32_t    absPartIdx,
	TComYuv*    fencYuv,
	TComYuv*    predYuv,
	TShortYUV*  resiYuv,
	uint32_t&   outDist)
{
	uint32_t fullDepth = cu->getDepth(0) + trDepth;
	uint32_t trMode = cu->getTransformIdx(absPartIdx);

	if (trMode == trDepth)
	{
		bool checkTransformSkip = false; //cu->getSlice()->getPPS()->getUseTransformSkip();
		uint32_t trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> fullDepth] + 2;

		uint32_t actualTrDepth = trDepth;
		if (trSizeLog2 == 2)
		{
			assert(trDepth > 0);
			actualTrDepth--;
			uint32_t qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + actualTrDepth) << 1);
			bool bFirstQ = ((absPartIdx % qpdiv) == 0);
			if (!bFirstQ)
			{
				return;
			}
		}

		checkTransformSkip &= (trSizeLog2 <= 3);
		if (m_cfg->param.bEnableTSkipFast)
		{
			checkTransformSkip &= (trSizeLog2 < 3);
			if (checkTransformSkip)
			{
				int nbLumaSkip = 0;
				for (uint32_t absPartIdxSub = absPartIdx; absPartIdxSub < absPartIdx + 4; absPartIdxSub++)
				{
					nbLumaSkip += cu->getTransformSkip(absPartIdxSub, TEXT_LUMA);
				}

				checkTransformSkip &= (nbLumaSkip > 0);
			}
		}

		if (checkTransformSkip)
		{
			// use RDO to decide whether Cr/Cb takes TS
			m_rdGoOnSbacCoder->store(m_rdSbacCoders[fullDepth][CI_QT_TRAFO_ROOT]);

			for (int chromaId = 0; chromaId < 2; chromaId++)
			{
				uint64_t singleCost = MAX_INT64;
				int      bestModeId = 0;
				uint32_t singleDistC = 0;
				uint32_t singleCbfC = 0;
				uint32_t singleDistCTmp = 0;
				uint64_t singleCostTmp = 0;
				uint32_t singleCbfCTmp = 0;

				int     default0Save1Load2 = 0;
				int     firstCheckId = 0;

				for (int chromaModeId = firstCheckId; chromaModeId < 2; chromaModeId++)
				{
					cu->setTransformSkipSubParts(chromaModeId, (TextType)(chromaId + 2), absPartIdx, cu->getDepth(0) + actualTrDepth);
					if (chromaModeId == firstCheckId)
					{
						default0Save1Load2 = 1;
					}
					else
					{
						default0Save1Load2 = 2;
					}
					singleDistCTmp = 0;
					xIntraCodingChromaBlk(cu, trDepth, absPartIdx, fencYuv, predYuv, resiYuv, singleDistCTmp, chromaId, default0Save1Load2);
					singleCbfCTmp = cu->getCbf(absPartIdx, (TextType)(chromaId + 2), trDepth);

					if (chromaModeId == 1 && singleCbfCTmp == 0)
					{
						//In order not to code TS flag when cbf is zero, the case for TS with cbf being zero is forbidden.
						singleCostTmp = MAX_INT64;
					}
					else
					{
						uint32_t bitsTmp = xGetIntraBitsQTChroma(cu, trDepth, absPartIdx, chromaId + 2);
						singleCostTmp = m_rdCost->calcRdCost(singleDistCTmp, bitsTmp);
					}

					if (singleCostTmp < singleCost)
					{
						singleCost = singleCostTmp;
						singleDistC = singleDistCTmp;
						bestModeId = chromaModeId;
						singleCbfC = singleCbfCTmp;

						if (bestModeId == firstCheckId)
						{
							xStoreIntraResultChromaQT(cu, trDepth, absPartIdx, chromaId);
							m_rdGoOnSbacCoder->store(m_rdSbacCoders[fullDepth][CI_TEMP_BEST]);
						}
					}
					if (chromaModeId == firstCheckId)
					{
						m_rdGoOnSbacCoder->load(m_rdSbacCoders[fullDepth][CI_QT_TRAFO_ROOT]);
					}
				}

				if (bestModeId == firstCheckId)
				{
					xLoadIntraResultChromaQT(cu, trDepth, absPartIdx, chromaId);
					cu->setCbfSubParts(singleCbfC << trDepth, (TextType)(chromaId + 2), absPartIdx, cu->getDepth(0) + actualTrDepth);
					m_rdGoOnSbacCoder->load(m_rdSbacCoders[fullDepth][CI_TEMP_BEST]);
				}
				cu->setTransformSkipSubParts(bestModeId, (TextType)(chromaId + 2), absPartIdx, cu->getDepth(0) + actualTrDepth);
				outDist += singleDistC;

				if (chromaId == 0)
				{
					m_rdGoOnSbacCoder->store(m_rdSbacCoders[fullDepth][CI_QT_TRAFO_ROOT]);
				}
			}
		}
		else
		{
			cu->setTransformSkipSubParts(0, TEXT_CHROMA_U, absPartIdx, cu->getDepth(0) + actualTrDepth);
			cu->setTransformSkipSubParts(0, TEXT_CHROMA_V, absPartIdx, cu->getDepth(0) + actualTrDepth);
			xIntraCodingChromaBlk(cu, trDepth, absPartIdx, fencYuv, predYuv, resiYuv, outDist, 0);
			xIntraCodingChromaBlk(cu, trDepth, absPartIdx, fencYuv, predYuv, resiYuv, outDist, 1);
		}
	}
	else
	{
		uint32_t splitCbfU = 0;
		uint32_t splitCbfV = 0;
		uint32_t qPartsDiv = cu->getPic()->getNumPartInCU() >> ((fullDepth + 1) << 1);
		uint32_t absPartIdxSub = absPartIdx;
		for (uint32_t part = 0; part < 4; part++, absPartIdxSub += qPartsDiv)
		{
			xRecurIntraChromaCodingQT(cu, trDepth + 1, absPartIdxSub, fencYuv, predYuv, resiYuv, outDist);
			splitCbfU |= cu->getCbf(absPartIdxSub, TEXT_CHROMA_U, trDepth + 1);
			splitCbfV |= cu->getCbf(absPartIdxSub, TEXT_CHROMA_V, trDepth + 1);
		}

		for (uint32_t offs = 0; offs < 4 * qPartsDiv; offs++)
		{
			cu->getCbf(TEXT_CHROMA_U)[absPartIdx + offs] |= (splitCbfU << trDepth);
			cu->getCbf(TEXT_CHROMA_V)[absPartIdx + offs] |= (splitCbfV << trDepth);
		}
	}
}

void TEncSearch::xSetIntraResultChromaQT(TComDataCU* cu, uint32_t trDepth, uint32_t absPartIdx, TComYuv* reconYuv)
{
	uint32_t fullDepth = cu->getDepth(0) + trDepth;
	uint32_t trMode = cu->getTransformIdx(absPartIdx);

	if (trMode == trDepth)
	{
		uint32_t trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> fullDepth] + 2;
		uint32_t qtlayer = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() - trSizeLog2;

		bool bChromaSame = false;
		if (trSizeLog2 == 2)
		{
			assert(trDepth > 0);
			uint32_t qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + trDepth - 1) << 1);
			if ((absPartIdx % qpdiv) != 0)
			{
				return;
			}
			bChromaSame = true;
		}

		//===== copy transform coefficients =====
		uint32_t numCoeffC = (cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight()) >> (fullDepth << 1);
		if (!bChromaSame)
		{
			numCoeffC >>= 2;
		}
		uint32_t numCoeffIncC = (cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight()) >> ((cu->getSlice()->getSPS()->getMaxCUDepth() << 1) + 2);
		TCoeff* coeffSrcU = m_qtTempCoeffCb[qtlayer] + (numCoeffIncC * absPartIdx);
		TCoeff* coeffSrcV = m_qtTempCoeffCr[qtlayer] + (numCoeffIncC * absPartIdx);
		TCoeff* coeffDstU = cu->getCoeffCb() + (numCoeffIncC * absPartIdx);
		TCoeff* coeffDstV = cu->getCoeffCr() + (numCoeffIncC * absPartIdx);
		::memcpy(coeffDstU, coeffSrcU, sizeof(TCoeff)* numCoeffC);
		::memcpy(coeffDstV, coeffSrcV, sizeof(TCoeff)* numCoeffC);

		//===== copy reconstruction =====
		uint32_t trSizeCLog2 = (bChromaSame ? trSizeLog2 : trSizeLog2 - 1);
		m_qtTempTComYuv[qtlayer].copyPartToPartChroma(reconYuv, absPartIdx, 1 << trSizeCLog2, 1 << trSizeCLog2);
	}
	else
	{
		uint32_t numQPart = cu->getPic()->getNumPartInCU() >> ((fullDepth + 1) << 1);
		for (uint32_t part = 0; part < 4; part++)
		{
			xSetIntraResultChromaQT(cu, trDepth + 1, absPartIdx + part * numQPart, reconYuv);
		}
	}
}

void TEncSearch::residualQTIntrachroma(TComDataCU* cu,
	uint32_t    trDepth,
	uint32_t    absPartIdx,
	TComYuv*    fencYuv,
	TComYuv*    predYuv,
	TShortYUV*  resiYuv,
	TComYuv*    reconYuv)
{
	uint32_t fullDepth = cu->getDepth(0) + trDepth;
	uint32_t trMode = cu->getTransformIdx(absPartIdx);
	if (trMode == trDepth)
	{
		uint32_t trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> fullDepth] + 2;
		uint32_t actualTrDepth = trDepth;
		if (trSizeLog2 == 2)
		{
			assert(trDepth > 0);
			actualTrDepth--;
			uint32_t qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + actualTrDepth) << 1);
			bool bFirstQ = ((absPartIdx % qpdiv) == 0);
			if (!bFirstQ)
			{
				return;
			}
		}

		cu->setTransformSkipSubParts(0, TEXT_CHROMA_U, absPartIdx, cu->getDepth(0) + actualTrDepth);
		cu->setTransformSkipSubParts(0, TEXT_CHROMA_V, absPartIdx, cu->getDepth(0) + actualTrDepth);
		uint32_t width = cu->getWidth(0) >> (trDepth + m_hChromaShift);
		uint32_t height = cu->getHeight(0) >> (trDepth + m_vChromaShift);
		uint32_t stride = fencYuv->getCStride();

		for (uint32_t chromaId = 0; chromaId < 2; chromaId++)
		{
			TextType ttype = (chromaId > 0 ? TEXT_CHROMA_V : TEXT_CHROMA_U);
			uint32_t chromaPredMode = cu->getChromaIntraDir(absPartIdx);
			Pel*     fenc = (chromaId > 0 ? fencYuv->getCrAddr(absPartIdx) : fencYuv->getCbAddr(absPartIdx));
			Pel*     pred = (chromaId > 0 ? predYuv->getCrAddr(absPartIdx) : predYuv->getCbAddr(absPartIdx));
			int16_t* residual = (chromaId > 0 ? resiYuv->getCrAddr(absPartIdx) : resiYuv->getCbAddr(absPartIdx));
			Pel*     recon = (chromaId > 0 ? reconYuv->getCrAddr(absPartIdx) : reconYuv->getCbAddr(absPartIdx));
			uint32_t numCoeffPerInc = (cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight() >> (cu->getSlice()->getSPS()->getMaxCUDepth() << 1)) >> 2;
			TCoeff*  coeff = (chromaId > 0 ? cu->getCoeffCr() : cu->getCoeffCb()) + numCoeffPerInc * absPartIdx;

			uint32_t zorder = cu->getZorderIdxInCU() + absPartIdx;
			Pel*     reconIPred = (chromaId > 0 ? cu->getPic()->getPicYuvRec()->getCrAddr(cu->getAddr(), zorder) : cu->getPic()->getPicYuvRec()->getCbAddr(cu->getAddr(), zorder));
			uint32_t reconIPredStride = cu->getPic()->getPicYuvRec()->getCStride();
			bool     useTransformSkipChroma = cu->getTransformSkip(absPartIdx, ttype);
			//===== update chroma mode =====
			if (chromaPredMode == DM_CHROMA_IDX)
			{
				chromaPredMode = cu->getLumaIntraDir(0);
			}
			//===== init availability pattern =====
			cu->getPattern()->initPattern(cu, trDepth, absPartIdx);
			cu->getPattern()->initAdiPatternChroma(cu, absPartIdx, trDepth, m_predBuf, m_predBufStride, m_predBufHeight);
			Pel* chromaPred = (chromaId > 0 ? cu->getPattern()->getAdiCrBuf(width, height, m_predBuf) : cu->getPattern()->getAdiCbBuf(width, height, m_predBuf));

			//===== get prediction signal =====
			predIntraChromaAng(chromaPred, chromaPredMode, pred, stride, width);

			//===== get residual signal =====
			assert(!((uint32_t)(size_t)fenc & (width - 1)));
			assert(!((uint32_t)(size_t)pred & (width - 1)));
			assert(!((uint32_t)(size_t)residual & (width - 1)));
			int size = g_convertToBit[width];
			primitives.calcresidual[size](fenc, pred, residual, stride);

			//--- transform and quantization ---
			uint32_t absSum = 0;
			int lastPos = -1;

			int curChromaQpOffset;
			if (ttype == TEXT_CHROMA_U)
			{
				curChromaQpOffset = cu->getSlice()->getPPS()->getChromaCbQpOffset() + cu->getSlice()->getSliceQpDeltaCb();
			}
			else
			{
				curChromaQpOffset = cu->getSlice()->getPPS()->getChromaCrQpOffset() + cu->getSlice()->getSliceQpDeltaCr();
			}
			m_trQuant->setQPforQuant(cu->getQP(0), TEXT_CHROMA, cu->getSlice()->getSPS()->getQpBDOffsetC(), curChromaQpOffset);

			m_trQuant->selectLambda(TEXT_CHROMA);

			absSum = m_trQuant->transformNxN(cu, residual, stride, coeff, width, height, ttype, absPartIdx, &lastPos, useTransformSkipChroma);

			//--- set coded block flag ---
			cu->setCbfSubParts((absSum ? 1 : 0) << trDepth, ttype, absPartIdx, cu->getDepth(0) + trDepth);

			//--- inverse transform ---
			if (absSum)
			{
				int scalingListType = 0 + g_eTTable[(int)ttype];
				assert(scalingListType < 6);
				m_trQuant->invtransformNxN(cu->getCUTransquantBypass(absPartIdx), REG_DCT, residual, stride, coeff, width, height, scalingListType, useTransformSkipChroma, lastPos);
			}
			else
			{
				int16_t* resiTmp = residual;
				memset(coeff, 0, sizeof(TCoeff)* width * height);
				primitives.blockfill_s[size](resiTmp, stride, 0);
			}

			//===== reconstruction =====
			assert(((uint32_t)(size_t)residual & (width - 1)) == 0);
			assert(width <= 32);
			int part = partitionFromSizes(cu->getWidth(0) >> (trDepth), cu->getHeight(0) >> (trDepth));
			primitives.chroma[m_cfg->param.internalCsp].add_ps[part](recon, stride, pred, residual, stride, stride);
			primitives.chroma[m_cfg->param.internalCsp].copy_pp[part](reconIPred, reconIPredStride, recon, stride);
		}
	}
	else
	{
		uint32_t splitCbfU = 0;
		uint32_t splitCbfV = 0;
		uint32_t qPartsDiv = cu->getPic()->getNumPartInCU() >> ((fullDepth + 1) << 1);
		uint32_t absPartIdxSub = absPartIdx;
		for (uint32_t part = 0; part < 4; part++, absPartIdxSub += qPartsDiv)
		{
			residualQTIntrachroma(cu, trDepth + 1, absPartIdxSub, fencYuv, predYuv, resiYuv, reconYuv);
			splitCbfU |= cu->getCbf(absPartIdxSub, TEXT_CHROMA_U, trDepth + 1);
			splitCbfV |= cu->getCbf(absPartIdxSub, TEXT_CHROMA_V, trDepth + 1);
		}

		for (uint32_t offs = 0; offs < 4 * qPartsDiv; offs++)
		{
			cu->getCbf(TEXT_CHROMA_U)[absPartIdx + offs] |= (splitCbfU << trDepth);
			cu->getCbf(TEXT_CHROMA_V)[absPartIdx + offs] |= (splitCbfV << trDepth);
		}
	}
}

void TEncSearch::preestChromaPredMode(TComDataCU* cu, TComYuv* fencYuv, TComYuv* predYuv)
{
	uint32_t width = cu->getWidth(0) >> 1;
	uint32_t height = cu->getHeight(0) >> 1;
	uint32_t stride = fencYuv->getCStride();
	Pel* fencU = fencYuv->getCbAddr(0);
	Pel* fencV = fencYuv->getCrAddr(0);
	Pel* predU = predYuv->getCbAddr(0);
	Pel* predV = predYuv->getCrAddr(0);

	//===== init pattern =====
	assert(width == height);
	cu->getPattern()->initPattern(cu, 0, 0);
	cu->getPattern()->initAdiPatternChroma(cu, 0, 0, m_predBuf, m_predBufStride, m_predBufHeight);
	Pel* patChromaU = cu->getPattern()->getAdiCbBuf(width, height, m_predBuf);
	Pel* patChromaV = cu->getPattern()->getAdiCrBuf(width, height, m_predBuf);

	//===== get best prediction modes (using SAD) =====
	uint32_t minMode = 0;
	uint32_t maxMode = 4;
	uint32_t bestMode = MAX_UINT;
	uint32_t minSAD = MAX_UINT;
	pixelcmp_t sa8d = primitives.sa8d[(int)g_convertToBit[width]];
	for (uint32_t mode = minMode; mode < maxMode; mode++)
	{
		//--- get prediction ---
		predIntraChromaAng(patChromaU, mode, predU, stride, width);
		predIntraChromaAng(patChromaV, mode, predV, stride, width);

		//--- get SAD ---
		uint32_t sad = sa8d(fencU, stride, predU, stride) + sa8d(fencV, stride, predV, stride);

		//--- check ---
		if (sad < minSAD)
		{
			minSAD = sad;
			bestMode = mode;
		}
	}

	x265_emms();

	//===== set chroma pred mode =====
	cu->setChromIntraDirSubParts(bestMode, 0, cu->getDepth(0));
}

void TEncSearch::estIntraPredQT(TComDataCU* cu, TComYuv* fencYuv, TComYuv* predYuv, TShortYUV* resiYuv, TComYuv* reconYuv, uint32_t& outDistC, bool bLumaOnly)
{
	uint32_t depth = cu->getDepth(0);
	uint32_t numPU = cu->getNumPartInter();
	uint32_t initTrDepth = cu->getPartitionSize(0) == SIZE_2Nx2N ? 0 : 1;
	uint32_t width = cu->getWidth(0) >> initTrDepth;
	uint32_t qNumParts = cu->getTotalNumPart() >> 2;
	uint32_t widthBit = cu->getIntraSizeIdx(0);
	uint32_t overallDistY = 0;
	uint32_t overallDistC = 0;
	uint32_t candNum;
	uint64_t candCostList[FAST_UDI_MAX_RDMODE_NUM];



	//===== set QP and clear Cbf =====
	if (cu->getSlice()->getPPS()->getUseDQP() == true)
	{
		cu->setQPSubParts(cu->getQP(0), 0, depth);
	}
	else
	{
		cu->setQPSubParts(cu->getSlice()->getSliceQp(), 0, depth);
	}

	//===== loop over partitions =====
	uint32_t partOffset = 0;
#if INTRA_PU_4x4_MODIFY
	m_rdGoOnSbacCoder->load(m_rdSbacCoders[depth][CI_CURR_BEST]);
	m_rdGoOnSbacCoder->store(m_rdSbacCoders[depth][CI_TEMP_BEST]);
#endif
	for (uint32_t pu = 0; pu < numPU; pu++, partOffset += qNumParts)
	{
		//===== init pattern for luma prediction =====
		cu->getPattern()->initPattern(cu, initTrDepth, partOffset);

		// Reference sample smoothing
		cu->getPattern()->initAdiPattern(cu, partOffset, initTrDepth, m_predBuf, m_predBufStride, m_predBufHeight, refAbove, refLeft, refAboveFlt, refLeftFlt);

		//===== determine set of modes to be tested (using prediction signal only) =====
		int numModesAvailable = 35; //total number of Intra modes
		Pel* fenc = fencYuv->getLumaAddr(pu, width);
		uint32_t stride = predYuv->getStride();
		uint32_t rdModeList[FAST_UDI_MAX_RDMODE_NUM];
#ifdef RK_INTRA_MODE_CHOOSE
		int rk_candidate[20];
		::memset(rk_candidate,0xff,sizeof(rk_candidate));
		uint32_t modeCostsWithCabac[35];
#endif
		int numModesForFullRD = g_intraModeNumFast[widthBit];

		bool doFastSearch = (numModesForFullRD != numModesAvailable);
		if (doFastSearch)
		{
			assert(numModesForFullRD < numModesAvailable);
#ifdef RK_INTRA_MODE_CHOOSE
			for (int i = 0; i < numModesAvailable; i++)
			{
				candCostList[i] = MAX_INT64;
			}
#else
			for (int i = 0; i < numModesForFullRD; i++)
			{
				candCostList[i] = MAX_INT64;
			}
#endif
			candNum = 0;
			uint32_t modeCosts[35];
			uint32_t modeCosts_SAD[35];

			Pel *above = refAbove + width - 1;
			Pel *aboveFiltered = refAboveFlt + width - 1;
			Pel *left = refLeft + width - 1;
			Pel *leftFiltered = refLeftFlt + width - 1;

			// 33 Angle modes once
			ALIGN_VAR_32(Pel, buf_trans[32 * 32]);
			ALIGN_VAR_32(Pel, tmp[33 * 32 * 32]);
			int scaleWidth = width;
			int scaleStride = stride;
			int costMultiplier = 1;

			if (width > 32)
			{
				// origin is 64x64, we scale to 32x32 and setup required parameters
				ALIGN_VAR_32(Pel, bufScale[32 * 32]);
				primitives.scale2D_64to32(bufScale, fenc, stride);
				fenc = bufScale;

				// reserve space in case primitives need to store data in above
				// or left buffers
				Pel _above[4 * 32 + 1];
				Pel _left[4 * 32 + 1];
				Pel *aboveScale = _above + 2 * 32;
				Pel *leftScale = _left + 2 * 32;
				aboveScale[0] = leftScale[0] = above[0];
				primitives.scale1D_128to64(aboveScale + 1, above + 1, 0);
				primitives.scale1D_128to64(leftScale + 1, left + 1, 0);

				scaleWidth = 32;
				scaleStride = 32;
				costMultiplier = 4;

				// Filtered and Unfiltered refAbove and refLeft pointing to above and left.
				above = aboveScale;
				left = leftScale;
				aboveFiltered = aboveScale;
				leftFiltered = leftScale;
			}

			int log2SizeMinus2 = g_convertToBit[scaleWidth];
			pixelcmp_t sa8d = primitives.sa8d[log2SizeMinus2];
			//  获取 square的 SAD，
			// 			4x4 8x8 16x16 32x32 64x64
			//	org		0	 1	  2     3     4
			// 	correct	0    1    4     11    18
			int squareBlockIdx[5] = { 0, 1, 4, 11, 18 };
			pixelcmp_t sad = primitives.sad[squareBlockIdx[log2SizeMinus2]];


			// DC
			primitives.intra_pred[log2SizeMinus2][DC_IDX](tmp, scaleStride, left, above, 0, (scaleWidth <= 16));
			modeCosts[DC_IDX] = costMultiplier * sa8d(fenc, scaleStride, tmp, scaleStride);
			modeCosts_SAD[DC_IDX] = costMultiplier * sad(fenc, scaleStride, tmp, scaleStride);

			Pel *abovePlanar = above;
			Pel *leftPlanar = left;

			if (width >= 8 && width <= 32)
			{
				abovePlanar = aboveFiltered;
				leftPlanar = leftFiltered;
			}

			// PLANAR
			primitives.intra_pred[log2SizeMinus2][PLANAR_IDX](tmp, scaleStride, leftPlanar, abovePlanar, 0, 0);
			modeCosts[PLANAR_IDX] = costMultiplier * sa8d(fenc, scaleStride, tmp, scaleStride);
			modeCosts_SAD[PLANAR_IDX] = costMultiplier * sad(fenc, scaleStride, tmp, scaleStride);

			// Transpose NxN
			primitives.transpose[log2SizeMinus2](buf_trans, fenc, scaleStride);

			primitives.intra_pred_allangs[log2SizeMinus2](tmp, above, left, aboveFiltered, leftFiltered, (scaleWidth <= 16));

			for (uint32_t mode = 2; mode < numModesAvailable; mode++)
			{
				bool modeHor = (mode < 18);
				Pel *cmp = (modeHor ? buf_trans : fenc);
				intptr_t srcStride = (modeHor ? scaleWidth : scaleStride);
				modeCosts[mode] = costMultiplier * sa8d(cmp, srcStride, &tmp[(mode - 2) * (scaleWidth * scaleWidth)], scaleWidth);
				modeCosts_SAD[mode] = costMultiplier * sad(cmp, srcStride, &tmp[(mode - 2) * (scaleWidth * scaleWidth)], scaleWidth);
			}

#if 0
			uint32_t temp_mode_satd;
			FILE *fp = fopen("resi.txt", "wb+");
			temp_mode_satd = modeCosts[mode];
			for (uint32_t mode = 0; mode < numModesAvailable; mode++)
			{
				if (temp_mode_satd < modeCosts[mode])
					temp_mode_satd = modeCosts[mode];
			}

			uint32_t temp_mode_sad;
			temp_mode_sad = modeCosts_SAD[mode];
			for (uint32_t mode = 0; mode < numModesAvailable; mode++)
			{
				if (temp_mode_sad < modeCosts_SAD[mode])
					temp_mode_sad = modeCosts_SAD[mode];
			}


			int preds_sad[3] = { -1, -1, -1 };
			int mode_sad = -1;
			int numCand_sad = 3;
			cu->getIntraDirLumaPredictor(partOffset, preds_sad, &mode_sad);

			if(mode_sad >= 0)
				numCand_sad = mode_sad;
#endif

			// Find N least cost modes. N = numModesForFullRD
			for (uint32_t mode = 0; mode < numModesAvailable; mode++)
			{
#ifdef RK_INTRA_SAD_REPALCE_SATD
				uint32_t nSad = modeCosts_SAD[mode];
#else
				uint32_t nSad = modeCosts[mode];
#endif
				uint32_t bits = xModeBitsIntra(cu, mode, partOffset, depth, initTrDepth);
#if GET_X265_ORG_DATA
				if (depth != 0)
				{
					g_intra_pu_lumaDir_bits[depth + initTrDepth][cu->getZorderIdxInCU()+partOffset][mode] = bits;
					uint64_t temp = ((x265::TEncSbac*)(m_entropyCoder->m_entropyCoderIf))->m_binIf->m_fracBits;
					g_intra_est_bit_luma_pred_mode_all_case[depth + initTrDepth][cu->getZorderIdxInCU()+partOffset][mode] = temp;
				}
#else
				if (depth != 0)
				{
					g_intra_pu_lumaDir_bits[depth + initTrDepth][cu->getZorderIdxInCU()+partOffset][mode] = bits;
				}
#endif
#if 0
				for(int j=0; j<numCand_sad; j++)
				if(mode == preds_sad[j]){
					if(j == 0)
						bits = 2;
					else
						bits = 3;
					break;
				}
				else
					bits = 6;
#endif

				uint64_t cost = m_rdCost->calcRdSADCost(nSad, bits);
				if (!INTRA_REDUCE_DIR(mode,width))
					cost = MAX_INT64;// exclude the directions.
#ifdef X265_INTRA_DEBUG
				// 存储所有的bits
				m_rkIntraPred->rk_bits[partOffset][mode] = bits;
				m_rkIntraPred->rk_modeCostsSadAndCabacCorrect[mode] = cost;
				m_rkIntraPred->rk_lambdaMotionSAD = m_rdCost->getlambdaMotionSAD();

#endif
#ifdef RK_INTRA_MODE_CHOOSE
				modeCostsWithCabac[mode] = cost;
				candNum += xUpdateCandList(mode, cost, 35, rdModeList, candCostList);
#else
				candNum += xUpdateCandList(mode, cost, numModesForFullRD, rdModeList, candCostList);
#endif
			}

			int preds[3] = { -1, -1, -1 };
			int mode = -1;
			int numCand = 3;
			cu->getIntraDirLumaPredictor(partOffset, preds, &mode);
			if (mode >= 0)
			{
				numCand = mode;
			}

			for (int j = 0; j < numCand; j++)
			{
				bool mostProbableModeIncluded = false;
				int mostProbableMode = preds[j];

				for (int i = 0; i < numModesForFullRD; i++)
				{
					mostProbableModeIncluded |= (mostProbableMode == rdModeList[i]);
				}

				if (!mostProbableModeIncluded)
				{
					rdModeList[numModesForFullRD++] = mostProbableMode;
				}
			}

#ifdef RK_INTRA_MODE_CHOOSE
			bool missflag[2] = {true,true};
			assert(rk_candidate[18] == -1);
			m_rkIntraPredFast->RkIntraPriorModeChoose(rk_candidate,modeCosts,false);

			RK_HEVC_FPRINT(m_rkIntraPredFast->rk_logIntraPred[1],"%02d\n",rdModeList[0]);
			for (uint32_t mode = 0; mode < 18; mode++)
			{
				RK_HEVC_FPRINT(m_rkIntraPredFast->rk_logIntraPred[1],"%02d ",rk_candidate[mode] );
				// 判断是否命中
				if ( rdModeList[0] == rk_candidate[mode] )
				{
					missflag[0] = false;
				}

			}
			RK_HEVC_FPRINT(m_rkIntraPredFast->rk_logIntraPred[1],"\n");

			// 从rk_candidate的18种选择SAD/SATD最小的作为 bestMode
			m_rkIntraPredFast->RkIntraPriorModeChoose(rk_candidate,modeCostsWithCabac,true);

			if ( rdModeList[0] == rk_candidate[0] )
			{
				missflag[1] = false;
			}
			if ( missflag[0] != missflag[1] )
			{
				missflag[0] = missflag[0];
			}
			RK_HEVC_FPRINT(m_rkIntraPredFast->rk_logIntraPred[1],"\n");
			if( !missflag[0] )
			{
				RK_HEVC_FPRINT(m_rkIntraPredFast->rk_logIntraPred[1],"yes\n");
			}
			else
			{
				RK_HEVC_FPRINT(m_rkIntraPredFast->rk_logIntraPred[1],"miss\n");
			}
#endif


		}
		else
		{
			for (int i = 0; i < numModesForFullRD; i++)
			{
				rdModeList[i] = i;
			}
		}
		x265_emms();

		//===== check modes (using r-d costs) =====
		uint32_t bestPUMode = 0;
		uint32_t bestPUDistY = 0;
		uint32_t bestPUDistC = 0;
		uint64_t bestPUCost = MAX_INT64;
		if (RK_INTRA_RDO_ONLY_ONCE) //用satd代替原来的full rdo
		{
#ifndef RK_INTRA_MODE_CHOOSE
			bestPUMode = rdModeList[0];
#else
			if ( rk_candidate[0] < 0 )
			{
				rk_candidate[0] = rk_candidate[0];
			}
			assert(rk_candidate[0] >= 0);
			bestPUMode = rk_candidate[0];
#endif
		}
		else
		{
			for (uint32_t mode = 0; mode < numModesForFullRD; mode++)
			{
				// set luma prediction mode
				uint32_t origMode = rdModeList[mode];

				cu->setLumaIntraDirSubParts(origMode, partOffset, depth + initTrDepth);

				// set context models
				m_rdGoOnSbacCoder->load(m_rdSbacCoders[depth][CI_CURR_BEST]);
#if INTRA_PU_4x4_MODIFY
				m_rdGoOnSbacCoder->load(m_rdSbacCoders[depth][CI_TEMP_BEST]);
#endif
				// determine residual for partition
				uint32_t puDistY = 0;
				uint32_t puDistC = 0;
				uint64_t puCost = 0;
				xRecurIntraCodingQT(cu, initTrDepth, partOffset, bLumaOnly, fencYuv, predYuv, resiYuv, puDistY, puDistC, true, puCost);

				// check r-d cost
				if (puCost < bestPUCost)
				{
					bestPUMode = origMode;
					bestPUDistY = puDistY;
					bestPUDistC = puDistC;
					bestPUCost = puCost;

					xSetIntraResultQT(cu, initTrDepth, partOffset, bLumaOnly, reconYuv);

					uint32_t qPartNum = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + initTrDepth) << 1);
					::memcpy(m_qtTempTrIdx, cu->getTransformIdx() + partOffset, qPartNum * sizeof(UChar));
					::memcpy(m_qtTempCbf[0], cu->getCbf(TEXT_LUMA) + partOffset, qPartNum * sizeof(UChar));
					::memcpy(m_qtTempCbf[1], cu->getCbf(TEXT_CHROMA_U) + partOffset, qPartNum * sizeof(UChar));
					::memcpy(m_qtTempCbf[2], cu->getCbf(TEXT_CHROMA_V) + partOffset, qPartNum * sizeof(UChar));
					::memcpy(m_qtTempTransformSkipFlag[0], cu->getTransformSkip(TEXT_LUMA) + partOffset, qPartNum * sizeof(UChar));
					::memcpy(m_qtTempTransformSkipFlag[1], cu->getTransformSkip(TEXT_CHROMA_U) + partOffset, qPartNum * sizeof(UChar));
					::memcpy(m_qtTempTransformSkipFlag[2], cu->getTransformSkip(TEXT_CHROMA_V) + partOffset, qPartNum * sizeof(UChar));
				}
			} // Mode loop
		}

		{
			uint32_t origMode = bestPUMode;

			cu->setLumaIntraDirSubParts(origMode, partOffset, depth + initTrDepth);

			// set context models
			m_rdGoOnSbacCoder->load(m_rdSbacCoders[depth][CI_CURR_BEST]);
#if INTRA_PU_4x4_MODIFY
			m_rdGoOnSbacCoder->load(m_rdSbacCoders[depth][CI_TEMP_BEST]);
#endif
			// determine residual for partition
			uint32_t puDistY = 0;
			uint32_t puDistC = 0;
			uint64_t puCost = 0;
#ifdef X265_INTRA_DEBUG
			if (( initTrDepth == 1 ) && (width != 4))
			{
				RK_HEVC_PRINT("NxN and width = %d\n",width);
			}
			if ( cu->getWidth(0) != SIZE_64x64)
			{
				// xRecurIntraCodingQT 中有可能1整块处理，也有可能分4块处理
				// RK的硬件流程上层不存在4块的可能，只存储32x32 16x16 8x8的数据块
				// 4x4由内部完成
				rk_Interface_Intra.fenc 			= (Pel*)X265_MALLOC(Pel, 32*32);
				rk_Interface_Intra.reconEdgePixel 	= (Pel*)X265_MALLOC(Pel, 64+64+1);// 指向左下角，不是指向中间
				rk_Interface_Intra.bNeighborFlags 	= (bool*)X265_MALLOC(Pel, 4 * MAX_NUM_SPU_W + 1); // 指向起始
				rk_Interface_Intra.pred 			= (Pel*)X265_MALLOC(Pel, 32*32);
				rk_Interface_Intra.resi				= (int16_t*)X265_MALLOC(int16_t, 32*32);

				::memset(rk_Interface_Intra.fenc,0, 32*32);
				::memset(rk_Interface_Intra.reconEdgePixel,0, 64+64+1);
				::memset(rk_Interface_Intra.bNeighborFlags,0, 4 * MAX_NUM_SPU_W + 1);
				::memset(rk_Interface_Intra.pred,0, 32*32);
				::memset(rk_Interface_Intra.resi,0, 2*32*32);

			}
#endif
			xRecurIntraCodingQT(cu, initTrDepth, partOffset, bLumaOnly, fencYuv, predYuv, resiYuv, puDistY, puDistC, false, puCost);
#if INTRA_PU_4x4_MODIFY
			m_rdGoOnSbacCoder->store(m_rdSbacCoders[depth][CI_TEMP_BEST]);
#endif
#ifdef X265_INTRA_DEBUG
			//if ( cu->getWidth(0) != SIZE_64x64)
			if ( MATCH_CASE(cu->getWidth(0),cu->getPartitionSize(0)) )
			{
				assert(partOffset < 4);
				// 查看这个log，注意最好把wpp关掉，不然是多线程交叉显示
				//RK_HEVC_PRINT("[width = %d] partOffset %d in [%d] dirmode  is %d\n",
				//	cu->getWidth(0),partOffset, numPU, origMode);
				// bestMode
				m_rkIntraPred->rk_bestMode[X265_COMPENT][partOffset] = bestPUMode;
				// pred

				// resi,在xIntraCodingLumaBlk中赋值


				// intra_proc
				rk_Interface_Intra.cidx = 0;
				m_rkIntraPred->RkIntra_proc(&rk_Interface_Intra,
					partOffset,
					depth + initTrDepth,
					cu->getCUPelX()%64,
					cu->getCUPelY()%64);

				// TODO
				// QT proc

				// check
#ifdef LOG_INTRA_PARAMS_2_FILE
				// logLumaParam2File
				if ( initTrDepth == 0 )// 屏蔽 4x4 这一层
				{
					// log rk_Interface_Intra.bNeighborFlags
					bool* bNeighbour = rk_Interface_Intra.bNeighborFlags;
					uint32_t x_pos = cu->m_cuPelX % 64;          ///< CU position in a pixel (X)
					uint32_t y_pos = cu->m_cuPelY % 64;          ///< CU position in a pixel (Y)
					RK_HEVC_FPRINT(m_rkIntraPred->rk_logIntraPred[0],"[cu_x = %d] [cu_y = %d]\n", cu->m_cuPelX&(~63), cu->m_cuPelY&(~63));
					RK_HEVC_FPRINT(m_rkIntraPred->rk_logIntraPred[0],"[x_pos = %d] [y_pos = %d]\n", x_pos, y_pos);
					RK_HEVC_FPRINT(m_rkIntraPred->rk_logIntraPred[0],"[num = %d]\n",rk_Interface_Intra.numintraNeighbor);
					RK_HEVC_FPRINT(m_rkIntraPred->rk_logIntraPred[0],"[size = %d]\n",cu->getWidth(0) >> initTrDepth);
					RK_HEVC_FPRINT(m_rkIntraPred->rk_logIntraPred[0],"[initTrDepth = %d]\n",initTrDepth);
					RK_HEVC_FPRINT(m_rkIntraPred->rk_logIntraPred[0],"[part:%d]\n",partOffset);
					for (uint8_t i = 0 ; i < 2*rk_Interface_Intra.size/MIN_PU_SIZE; i++ )
					{
						RK_HEVC_FPRINT(m_rkIntraPred->rk_logIntraPred[0],"%d ",*bNeighbour++ );
					}
					RK_HEVC_FPRINT(m_rkIntraPred->rk_logIntraPred[0],"	 %d   ",*bNeighbour++ );
					for (uint8_t i = 0 ; i < 2*rk_Interface_Intra.size/MIN_PU_SIZE ; i++ )
					{
						RK_HEVC_FPRINT(m_rkIntraPred->rk_logIntraPred[0],"%d ",*bNeighbour++ );
					}
					RK_HEVC_FPRINT(m_rkIntraPred->rk_logIntraPred[0],"\n\n");

				}

				logRefAndFencParam2File(m_rkIntraPred->rk_logIntraPred[0], initTrDepth, rk_Interface_Intra);
#endif
				// free rk_Interface_Intra
				X265_FREE(rk_Interface_Intra.fenc);
				X265_FREE(rk_Interface_Intra.reconEdgePixel);
				X265_FREE(rk_Interface_Intra.bNeighborFlags);
				X265_FREE(rk_Interface_Intra.pred);
				X265_FREE(rk_Interface_Intra.resi);


			}
#endif

			// check r-d cost
			if (puCost < bestPUCost)
			{

				bestPUMode = origMode;
				bestPUDistY = puDistY;
				bestPUDistC = puDistC;
				bestPUCost = puCost;

				xSetIntraResultQT(cu, initTrDepth, partOffset, bLumaOnly, reconYuv);

				uint32_t qPartNum = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + initTrDepth) << 1);
				::memcpy(m_qtTempTrIdx, cu->getTransformIdx() + partOffset, qPartNum * sizeof(UChar));
				::memcpy(m_qtTempCbf[0], cu->getCbf(TEXT_LUMA) + partOffset, qPartNum * sizeof(UChar));
				::memcpy(m_qtTempCbf[1], cu->getCbf(TEXT_CHROMA_U) + partOffset, qPartNum * sizeof(UChar));
				::memcpy(m_qtTempCbf[2], cu->getCbf(TEXT_CHROMA_V) + partOffset, qPartNum * sizeof(UChar));
				::memcpy(m_qtTempTransformSkipFlag[0], cu->getTransformSkip(TEXT_LUMA) + partOffset, qPartNum * sizeof(UChar));
				::memcpy(m_qtTempTransformSkipFlag[1], cu->getTransformSkip(TEXT_CHROMA_U) + partOffset, qPartNum * sizeof(UChar));
				::memcpy(m_qtTempTransformSkipFlag[2], cu->getTransformSkip(TEXT_CHROMA_V) + partOffset, qPartNum * sizeof(UChar));
			}
		}

		//--- update overall distortion ---
		overallDistY += bestPUDistY;
		overallDistC += bestPUDistC;

		//--- update transform index and cbf ---
		uint32_t qPartNum = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + initTrDepth) << 1);
		::memcpy(cu->getTransformIdx() + partOffset, m_qtTempTrIdx, qPartNum * sizeof(UChar));
		::memcpy(cu->getCbf(TEXT_LUMA) + partOffset, m_qtTempCbf[0], qPartNum * sizeof(UChar));
		::memcpy(cu->getCbf(TEXT_CHROMA_U) + partOffset, m_qtTempCbf[1], qPartNum * sizeof(UChar));
		::memcpy(cu->getCbf(TEXT_CHROMA_V) + partOffset, m_qtTempCbf[2], qPartNum * sizeof(UChar));
		::memcpy(cu->getTransformSkip(TEXT_LUMA) + partOffset, m_qtTempTransformSkipFlag[0], qPartNum * sizeof(UChar));
		::memcpy(cu->getTransformSkip(TEXT_CHROMA_U) + partOffset, m_qtTempTransformSkipFlag[1], qPartNum * sizeof(UChar));
		::memcpy(cu->getTransformSkip(TEXT_CHROMA_V) + partOffset, m_qtTempTransformSkipFlag[2], qPartNum * sizeof(UChar));
		//--- set reconstruction for next intra prediction blocks ---
		if (pu != numPU - 1)
		{
			bool bSkipChroma = false;
			bool bChromaSame = false;
			uint32_t trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> (cu->getDepth(0) + initTrDepth)] + 2;
			if (!bLumaOnly && trSizeLog2 == 2)
			{
				assert(initTrDepth > 0);
				bSkipChroma = (pu != 0);
				bChromaSame = true;
			}

			uint32_t compWidth = cu->getWidth(0) >> initTrDepth;
			uint32_t compHeight = cu->getHeight(0) >> initTrDepth;
			uint32_t zorder = cu->getZorderIdxInCU() + partOffset;
			int      part = partitionFromSizes(compWidth, compHeight);
			Pel*     dst = cu->getPic()->getPicYuvRec()->getLumaAddr(cu->getAddr(), zorder);
			uint32_t dststride = cu->getPic()->getPicYuvRec()->getStride();
			Pel*     src = reconYuv->getLumaAddr(partOffset);
			uint32_t srcstride = reconYuv->getStride();
			primitives.luma_copy_pp[part](dst, dststride, src, srcstride);

			if (!bLumaOnly && !bSkipChroma)
			{
				dst = cu->getPic()->getPicYuvRec()->getCbAddr(cu->getAddr(), zorder);
				dststride = cu->getPic()->getPicYuvRec()->getCStride();
				src = reconYuv->getCbAddr(partOffset);
				srcstride = reconYuv->getCStride();
				if (bChromaSame)
					primitives.luma_copy_pp[part](dst, dststride, src, srcstride);
				else
					primitives.blockcpy_pp(compWidth, compHeight, dst, dststride, src, srcstride);

				dst = cu->getPic()->getPicYuvRec()->getCrAddr(cu->getAddr(), zorder);
				src = reconYuv->getCrAddr(partOffset);
				if (bChromaSame)
					primitives.luma_copy_pp[part](dst, dststride, src, srcstride);
				else
					primitives.blockcpy_pp(compWidth, compHeight, dst, dststride, src, srcstride);
			}
		}

		//=== update PU data ====
		cu->setLumaIntraDirSubParts(bestPUMode, partOffset, depth + initTrDepth);
		cu->copyToPic(depth, pu, initTrDepth);
	} // PU loop

	if (numPU > 1)
	{ // set Cbf for all blocks
		uint32_t combCbfY = 0;
		uint32_t combCbfU = 0;
		uint32_t combCbfV = 0;
		uint32_t partIdx = 0;
		for (uint32_t part = 0; part < 4; part++, partIdx += qNumParts)
		{
			combCbfY |= cu->getCbf(partIdx, TEXT_LUMA, 1);
			combCbfU |= cu->getCbf(partIdx, TEXT_CHROMA_U, 1);
			combCbfV |= cu->getCbf(partIdx, TEXT_CHROMA_V, 1);
		}

		for (uint32_t offs = 0; offs < 4 * qNumParts; offs++)
		{
			cu->getCbf(TEXT_LUMA)[offs] |= combCbfY;
			cu->getCbf(TEXT_CHROMA_U)[offs] |= combCbfU;
			cu->getCbf(TEXT_CHROMA_V)[offs] |= combCbfV;
		}
	}

	//===== reset context models =====
	m_rdGoOnSbacCoder->load(m_rdSbacCoders[depth][CI_CURR_BEST]);

	//===== set distortion (rate and r-d costs are determined later) =====
	outDistC = overallDistC;
	cu->m_totalDistortion = overallDistY + overallDistC;
}

void TEncSearch::getBestIntraModeChroma(TComDataCU* cu, TComYuv* fencYuv, TComYuv* predYuv)
{
	uint32_t depth = cu->getDepth(0);
	uint32_t trDepth = 0;
	uint32_t absPartIdx = 0;
	uint32_t bestMode = 0;
	uint64_t bestCost = MAX_INT64;
	//----- init mode list -----
	uint32_t minMode = 0;
	uint32_t maxMode = NUM_CHROMA_MODE;
	uint32_t modeList[NUM_CHROMA_MODE];

	uint32_t width = cu->getWidth(0) >> (trDepth + m_hChromaShift);
	uint32_t height = cu->getHeight(0) >> (trDepth + m_vChromaShift);
	uint32_t stride = fencYuv->getCStride();
	int scaleWidth = width;
	int scaleStride = stride;
	int costMultiplier = 1;

	if (width > 32)
	{
		scaleWidth = 32;
		scaleStride = 32;
		costMultiplier = 4;
	}

	cu->getPattern()->initPattern(cu, trDepth, absPartIdx);
	cu->getPattern()->initAdiPatternChroma(cu, absPartIdx, trDepth, m_predBuf, m_predBufStride, m_predBufHeight);

	cu->getAllowedChromaDir(0, modeList);
	//----- check chroma modes -----
	for (uint32_t mode = minMode; mode < maxMode; mode++)
	{
		uint64_t cost = 0;
		for (int chromaId = 0; chromaId < 2; chromaId++)
		{
			int sad = 0;
			uint32_t chromaPredMode = modeList[mode];
			if (chromaPredMode == DM_CHROMA_IDX)
				chromaPredMode = cu->getLumaIntraDir(0);
			Pel*     fenc = (chromaId > 0 ? fencYuv->getCrAddr(absPartIdx) : fencYuv->getCbAddr(absPartIdx));
			Pel*     pred = (chromaId > 0 ? predYuv->getCrAddr(absPartIdx) : predYuv->getCbAddr(absPartIdx));

			Pel* chromaPred = (chromaId > 0 ? cu->getPattern()->getAdiCrBuf(width, height, m_predBuf) : cu->getPattern()->getAdiCbBuf(width, height, m_predBuf));

			//===== get prediction signal =====
			predIntraChromaAng(chromaPred, chromaPredMode, pred, stride, width);
			int log2SizeMinus2 = g_convertToBit[scaleWidth];
			pixelcmp_t sa8d = primitives.sa8d[log2SizeMinus2];
			sad = costMultiplier * sa8d(fenc, scaleStride, pred, scaleStride);
			cost += sad;
		}

		//----- compare -----
		if (cost < bestCost)
		{
			bestCost = cost;
			bestMode = modeList[mode];
		}
	}

	cu->setChromIntraDirSubParts(bestMode, 0, depth);
}

void TEncSearch::estIntraPredChromaQT(TComDataCU* cu,
	TComYuv*    fencYuv,
	TComYuv*    predYuv,
	TShortYUV*  resiYuv,
	TComYuv*    reconYuv,
	uint32_t    preCalcDistC)
{
	uint32_t depth = cu->getDepth(0);
	uint32_t bestMode = 0;
	uint32_t bestDist = 0;
	uint64_t bestCost = MAX_INT64;

	//----- init mode list -----
	uint32_t minMode = 0;
	uint32_t maxMode = NUM_CHROMA_MODE;
	uint32_t modeList[NUM_CHROMA_MODE];

	cu->getAllowedChromaDir(0, modeList);

#if RK_INTRA_RDO_ONLY_ONCE
#if 0
	/*chroma 方向从5个方向中用satd或sad或sse选一个方向做FULL RDO   add by hdl*/
	//这种方式行不通，因此关掉  add by hdl
	uint32_t outDistCr = 0, outDistCb = 0, outDist = 0;//, outBestDist = MAX_INT;
	uint64_t BestCost = MAX_INT64;
	char BestIdx = 0;
	for (char mode = minMode; mode < maxMode; mode++)
	{
		m_rdGoOnSbacCoder->load(m_rdSbacCoders[depth][CI_CURR_BEST]);
		cu->setChromIntraDirSubParts(modeList[mode], 0, depth);
		char CostMode = 0; //0: satd, 1: sad, 2: sse
		xIntraCodingChromaBlk(cu, fencYuv, predYuv, outDistCb, 0, CostMode);
		xIntraCodingChromaBlk(cu, fencYuv, predYuv, outDistCr, 1, CostMode);
		outDist = outDistCb + outDistCr;
		uint32_t bits = xGetIntraBitsQT(cu, 0, 0);
		//uint64_t cost = m_rdCost->calcRdCost(outDist, bits);
		uint64_t cost = m_rdCost->calcRdSADCost(outDist, bits);

		if (cost < BestCost)
		{
			BestIdx = mode;
			BestCost = cost;
		}
	}
	modeList[0] = modeList[BestIdx];
	maxMode = 1;
	/*chroma 方向从5个方向中用satd或sad或sse选一个方向做FULL RDO   add by hdl*/
#else
	/*chroma 方向选择跟luma方向一致  add by hdl*/
	minMode = 4;
	/*chroma 方向选择跟luma方向一致  add by hdl*/
#endif //end if 1/0
#endif //end RK_INTRA_RDO_ONLY_ONCE
	//----- check chroma modes -----
	for (uint32_t mode = minMode; mode < maxMode; mode++)
	{
		//----- restore context models -----
		m_rdGoOnSbacCoder->load(m_rdSbacCoders[depth][CI_CURR_BEST]);

		//----- chroma coding -----
		uint32_t dist = 0;
		cu->setChromIntraDirSubParts(modeList[mode], 0, depth);

#ifdef X265_INTRA_DEBUG
		if( cu->getWidth(0) != SIZE_64x64)
		{
			// xRecurIntraChromaCodingQT 不关心下划的可能
			rk_Interface_IntraCb.fenc 			= (Pel*)X265_MALLOC(Pel, 16*16);
			rk_Interface_IntraCb.reconEdgePixel = (Pel*)X265_MALLOC(Pel, 32+32+1);// 指向左下角，不是指向中间
			rk_Interface_IntraCb.bNeighborFlags = (bool*)X265_MALLOC(Pel, 4 * MAX_NUM_SPU_W + 1); // 指向起始
			rk_Interface_IntraCb.pred 			= (Pel*)X265_MALLOC(Pel, 16*16);
			rk_Interface_IntraCb.resi			= (int16_t*)X265_MALLOC(int16_t, 16*16);

			// xRecurIntraChromaCodingQT 不关心下划的可能
			rk_Interface_IntraCr.fenc 			= (Pel*)X265_MALLOC(Pel, 16*16);
			rk_Interface_IntraCr.reconEdgePixel = (Pel*)X265_MALLOC(Pel, 32+32+1);// 指向左下角，不是指向中间
			rk_Interface_IntraCr.bNeighborFlags = (bool*)X265_MALLOC(Pel, 4 * MAX_NUM_SPU_W + 1); // 指向起始
			rk_Interface_IntraCr.pred 			= (Pel*)X265_MALLOC(Pel, 16*16);
			rk_Interface_IntraCr.resi			= (int16_t*)X265_MALLOC(int16_t, 16*16);

		}
#endif

		xRecurIntraChromaCodingQT(cu, 0, 0, fencYuv, predYuv, resiYuv, dist);

#ifdef X265_INTRA_DEBUG
		//if (( cu->getWidth(0) != SIZE_64x64) && ( cu->getPartitionSize(0) == SIZE_2Nx2N )) // 屏蔽4x4
		if ( MATCH_CASE(cu->getWidth(0),cu->getPartitionSize(0)) )
		{
			//RK_HEVC_PRINT("[width = %d] partOffset %d in [%d] dirmode  is %d\n",
			//	cu->getWidth(0) >> m_hChromaShift, 0, 0, cu->getLumaIntraDir(0));
			// bestMode
			// m_rkIntraPred->rk_bestMode[X265_COMPENT][0] = modeList[mode];
			// pred

			// resi,在xIntraCodingLumaBlk中赋值


			// intra_proc [Cb]
			rk_Interface_IntraCb.cidx = 1;
			m_rkIntraPred->RkIntra_proc(&rk_Interface_IntraCb,
				0,
				depth,
				cu->getCUPelX()%64,
				cu->getCUPelY()%64);



			// intra_proc [Cr]
			rk_Interface_IntraCr.cidx = 2;
			m_rkIntraPred->RkIntra_proc(&rk_Interface_IntraCr,
				0,
				depth,
				cu->getCUPelX()%64,
				cu->getCUPelY()%64);


			// check
#ifdef LOG_INTRA_PARAMS_2_FILE
			// 4x4 的chroma会做2次，一个对应 8x8 一个对应 4个 4x4
			// 屏蔽 4x4
			if ( cu->getPartitionSize(0) == SIZE_2Nx2N)
			{
				logRefAndFencParam2File(m_rkIntraPred->rk_logIntraPred[0],0, rk_Interface_IntraCb);
				logRefAndFencParam2File(m_rkIntraPred->rk_logIntraPred[0],0, rk_Interface_IntraCr);
			}

#endif


			// free rk_Interface_Intra
			X265_FREE(rk_Interface_IntraCb.fenc);
			X265_FREE(rk_Interface_IntraCb.reconEdgePixel);
			X265_FREE(rk_Interface_IntraCb.bNeighborFlags);
			X265_FREE(rk_Interface_IntraCb.pred);
			X265_FREE(rk_Interface_IntraCb.resi);

			X265_FREE(rk_Interface_IntraCr.fenc);
			X265_FREE(rk_Interface_IntraCr.reconEdgePixel);
			X265_FREE(rk_Interface_IntraCr.bNeighborFlags);
			X265_FREE(rk_Interface_IntraCr.pred);
			X265_FREE(rk_Interface_IntraCr.resi);
		}
#endif

		if (cu->getSlice()->getPPS()->getUseTransformSkip())
		{
			m_rdGoOnSbacCoder->load(m_rdSbacCoders[depth][CI_CURR_BEST]);
		}


		uint32_t bits = xGetIntraBitsQT(cu, 0, 0, false, true);
		uint64_t cost = m_rdCost->calcRdCost(dist, bits);

		//----- compare -----
		if (cost < bestCost)
		{
			bestCost = cost;
			bestDist = dist;
			bestMode = modeList[mode];
			uint32_t qpn = cu->getPic()->getNumPartInCU() >> (depth << 1);
			xSetIntraResultChromaQT(cu, 0, 0, reconYuv);
			::memcpy(m_qtTempCbf[1], cu->getCbf(TEXT_CHROMA_U), qpn * sizeof(UChar));
			::memcpy(m_qtTempCbf[2], cu->getCbf(TEXT_CHROMA_V), qpn * sizeof(UChar));
			::memcpy(m_qtTempTransformSkipFlag[1], cu->getTransformSkip(TEXT_CHROMA_U), qpn * sizeof(UChar));
			::memcpy(m_qtTempTransformSkipFlag[2], cu->getTransformSkip(TEXT_CHROMA_V), qpn * sizeof(UChar));
		}
	}

	//----- set data -----
	uint32_t qpn = cu->getPic()->getNumPartInCU() >> (depth << 1);
	::memcpy(cu->getCbf(TEXT_CHROMA_U), m_qtTempCbf[1], qpn * sizeof(UChar));
	::memcpy(cu->getCbf(TEXT_CHROMA_V), m_qtTempCbf[2], qpn * sizeof(UChar));
	::memcpy(cu->getTransformSkip(TEXT_CHROMA_U), m_qtTempTransformSkipFlag[1], qpn * sizeof(UChar));
	::memcpy(cu->getTransformSkip(TEXT_CHROMA_V), m_qtTempTransformSkipFlag[2], qpn * sizeof(UChar));
	cu->setChromIntraDirSubParts(bestMode, 0, depth);
	cu->m_totalDistortion += bestDist - preCalcDistC;

	//----- restore context models -----
	m_rdGoOnSbacCoder->load(m_rdSbacCoders[depth][CI_CURR_BEST]);
}
#if RK_INTRA_RDO_ONLY_ONCE
void TEncSearch::xIntraCodingChromaBlk(TComDataCU* cu,
	TComYuv*    fencYuv,
	TComYuv*    predYuv,
	uint32_t&   outDist,
	uint32_t    chromaId,
	char         CostMode)
{
	uint32_t absPartIdx = 0;
	uint32_t trDepth = 0;
	uint32_t fullDepth   = cu->getDepth(0) + trDepth;
	uint32_t trSizeLog2  = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> fullDepth] + 2;

	if (trSizeLog2 == 2)
	{
		assert(trDepth > 0);
		trDepth--;
		uint32_t qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + trDepth) << 1);
		bool bFirstQ = ((absPartIdx % qpdiv) == 0);
		if (!bFirstQ)
		{
			return;
		}
	}

	uint32_t chromaPredMode = cu->getChromaIntraDir(absPartIdx);
	uint32_t width          = cu->getWidth(0) >> (trDepth + m_hChromaShift);
	uint32_t height         = cu->getHeight(0) >> (trDepth + m_vChromaShift);
	uint32_t stride         = fencYuv->getCStride();
	Pel*     fenc           = (chromaId > 0 ? fencYuv->getCrAddr(absPartIdx) : fencYuv->getCbAddr(absPartIdx));
	Pel*     pred           = (chromaId > 0 ? predYuv->getCrAddr(absPartIdx) : predYuv->getCbAddr(absPartIdx));
	int part = partitionFromSizes(width, height);

	//===== update chroma mode =====
	if (chromaPredMode == DM_CHROMA_IDX)
	{
		chromaPredMode = cu->getLumaIntraDir(0);
	}
	cu->getPattern()->initPattern(cu, trDepth, absPartIdx);
	cu->getPattern()->initAdiPatternChroma(cu, absPartIdx, trDepth, m_predBuf, m_predBufStride, m_predBufHeight);

	Pel* chromaPred = (chromaId > 0 ? cu->getPattern()->getAdiCrBuf(width, height, m_predBuf) : cu->getPattern()->getAdiCbBuf(width, height, m_predBuf));
	predIntraChromaAng(chromaPred, chromaPredMode, pred, stride, width);
	switch(CostMode)
	{
	case 0: outDist = primitives.sa8d_inter[part](fenc, stride, pred, stride); break; //satd
	case 1: outDist = primitives.sad[part](fenc, stride, pred, stride); break; //sad
	case 2: outDist = primitives.sse_pp[part](fenc, stride, pred, stride); break; //sse_pp
	default: outDist = primitives.sa8d_inter[part](fenc, stride, pred, stride); break; //satd
	}
}
uint32_t TEncSearch::xGetIntraBitsQT(TComDataCU* cu, uint32_t trDepth, uint32_t absPartIdx)
{//本函数只用来实现对chroma部分的编码  add by hdl
	m_entropyCoder->resetBits();
	xEncIntraHeader(cu, trDepth, absPartIdx, false, true);
	xEncSubdivCbfQT(cu, trDepth, absPartIdx, false, true);
	return m_entropyCoder->getNumberOfWrittenBits();
}
#endif
/** Function for encoding and reconstructing luma/chroma samples of a PCM mode CU.
 * \param cu pointer to current CU
 * \param absPartIdx part index
 * \param fenc pointer to original sample arrays
 * \param pcm pointer to PCM code arrays
 * \param pred pointer to prediction signal arrays
 * \param resi pointer to residual signal arrays
 * \param reco pointer to reconstructed sample arrays
 * \param stride stride of the original/prediction/residual sample arrays
 * \param width block width
 * \param height block height
 * \param ttText texture component type
 * \returns void
 */
void TEncSearch::xEncPCM(TComDataCU* cu, uint32_t absPartIdx, Pel* fenc, Pel* pcm, Pel* pred, int16_t* resi, Pel* recon, uint32_t stride, uint32_t width, uint32_t height, TextType eText)
{
	uint32_t x, y;
	uint32_t reconStride;
	Pel* pcmTmp = pcm;
	Pel* reconPic;
	int shiftPcm;

	if (eText == TEXT_LUMA)
	{
		reconStride = cu->getPic()->getPicYuvRec()->getStride();
		reconPic = cu->getPic()->getPicYuvRec()->getLumaAddr(cu->getAddr(), cu->getZorderIdxInCU() + absPartIdx);
		shiftPcm = X265_DEPTH - cu->getSlice()->getSPS()->getPCMBitDepthLuma();
	}
	else
	{
		reconStride = cu->getPic()->getPicYuvRec()->getCStride();
		if (eText == TEXT_CHROMA_U)
		{
			reconPic = cu->getPic()->getPicYuvRec()->getCbAddr(cu->getAddr(), cu->getZorderIdxInCU() + absPartIdx);
		}
		else
		{
			reconPic = cu->getPic()->getPicYuvRec()->getCrAddr(cu->getAddr(), cu->getZorderIdxInCU() + absPartIdx);
		}
		shiftPcm = X265_DEPTH - cu->getSlice()->getSPS()->getPCMBitDepthChroma();
	}

	// zero prediction and residual
	for (y = 0; y < height; y++)
	{
		for (x = 0; x < width; x++)
		{
			pred[x] = resi[x] = 0;
		}

		pred += stride;
		resi += stride;
	}

	// Encode
	for (y = 0; y < height; y++)
	{
		for (x = 0; x < width; x++)
		{
			pcmTmp[x] = fenc[x] >> shiftPcm;
		}

		pcmTmp += width;
		fenc += stride;
	}

	pcmTmp = pcm;

	// Reconstruction
	for (y = 0; y < height; y++)
	{
		for (x = 0; x < width; x++)
		{
			recon[x] = pcmTmp[x] << shiftPcm;
			reconPic[x] = recon[x];
		}

		pcmTmp += width;
		recon += stride;
		reconPic += reconStride;
	}
}

/**  Function for PCM mode estimation.
 * \param cu
 * \param fencYuv
 * \param rpcPredYuv
 * \param rpcResiYuv
 * \param rpcRecoYuv
 * \returns void
 */
void TEncSearch::IPCMSearch(TComDataCU* cu, TComYuv* fencYuv, TComYuv* predYuv, TShortYUV* resiYuv, TComYuv* reconYuv)
{
	uint32_t depth = cu->getDepth(0);
	uint32_t width = cu->getWidth(0);
	uint32_t height = cu->getHeight(0);
	uint32_t stride = predYuv->getStride();
	uint32_t strideC = predYuv->getCStride();
	uint32_t widthC = width >> 1;
	uint32_t heightC = height >> 1;
	uint32_t distortion = 0;
	uint32_t bits;
	uint64_t cost;

	uint32_t absPartIdx = 0;
	uint32_t minCoeffSize = cu->getPic()->getMinCUWidth() * cu->getPic()->getMinCUHeight();
	uint32_t lumaOffset = minCoeffSize * absPartIdx;
	uint32_t chromaOffset = lumaOffset >> 2;

	// Luminance
	Pel*   fenc = fencYuv->getLumaAddr(0, width);
	int16_t* resi = resiYuv->getLumaAddr(0, width);
	Pel*   pred = predYuv->getLumaAddr(0, width);
	Pel*   recon = reconYuv->getLumaAddr(0, width);
	Pel*   pcm = cu->getPCMSampleY() + lumaOffset;

	xEncPCM(cu, 0, fenc, pcm, pred, resi, recon, stride, width, height, TEXT_LUMA);

	// Chroma U
	fenc = fencYuv->getCbAddr();
	resi = resiYuv->getCbAddr();
	pred = predYuv->getCbAddr();
	recon = reconYuv->getCbAddr();
	pcm = cu->getPCMSampleCb() + chromaOffset;

	xEncPCM(cu, 0, fenc, pcm, pred, resi, recon, strideC, widthC, heightC, TEXT_CHROMA_U);

	// Chroma V
	fenc = fencYuv->getCrAddr();
	resi = resiYuv->getCrAddr();
	pred = predYuv->getCrAddr();
	recon = reconYuv->getCrAddr();
	pcm = cu->getPCMSampleCr() + chromaOffset;

	xEncPCM(cu, 0, fenc, pcm, pred, resi, recon, strideC, widthC, heightC, TEXT_CHROMA_V);

	m_entropyCoder->resetBits();
	xEncIntraHeader(cu, depth, absPartIdx, true, false);
	bits = m_entropyCoder->getNumberOfWrittenBits();
	cost = m_rdCost->calcRdCost(distortion, bits);

	m_rdGoOnSbacCoder->load(m_rdSbacCoders[depth][CI_CURR_BEST]);

	cu->m_totalBits = bits;
	cu->m_totalCost = cost;
	cu->m_totalDistortion = distortion;

	cu->copyToPic(depth, 0, 0);
}

uint32_t TEncSearch::xGetInterPredictionError(TComDataCU* cu, int partIdx)
{
	uint32_t absPartIdx;
	int width, height;

	motionCompensation(cu, &m_tmpYuvPred, REF_PIC_LIST_X, partIdx, true, false);
	cu->getPartIndexAndSize(partIdx, absPartIdx, width, height);
	uint32_t cost = m_me.bufSA8D(m_tmpYuvPred.getLumaAddr(absPartIdx), m_tmpYuvPred.getStride());
	x265_emms();
	return cost;
}

/** estimation of best merge coding
 * \param cu
 * \param fencYuv
 * \param iPUIdx
 * \param uiInterDir
 * \param pacMvField
 * \param uiMergeIndex
 * \param outCost
 * \param outBits
 * \param puhNeighCands
 * \param bValid
 * \returns void
 */
void TEncSearch::xMergeEstimation(TComDataCU* cu, int puIdx, uint32_t& interDir, TComMvField* mvField, uint32_t& mergeIndex, uint32_t& outCost, uint32_t& outbits, TComMvField* mvFieldNeighbours, UChar* interDirNeighbours, int& numValidMergeCand)
{
	uint32_t absPartIdx = 0;
	int width = 0;
	int height = 0;

	cu->getPartIndexAndSize(puIdx, absPartIdx, width, height);
	uint32_t depth = cu->getDepth(absPartIdx);
	PartSize partSize = cu->getPartitionSize(0);
	if (cu->getSlice()->getPPS()->getLog2ParallelMergeLevelMinus2() && partSize != SIZE_2Nx2N && cu->getWidth(0) <= 8)
	{
		cu->setPartSizeSubParts(SIZE_2Nx2N, 0, depth);
		if (puIdx == 0)
		{
			cu->getInterMergeCandidates(0, 0, mvFieldNeighbours, interDirNeighbours, numValidMergeCand);
		}
		cu->setPartSizeSubParts(partSize, 0, depth);
	}
	else
	{
		cu->getInterMergeCandidates(absPartIdx, puIdx, mvFieldNeighbours, interDirNeighbours, numValidMergeCand);
	}
	xRestrictBipredMergeCand(cu, puIdx, mvFieldNeighbours, interDirNeighbours, numValidMergeCand);

	outCost = MAX_UINT;
	for (uint32_t mergeCand = 0; mergeCand < numValidMergeCand; ++mergeCand)
	{
		uint32_t costCand = MAX_UINT;
		uint32_t bitsCand = 0;

		cu->getCUMvField(REF_PIC_LIST_0)->m_mv[absPartIdx] = mvFieldNeighbours[0 + 2 * mergeCand].mv;
		cu->getCUMvField(REF_PIC_LIST_0)->m_refIdx[absPartIdx] = mvFieldNeighbours[0 + 2 * mergeCand].refIdx;
		cu->getCUMvField(REF_PIC_LIST_1)->m_mv[absPartIdx] = mvFieldNeighbours[1 + 2 * mergeCand].mv;
		cu->getCUMvField(REF_PIC_LIST_1)->m_refIdx[absPartIdx] = mvFieldNeighbours[1 + 2 * mergeCand].refIdx;

		costCand = xGetInterPredictionError(cu, puIdx);
		bitsCand = mergeCand + 1;
		if (mergeCand == m_cfg->param.maxNumMergeCand - 1)
		{
			bitsCand--;
		}
		costCand = costCand + m_rdCost->getCost(bitsCand);
		if (costCand < outCost)
		{
			outCost = costCand;
			outbits = bitsCand;
			mvField[0] = mvFieldNeighbours[0 + 2 * mergeCand];
			mvField[1] = mvFieldNeighbours[1 + 2 * mergeCand];
			interDir = interDirNeighbours[mergeCand];
			mergeIndex = mergeCand;
		}
	}
}

/** convert bi-pred merge candidates to uni-pred
 * \param cu
 * \param puIdx
 * \param mvFieldNeighbours
 * \param interDirNeighbours
 * \param numValidMergeCand
 * \returns void
 */
void TEncSearch::xRestrictBipredMergeCand(TComDataCU* cu, uint32_t puIdx, TComMvField* mvFieldNeighbours, UChar* interDirNeighbours, int numValidMergeCand)
{
	if (cu->isBipredRestriction(puIdx))
	{
		for (uint32_t mergeCand = 0; mergeCand < numValidMergeCand; ++mergeCand)
		{
			if (interDirNeighbours[mergeCand] == 3)
			{
				interDirNeighbours[mergeCand] = 1;
				mvFieldNeighbours[(mergeCand << 1) + 1].setMvField(MV(0, 0), -1);
			}
		}
	}
}

/** search of the best candidate for inter prediction
 * \param cu
 * \param predYuv
 * \param bUseMRG
 * \returns void
 */
#if RK_INTER_METEST
void TEncSearch::predInterSearch(TComYuv* predYuv, TComDataCU* cu)
{
	MV mvzero(0, 0);
	MV mv[2];
	MV mvBidir[2];
	MV mvTemp[2][33];
	MV mvPred[2][33];
	MV mvPredBi[2][33];

	int mvpIdxBi[2][33];
	int mvpIdx[2][33];
	int mvpNum[2][33];
	AMVPInfo amvpInfo[2][33];

	uint32_t mbBits[3] = { 1, 1, 0 };
	int refIdx[2] = { 0, 0 }; // If un-initialized, may cause SEGV in bi-directional prediction iterative stage.
	int refIdxBidir[2] = { 0, 0 };

	PartSize partSize = cu->getPartitionSize(0);
	uint32_t lastMode = 0;
	int numPart = cu->getNumPartInter();
	int numPredDir = cu->getSlice()->isInterP() ? 1 : 2;

	TComPicYuv *fenc = cu->getSlice()->getPic()->getPicYuvOrg();

	int totalmebits = 0;

	for (int partIdx = 0; partIdx < numPart; partIdx++)
	{
		uint32_t listCost[2] = { MAX_UINT, MAX_UINT };
		uint32_t bits[3];
		uint32_t costbi = MAX_UINT;
		uint32_t costTemp = 0;
		uint32_t bitsTemp;
		MV   mvValidList1;
		int  refIdxValidList1 = 0;
		uint32_t bitsValidList1 = MAX_UINT;
		uint32_t costValidList1 = MAX_UINT;

		uint32_t partAddr;
		int  roiWidth, roiHeight; //pu的宽度和高度  add by hdl
		xGetBlkBits(partSize, cu->getSlice()->isInterP(), partIdx, lastMode, mbBits);
		cu->getPartIndexAndSize(partIdx, partAddr, roiWidth, roiHeight);

		Pel* pu = fenc->getLumaAddr(cu->getAddr(), cu->getZorderIdxInCU() + partAddr);
		m_me.setSourcePU(pu - fenc->getLumaAddr(), roiWidth, roiHeight);

		/*用来实现4抽+CTU周围18个mv+7x7块全搜  add by hdl*/
		MV tmpMv[nAllNeighMv]; //保存当前CTU周围的18个mv add by hdl
		static MV TenMv[nMaxRefPic][10];
		static int nCand[nMaxRefPic] = { 0 };
		TComDataCU *CTU = cu->getPic()->getCU(cu->getAddr());//当前CU/PU所处的CTU add by hdl;
		/*用来实现4抽+CTU周围18个mv+7x7块全搜  add by hdl*/

		// Uni-directional prediction
		for (int list = 0; list < numPredDir; list++)
		{
			for (int idx = 0; idx < cu->getSlice()->getNumRefIdx(list); idx++)
			{
				bitsTemp = mbBits[list];
				if (cu->getSlice()->getNumRefIdx(list) > 1)
				{
					bitsTemp += idx + 1;
					if (idx == cu->getSlice()->getNumRefIdx(list) - 1) bitsTemp--;
				}
				MV& outmv = mvTemp[list][idx];
				/*用来实现4抽+CTU周围18个mv+7x7块全搜  add by hdl*/
				MV mvmin, mvmax;
				short merangeX = cu->getSlice()->getSPS()->getMeRangeX() / 4 * 4;
				short merangeY = cu->getSlice()->getSPS()->getMeRangeY() / 4 * 4;
				bool isHaveMvd = true;

				int nIdx = idx;
				if (list)
					nIdx = nMaxRefPic / 2 + idx;
				MV *pTenMv = TenMv[nIdx];
				uint32_t nCtuAddr = cu->getAddr();
				uint32_t numCtuInWidth = cu->getPic()->getFrameWidthInCU();
				uint32_t numCtuInHeight = cu->getPic()->getFrameHeightInCU();
				int nCtuPosWidth = nCtuAddr%numCtuInWidth;
				int nCtuPosHeight = nCtuAddr / numCtuInWidth;
				uint32_t nPicWidth = cu->getSlice()->getSPS()->getPicWidthInLumaSamples();
				uint32_t nPicHeight = cu->getSlice()->getSPS()->getPicHeightInLumaSamples();
				uint32_t offsIdx = getOffsetIdx(g_maxCUWidth, cu->getCUPelX(), cu->getCUPelY(), cu->getWidth(0));
				merangeX = MIN_MINE(384, merangeX);
				merangeY = MIN_MINE(320, merangeY);
				if (nPicWidth < merangeX + 60 || nPicWidth <= 352)
				{
					merangeX = MIN_MINE(nPicWidth, merangeX) / 4 * 2;
				}
				if (nPicHeight < merangeY + 60 || nPicHeight <= 288)
				{
					merangeY = MIN_MINE(nPicHeight, merangeY) / 4 * 2;
				}
				intptr_t blockOffset_ds = 0;
				int stride = 0;
				int nValidCtuWidth = g_maxCUWidth;
				int nValidCtuHeight = g_maxCUWidth;
				bool isSavePmv = checkSavePmv(g_maxCUWidth, cu->getWidth(0), offsIdx, nPicWidth, nPicHeight, nCtuAddr);
				static MV Mvmin, Mvmax;
				if (isSavePmv)
				{
					for (int idxTenMv = 0; idxTenMv < 10; idxTenMv++)
					{
						pTenMv[idxTenMv].x = 32767;
						pTenMv[idxTenMv].y = 32767;
					}
					bool isValid[nAllNeighMv];
					for (int i = 0; i < nAllNeighMv; i++)
					{
						tmpMv[i].x = 0;
						tmpMv[i].y = 0;
					}
					getNeighMvs(CTU, tmpMv, list, isValid, nIdx, true);
					/*从36个mv中选择2个用于确定精搜索位置*/
					int idxFirst = MAX_INT;
					for (int idy = 0; idy < nAllNeighMv; idy++)
					{
						if (isValid[idy])
						{
							idxFirst = idy;
							break;
						}
					}

					for (int idy = idxFirst; idy < nAllNeighMv - 1; idy++)
					{
						if (!isValid[idy])
							continue;
						for (int Idx = idy + 1; Idx < nAllNeighMv; Idx++)
						{
							//if (tmpMv[idx].word == tmpMv[idy].word)
							if (abs(tmpMv[Idx].x - tmpMv[idy].x) <= nRectSize && abs(tmpMv[Idx].y - tmpMv[idy].y) <= nRectSize)
							{//这里8个修改对应到motion.cpp文件里的rectsize的修改
								isValid[Idx] = false;
							}
						}
					}

					nCand[nIdx] = 0; //TenMv后三个存放mvp的替代
					int tmpCand = 0;
					for (int i = 0; i < nAllNeighMv; i++)
					{
						if (isValid[i] && nCand[nIdx] < nNeightMv)
						{
							pTenMv[nCand[nIdx]++] = tmpMv[i];
						}
						if (isValid[i] && tmpCand < 3) //replace real mvp
						{
							pTenMv[7 + tmpCand++] = tmpMv[i];
						}
					}
					if (tmpCand < 3)
					{
						for (int i = tmpCand; i < 3; i++) //如果没满3个就设置为跟第一个一样
							pTenMv[7 + i] = pTenMv[7];
					}
					/*从36个mv中选择2个用于确定精搜索位置*/

					int MarginX = g_maxCUWidth + g_nSearchRangeWidth + MAX_MINE(nRimeHeight, nRimeWidth) + 24;
					stride = cu->getSlice()->getSPS()->getPicWidthInLumaSamples() / 4 + MarginX / 2;
					blockOffset_ds = nCtuPosWidth * g_maxCUWidth / 4 + nCtuPosHeight * g_maxCUWidth / 4 * stride;
					if (nPicHeight / g_maxCUWidth < numCtuInHeight && nCtuPosHeight == numCtuInHeight - 1)
					{
						nValidCtuHeight = nPicHeight - nPicHeight / g_maxCUWidth*g_maxCUWidth;
					}
					if (nPicWidth / g_maxCUWidth < numCtuInWidth && nCtuPosWidth == numCtuInWidth - 1)
					{
						nValidCtuWidth = nPicWidth - nPicWidth / g_maxCUWidth*g_maxCUWidth;
					}
				}
				xSetSearchRange(cu, MV(0, 0), merangeX, merangeY, mvmin, mvmax);
				int satdCost = m_me.motionEstimate(m_mref[list][idx], mvmin, mvmax, outmv, pTenMv, nCand[nIdx], isHaveMvd,
					isSavePmv, offsIdx, cu->getDepth(0), blockOffset_ds, stride, nValidCtuWidth, nValidCtuHeight, nIdx);

				xEstimateMvPredAMVP(cu, partIdx, list, idx, mvPred[list][idx], outmv);
				mvpIdx[list][idx] = cu->getMVPIdx(list, partAddr);
				mvpNum[list][idx] = cu->getMVPNum(list, partAddr);
				m_me.setMVP(mvPred[list][idx]);
				bitsTemp++; // add mvp idx bit cost
				/* Get total cost of partition, but only include MV bit cost once */
				bitsTemp += m_me.bitcost(outmv); //这两步计算的开mvd的代价  add by hdl
				costTemp = satdCost + m_rdCost->getCost(bitsTemp);
				xCopyAMVPInfo(cu->getCUMvField(list)->getAMVPInfo(), &amvpInfo[list][idx]); // must always be done ( also when AMVP_MODE = AM_NONE )

				g_mvAmvp[nIdx][offsIdx][0].x = cu->getCUMvField(list)->getAMVPInfo()->m_mvCand[0].x;
				g_mvAmvp[nIdx][offsIdx][0].y = cu->getCUMvField(list)->getAMVPInfo()->m_mvCand[0].y;
				g_mvAmvp[nIdx][offsIdx][1].x = cu->getCUMvField(list)->getAMVPInfo()->m_mvCand[1].x;
				g_mvAmvp[nIdx][offsIdx][1].y = cu->getCUMvField(list)->getAMVPInfo()->m_mvCand[1].y;

				if (costTemp < listCost[list])
				{
					listCost[list] = costTemp;
					bits[list] = bitsTemp; // storing for bi-prediction

					// set motion
					mv[list] = mvTemp[list][idx];
					refIdx[list] = idx;
				}

				if (list == 1 && costTemp < costValidList1)
				{
					costValidList1 = costTemp; //list为1时保存的内容同 listCost[list]  add by hdl
					bitsValidList1 = bitsTemp; //list为1时保存的内容同 bits[list]  add by hdl

					// set motion
					mvValidList1 = mvTemp[list][idx]; //list为1时保存的内容同 mv[list]  add by hdl
					refIdxValidList1 = idx; //list为1时保存的内容同 refIdx[list]  add by hdl
				}
			}
		}

		// Bi-directional prediction
		if ((cu->getSlice()->isInterB()) && (cu->isBipredRestriction(partIdx) == false))
		{
			mvBidir[0] = mv[0];
			mvBidir[1] = mv[1];
			refIdxBidir[0] = refIdx[0];
			refIdxBidir[1] = refIdx[1];

			::memcpy(mvPredBi, mvPred, sizeof(mvPred));
			::memcpy(mvpIdxBi, mvpIdx, sizeof(mvpIdx));

			// Generate reference subpels
			xPredInterLumaBlk(cu, cu->getSlice()->getRefPic(REF_PIC_LIST_0, refIdx[0])->getPicYuvRec(), partAddr, &mv[0], roiWidth, roiHeight, &m_predYuv[0]);
			xPredInterLumaBlk(cu, cu->getSlice()->getRefPic(REF_PIC_LIST_1, refIdx[1])->getPicYuvRec(), partAddr, &mv[1], roiWidth, roiHeight, &m_predYuv[1]);

			pixel *ref0 = m_predYuv[0].getLumaAddr(partAddr); //插值后的luma图像  add by hdl
			pixel *ref1 = m_predYuv[1].getLumaAddr(partAddr); //插值后的luma图像  add by hdl

			ALIGN_VAR_32(pixel, avg[MAX_CU_SIZE * MAX_CU_SIZE]);
			//avg开两个重构之后的图像的平均，pu开原始图像信息  add by hdl
			int partEnum = partitionFromSizes(roiWidth, roiHeight);
			primitives.pixelavg_pp[partEnum](avg, roiWidth, ref0, m_predYuv[0].getStride(), ref1, m_predYuv[1].getStride(), 32);
			int satdCost = primitives.satd[partEnum](pu, fenc->getStride(), avg, roiWidth);
			x265_emms();
			bits[2] = bits[0] + bits[1] - mbBits[0] - mbBits[1] + mbBits[2];
			costbi = satdCost + m_rdCost->getCost(bits[2]);
		} // if (B_SLICE)

		//  Clear Motion Field
		cu->getCUMvField(REF_PIC_LIST_0)->setAllMvField(TComMvField(), partSize, partAddr, 0, partIdx);
		cu->getCUMvField(REF_PIC_LIST_1)->setAllMvField(TComMvField(), partSize, partAddr, 0, partIdx);
		cu->getCUMvField(REF_PIC_LIST_0)->setAllMvd(mvzero, partSize, partAddr, 0, partIdx);
		cu->getCUMvField(REF_PIC_LIST_1)->setAllMvd(mvzero, partSize, partAddr, 0, partIdx);

		cu->setMVPIdxSubParts(-1, REF_PIC_LIST_0, partAddr, partIdx, cu->getDepth(partAddr));
		cu->setMVPNumSubParts(-1, REF_PIC_LIST_0, partAddr, partIdx, cu->getDepth(partAddr));
		cu->setMVPIdxSubParts(-1, REF_PIC_LIST_1, partAddr, partIdx, cu->getDepth(partAddr));
		cu->setMVPNumSubParts(-1, REF_PIC_LIST_1, partAddr, partIdx, cu->getDepth(partAddr));

		uint32_t mebits = 0;
		// Set Motion Field_
		mv[1] = mvValidList1;
		refIdx[1] = refIdxValidList1;
		bits[1] = bitsValidList1;
		listCost[1] = costValidList1;

		if (costbi <= listCost[0] && costbi <= listCost[1])
		{
			lastMode = 2;
			{
				cu->getCUMvField(REF_PIC_LIST_0)->setAllMv(mvBidir[0], partSize, partAddr, 0, partIdx);
				cu->getCUMvField(REF_PIC_LIST_0)->setAllRefIdx(refIdxBidir[0], partSize, partAddr, 0, partIdx);
				cu->getCUMvField(REF_PIC_LIST_1)->setAllMv(mvBidir[1], partSize, partAddr, 0, partIdx);
				cu->getCUMvField(REF_PIC_LIST_1)->setAllRefIdx(refIdxBidir[1], partSize, partAddr, 0, partIdx);
			}
			{
				MV mvtmp = mvBidir[0] - mvPredBi[0][refIdxBidir[0]];
				cu->getCUMvField(REF_PIC_LIST_0)->setAllMvd(mvtmp, partSize, partAddr, 0, partIdx);
			}
			{
				MV mvtmp = mvBidir[1] - mvPredBi[1][refIdxBidir[1]];
				cu->getCUMvField(REF_PIC_LIST_1)->setAllMvd(mvtmp, partSize, partAddr, 0, partIdx);
			}

			cu->setInterDirSubParts(3, partAddr, partIdx, cu->getDepth(0));

			cu->setMVPIdxSubParts(mvpIdxBi[0][refIdxBidir[0]], REF_PIC_LIST_0, partAddr, partIdx, cu->getDepth(partAddr));
			cu->setMVPNumSubParts(mvpNum[0][refIdxBidir[0]], REF_PIC_LIST_0, partAddr, partIdx, cu->getDepth(partAddr));
			cu->setMVPIdxSubParts(mvpIdxBi[1][refIdxBidir[1]], REF_PIC_LIST_1, partAddr, partIdx, cu->getDepth(partAddr));
			cu->setMVPNumSubParts(mvpNum[1][refIdxBidir[1]], REF_PIC_LIST_1, partAddr, partIdx, cu->getDepth(partAddr));

			mebits = bits[2];
		}
		else if (listCost[0] <= listCost[1])
		{
			lastMode = 0;
			cu->getCUMvField(REF_PIC_LIST_0)->setAllMv(mv[0], partSize, partAddr, 0, partIdx);
			cu->getCUMvField(REF_PIC_LIST_0)->setAllRefIdx(refIdx[0], partSize, partAddr, 0, partIdx);
			{
				MV mvtmp = mv[0] - mvPred[0][refIdx[0]];
				cu->getCUMvField(REF_PIC_LIST_0)->setAllMvd(mvtmp, partSize, partAddr, 0, partIdx);
			}
			cu->setInterDirSubParts(1, partAddr, partIdx, cu->getDepth(0));

			cu->setMVPIdxSubParts(mvpIdx[0][refIdx[0]], REF_PIC_LIST_0, partAddr, partIdx, cu->getDepth(partAddr));
			cu->setMVPNumSubParts(mvpNum[0][refIdx[0]], REF_PIC_LIST_0, partAddr, partIdx, cu->getDepth(partAddr));

			mebits = bits[0];
		}
		else
		{
			lastMode = 1;
			cu->getCUMvField(REF_PIC_LIST_1)->setAllMv(mv[1], partSize, partAddr, 0, partIdx);
			cu->getCUMvField(REF_PIC_LIST_1)->setAllRefIdx(refIdx[1], partSize, partAddr, 0, partIdx);
			{
				MV mvtmp = mv[1] - mvPred[1][refIdx[1]];
				cu->getCUMvField(REF_PIC_LIST_1)->setAllMvd(mvtmp, partSize, partAddr, 0, partIdx);
			}
			cu->setInterDirSubParts(2, partAddr, partIdx, cu->getDepth(0));

			cu->setMVPIdxSubParts(mvpIdx[1][refIdx[1]], REF_PIC_LIST_1, partAddr, partIdx, cu->getDepth(partAddr));
			cu->setMVPNumSubParts(mvpNum[1][refIdx[1]], REF_PIC_LIST_1, partAddr, partIdx, cu->getDepth(partAddr));

			mebits = bits[1];
		}

		totalmebits += mebits;
		motionCompensation(cu, predYuv, REF_PIC_LIST_X, partIdx, true, true);
	}

	cu->m_totalBits = totalmebits;

	setWpScalingDistParam(cu, -1, REF_PIC_LIST_X);
}
unsigned int TEncSearch::getOffsetIdx(int nCtu, int nCuPelX, int nCuPelY, unsigned int width)
{
	unsigned int offsIdx = 0;
	if (64 == nCtu)
	{
		if (width == 8)
			offsIdx = (nCuPelX % 64) / 8 + (nCuPelY % 64) / 8 * 8;
		else if (width == 16)
			offsIdx = 64 + (nCuPelX % 64) / 16 + (nCuPelY % 64) / 16 * 4;
		else if (width == 32)
			offsIdx = 80 + (nCuPelX % 64) / 32 + (nCuPelY % 64) / 32 * 2;
		else
			offsIdx = 84;
	}
	else if (32 == nCtu)
	{
		if (width == 8)
			offsIdx = (nCuPelX % 32) / 8 + (nCuPelY % 32) / 8 * 4;
		else if (width == 16)
			offsIdx = 16 + (nCuPelX % 32) / 16 + (nCuPelY % 32) / 16 * 2;
		else if (width == 32)
			offsIdx = 20;
	}
	else
	{
		if (width == 8)
			offsIdx = (nCuPelX % 16) / 8 + (nCuPelY % 16) / 8 * 2;
		else if (width == 16)
			offsIdx = 4;
	}
	return offsIdx;
}
bool TEncSearch::checkSavePmv(unsigned int nCtu, unsigned int nCuSize, unsigned int offsIdx, unsigned int nPicWidth, unsigned int nPicHeight, unsigned int nCtuAddr)
{
	if (!(0 == offsIdx || 64 == offsIdx || 80 == offsIdx || 84 == offsIdx))
		return false;
	if (nCuSize == nCtu)
		return true;

	bool PicWidthNotDivCtu = nPicWidth / nCtu*nCtu < nPicWidth;
	bool PicHeightNotDivCtu = nPicHeight / nCtu*nCtu < nPicHeight;
	int numCtuInPicWidth = nPicWidth / nCtu + (PicWidthNotDivCtu ? 1 : 0);
	int numCtuInPicHeight = nPicHeight / nCtu + (PicHeightNotDivCtu ? 1 : 0);
	int nCtuPosWidth = nCtuAddr % numCtuInPicWidth;
	int nCtuPosHeight = nCtuAddr / numCtuInPicWidth;
	int nValidCtuHeight = nCtu;
	int nValidCtuWidth = nCtu;
	if (nPicHeight / nCtu < numCtuInPicHeight && nCtuPosHeight == numCtuInPicHeight - 1)
	{
		nValidCtuHeight = nPicHeight - nPicHeight / nCtu*nCtu;
	}
	if (nPicWidth / nCtu < numCtuInPicWidth && nCtuPosWidth == numCtuInPicWidth - 1)
	{
		nValidCtuWidth = nPicWidth - nPicWidth / nCtu*nCtu;
	}
	int size = X265_MIN(nValidCtuWidth, nValidCtuHeight);
	if (size >= 64)
	{
		if (84 == offsIdx)
			return true;
		else
			return false;
	}
	else if (size >= 32)
	{
		if (80 == offsIdx)
			return true;
		else
			return false;
	}
	else if (size >= 16)
	{
		if (64 == offsIdx)
			return true;
		else
			return false;
	}
	else if (size >= 8)
	{
		if (0 == offsIdx)
			return true;
		else
			return false;
	}

	return false;
}
void TEncSearch::getNeighMvs(TComDataCU* CTU, MV *tmpMv, int list, bool *isValid, int nRefPicIdx, bool isFirst)
{
	memset(isValid, 1, nAllNeighMv); //36个位置全赋值为true
	TComMvField tmpMvField;
	/*当前ctu的的16个tmvp*/
	CTU->getTMVP(tmpMv[0], 0, 16, list);
	CTU->getTMVP(tmpMv[1], 64, 16, list);
	CTU->getTMVP(tmpMv[3], 128, 16, list);
	CTU->getTMVP(tmpMv[4], 192, 16, list);
	CTU->getTMVP(tmpMv[6], 16, 16, list);
	CTU->getTMVP(tmpMv[7], 80, 16, list);
	CTU->getTMVP(tmpMv[9], 144, 16, list);
	CTU->getTMVP(tmpMv[10], 208, 16, list);
	CTU->getTMVP(tmpMv[12], 32, 16, list);
	CTU->getTMVP(tmpMv[13], 96, 16, list);
	CTU->getTMVP(tmpMv[15], 160, 16, list);
	CTU->getTMVP(tmpMv[16], 224, 16, list);
	CTU->getTMVP(tmpMv[18], 48, 16, list);
	CTU->getTMVP(tmpMv[19], 112, 16, list);
	CTU->getTMVP(tmpMv[21], 176, 16, list);
	CTU->getTMVP(tmpMv[22], 240, 16, list);
	/*当前ctu的的16个tmvp*/

	/*当前ctu左边ctu的4个tmvp*/
	if (CTU->getCULeft())
	{
		/*粗搜索得到的左侧ctu的pmv*/
		if (isFirst)
		{
			tmpMv[26].x = g_leftPMV[nRefPicIdx].x; tmpMv[26].y = g_leftPMV[nRefPicIdx].y;
		}
		else
		{
			tmpMv[26].x = g_leftPmv[nRefPicIdx].x; tmpMv[26].y = g_leftPmv[nRefPicIdx].y;
		}
		/*粗搜索得到的左侧ctu的pmv*/
	}
	else
	{
		isValid[26] = false;
	}
	/*当前ctu左边ctu的4个tmvp*/

	/*当前ctu上方ctu的4个tmvp和8个空间mv*/
	if (CTU->getCUAbove())
	{
		CTU->getCUAbove()->getMvField(CTU->getCUAbove(), 168, list, tmpMvField); tmpMv[24] = tmpMvField.mv;
		CTU->getCUAbove()->getMvField(CTU->getCUAbove(), 172, list, tmpMvField); tmpMv[17] = tmpMvField.mv;
		CTU->getCUAbove()->getMvField(CTU->getCUAbove(), 184, list, tmpMvField); tmpMv[23] = tmpMvField.mv;
		CTU->getCUAbove()->getMvField(CTU->getCUAbove(), 188, list, tmpMvField); tmpMv[5] = tmpMvField.mv;
		CTU->getCUAbove()->getMvField(CTU->getCUAbove(), 232, list, tmpMvField); tmpMv[25] = tmpMvField.mv;
		CTU->getCUAbove()->getMvField(CTU->getCUAbove(), 236, list, tmpMvField); tmpMv[14] = tmpMvField.mv;
		CTU->getCUAbove()->getMvField(CTU->getCUAbove(), 248, list, tmpMvField); tmpMv[20] = tmpMvField.mv;
		CTU->getCUAbove()->getMvField(CTU->getCUAbove(), 252, list, tmpMvField); tmpMv[2] = tmpMvField.mv;
	}
	else
	{
		isValid[24] = false;	isValid[17] = false; isValid[23] = false; isValid[5] = false; 
		isValid[25] = false; isValid[14] = false;isValid[20] = false; isValid[2] = false;
	}
	/*当前ctu上方ctu的4个tmvp和8个空间mv*/

	/*当前ctu左上方ctu的1个空间mv*/
	if (CTU->getCUAboveLeft())
	{
		CTU->getCUAboveLeft()->getMvField(CTU->getCUAboveLeft(), 252, list, tmpMvField); tmpMv[11] = tmpMvField.mv;
	}
	else
	{
		isValid[11] = false;
	}
	/*当前ctu左上方ctu的1个空间mv*/

	/*当前ctu右上方ctu的1个空间mv*/
	if (CTU->getCUAboveRight()) //右上方CTU存在,找出其中的1个mv add by hdl
	{
		CTU->getCUAboveRight()->getMvField(CTU->getCUAboveRight(), 168, list, tmpMvField); tmpMv[8] = tmpMvField.mv;
	}
	else
	{
		isValid[8] = false;
	}
	/*当前ctu右上方ctu的1个空间mv*/

	/*(0,0)点*/
	tmpMv[27] = MV(0, 0);
	/*(0,0)点*/

	/*前35个mv清除分数部分, 最后一个因为只有整数部分就不清除了*/
	for (int i = 0; i < nAllNeighMv; i++)
	{
		MV mvTemp = MV(tmpMv[i].x << 2, tmpMv[i].y << 2);
		CTU->clipMv(mvTemp, true);
		tmpMv[i] = MV(mvTemp.x >> 2, mvTemp.y >> 2);
		if (26 == i)
			continue;
		tmpMv[i].x /= 4;
		tmpMv[i].y /= 4;
	}
	for (int i = 0; i < nAllNeighMv; i++)
	{
		tmpMv[i].x = tmpMv[i].x / 2 * 2; //取偶,为了取chroma的精搜索窗时正确性
		tmpMv[i].y = tmpMv[i].y / 2 * 2;
	}
	/*前35个mv清除分数部分, 最后一个因为只有整数部分就不清除了*/
}
uint32_t TEncSearch::xSymbolBitsInter(TComDataCU* cu, bool isCalcRDOTwice)
{
	if (isCalcRDOTwice)
	{
		m_entropyCoder->resetBits();
		if (cu->getSlice()->getPPS()->getTransquantBypassEnableFlag())
		{
			m_entropyCoder->encodeCUTransquantBypassFlag(cu, 0, true);
		}
		m_entropyCoder->encodeSkipFlag(cu, 0, true);
		m_entropyCoder->encodePredMode(cu, 0, true);
		m_entropyCoder->encodePartSize(cu, 0, cu->getDepth(0), true);
		m_entropyCoder->encodePredInfo(cu, 0, true);
		bool bDummy = false;
		m_entropyCoder->encodeCoeff(cu, 0, cu->getDepth(0), cu->getWidth(0), cu->getHeight(0), bDummy);
		return m_entropyCoder->getNumberOfWrittenBits();
	}
	else
		return 0;
}
void TEncSearch::xSetSearchRange(TComDataCU* cu, MV mvp, int merange_x, int merange_y, MV& mvmin, MV& mvmax)
{
	cu->clipMv(mvp, true);
	merange_x /= 2;
	merange_y /= 2;
	MV dist(merange_x << 2, merange_y << 2);
	mvmin = mvp - dist;
	mvmax = mvp + dist - MV(4, 4); //去除最右边的点 add by hdl

	cu->clipMv(mvmin, true);
	cu->clipMv(mvmax, true);

	mvmin >>= 2;
	mvmax >>= 2;

	mvmin.x /= 4;
	mvmin.y /= 4;
	mvmax.x /= 4;
	mvmax.y /= 4;
	mvmin.x = -X265_MIN(abs(mvmin.x), mvmax.x) * 4;
	mvmin.y = -X265_MIN(abs(mvmin.y), mvmax.y) * 4;
	mvmax.x = (X265_MIN(abs(mvmin.x), mvmax.x) - 1) * 4;
	mvmax.y = (X265_MIN(abs(mvmin.y), mvmax.y) - 1) * 4;

	/* conditional clipping for frame parallelism */
	mvmin.y = X265_MIN(mvmin.y, m_refLagPixels);
	mvmax.y = X265_MIN(mvmax.y, m_refLagPixels);
}
#endif
void TEncSearch::predInterSearch(TComDataCU* cu, TComYuv* predYuv, bool bUseMRG, bool bLuma, bool bChroma)
{
	MV mvzero(0, 0);
	MV mv[2];
	MV mvBidir[2];
	MV mvTemp[2][33];
	MV mvPred[2][33];
	MV mvPredBi[2][33];

	int mvpIdxBi[2][33];
	int mvpIdx[2][33];
	int mvpNum[2][33];
	AMVPInfo amvpInfo[2][33];

	uint32_t mbBits[3] = { 1, 1, 0 };
	int refIdx[2] = { 0, 0 }; // If un-initialized, may cause SEGV in bi-directional prediction iterative stage.
	int refIdxBidir[2] = { 0, 0 };

	PartSize partSize = cu->getPartitionSize(0);
	uint32_t lastMode = 0;
	int numPart = cu->getNumPartInter();
	int numPredDir = cu->getSlice()->isInterP() ? 1 : 2;
	uint32_t biPDistTemp = MAX_INT;

	TComPicYuv *fenc = cu->getSlice()->getPic()->getPicYuvOrg();
	TComMvField mvFieldNeighbours[MRG_MAX_NUM_CANDS << 1]; // double length for mv of both lists
	UChar interDirNeighbours[MRG_MAX_NUM_CANDS];
	int numValidMergeCand = 0;

	int totalmebits = 0;

	for (int partIdx = 0; partIdx < numPart; partIdx++)
	{
		uint32_t listCost[2] = { MAX_UINT, MAX_UINT };
		uint32_t bits[3];
		uint32_t costbi = MAX_UINT;
		uint32_t costTemp = 0;
		uint32_t bitsTemp;
		MV   mvValidList1;
		int  refIdxValidList1 = 0;
		uint32_t bitsValidList1 = MAX_UINT;
		uint32_t costValidList1 = MAX_UINT;

		uint32_t partAddr;
		int  roiWidth, roiHeight;
		xGetBlkBits(partSize, cu->getSlice()->isInterP(), partIdx, lastMode, mbBits);
		cu->getPartIndexAndSize(partIdx, partAddr, roiWidth, roiHeight);

		Pel* pu = fenc->getLumaAddr(cu->getAddr(), cu->getZorderIdxInCU() + partAddr);
		m_me.setSourcePU(pu - fenc->getLumaAddr(), roiWidth, roiHeight);

		cu->getMvPredLeft(m_mvPredictors[0]);
		cu->getMvPredAbove(m_mvPredictors[1]);
		cu->getMvPredAboveRight(m_mvPredictors[2]);

		bool bTestNormalMC = true;

		if (bUseMRG && cu->getWidth(0) > 8 && numPart == 2)
		{
			bTestNormalMC = false;
		}

		if (bTestNormalMC)
		{
			// Uni-directional prediction
			for (int list = 0; list < numPredDir; list++)
			{
				for (int idx = 0; idx < cu->getSlice()->getNumRefIdx(list); idx++)
				{
					bitsTemp = mbBits[list];
					if (cu->getSlice()->getNumRefIdx(list) > 1)
					{
						bitsTemp += idx + 1;
						if (idx == cu->getSlice()->getNumRefIdx(list) - 1) bitsTemp--;
					}
					xEstimateMvPredAMVP(cu, partIdx, list, idx, mvPred[list][idx], &biPDistTemp);
					mvpIdx[list][idx] = cu->getMVPIdx(list, partAddr);
					mvpNum[list][idx] = cu->getMVPNum(list, partAddr);

					bitsTemp += m_mvpIdxCost[mvpIdx[list][idx]][AMVP_MAX_NUM_CANDS];
					int merange = m_adaptiveRange[list][idx];
					MV& mvp = mvPred[list][idx];
					MV& outmv = mvTemp[list][idx];

					MV mvmin, mvmax;
					xSetSearchRange(cu, mvp, merange, mvmin, mvmax);
					int satdCost = m_me.motionEstimate(m_mref[list][idx],
						mvmin, mvmax, mvp, 3, m_mvPredictors, merange, outmv);

					/* Get total cost of partition, but only include MV bit cost once */
					bitsTemp += m_me.bitcost(outmv);
					costTemp = (satdCost - m_me.mvcost(outmv)) + m_rdCost->getCost(bitsTemp);

					xCopyAMVPInfo(cu->getCUMvField(list)->getAMVPInfo(), &amvpInfo[list][idx]); // must always be done ( also when AMVP_MODE = AM_NONE )
					xCheckBestMVP(cu, list, mvTemp[list][idx], mvPred[list][idx], mvpIdx[list][idx], bitsTemp, costTemp);

					if (costTemp < listCost[list])
					{
						listCost[list] = costTemp;
						bits[list] = bitsTemp; // storing for bi-prediction

						// set motion
						mv[list] = mvTemp[list][idx];
						refIdx[list] = idx;
					}

					if (list == 1 && costTemp < costValidList1)
					{
						costValidList1 = costTemp;
						bitsValidList1 = bitsTemp;

						// set motion
						mvValidList1     = mvTemp[list][idx];
						refIdxValidList1 = idx;
					}
				}
			}

			// Bi-directional prediction
			if ((cu->getSlice()->isInterB()) && (cu->isBipredRestriction(partIdx) == false))
			{
				mvBidir[0] = mv[0];
				mvBidir[1] = mv[1];
				refIdxBidir[0] = refIdx[0];
				refIdxBidir[1] = refIdx[1];

				::memcpy(mvPredBi, mvPred, sizeof(mvPred));
				::memcpy(mvpIdxBi, mvpIdx, sizeof(mvpIdx));

				// Generate reference subpels
				xPredInterLumaBlk(cu, cu->getSlice()->getRefPic(REF_PIC_LIST_0, refIdx[0])->getPicYuvRec(), partAddr, &mv[0], roiWidth, roiHeight, &m_predYuv[0]);
				xPredInterLumaBlk(cu, cu->getSlice()->getRefPic(REF_PIC_LIST_1, refIdx[1])->getPicYuvRec(), partAddr, &mv[1], roiWidth, roiHeight, &m_predYuv[1]);

				pixel *ref0 = m_predYuv[0].getLumaAddr(partAddr);
				pixel *ref1 = m_predYuv[1].getLumaAddr(partAddr);

				ALIGN_VAR_32(pixel, avg[MAX_CU_SIZE * MAX_CU_SIZE]);

				int partEnum = partitionFromSizes(roiWidth, roiHeight);
				primitives.pixelavg_pp[partEnum](avg, roiWidth, ref0, m_predYuv[0].getStride(), ref1, m_predYuv[1].getStride(), 32);
				int satdCost = primitives.satd[partEnum](pu, fenc->getStride(), avg, roiWidth);
				x265_emms();
				bits[2] = bits[0] + bits[1] - mbBits[0] - mbBits[1] + mbBits[2];
				costbi =  satdCost + m_rdCost->getCost(bits[2]);

				if (mv[0].notZero() || mv[1].notZero())
				{
					ref0 = m_mref[0][refIdx[0]]->fpelPlane + (pu - fenc->getLumaAddr());  //MV(0,0) of ref0
					ref1 = m_mref[1][refIdx[1]]->fpelPlane + (pu - fenc->getLumaAddr());  //MV(0,0) of ref1
					intptr_t refStride = m_mref[0][refIdx[0]]->lumaStride;

					primitives.pixelavg_pp[partEnum](avg, roiWidth, ref0, refStride, ref1, refStride, 32);
					satdCost = primitives.satd[partEnum](pu, fenc->getStride(), avg, roiWidth);
					x265_emms();

					unsigned int bitsZero0, bitsZero1;
					m_me.setMVP(mvPredBi[0][refIdxBidir[0]]);
					bitsZero0 = bits[0] - m_me.bitcost(mv[0]) + m_me.bitcost(mvzero);

					m_me.setMVP(mvPredBi[1][refIdxBidir[1]]);
					bitsZero1 = bits[1] - m_me.bitcost(mv[1]) + m_me.bitcost(mvzero);

					uint32_t costZero = satdCost + m_rdCost->getCost(bitsZero0) + m_rdCost->getCost(bitsZero1);

					MV mvpZero[2];
					int mvpidxZero[2];
					mvpZero[0] = mvPredBi[0][refIdxBidir[0]];
					mvpidxZero[0] = mvpIdxBi[0][refIdxBidir[0]];
					xCopyAMVPInfo(&amvpInfo[0][refIdxBidir[0]], cu->getCUMvField(REF_PIC_LIST_0)->getAMVPInfo());
					xCheckBestMVP(cu, REF_PIC_LIST_0, mvzero, mvpZero[0], mvpidxZero[0], bitsZero0, costZero);
					mvpZero[1] = mvPredBi[1][refIdxBidir[1]];
					mvpidxZero[1] = mvpIdxBi[1][refIdxBidir[1]];
					xCopyAMVPInfo(&amvpInfo[1][refIdxBidir[1]], cu->getCUMvField(REF_PIC_LIST_1)->getAMVPInfo());
					xCheckBestMVP(cu, REF_PIC_LIST_1, mvzero, mvpZero[1], mvpidxZero[1], bitsZero1, costZero);

					if (costZero < costbi)
					{
						costbi = costZero;
						mvBidir[0].x = mvBidir[0].y = 0;
						mvBidir[1].x = mvBidir[1].y = 0;
						mvPredBi[0][refIdxBidir[0]] = mvpZero[0];
						mvPredBi[1][refIdxBidir[1]] = mvpZero[1];
						mvpIdxBi[0][refIdxBidir[0]] = mvpidxZero[0];
						mvpIdxBi[1][refIdxBidir[1]] = mvpidxZero[1];
						bits[2] = bitsZero0 + bitsZero1 - mbBits[0] - mbBits[1] + mbBits[2];
					}
				}
			} // if (B_SLICE)
		} //end if bTestNormalMC

		//  Clear Motion Field
		cu->getCUMvField(REF_PIC_LIST_0)->setAllMvField(TComMvField(), partSize, partAddr, 0, partIdx);
		cu->getCUMvField(REF_PIC_LIST_1)->setAllMvField(TComMvField(), partSize, partAddr, 0, partIdx);
		cu->getCUMvField(REF_PIC_LIST_0)->setAllMvd(mvzero, partSize, partAddr, 0, partIdx);
		cu->getCUMvField(REF_PIC_LIST_1)->setAllMvd(mvzero, partSize, partAddr, 0, partIdx);

		cu->setMVPIdxSubParts(-1, REF_PIC_LIST_0, partAddr, partIdx, cu->getDepth(partAddr));
		cu->setMVPNumSubParts(-1, REF_PIC_LIST_0, partAddr, partIdx, cu->getDepth(partAddr));
		cu->setMVPIdxSubParts(-1, REF_PIC_LIST_1, partAddr, partIdx, cu->getDepth(partAddr));
		cu->setMVPNumSubParts(-1, REF_PIC_LIST_1, partAddr, partIdx, cu->getDepth(partAddr));

		uint32_t mebits = 0;
		// Set Motion Field_
		mv[1] = mvValidList1;
		refIdx[1] = refIdxValidList1;
		bits[1] = bitsValidList1;
		listCost[1] = costValidList1;

		if (bTestNormalMC)
		{
			if (costbi <= listCost[0] && costbi <= listCost[1])
			{
				lastMode = 2;
				{
					cu->getCUMvField(REF_PIC_LIST_0)->setAllMv(mvBidir[0], partSize, partAddr, 0, partIdx);
					cu->getCUMvField(REF_PIC_LIST_0)->setAllRefIdx(refIdxBidir[0], partSize, partAddr, 0, partIdx);
					cu->getCUMvField(REF_PIC_LIST_1)->setAllMv(mvBidir[1], partSize, partAddr, 0, partIdx);
					cu->getCUMvField(REF_PIC_LIST_1)->setAllRefIdx(refIdxBidir[1], partSize, partAddr, 0, partIdx);
				}
				{
					MV mvtmp = mvBidir[0] - mvPredBi[0][refIdxBidir[0]];
					cu->getCUMvField(REF_PIC_LIST_0)->setAllMvd(mvtmp, partSize, partAddr, 0, partIdx);
				}
				{
					MV mvtmp = mvBidir[1] - mvPredBi[1][refIdxBidir[1]];
					cu->getCUMvField(REF_PIC_LIST_1)->setAllMvd(mvtmp, partSize, partAddr, 0, partIdx);
				}

				cu->setInterDirSubParts(3, partAddr, partIdx, cu->getDepth(0));

				cu->setMVPIdxSubParts(mvpIdxBi[0][refIdxBidir[0]], REF_PIC_LIST_0, partAddr, partIdx, cu->getDepth(partAddr));
				cu->setMVPNumSubParts(mvpNum[0][refIdxBidir[0]], REF_PIC_LIST_0, partAddr, partIdx, cu->getDepth(partAddr));
				cu->setMVPIdxSubParts(mvpIdxBi[1][refIdxBidir[1]], REF_PIC_LIST_1, partAddr, partIdx, cu->getDepth(partAddr));
				cu->setMVPNumSubParts(mvpNum[1][refIdxBidir[1]], REF_PIC_LIST_1, partAddr, partIdx, cu->getDepth(partAddr));

				mebits = bits[2];
			}
			else if (listCost[0] <= listCost[1])
			{
				lastMode = 0;
				cu->getCUMvField(REF_PIC_LIST_0)->setAllMv(mv[0], partSize, partAddr, 0, partIdx);
				cu->getCUMvField(REF_PIC_LIST_0)->setAllRefIdx(refIdx[0], partSize, partAddr, 0, partIdx);
				{
					MV mvtmp = mv[0] - mvPred[0][refIdx[0]];
					cu->getCUMvField(REF_PIC_LIST_0)->setAllMvd(mvtmp, partSize, partAddr, 0, partIdx);
				}
				cu->setInterDirSubParts(1, partAddr, partIdx, cu->getDepth(0));

				cu->setMVPIdxSubParts(mvpIdx[0][refIdx[0]], REF_PIC_LIST_0, partAddr, partIdx, cu->getDepth(partAddr));
				cu->setMVPNumSubParts(mvpNum[0][refIdx[0]], REF_PIC_LIST_0, partAddr, partIdx, cu->getDepth(partAddr));

				mebits = bits[0];
			}
			else
			{
				lastMode = 1;
				cu->getCUMvField(REF_PIC_LIST_1)->setAllMv(mv[1], partSize, partAddr, 0, partIdx);
				cu->getCUMvField(REF_PIC_LIST_1)->setAllRefIdx(refIdx[1], partSize, partAddr, 0, partIdx);
				{
					MV mvtmp = mv[1] - mvPred[1][refIdx[1]];
					cu->getCUMvField(REF_PIC_LIST_1)->setAllMvd(mvtmp, partSize, partAddr, 0, partIdx);
				}
				cu->setInterDirSubParts(2, partAddr, partIdx, cu->getDepth(0));

				cu->setMVPIdxSubParts(mvpIdx[1][refIdx[1]], REF_PIC_LIST_1, partAddr, partIdx, cu->getDepth(partAddr));
				cu->setMVPNumSubParts(mvpNum[1][refIdx[1]], REF_PIC_LIST_1, partAddr, partIdx, cu->getDepth(partAddr));

				mebits = bits[1];
			}
		} // end if bTestNormalMC

		if (cu->getPartitionSize(partAddr) != SIZE_2Nx2N)
		{
			uint32_t mrgInterDir = 0;
			TComMvField mrgMvField[2];
			uint32_t mrgIndex = 0;

			uint32_t meInterDir = 0;
			TComMvField meMvField[2];

			// calculate ME cost
			uint32_t meError = MAX_UINT;
			uint32_t meCost = MAX_UINT;

			if (bTestNormalMC)
			{
				meError = xGetInterPredictionError(cu, partIdx);
				meCost = meError + m_rdCost->getCost(mebits);
			}

			// save ME result.
			meInterDir = cu->getInterDir(partAddr);
			cu->getMvField(cu, partAddr, REF_PIC_LIST_0, meMvField[0]);
			cu->getMvField(cu, partAddr, REF_PIC_LIST_1, meMvField[1]);

			// find Merge result
			uint32_t mrgCost = MAX_UINT;
			uint32_t mrgBits = 0;
			xMergeEstimation(cu, partIdx, mrgInterDir, mrgMvField, mrgIndex, mrgCost, mrgBits, mvFieldNeighbours, interDirNeighbours, numValidMergeCand);
			if (mrgCost < meCost)
			{
				// set Merge result
				cu->setMergeFlagSubParts(true, partAddr, partIdx, cu->getDepth(partAddr));
				cu->setMergeIndexSubParts(mrgIndex, partAddr, partIdx, cu->getDepth(partAddr));
				cu->setInterDirSubParts(mrgInterDir, partAddr, partIdx, cu->getDepth(partAddr));
				{
					cu->getCUMvField(REF_PIC_LIST_0)->setAllMvField(mrgMvField[0], partSize, partAddr, 0, partIdx);
					cu->getCUMvField(REF_PIC_LIST_1)->setAllMvField(mrgMvField[1], partSize, partAddr, 0, partIdx);
				}

				cu->getCUMvField(REF_PIC_LIST_0)->setAllMvd(mvzero, partSize, partAddr, 0, partIdx);
				cu->getCUMvField(REF_PIC_LIST_1)->setAllMvd(mvzero, partSize, partAddr, 0, partIdx);

				cu->setMVPIdxSubParts(-1, REF_PIC_LIST_0, partAddr, partIdx, cu->getDepth(partAddr));
				cu->setMVPNumSubParts(-1, REF_PIC_LIST_0, partAddr, partIdx, cu->getDepth(partAddr));
				cu->setMVPIdxSubParts(-1, REF_PIC_LIST_1, partAddr, partIdx, cu->getDepth(partAddr));
				cu->setMVPNumSubParts(-1, REF_PIC_LIST_1, partAddr, partIdx, cu->getDepth(partAddr));
				totalmebits += mrgBits;
			}
			else
			{
				// set ME result
				cu->setMergeFlagSubParts(false, partAddr, partIdx, cu->getDepth(partAddr));
				cu->setInterDirSubParts(meInterDir, partAddr, partIdx, cu->getDepth(partAddr));
				{
					cu->getCUMvField(REF_PIC_LIST_0)->setAllMvField(meMvField[0], partSize, partAddr, 0, partIdx);
					cu->getCUMvField(REF_PIC_LIST_1)->setAllMvField(meMvField[1], partSize, partAddr, 0, partIdx);
				}
				totalmebits += mebits;
			}
		}
		else
		{
			totalmebits += mebits;
		}
		motionCompensation(cu, predYuv, REF_PIC_LIST_X, partIdx, bLuma, bChroma);
	}

	cu->m_totalBits = totalmebits;

	setWpScalingDistParam(cu, -1, REF_PIC_LIST_X);
}

// AMVP
void TEncSearch::xEstimateMvPredAMVP(TComDataCU* cu, uint32_t partIdx, int list, int refIdx, MV& mvPred, uint32_t* distBiP)
{
	AMVPInfo* amvpInfo = cu->getCUMvField(list)->getAMVPInfo();

	MV   bestMv;
	int  bestIdx = 0;
	uint32_t bestCost = MAX_INT;
	uint32_t partAddr = 0;
	int  roiWidth, roiHeight;
	int  i;

	cu->getPartIndexAndSize(partIdx, partAddr, roiWidth, roiHeight);

	// Fill the MV Candidates
	cu->fillMvpCand(partIdx, partAddr, list, refIdx, amvpInfo);

	bestMv = amvpInfo->m_mvCand[0];
	if (amvpInfo->m_num <= 1)
	{
		mvPred = bestMv;

		cu->setMVPIdxSubParts(bestIdx, list, partAddr, partIdx, cu->getDepth(partAddr));
		cu->setMVPNumSubParts(amvpInfo->m_num, list, partAddr, partIdx, cu->getDepth(partAddr));

		if (cu->getSlice()->getMvdL1ZeroFlag() && list == REF_PIC_LIST_1)
		{
			(*distBiP) = xGetTemplateCost(cu, partAddr, &m_predTempYuv, mvPred, 0, AMVP_MAX_NUM_CANDS, list, refIdx, roiWidth, roiHeight);
		}
		return;
	}

	m_predTempYuv.clear();

	//-- Check Minimum Cost.
	for (i = 0; i < amvpInfo->m_num; i++)
	{
		uint32_t cost = xGetTemplateCost(cu, partAddr, &m_predTempYuv, amvpInfo->m_mvCand[i], i, AMVP_MAX_NUM_CANDS, list, refIdx, roiWidth, roiHeight);
		if (bestCost > cost)
		{
			bestCost = cost;
			bestMv = amvpInfo->m_mvCand[i];
			bestIdx = i;
			(*distBiP) = cost;
		}
	}

	m_predTempYuv.clear();

	// Setting Best MVP
	mvPred = bestMv;
	cu->setMVPIdxSubParts(bestIdx, list, partAddr, partIdx, cu->getDepth(partAddr));
	cu->setMVPNumSubParts(amvpInfo->m_num, list, partAddr, partIdx, cu->getDepth(partAddr));
}

void TEncSearch::xEstimateMvPredAMVP(TComDataCU* cu, uint32_t partIdx, int list, int refIdx, MV& mvPred, MV &mvInput)
{
	AMVPInfo* amvpInfo = cu->getCUMvField(list)->getAMVPInfo();

	MV   bestMv;

	int  bestIdx = 0;
	uint32_t partAddr = 0;
	uint32_t bestCost = MAX_INT;
	int  roiWidth, roiHeight;
	int  i;

	cu->getPartIndexAndSize(partIdx, partAddr, roiWidth, roiHeight);

	// Fill the MV Candidates
	cu->fillMvpCand(partIdx, partAddr, list, refIdx, amvpInfo); //amvpInfo最多有两个candidate  add by hdl 
#if RK_INTER_METEST
	AmvpInfo amvpMine;
	Amvp.fillMvpCand(list, refIdx, &amvpMine);
	for (i = 0; i < amvpMine.m_num; i++)
	{
		assert(amvpMine.m_mvCand[i].x == amvpInfo->m_mvCand[i].x);
		assert(amvpMine.m_mvCand[i].y == amvpInfo->m_mvCand[i].y);
	}
#endif
	bestMv = amvpInfo->m_mvCand[0];
	if (amvpInfo->m_num <= 1)
	{
		mvPred = bestMv;
		cu->setMVPIdxSubParts(bestIdx, list, partAddr, partIdx, cu->getDepth(partAddr));
		cu->setMVPNumSubParts(amvpInfo->m_num, list, partAddr, partIdx, cu->getDepth(partAddr));
		return;
	}
	m_predTempYuv.clear();
	//-- Check Minimum Cost.
	for (i = 0; i < amvpInfo->m_num; i++)
	{
		uint32_t cost = abs(amvpInfo->m_mvCand[i].x - mvInput.x) + abs(amvpInfo->m_mvCand[i].y - mvInput.y);
		if (bestCost > cost)
		{
			bestCost = cost;
			bestMv = amvpInfo->m_mvCand[i];
			bestIdx = i;
		}
	}
	m_predTempYuv.clear();

	mvPred = bestMv; // Setting Best MVP
	cu->setMVPIdxSubParts(bestIdx, list, partAddr, partIdx, cu->getDepth(partAddr));
	cu->setMVPNumSubParts(amvpInfo->m_num, list, partAddr, partIdx, cu->getDepth(partAddr));
}

uint32_t TEncSearch::xGetMvpIdxBits(int idx, int num)
{
	assert(idx >= 0 && num >= 0 && idx < num);

	if (num == 1)
		return 0;

	uint32_t length = 1;
	int temp = idx;
	if (temp == 0)
	{
		return length;
	}

	bool bCodeLast = (num - 1 > temp);

	length += (temp - 1);

	if (bCodeLast)
	{
		length++;
	}

	return length;
}

void TEncSearch::xGetBlkBits(PartSize cuMode, bool bPSlice, int partIdx, uint32_t lastMode, uint32_t blockBit[3])
{
	if (cuMode == SIZE_2Nx2N)
	{
		blockBit[0] = (!bPSlice) ? 3 : 1;
		blockBit[1] = 3;
		blockBit[2] = 5;
	}
	else if ((cuMode == SIZE_2NxN || cuMode == SIZE_2NxnU) || cuMode == SIZE_2NxnD)
	{
		uint32_t aauiMbBits[2][3][3] = { { { 0, 0, 3 }, { 0, 0, 0 }, { 0, 0, 0 } }, { { 5, 7, 7 }, { 7, 5, 7 }, { 9 - 3, 9 - 3, 9 - 3 } } };
		if (bPSlice)
		{
			blockBit[0] = 3;
			blockBit[1] = 0;
			blockBit[2] = 0;
		}
		else
		{
			::memcpy(blockBit, aauiMbBits[partIdx][lastMode], 3 * sizeof(uint32_t));
		}
	}
	else if ((cuMode == SIZE_Nx2N || cuMode == SIZE_nLx2N) || cuMode == SIZE_nRx2N)
	{
		uint32_t aauiMbBits[2][3][3] = { { { 0, 2, 3 }, { 0, 0, 0 }, { 0, 0, 0 } }, { { 5, 7, 7 }, { 7 - 2, 7 - 2, 9 - 2 }, { 9 - 3, 9 - 3, 9 - 3 } } };
		if (bPSlice)
		{
			blockBit[0] = 3;
			blockBit[1] = 0;
			blockBit[2] = 0;
		}
		else
		{
			::memcpy(blockBit, aauiMbBits[partIdx][lastMode], 3 * sizeof(uint32_t));
		}
	}
	else if (cuMode == SIZE_NxN)
	{
		blockBit[0] = (!bPSlice) ? 3 : 1;
		blockBit[1] = 3;
		blockBit[2] = 5;
	}
	else
	{
		printf("Wrong!\n");
		assert(0);
	}
}

void TEncSearch::xCopyAMVPInfo(AMVPInfo* src, AMVPInfo* dst)
{
	dst->m_num = src->m_num;
	for (int i = 0; i < src->m_num; i++)
	{
		dst->m_mvCand[i] = src->m_mvCand[i];
	}
}

/* Check if using an alternative MVP would result in a smaller MVD + signal bits */
void TEncSearch::xCheckBestMVP(TComDataCU* cu, int list, MV mv, MV& mvPred, int& outMvpIdx, uint32_t& outBits, uint32_t& outCost)
{
	AMVPInfo* amvpInfo = cu->getCUMvField(list)->getAMVPInfo();

	assert(amvpInfo->m_mvCand[outMvpIdx] == mvPred);
	if (amvpInfo->m_num < 2) return;

	m_me.setMVP(mvPred);
	int bestMvpIdx = outMvpIdx;
	int mvBitsOrig = m_me.bitcost(mv) + m_mvpIdxCost[outMvpIdx][AMVP_MAX_NUM_CANDS];
	int bestMvBits = mvBitsOrig;

	for (int mvpIdx = 0; mvpIdx < amvpInfo->m_num; mvpIdx++)
	{
		if (mvpIdx == outMvpIdx)
			continue;

		m_me.setMVP(amvpInfo->m_mvCand[mvpIdx]);
		int mvbits = m_me.bitcost(mv) + m_mvpIdxCost[mvpIdx][AMVP_MAX_NUM_CANDS];

		if (mvbits < bestMvBits)
		{
			bestMvBits = mvbits;
			bestMvpIdx = mvpIdx;
		}
	}

	if (bestMvpIdx != outMvpIdx) // if changed
	{
		mvPred = amvpInfo->m_mvCand[bestMvpIdx];

		outMvpIdx = bestMvpIdx;
		uint32_t origOutBits = outBits;
		outBits = origOutBits - mvBitsOrig + bestMvBits;
		outCost = (outCost - m_rdCost->getCost(origOutBits)) + m_rdCost->getCost(outBits);
	}
}

uint32_t TEncSearch::xGetTemplateCost(TComDataCU* cu, uint32_t partAddr, TComYuv* templateCand, MV mvCand, int mvpIdx,
	int mvpCandCount, int list, int refIdx, int sizex, int sizey)
{
	// TODO: does it clip with m_referenceRowsAvailable?
	cu->clipMv(mvCand);

	// prediction pattern
	xPredInterLumaBlk(cu, cu->getSlice()->getRefPic(list, refIdx)->getPicYuvRec(), partAddr, &mvCand, sizex, sizey, templateCand);

	// calc distortion
	uint32_t cost = m_me.bufSAD(templateCand->getLumaAddr(partAddr), templateCand->getStride());
	x265_emms();
	return m_rdCost->calcRdSADCost(cost, m_mvpIdxCost[mvpIdx][mvpCandCount]);
}

void TEncSearch::xSetSearchRange(TComDataCU* cu, MV mvp, int merange, MV& mvmin, MV& mvmax)
{
	cu->clipMv(mvp);

	MV dist(merange << 2, merange << 2);
	mvmin = mvp - dist;
	mvmax = mvp + dist;

	cu->clipMv(mvmin);
	cu->clipMv(mvmax);

	mvmin >>= 2;
	mvmax >>= 2;

	/* conditional clipping for frame parallelism */
	mvmin.y = X265_MIN(mvmin.y, m_refLagPixels);
	mvmax.y = X265_MIN(mvmax.y, m_refLagPixels);
}
/** encode residual and calculate rate-distortion for a CU block
 * \param cu
 * \param fencYuv
 * \param predYuv
 * \param outResiYuv
 * \param rpcYuvResiBest
 * \param outReconYuv
 * \param bSkipRes
 * \returns void
 */
void TEncSearch::encodeResAndCalcRdInterCU(TComDataCU* cu, TComYuv* fencYuv, TComYuv* predYuv, TShortYUV* outResiYuv,
	TShortYUV* outBestResiYuv, TComYuv* outReconYuv, bool bSkipRes, bool curUseRDOQ)
{
	if (cu->isIntra(0))
	{
		return;
	}

	bool bHighPass = cu->getSlice()->getSliceType() == B_SLICE;
	uint32_t bits = 0, bestBits = 0;
	uint32_t distortion = 0, bdist = 0;

	uint32_t width = cu->getWidth(0);
	uint32_t height = cu->getHeight(0);

	//  No residual coding : SKIP mode
	if (bSkipRes)
	{
		cu->setSkipFlagSubParts(true, 0, cu->getDepth(0));

		outResiYuv->clear();

		predYuv->copyToPartYuv(outReconYuv, 0);

		int part = partitionFromSizes(width, height);
		distortion = primitives.sse_pp[part](fencYuv->getLumaAddr(), fencYuv->getStride(), outReconYuv->getLumaAddr(), outReconYuv->getStride());
		part = partitionFromSizes(width >> 1, height >> 1);
		distortion += m_rdCost->scaleChromaDistCb(primitives.sse_pp[part](fencYuv->getCbAddr(), fencYuv->getCStride(), outReconYuv->getCbAddr(), outReconYuv->getCStride()));
		distortion += m_rdCost->scaleChromaDistCr(primitives.sse_pp[part](fencYuv->getCrAddr(), fencYuv->getCStride(), outReconYuv->getCrAddr(), outReconYuv->getCStride()));

		m_rdGoOnSbacCoder->load(m_rdSbacCoders[cu->getDepth(0)][CI_CURR_BEST]);
		m_entropyCoder->resetBits();
		if (cu->getSlice()->getPPS()->getTransquantBypassEnableFlag())
		{
			m_entropyCoder->encodeCUTransquantBypassFlag(cu, 0, true);
		}
		m_entropyCoder->encodeSkipFlag(cu, 0, true);
		m_entropyCoder->encodeMergeIndex(cu, 0, true);

		bits = m_entropyCoder->getNumberOfWrittenBits();
		m_rdGoOnSbacCoder->store(m_rdSbacCoders[cu->getDepth(0)][CI_TEMP_BEST]);

		cu->m_totalBits = bits;
		cu->m_totalDistortion = distortion;
		cu->m_totalCost = m_rdCost->calcRdCost(distortion, bits);

		m_rdGoOnSbacCoder->store(m_rdSbacCoders[cu->getDepth(0)][CI_TEMP_BEST]);

		cu->setCbfSubParts(0, 0, 0, 0, cu->getDepth(0));
		cu->setTrIdxSubParts(0, 0, cu->getDepth(0));
		return;
	}

	//  Residual coding.
	int      qp, qpBest = 0;
	uint64_t cost, bcost = MAX_INT64;

	uint32_t trLevel = 0;
	if ((cu->getWidth(0) > cu->getSlice()->getSPS()->getMaxTrSize()))
	{
		while (cu->getWidth(0) > (cu->getSlice()->getSPS()->getMaxTrSize() << trLevel))
		{
			trLevel++;
		}
	}
	uint32_t maxTrLevel = 1 + trLevel;

	while ((width >> maxTrLevel) < (g_maxCUWidth >> g_maxCUDepth))
	{
		maxTrLevel--;
	}

	qp = bHighPass ? Clip3(-cu->getSlice()->getSPS()->getQpBDOffsetY(), MAX_QP, (int)cu->getQP(0)) : cu->getQP(0);

	outResiYuv->subtract(fencYuv, predYuv, 0, width);

	cost = 0;
	bits = 0;
	distortion = 0;
	m_rdGoOnSbacCoder->load(m_rdSbacCoders[cu->getDepth(0)][CI_CURR_BEST]);

	uint32_t zeroDistortion = 0;
	xEstimateResidualQT(cu, 0, 0, outResiYuv, cu->getDepth(0), cost, bits, distortion, &zeroDistortion, curUseRDOQ);

	m_entropyCoder->resetBits();
	m_entropyCoder->encodeQtRootCbfZero(cu);

#if RK_CHOOSE
	uint64_t zeroCost = MAX_INT64;
#else
	uint32_t zeroResiBits = m_entropyCoder->getNumberOfWrittenBits();
	uint64_t zeroCost = m_rdCost->calcRdCost(zeroDistortion, zeroResiBits);
#endif

	if (cu->isLosslessCoded(0))
	{
		zeroCost = cost + 1;
	}
	if (zeroCost < cost)
	{
		cost = zeroCost;
		bits = 0;
		distortion = zeroDistortion;

		const uint32_t qpartnum = cu->getPic()->getNumPartInCU() >> (cu->getDepth(0) << 1);
		::memset(cu->getTransformIdx(), 0, qpartnum * sizeof(UChar));
		::memset(cu->getCbf(TEXT_LUMA), 0, qpartnum * sizeof(UChar));
		::memset(cu->getCbf(TEXT_CHROMA_U), 0, qpartnum * sizeof(UChar));
		::memset(cu->getCbf(TEXT_CHROMA_V), 0, qpartnum * sizeof(UChar));
		::memset(cu->getCoeffY(), 0, width * height * sizeof(TCoeff));
		::memset(cu->getCoeffCb(), 0, width * height * sizeof(TCoeff) >> 2);
		::memset(cu->getCoeffCr(), 0, width * height * sizeof(TCoeff) >> 2);
		cu->setTransformSkipSubParts(0, 0, 0, 0, cu->getDepth(0));
	}
	else
	{
		xSetResidualQTData(cu, 0, 0, NULL, cu->getDepth(0), false);
	}

	m_rdGoOnSbacCoder->load(m_rdSbacCoders[cu->getDepth(0)][CI_CURR_BEST]);

	bits = xSymbolBitsInter(cu);

	uint64_t exactCost = m_rdCost->calcRdCost(distortion, bits);
	cost = exactCost;

	if (cost < bcost)
	{
		if (!cu->getQtRootCbf(0))
		{
			outBestResiYuv->clear();
		}
		else
		{
			xSetResidualQTData(cu, 0, 0, outBestResiYuv, cu->getDepth(0), true);
		}

		bestBits = bits;
		bdist = distortion;
		bcost = cost;
		qpBest = qp;
		m_rdGoOnSbacCoder->store(m_rdSbacCoders[cu->getDepth(0)][CI_TEMP_BEST]);
	}

	assert(bcost != MAX_INT64);

	outReconYuv->addClip(predYuv, outBestResiYuv, 0, width);

	// update with clipped distortion and cost (qp estimation loop uses unclipped values)
	int part = partitionFromSizes(width, height);
	bdist = primitives.sse_pp[part](fencYuv->getLumaAddr(), fencYuv->getStride(), outReconYuv->getLumaAddr(), outReconYuv->getStride());
	part = partitionFromSizes(width >> 1, height >> 1);
	bdist += m_rdCost->scaleChromaDistCb(primitives.sse_pp[part](fencYuv->getCbAddr(), fencYuv->getCStride(), outReconYuv->getCbAddr(), outReconYuv->getCStride()));
	bdist += m_rdCost->scaleChromaDistCr(primitives.sse_pp[part](fencYuv->getCrAddr(), fencYuv->getCStride(), outReconYuv->getCrAddr(), outReconYuv->getCStride()));
	bcost = m_rdCost->calcRdCost(bdist, bestBits);

	cu->m_totalBits = bestBits;
	cu->m_totalDistortion = bdist;
	cu->m_totalCost = bcost;

	if (cu->isSkipped(0))
	{
		cu->setCbfSubParts(0, 0, 0, 0, cu->getDepth(0));
	}

	cu->setQPSubParts(qpBest, 0, cu->getDepth(0));
}

void TEncSearch::generateCoeffRecon(TComDataCU* cu, TComYuv* fencYuv, TComYuv* predYuv, TShortYUV* resiYuv, TComYuv* reconYuv, bool skipRes)
{
	if (skipRes && cu->getPredictionMode(0) == MODE_INTER && cu->getMergeFlag(0) && cu->getPartitionSize(0) == SIZE_2Nx2N)
	{
		predYuv->copyToPartYuv(reconYuv, 0);
		cu->setCbfSubParts(0, TEXT_LUMA, 0, 0, cu->getDepth(0));
		cu->setCbfSubParts(0, TEXT_CHROMA_U, 0, 0, cu->getDepth(0));
		cu->setCbfSubParts(0, TEXT_CHROMA_V, 0, 0, cu->getDepth(0));
		return;
	}
	if (cu->getPredictionMode(0) == MODE_INTER)
	{
		residualTransformQuantInter(cu, 0, 0, resiYuv, cu->getDepth(0), true);
		uint32_t width = cu->getWidth(0);
		reconYuv->addClip(predYuv, resiYuv, 0, width);

		if (cu->getMergeFlag(0) && cu->getPartitionSize(0) == SIZE_2Nx2N && cu->getQtRootCbf(0) == 0)
		{
			cu->setSkipFlagSubParts(true, 0, cu->getDepth(0));
		}
	}
	else if (cu->getPredictionMode(0) == MODE_INTRA)
	{
		uint32_t initTrDepth = cu->getPartitionSize(0) == SIZE_2Nx2N ? 0 : 1;
		residualTransformQuantIntra(cu, initTrDepth, 0, true, fencYuv, predYuv, resiYuv, reconYuv);
		getBestIntraModeChroma(cu, fencYuv, predYuv);
		residualQTIntrachroma(cu, 0, 0, fencYuv, predYuv, resiYuv, reconYuv);
	}
}

#if _MSC_VER
#pragma warning(disable: 4701) // potentially uninitialized local variable
#endif

void TEncSearch::residualTransformQuantInter(TComDataCU* cu, uint32_t absPartIdx, uint32_t absTUPartIdx, TShortYUV* resiYuv, const uint32_t depth, bool curuseRDOQ)
{
	assert(cu->getDepth(0) == cu->getDepth(absPartIdx));
	const uint32_t trMode = depth - cu->getDepth(0);
	const uint32_t trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> depth] + 2;

	bool bSplitFlag = ((cu->getSlice()->getSPS()->getQuadtreeTUMaxDepthInter() == 1) && cu->getPredictionMode(absPartIdx) == MODE_INTER && (cu->getPartitionSize(absPartIdx) != SIZE_2Nx2N));
	bool bCheckFull;
	if (bSplitFlag && depth == cu->getDepth(absPartIdx) && (trSizeLog2 > cu->getQuadtreeTULog2MinSizeInCU(absPartIdx)))
		bCheckFull = false;
	else
		bCheckFull = (trSizeLog2 <= cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize());
	const bool bCheckSplit = (trSizeLog2 > cu->getQuadtreeTULog2MinSizeInCU(absPartIdx));
	assert(bCheckFull || bCheckSplit);

	bool  bCodeChroma = true;
	uint32_t  trModeC = trMode;
	uint32_t  trSizeCLog2 = trSizeLog2 - 1;
	if (trSizeLog2 == 2)
	{
		trSizeCLog2++;
		trModeC--;
		uint32_t qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + trModeC) << 1);
		bCodeChroma = ((absPartIdx % qpdiv) == 0);
	}

	const uint32_t setCbf = 1 << trMode;
	// code full block
	uint32_t absSumY = 0, absSumU = 0, absSumV = 0;
	int lastPosY = -1, lastPosU = -1, lastPosV = -1;
	if (bCheckFull)
	{
		const uint32_t numCoeffPerAbsPartIdxIncrement = cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight() >> (cu->getSlice()->getSPS()->getMaxCUDepth() << 1);

		TCoeff *coeffCurY = cu->getCoeffY() + (numCoeffPerAbsPartIdxIncrement * absPartIdx);
		TCoeff *coeffCurU = cu->getCoeffCb() + (numCoeffPerAbsPartIdxIncrement * absPartIdx >> 2);
		TCoeff *coeffCurV = cu->getCoeffCr() + (numCoeffPerAbsPartIdxIncrement * absPartIdx >> 2);

		int trWidth = 0, trHeight = 0, trWidthC = 0, trHeightC = 0;
		uint32_t absTUPartIdxC = absPartIdx;

		trWidth = trHeight = 1 << trSizeLog2;
		trWidthC = trHeightC = 1 << trSizeCLog2;
		cu->setTrIdxSubParts(depth - cu->getDepth(0), absPartIdx, depth);

		cu->setTransformSkipSubParts(0, TEXT_LUMA, absPartIdx, depth);
		if (bCodeChroma)
		{
			cu->setTransformSkipSubParts(0, TEXT_CHROMA_U, absPartIdx, cu->getDepth(0) + trModeC);
			cu->setTransformSkipSubParts(0, TEXT_CHROMA_V, absPartIdx, cu->getDepth(0) + trModeC);
		}

		m_trQuant->setQPforQuant(cu->getQP(0), TEXT_LUMA, cu->getSlice()->getSPS()->getQpBDOffsetY(), 0);
		m_trQuant->selectLambda(TEXT_LUMA);

		absSumY = m_trQuant->transformNxN(cu, resiYuv->getLumaAddr(absTUPartIdx), resiYuv->m_width, coeffCurY,
			trWidth, trHeight, TEXT_LUMA, absPartIdx, &lastPosY, false, curuseRDOQ);

		cu->setCbfSubParts(absSumY ? setCbf : 0, TEXT_LUMA, absPartIdx, depth);

		if (bCodeChroma)
		{
			int curChromaQpOffset = cu->getSlice()->getPPS()->getChromaCbQpOffset() + cu->getSlice()->getSliceQpDeltaCb();
			m_trQuant->setQPforQuant(cu->getQP(0), TEXT_CHROMA, cu->getSlice()->getSPS()->getQpBDOffsetC(), curChromaQpOffset);

			m_trQuant->selectLambda(TEXT_CHROMA);

			absSumU = m_trQuant->transformNxN(cu, resiYuv->getCbAddr(absTUPartIdxC), resiYuv->m_cwidth, coeffCurU,
				trWidthC, trHeightC, TEXT_CHROMA_U, absPartIdx, &lastPosU, false, curuseRDOQ);

			curChromaQpOffset = cu->getSlice()->getPPS()->getChromaCrQpOffset() + cu->getSlice()->getSliceQpDeltaCr();
			m_trQuant->setQPforQuant(cu->getQP(0), TEXT_CHROMA, cu->getSlice()->getSPS()->getQpBDOffsetC(), curChromaQpOffset);
			absSumV = m_trQuant->transformNxN(cu, resiYuv->getCrAddr(absTUPartIdxC), resiYuv->m_cwidth, coeffCurV,
				trWidthC, trHeightC, TEXT_CHROMA_V, absPartIdx, &lastPosV, false, curuseRDOQ);

			cu->setCbfSubParts(absSumU ? setCbf : 0, TEXT_CHROMA_U, absPartIdx, cu->getDepth(0) + trModeC);
			cu->setCbfSubParts(absSumV ? setCbf : 0, TEXT_CHROMA_V, absPartIdx, cu->getDepth(0) + trModeC);
		}

		if (absSumY)
		{
			int16_t *curResiY = resiYuv->getLumaAddr(absTUPartIdx);

			m_trQuant->setQPforQuant(cu->getQP(0), TEXT_LUMA, cu->getSlice()->getSPS()->getQpBDOffsetY(), 0);

			int scalingListType = 3 + g_eTTable[(int)TEXT_LUMA];
			assert(scalingListType < 6);
			m_trQuant->invtransformNxN(cu->getCUTransquantBypass(absPartIdx), REG_DCT, curResiY, resiYuv->m_width, coeffCurY, trWidth, trHeight, scalingListType, false, lastPosY); //this is for inter mode only
		}
		else
		{
			int16_t *ptr = resiYuv->getLumaAddr(absTUPartIdx);
			assert(trWidth == trHeight);
			primitives.blockfill_s[(int)g_convertToBit[trWidth]](ptr, resiYuv->m_width, 0);
		}

		if (bCodeChroma)
		{
			if (absSumU)
			{
				int16_t *pcResiCurrU = resiYuv->getCbAddr(absTUPartIdxC);

				int curChromaQpOffset = cu->getSlice()->getPPS()->getChromaCbQpOffset() + cu->getSlice()->getSliceQpDeltaCb();
				m_trQuant->setQPforQuant(cu->getQP(0), TEXT_CHROMA, cu->getSlice()->getSPS()->getQpBDOffsetC(), curChromaQpOffset);

				int scalingListType = 3 + g_eTTable[(int)TEXT_CHROMA_U];
				assert(scalingListType < 6);
				m_trQuant->invtransformNxN(cu->getCUTransquantBypass(absPartIdx), REG_DCT, pcResiCurrU, resiYuv->m_cwidth, coeffCurU, trWidthC, trHeightC, scalingListType, false, lastPosU);
			}
			else
			{
				int16_t *ptr = resiYuv->getCbAddr(absTUPartIdxC);
				assert(trWidthC == trHeightC);
				primitives.blockfill_s[(int)g_convertToBit[trWidthC]](ptr, resiYuv->m_cwidth, 0);
			}
			if (absSumV)
			{
				int16_t *curResiV = resiYuv->getCrAddr(absTUPartIdxC);
				int curChromaQpOffset = cu->getSlice()->getPPS()->getChromaCrQpOffset() + cu->getSlice()->getSliceQpDeltaCr();
				m_trQuant->setQPforQuant(cu->getQP(0), TEXT_CHROMA, cu->getSlice()->getSPS()->getQpBDOffsetC(), curChromaQpOffset);

				int scalingListType = 3 + g_eTTable[(int)TEXT_CHROMA_V];
				assert(scalingListType < 6);
				m_trQuant->invtransformNxN(cu->getCUTransquantBypass(absPartIdx), REG_DCT, curResiV, resiYuv->m_cwidth, coeffCurV, trWidthC, trHeightC, scalingListType, false, lastPosV);
			}
			else
			{
				int16_t *ptr = resiYuv->getCrAddr(absTUPartIdxC);
				assert(trWidthC == trHeightC);
				primitives.blockfill_s[(int)g_convertToBit[trWidthC]](ptr, resiYuv->m_cwidth, 0);
			}
		}
		cu->setCbfSubParts(absSumY ? setCbf : 0, TEXT_LUMA, absPartIdx, depth);
		if (bCodeChroma)
		{
			cu->setCbfSubParts(absSumU ? setCbf : 0, TEXT_CHROMA_U, absPartIdx, cu->getDepth(0) + trModeC);
			cu->setCbfSubParts(absSumV ? setCbf : 0, TEXT_CHROMA_V, absPartIdx, cu->getDepth(0) + trModeC);
		}
	}

	// code sub-blocks
	if (bCheckSplit && !bCheckFull)
	{
		const uint32_t qPartNumSubdiv = cu->getPic()->getNumPartInCU() >> ((depth + 1) << 1);
		for (uint32_t i = 0; i < 4; ++i)
		{
			uint32_t nsAddr = absPartIdx + i * qPartNumSubdiv;
			residualTransformQuantInter(cu, absPartIdx + i * qPartNumSubdiv, nsAddr, resiYuv, depth + 1, curuseRDOQ);
		}

		uint32_t ycbf = 0;
		uint32_t ucbf = 0;
		uint32_t vcbf = 0;
		for (uint32_t i = 0; i < 4; ++i)
		{
			ycbf |= cu->getCbf(absPartIdx + i * qPartNumSubdiv, TEXT_LUMA, trMode + 1);
			ucbf |= cu->getCbf(absPartIdx + i * qPartNumSubdiv, TEXT_CHROMA_U, trMode + 1);
			vcbf |= cu->getCbf(absPartIdx + i * qPartNumSubdiv, TEXT_CHROMA_V, trMode + 1);
		}

		for (uint32_t i = 0; i < 4 * qPartNumSubdiv; ++i)
		{
			cu->getCbf(TEXT_LUMA)[absPartIdx + i] |= ycbf << trMode;
			cu->getCbf(TEXT_CHROMA_U)[absPartIdx + i] |= ucbf << trMode;
			cu->getCbf(TEXT_CHROMA_V)[absPartIdx + i] |= vcbf << trMode;
		}

		return;
	}

	cu->setTrIdxSubParts(trMode, absPartIdx, depth);
	cu->setCbfSubParts(absSumY ? setCbf : 0, TEXT_LUMA, absPartIdx, depth);

	if (bCodeChroma)
	{
		cu->setCbfSubParts(absSumU ? setCbf : 0, TEXT_CHROMA_U, absPartIdx, cu->getDepth(0) + trModeC);
		cu->setCbfSubParts(absSumV ? setCbf : 0, TEXT_CHROMA_V, absPartIdx, cu->getDepth(0) + trModeC);
	}
}

void TEncSearch::xEstimateResidualQT(TComDataCU*    cu,
	uint32_t       absPartIdx,
	uint32_t       absTUPartIdx,
	TShortYUV*     resiYuv,
	const uint32_t depth,
	uint64_t &     rdCost,
	uint32_t &     outBits,
	uint32_t &     outDist,
	uint32_t *     outZeroDist,
	bool           curuseRDOQ)
{
	assert(cu->getDepth(0) == cu->getDepth(absPartIdx));
	const uint32_t trMode = depth - cu->getDepth(0);
	const uint32_t trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> depth] + 2;

	bool bSplitFlag = ((cu->getSlice()->getSPS()->getQuadtreeTUMaxDepthInter() == 1) && cu->getPredictionMode(absPartIdx) == MODE_INTER && (cu->getPartitionSize(absPartIdx) != SIZE_2Nx2N));
	bool bCheckFull;
	if (bSplitFlag && depth == cu->getDepth(absPartIdx) && (trSizeLog2 > cu->getQuadtreeTULog2MinSizeInCU(absPartIdx)))
		bCheckFull = false;
	else
		bCheckFull = (trSizeLog2 <= cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize());
	const bool bCheckSplit = (trSizeLog2 > cu->getQuadtreeTULog2MinSizeInCU(absPartIdx));
	assert(bCheckFull || bCheckSplit);

	bool  bCodeChroma = true;
	uint32_t  trModeC = trMode;
	uint32_t  trSizeCLog2 = trSizeLog2 - 1;
	if (trSizeLog2 == 2)
	{
		trSizeCLog2++;
		trModeC--;
		uint32_t qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + trModeC) << 1);
		bCodeChroma = ((absPartIdx % qpdiv) == 0);
	}

	const uint32_t setCbf = 1 << trMode;
	// code full block
	uint64_t singleCost = MAX_INT64;
	uint32_t singleBits = 0;
	uint32_t singleDist = 0;
	uint32_t absSumY = 0, absSumU = 0, absSumV = 0;
	int lastPosY = -1, lastPosU = -1, lastPosV = -1;
	uint32_t bestTransformMode[3] = { 0 };

	m_rdGoOnSbacCoder->store(m_rdSbacCoders[depth][CI_QT_TRAFO_ROOT]);

	if (bCheckFull)
	{
		const uint32_t numCoeffPerAbsPartIdxIncrement = cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight() >> (cu->getSlice()->getSPS()->getMaxCUDepth() << 1);
		const uint32_t qtlayer = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() - trSizeLog2;
		TCoeff *coeffCurY = m_qtTempCoeffY[qtlayer] + (numCoeffPerAbsPartIdxIncrement * absPartIdx);
		TCoeff *coeffCurU = m_qtTempCoeffCb[qtlayer] + (numCoeffPerAbsPartIdxIncrement * absPartIdx >> 2);
		TCoeff *coeffCurV = m_qtTempCoeffCr[qtlayer] + (numCoeffPerAbsPartIdxIncrement * absPartIdx >> 2);

		int trWidth = 0, trHeight = 0, trWidthC = 0, trHeightC = 0;
		uint32_t absTUPartIdxC = absPartIdx;

#if TQ_RUN_IN_HWC_ME
		uint32_t ctuSize = cu->getSlice()->getSPS()->getMaxCUWidth();	
		int tuPos = 0, tuPosC = 0, cuPelX = 0, cuPelY = 0;
		uint32_t tmpDepth = 0, tmpAbsSumY = 0, tmpAbsSumU = 0, tmpAbsSumV = 0;
		if(cu->getDepth(0)==0) // cuWidth = 64
		{
			switch(absTUPartIdx)
			{
			case 0: 	tuPos=0;			tuPosC=0;	 		break;
			case 64:	tuPos=32;			tuPosC=16;	 		break;
			case 128:	tuPos=32*64;		tuPosC=16*32; 		break;
			case 192:	tuPos=32*64+32;		tuPosC=16*32+16;	break;
			default: 	assert(0);								break;
			}		
		}
		else // cuWidth<64
		{
			cuPelX = cu->getCUPelX() - cu->getCUPelX()/ctuSize*ctuSize; // CU in CTU x coordinate
			cuPelY = cu->getCUPelY() - cu->getCUPelY()/ctuSize*ctuSize;
			tuPos = ctuSize*cuPelY + cuPelX; // TU is CU 
			tuPosC = ctuSize/2 * cuPelY/2 + cuPelX/2;
		}
		tmpDepth = cu->getDepth(0);
		assert(tmpDepth>=0 || tmpDepth<4);
#endif 

		trWidth = trHeight = 1 << trSizeLog2;
		trWidthC = trHeightC = 1 << trSizeCLog2;
		cu->setTrIdxSubParts(depth - cu->getDepth(0), absPartIdx, depth);
		uint64_t minCostY = MAX_INT64;
		uint64_t minCostU = MAX_INT64;
		uint64_t minCostV = MAX_INT64;
		bool checkTransformSkipY = cu->getSlice()->getPPS()->getUseTransformSkip() && trWidth == 4 && trHeight == 4;
		bool checkTransformSkipUV = cu->getSlice()->getPPS()->getUseTransformSkip() && trWidthC == 4 && trHeightC == 4;

		checkTransformSkipY &= (!cu->isLosslessCoded(0));
		checkTransformSkipUV &= (!cu->isLosslessCoded(0));

		cu->setTransformSkipSubParts(0, TEXT_LUMA, absPartIdx, depth);
		if (bCodeChroma)
		{
			cu->setTransformSkipSubParts(0, TEXT_CHROMA_U, absPartIdx, cu->getDepth(0) + trModeC);
			cu->setTransformSkipSubParts(0, TEXT_CHROMA_V, absPartIdx, cu->getDepth(0) + trModeC);
		}

		if (m_cfg->bEnableRDOQ && curuseRDOQ)
		{
			m_entropyCoder->estimateBit(m_trQuant->m_estBitsSbac, trWidth, trHeight, TEXT_LUMA);
		}
#if TQ_RUN_IN_X265_ME // Y
		// get input
		if (cu->getSlice()->getSliceType() == B_SLICE) m_trQuant->m1_hevcQT->m_infoFromX265->sliceType = 0; // B
		if (cu->getSlice()->getSliceType() == P_SLICE) m_trQuant->m1_hevcQT->m_infoFromX265->sliceType = 1; // P
		if (cu->getSlice()->getSliceType() == I_SLICE) m_trQuant->m1_hevcQT->m_infoFromX265->sliceType = 2; // I
		m_trQuant->m1_hevcQT->m_infoFromX265->textType = 0; // Y
		m_trQuant->m1_hevcQT->m_infoFromX265->qpBdOffset = cu->getSlice()->getSPS()->getQpBDOffsetY();
		m_trQuant->m1_hevcQT->m_infoFromX265->chromaQPOffset = cu->getSlice()->getSPS()->getQpBDOffsetC();
		m_trQuant->m1_hevcQT->m_infoFromX265->size = (uint8_t)trWidth;
		m_trQuant->m1_hevcQT->m_infoFromX265->qp = cu->getQP(0);
		m_trQuant->m1_hevcQT->m_infoFromX265->transformSkip = false;

		// copy input residual, TU size in effect
		for (uint32_t k = 0; k < trWidth; k++)
		{
			memcpy(&(m_trQuant->m1_hevcQT->m_infoFromX265->inResi[k*CTU_SIZE]),
				&(resiYuv->getLumaAddr(absTUPartIdx)[k*resiYuv->m_width]), trWidth*sizeof(int16_t));
		}
		if (cu->getPredictionMode(absPartIdx) == MODE_INTER)	 m_trQuant->m1_hevcQT->m_infoFromX265->predMode = 1;
		if (cu->getPredictionMode(absPartIdx) == MODE_INTRA)	 m_trQuant->m1_hevcQT->m_infoFromX265->predMode = 0;


		m_trQuant->m1_hevcQT->m_infoFromX265->ctuWidth = cu->getWidth(0);
#endif
		m_trQuant->setQPforQuant(cu->getQP(0), TEXT_LUMA, cu->getSlice()->getSPS()->getQpBDOffsetY(), 0);
		m_trQuant->selectLambda(TEXT_LUMA);

		absSumY = m_trQuant->transformNxN(cu, resiYuv->getLumaAddr(absTUPartIdx), resiYuv->m_width, coeffCurY,
			trWidth, trHeight, TEXT_LUMA, absPartIdx, &lastPosY, false, curuseRDOQ);
#if TQ_RUN_IN_X265_ME //Y
		// get output
		memcpy(m_trQuant->m1_hevcQT->m_infoFromX265->coeffTQ, coeffCurY, trWidth*trHeight*sizeof(int32_t));
		m_trQuant->m1_hevcQT->m_infoFromX265->absSum = absSumY;
		m_trQuant->m1_hevcQT->m_infoFromX265->lastPos = lastPosY;
#endif

#if TQ_RUN_IN_HWC_ME // Y coeff	
		if(g_fme)	
		{
			for(int i = 0; i<trHeight; i++)				
			{
				for(int j=0; j<trWidth; j++)
				{					
					assert(tuPos+i*64>=0 && tuPos+i*64<64*64);
					g_fmeCoeffY[tmpDepth][tuPos+i*64+j] = (int16_t)coeffCurY[i*trWidth+j];
				}
			}	
		}
		if(g_merge)
		{
			for(int i = 0; i<trHeight; i++)				
			{
				for(int j=0; j<trWidth; j++)
				{

					assert(tuPos+i*64>=0 && tuPos+i*64<64*64);
					g_mergeCoeffY[tmpDepth][tuPos+i*64+j] = (int16_t)coeffCurY[i*trWidth+j];
				}
			}		
		}
		tmpAbsSumY = absSumY;
#endif
		cu->setCbfSubParts(absSumY ? setCbf : 0, TEXT_LUMA, absPartIdx, depth);

		if (bCodeChroma)
		{
			if (m_cfg->bEnableRDOQ && curuseRDOQ)
			{
				m_entropyCoder->estimateBit(m_trQuant->m_estBitsSbac, trWidthC, trHeightC, TEXT_CHROMA);
			}

			int curChromaQpOffset = cu->getSlice()->getPPS()->getChromaCbQpOffset() + cu->getSlice()->getSliceQpDeltaCb();
#if TQ_RUN_IN_X265_ME // Cb
			// get input
			if (cu->getSlice()->getSliceType() == B_SLICE) m_trQuant->m2_hevcQT->m_infoFromX265->sliceType = 0; // B
			if (cu->getSlice()->getSliceType() == P_SLICE) m_trQuant->m2_hevcQT->m_infoFromX265->sliceType = 1; // P
			if (cu->getSlice()->getSliceType() == I_SLICE) m_trQuant->m2_hevcQT->m_infoFromX265->sliceType = 2; // I
			m_trQuant->m2_hevcQT->m_infoFromX265->textType = 1; // Chroma
			m_trQuant->m2_hevcQT->m_infoFromX265->qpBdOffset = cu->getSlice()->getSPS()->getQpBDOffsetY();
			m_trQuant->m2_hevcQT->m_infoFromX265->chromaQPOffset = cu->getSlice()->getSPS()->getQpBDOffsetC();
			m_trQuant->m2_hevcQT->m_infoFromX265->size = (uint8_t)trWidthC;
			m_trQuant->m2_hevcQT->m_infoFromX265->qp = cu->getQP(0);
			m_trQuant->m2_hevcQT->m_infoFromX265->transformSkip = false;

			// copy input residual, TU size in effect
			for (uint32_t k = 0; k < trWidthC; k++)
			{
				memcpy(&(m_trQuant->m2_hevcQT->m_infoFromX265->inResi[k*CTU_SIZE]),
					&(resiYuv->getCbAddr(absTUPartIdxC)[k*resiYuv->m_cwidth]), trWidthC*sizeof(int16_t));
			}
			if (cu->getPredictionMode(absPartIdx) == MODE_INTER)	 m_trQuant->m2_hevcQT->m_infoFromX265->predMode = 1;
			if (cu->getPredictionMode(absPartIdx) == MODE_INTRA)	 m_trQuant->m2_hevcQT->m_infoFromX265->predMode = 0;


			m_trQuant->m2_hevcQT->m_infoFromX265->ctuWidth = cu->getWidth(0);
#endif
			m_trQuant->setQPforQuant(cu->getQP(0), TEXT_CHROMA, cu->getSlice()->getSPS()->getQpBDOffsetC(), curChromaQpOffset);

			m_trQuant->selectLambda(TEXT_CHROMA);

			absSumU = m_trQuant->transformNxN(cu, resiYuv->getCbAddr(absTUPartIdxC), resiYuv->m_cwidth, coeffCurU,
				trWidthC, trHeightC, TEXT_CHROMA_U, absPartIdx, &lastPosU, false, curuseRDOQ);
#if TQ_RUN_IN_X265_ME // Cb
			// get output
			memcpy(m_trQuant->m2_hevcQT->m_infoFromX265->coeffTQ, coeffCurU, trWidthC*trHeightC*sizeof(int32_t));
			m_trQuant->m2_hevcQT->m_infoFromX265->absSum = absSumU;
			m_trQuant->m2_hevcQT->m_infoFromX265->lastPos = lastPosU;
#endif

#if TQ_RUN_IN_HWC_ME // U coeff	
			if(g_fme)
			{
				for(int i = 0; i<trHeightC; i++)				
				{
					for(int j=0; j<trWidthC; j++)
					{
						assert(tuPosC+i*32>=0 && tuPosC+i*32<32*32);				
						g_fmeCoeffU[tmpDepth][tuPosC+i*32+j] = (int16_t)coeffCurU[i*trWidthC+j];
					}
				}			
			}

			if(g_merge)
			{
				for(int i = 0; i<trHeightC; i++)				
				{
					for(int j=0; j<trWidthC; j++)
					{
						assert(tuPosC+i*32>=0 && tuPosC+i*32<32*32);				
						g_mergeCoeffU[tmpDepth][tuPosC+i*32+j] = (int16_t)coeffCurU[i*trWidthC+j];
					}
				}			
			}
			tmpAbsSumU = absSumU;
#endif
			curChromaQpOffset = cu->getSlice()->getPPS()->getChromaCrQpOffset() + cu->getSlice()->getSliceQpDeltaCr();
#if TQ_RUN_IN_X265_ME // Cr
			// get input
			if (cu->getSlice()->getSliceType() == B_SLICE) m_trQuant->m3_hevcQT->m_infoFromX265->sliceType = 0; // B
			if (cu->getSlice()->getSliceType() == P_SLICE) m_trQuant->m3_hevcQT->m_infoFromX265->sliceType = 1; // P
			if (cu->getSlice()->getSliceType() == I_SLICE) m_trQuant->m3_hevcQT->m_infoFromX265->sliceType = 2; // I
			m_trQuant->m3_hevcQT->m_infoFromX265->textType = 1; // Chroma
			m_trQuant->m3_hevcQT->m_infoFromX265->qpBdOffset = cu->getSlice()->getSPS()->getQpBDOffsetY();
			m_trQuant->m3_hevcQT->m_infoFromX265->chromaQPOffset = cu->getSlice()->getSPS()->getQpBDOffsetC();
			m_trQuant->m3_hevcQT->m_infoFromX265->size = (uint8_t)trWidthC;
			m_trQuant->m3_hevcQT->m_infoFromX265->qp = cu->getQP(0);
			m_trQuant->m3_hevcQT->m_infoFromX265->transformSkip = false;

			// copy input residual, TU size in effect
			for (uint32_t k = 0; k < trWidthC; k++)
			{
				memcpy(&(m_trQuant->m3_hevcQT->m_infoFromX265->inResi[k*CTU_SIZE]),
					&(resiYuv->getCrAddr(absTUPartIdxC)[k*resiYuv->m_cwidth]), trWidthC*sizeof(int16_t));
			}
			if (cu->getPredictionMode(absPartIdx) == MODE_INTER)	 m_trQuant->m3_hevcQT->m_infoFromX265->predMode = 1;
			if (cu->getPredictionMode(absPartIdx) == MODE_INTRA)	 m_trQuant->m3_hevcQT->m_infoFromX265->predMode = 0;


			m_trQuant->m3_hevcQT->m_infoFromX265->ctuWidth = cu->getWidth(0);
#endif
			m_trQuant->setQPforQuant(cu->getQP(0), TEXT_CHROMA, cu->getSlice()->getSPS()->getQpBDOffsetC(), curChromaQpOffset);
			absSumV = m_trQuant->transformNxN(cu, resiYuv->getCrAddr(absTUPartIdxC), resiYuv->m_cwidth, coeffCurV,
				trWidthC, trHeightC, TEXT_CHROMA_V, absPartIdx, &lastPosV, false, curuseRDOQ);
#if TQ_RUN_IN_X265_ME // Cr
			// get output
			memcpy(m_trQuant->m3_hevcQT->m_infoFromX265->coeffTQ, coeffCurV, trWidthC*trHeightC*sizeof(int32_t));
			m_trQuant->m3_hevcQT->m_infoFromX265->absSum = absSumV;
			m_trQuant->m3_hevcQT->m_infoFromX265->lastPos = lastPosV;
#endif

#if TQ_RUN_IN_HWC_ME // V coeff	
			if(g_fme)				
			{
				for(int i = 0; i<trHeightC; i++)				
				{
					for(int j=0; j<trWidthC; j++)
					{
						assert(tuPosC+i*32>=0 && tuPosC+i*32<32*32);
						g_fmeCoeffV[tmpDepth][tuPosC+i*32+j] = (int16_t)coeffCurV[i*trWidthC+j];
					}
				}	
			}

			if(g_merge)				
			{
				for(int i = 0; i<trHeightC; i++)				
				{
					for(int j=0; j<trWidthC; j++)
					{
						assert(tuPosC+i*32>=0 && tuPosC+i*32<32*32);
						g_mergeCoeffV[tmpDepth][tuPosC+i*32+j] = (int16_t)coeffCurV[i*trWidthC+j];
					}
				}	
			}		
			tmpAbsSumV = absSumV;
#endif
			cu->setCbfSubParts(absSumU ? setCbf : 0, TEXT_CHROMA_U, absPartIdx, cu->getDepth(0) + trModeC);
			cu->setCbfSubParts(absSumV ? setCbf : 0, TEXT_CHROMA_V, absPartIdx, cu->getDepth(0) + trModeC);
		}

		m_entropyCoder->resetBits();
		m_entropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_LUMA, trMode);
		m_entropyCoder->encodeCoeffNxN(cu, coeffCurY, absPartIdx, trWidth, trHeight, depth, TEXT_LUMA);
		const uint32_t uiSingleBitsY = m_entropyCoder->getNumberOfWrittenBits();

		uint32_t singleBitsU = 0;
		uint32_t singleBitsV = 0;
		if (bCodeChroma)
		{
			m_entropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_CHROMA_U, trMode);
			m_entropyCoder->encodeCoeffNxN(cu, coeffCurU, absPartIdx, trWidthC, trHeightC, depth, TEXT_CHROMA_U);
			singleBitsU = m_entropyCoder->getNumberOfWrittenBits() - uiSingleBitsY;

			m_entropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_CHROMA_V, trMode);
			m_entropyCoder->encodeCoeffNxN(cu, coeffCurV, absPartIdx, trWidthC, trHeightC, depth, TEXT_CHROMA_V);
			singleBitsV = m_entropyCoder->getNumberOfWrittenBits() - (uiSingleBitsY + singleBitsU);
		}

		const uint32_t numSamplesLuma = 1 << (trSizeLog2 << 1);
		const uint32_t numSamplesChroma = 1 << (trSizeCLog2 << 1);

		::memset(m_tempPel, 0, sizeof(Pel)* numSamplesLuma); // not necessary needed for inside of recursion (only at the beginning)

		int partSize = partitionFromSizes(trWidth, trHeight);
		uint32_t distY = primitives.sse_sp[partSize](resiYuv->getLumaAddr(absTUPartIdx), resiYuv->m_width, m_tempPel, trWidth);

		if (outZeroDist)
		{
			*outZeroDist += distY;
		}
		if (absSumY)
		{
			int16_t *curResiY = m_qtTempTComYuv[qtlayer].getLumaAddr(absTUPartIdx);

			m_trQuant->setQPforQuant(cu->getQP(0), TEXT_LUMA, cu->getSlice()->getSPS()->getQpBDOffsetY(), 0);

			int scalingListType = 3 + g_eTTable[(int)TEXT_LUMA];
			assert(scalingListType < 6);
			assert(m_qtTempTComYuv[qtlayer].m_width == MAX_CU_SIZE);
			m_trQuant->invtransformNxN(cu->getCUTransquantBypass(absPartIdx), REG_DCT, curResiY, MAX_CU_SIZE, coeffCurY, trWidth, trHeight, scalingListType, false, lastPosY); //this is for inter mode only

#if TQ_RUN_IN_X265_ME // Y
			// get output
			// copy output residual, TU size in effect
			for (uint32_t k = 0; k < trHeight; k++)
			{
				memcpy(&(m_trQuant->m1_hevcQT->m_infoFromX265->outResi[k*CTU_SIZE]), &(curResiY[k*MAX_CU_SIZE]), trWidth*sizeof(int16_t));
			}
#endif

#if TQ_RUN_IN_HWC_ME // Y resi, absSumY>0	
			if(g_fme)	
			{
				for(int i = 0; i<trHeight; i++)				
				{			
					memcpy(g_fmeResiY[tmpDepth]+tuPos+i*64, curResiY+i*MAX_CU_SIZE, trWidth*sizeof(int16_t));
				}	
			}
			if(g_merge)
			{
				for(int i = 0; i<trHeight; i++)				
				{			
					memcpy(g_mergeResiY[tmpDepth]+tuPos+i*64, curResiY+i*MAX_CU_SIZE, trWidth*sizeof(int16_t));
				}				
			}
#endif
			const uint32_t nonZeroDistY = primitives.sse_ss[partSize](resiYuv->getLumaAddr(absTUPartIdx), resiYuv->m_width, m_qtTempTComYuv[qtlayer].getLumaAddr(absTUPartIdx), MAX_CU_SIZE);
			if (cu->isLosslessCoded(0))
			{
				distY = nonZeroDistY;
			}
			else
			{
				const uint64_t singleCostY = m_rdCost->calcRdCost(nonZeroDistY, uiSingleBitsY);
				m_entropyCoder->resetBits();
				m_entropyCoder->encodeQtCbfZero(cu, TEXT_LUMA, trMode); //只编码当前Cbf为0,不编码残差，用于对比。
#if RK_CHOOSE
				const uint64_t nullCostY = MAX_INT64;
#else
				const uint32_t nullBitsY = m_entropyCoder->getNumberOfWrittenBits();
				const uint64_t nullCostY = m_rdCost->calcRdCost(distY, nullBitsY);

#endif
				if (nullCostY < singleCostY)
				{
					absSumY = 0;
					::memset(coeffCurY, 0, sizeof(TCoeff)* numSamplesLuma);
					if (checkTransformSkipY)
					{
						minCostY = nullCostY;
					}
				}
				else
				{
					distY = nonZeroDistY;
					if (checkTransformSkipY)
					{
						minCostY = singleCostY;
					}
				}
			}
		}
		else if (checkTransformSkipY)
		{
			m_entropyCoder->resetBits();
			m_entropyCoder->encodeQtCbfZero(cu, TEXT_LUMA, trMode);
			const uint32_t nullBitsY = m_entropyCoder->getNumberOfWrittenBits();
			minCostY = m_rdCost->calcRdCost(distY, nullBitsY);
		}

#if TQ_RUN_IN_X265_ME // Y
		// get output
		// if original absSumY==0, clearResi for comparing
		int16_t *tmpResi1 = m_trQuant->m1_hevcQT->m_infoFromX265->outResi;
		if(m_trQuant->m1_hevcQT->m_infoFromX265->absSum==0)
		{
			m_trQuant->m1_hevcQT->fillResi(tmpResi1, 0, CTU_SIZE, trWidth);
		}

		m_trQuant->m1_hevcQT->proc(1); // QT proc for inter in x265
#endif

#if TQ_RUN_IN_HWC_ME // Y resi, absSumY==0	
		if(tmpAbsSumY==0)
		{
			if(g_fme)	
			{
				for(int i = 0; i<trHeight; i++)				
				{
					memset(g_fmeResiY[tmpDepth]+tuPos+i*64, 0, trWidth*sizeof(int16_t));
				}					
			}
			if(g_merge)	
			{
				for(int i = 0; i<trHeight; i++)				
				{
					memset(g_mergeResiY[tmpDepth]+tuPos+i*64, 0, trWidth*sizeof(int16_t));
				}					
			}	
		}
#endif
		if (!absSumY)
		{
			int16_t *ptr = m_qtTempTComYuv[qtlayer].getLumaAddr(absTUPartIdx);
			assert(m_qtTempTComYuv[qtlayer].m_width == MAX_CU_SIZE);

			assert(trWidth == trHeight);
			primitives.blockfill_s[(int)g_convertToBit[trWidth]](ptr, MAX_CU_SIZE, 0);
		}

		uint32_t distU = 0;
		uint32_t distV = 0;

		int partSizeC = partitionFromSizes(trWidthC, trHeightC);
		if (bCodeChroma)
		{
			distU = m_rdCost->scaleChromaDistCb(primitives.sse_sp[partSizeC](resiYuv->getCbAddr(absTUPartIdxC), resiYuv->m_cwidth, m_tempPel, trWidthC));

			if (outZeroDist)
			{
				*outZeroDist += distU;
			}
			if (absSumU)
			{
				int16_t *pcResiCurrU = m_qtTempTComYuv[qtlayer].getCbAddr(absTUPartIdxC);

				int curChromaQpOffset = cu->getSlice()->getPPS()->getChromaCbQpOffset() + cu->getSlice()->getSliceQpDeltaCb();
				m_trQuant->setQPforQuant(cu->getQP(0), TEXT_CHROMA, cu->getSlice()->getSPS()->getQpBDOffsetC(), curChromaQpOffset);

				int scalingListType = 3 + g_eTTable[(int)TEXT_CHROMA_U];
				assert(scalingListType < 6);
				assert(m_qtTempTComYuv[qtlayer].m_cwidth == MAX_CU_SIZE / 2);
				m_trQuant->invtransformNxN(cu->getCUTransquantBypass(absPartIdx), REG_DCT, pcResiCurrU, MAX_CU_SIZE / 2, coeffCurU, trWidthC, trHeightC, scalingListType, false, lastPosU);

#if TQ_RUN_IN_X265_ME // Cb
				// get output
				// copy output residual, TU size in effect
				int16_t *P = m_qtTempTComYuv[qtlayer].getCbAddr(absTUPartIdxC);
				for (uint32_t k = 0; k < trHeightC; k++)
				{
					memcpy(&(m_trQuant->m2_hevcQT->m_infoFromX265->outResi[k*CTU_SIZE]), &(P[k*MAX_CU_SIZE / 2]), trWidthC*sizeof(int16_t));
				}
#endif
#if TQ_RUN_IN_HWC_ME // U resi, absSumU>0	
				if(g_fme)				
				{
					for(int i = 0; i<trHeightC; i++)				
					{
						memcpy(g_fmeResiU[tmpDepth]+tuPosC+i*32, pcResiCurrU+i*MAX_CU_SIZE/2, trWidthC*sizeof(int16_t));
					}
				}
				if(g_merge)				
				{
					for(int i = 0; i<trHeightC; i++)				
					{
						memcpy(g_mergeResiU[tmpDepth]+tuPosC+i*32, pcResiCurrU+i*MAX_CU_SIZE/2, trWidthC*sizeof(int16_t));
					}
				}				
#endif
				uint32_t dist = primitives.sse_ss[partSizeC](resiYuv->getCbAddr(absTUPartIdxC), resiYuv->m_cwidth,
					m_qtTempTComYuv[qtlayer].getCbAddr(absTUPartIdxC),
					MAX_CU_SIZE / 2);
				const uint32_t nonZeroDistU = m_rdCost->scaleChromaDistCb(dist);

				if (cu->isLosslessCoded(0))
				{
					distU = nonZeroDistU;
				}
				else
				{
					const uint64_t singleCostU = m_rdCost->calcRdCost(nonZeroDistU, singleBitsU);
					m_entropyCoder->resetBits();
					m_entropyCoder->encodeQtCbfZero(cu, TEXT_CHROMA_U, trMode);
#if RK_CHOOSE
					const uint64_t nullCostU = MAX_INT64;
#else					
					const uint32_t nullBitsU = m_entropyCoder->getNumberOfWrittenBits();
					const uint64_t nullCostU = m_rdCost->calcRdCost(distU, nullBitsU);
#endif					
					if (nullCostU < singleCostU)
					{
						absSumU = 0;
						::memset(coeffCurU, 0, sizeof(TCoeff)* numSamplesChroma);
						if (checkTransformSkipUV)
						{
							minCostU = nullCostU;
						}
					}
					else
					{
						distU = nonZeroDistU;
						if (checkTransformSkipUV)
						{
							minCostU = singleCostU;
						}
					}
				}
			}
			else if (checkTransformSkipUV)
			{
				m_entropyCoder->resetBits();
				m_entropyCoder->encodeQtCbfZero(cu, TEXT_CHROMA_U, trModeC);
				const uint32_t nullBitsU = m_entropyCoder->getNumberOfWrittenBits();
				minCostU = m_rdCost->calcRdCost(distU, nullBitsU);
			}
			if (!absSumU)
			{
				int16_t *ptr = m_qtTempTComYuv[qtlayer].getCbAddr(absTUPartIdxC);
				assert(m_qtTempTComYuv[qtlayer].m_cwidth == MAX_CU_SIZE / 2);

				assert(trWidthC == trHeightC);
				primitives.blockfill_s[(int)g_convertToBit[trWidthC]](ptr, MAX_CU_SIZE / 2, 0);
			}

#if TQ_RUN_IN_X265_ME //Cb
			// get output
			// if original absSumU==0, clearResi for comparing
			int16_t *tmpResi2 = m_trQuant->m2_hevcQT->m_infoFromX265->outResi;
			if(m_trQuant->m2_hevcQT->m_infoFromX265->absSum==0)
			{
				m_trQuant->m2_hevcQT->fillResi(tmpResi2, 0, CTU_SIZE, trWidthC);
			}

			m_trQuant->m2_hevcQT->proc(1); // QT proc for inter in x265
#endif

#if TQ_RUN_IN_HWC_ME // U resi, absSumU==0	
			if(tmpAbsSumU==0)
			{
				if(g_fme)				
				{
					for(int i = 0; i<trHeightC; i++)				
					{
						memset(g_fmeResiU[tmpDepth]+tuPosC+i*32, 0, trWidthC*sizeof(int16_t));
					}					
				}
				if(g_merge)				
				{
					for(int i = 0; i<trHeightC; i++)				
					{
						memset(g_mergeResiU[tmpDepth]+tuPosC+i*32, 0, trWidthC*sizeof(int16_t));
					}					
				}				
			}
#endif
			distV = m_rdCost->scaleChromaDistCr(primitives.sse_sp[partSizeC](resiYuv->getCrAddr(absTUPartIdxC), resiYuv->m_cwidth, m_tempPel, trWidthC));
			if (outZeroDist)
			{
				*outZeroDist += distV;
			}
			if (absSumV)
			{
				int16_t *curResiV = m_qtTempTComYuv[qtlayer].getCrAddr(absTUPartIdxC);
				int curChromaQpOffset = cu->getSlice()->getPPS()->getChromaCrQpOffset() + cu->getSlice()->getSliceQpDeltaCr();
				m_trQuant->setQPforQuant(cu->getQP(0), TEXT_CHROMA, cu->getSlice()->getSPS()->getQpBDOffsetC(), curChromaQpOffset);

				int scalingListType = 3 + g_eTTable[(int)TEXT_CHROMA_V];
				assert(scalingListType < 6);
				assert(m_qtTempTComYuv[qtlayer].m_cwidth == MAX_CU_SIZE / 2);
				m_trQuant->invtransformNxN(cu->getCUTransquantBypass(absPartIdx), REG_DCT, curResiV, MAX_CU_SIZE / 2, coeffCurV, trWidthC, trHeightC, scalingListType, false, lastPosV);

#if TQ_RUN_IN_X265_ME // Cr
				// get output
				// copy output residual, TU size in effect
				for (uint32_t k = 0; k < trHeightC; k++)
				{
					memcpy(&(m_trQuant->m3_hevcQT->m_infoFromX265->outResi[k*CTU_SIZE]), &(curResiV[k*MAX_CU_SIZE/2]), trWidthC*sizeof(int16_t));
				}
#endif
#if TQ_RUN_IN_HWC_ME // V resi, absSumV>0	
				if(g_fme)
				{
					for(int i = 0; i<trHeightC; i++)				
					{
						memcpy(g_fmeResiV[tmpDepth]+tuPosC+i*32, curResiV+i*MAX_CU_SIZE/2, trWidthC*sizeof(int16_t));
					}	
				}
				if(g_merge)
				{
					for(int i = 0; i<trHeightC; i++)				
					{
						memcpy(g_mergeResiV[tmpDepth]+tuPosC+i*32, curResiV+i*MAX_CU_SIZE/2, trWidthC*sizeof(int16_t));
					}	
				}				
#endif
				uint32_t dist = primitives.sse_ss[partSizeC](resiYuv->getCrAddr(absTUPartIdxC), resiYuv->m_cwidth,
					m_qtTempTComYuv[qtlayer].getCrAddr(absTUPartIdxC),
					MAX_CU_SIZE / 2);
				const uint32_t nonZeroDistV = m_rdCost->scaleChromaDistCr(dist);

				if (cu->isLosslessCoded(0))
				{
					distV = nonZeroDistV;
				}
				else
				{
					const uint64_t singleCostV = m_rdCost->calcRdCost(nonZeroDistV, singleBitsV);
					m_entropyCoder->resetBits();
					m_entropyCoder->encodeQtCbfZero(cu, TEXT_CHROMA_V, trMode);
#if RK_CHOOSE
					const uint64_t nullCostV = MAX_INT64;
#else					
					const uint32_t nullBitsV = m_entropyCoder->getNumberOfWrittenBits();
					const uint64_t nullCostV = m_rdCost->calcRdCost(distV, nullBitsV);
#endif
					if (nullCostV < singleCostV)
					{
						absSumV = 0;
						::memset(coeffCurV, 0, sizeof(TCoeff)* numSamplesChroma);
						if (checkTransformSkipUV)
						{
							minCostV = nullCostV;
						}
					}
					else
					{
						distV = nonZeroDistV;
						if (checkTransformSkipUV)
						{
							minCostV = singleCostV;
						}
					}
				}
			}
			else if (checkTransformSkipUV)
			{
				m_entropyCoder->resetBits();
				m_entropyCoder->encodeQtCbfZero(cu, TEXT_CHROMA_V, trModeC);
				const uint32_t nullBitsV = m_entropyCoder->getNumberOfWrittenBits();
				minCostV = m_rdCost->calcRdCost(distV, nullBitsV);
			}
			if (!absSumV)
			{
				int16_t *ptr = m_qtTempTComYuv[qtlayer].getCrAddr(absTUPartIdxC);
				assert(m_qtTempTComYuv[qtlayer].m_cwidth == MAX_CU_SIZE / 2);

				assert(trWidthC == trHeightC);
				primitives.blockfill_s[(int)g_convertToBit[trWidthC]](ptr, MAX_CU_SIZE / 2, 0);
			}

#if TQ_RUN_IN_X265_ME //Cr
			// get output
			// if original absSumV==0, clearResi for comparing
			int16_t *tmpResi3 = m_trQuant->m3_hevcQT->m_infoFromX265->outResi;
			if(m_trQuant->m3_hevcQT->m_infoFromX265->absSum==0)
			{
				m_trQuant->m3_hevcQT->fillResi(tmpResi3, 0, CTU_SIZE, trWidthC);
			}

			m_trQuant->m3_hevcQT->proc(1); // QT proc for inter in x265
#endif

#if TQ_RUN_IN_HWC_ME // V resi, absSumV==0	
			if(tmpAbsSumV==0)
			{
				if(g_fme)
				{
					for(int i = 0; i<trHeightC; i++)				
					{
						memset(g_fmeResiV[tmpDepth]+tuPosC+i*32, 0, trWidthC*sizeof(int16_t));
					}	
				}
				if(g_merge)
				{
					for(int i = 0; i<trHeightC; i++)				
					{
						memset(g_mergeResiV[tmpDepth]+tuPosC+i*32, 0, trWidthC*sizeof(int16_t));
					}	
				}			
			}

			// update flags
			if (g_fme && ((cu->getDepth(0) == 0 && absTUPartIdx == 192) || cu->getDepth(0) > 0))
				g_fme = false;
			if (g_merge && ((cu->getDepth(0) == 0 && absTUPartIdx == 192) || cu->getDepth(0) > 0))			
				g_merge = false;
#endif
		}
		cu->setCbfSubParts(absSumY ? setCbf : 0, TEXT_LUMA, absPartIdx, depth);
		if (bCodeChroma)
		{
			cu->setCbfSubParts(absSumU ? setCbf : 0, TEXT_CHROMA_U, absPartIdx, cu->getDepth(0) + trModeC);
			cu->setCbfSubParts(absSumV ? setCbf : 0, TEXT_CHROMA_V, absPartIdx, cu->getDepth(0) + trModeC);
		}

		if (checkTransformSkipY)
		{
			uint32_t nonZeroDistY = 0, absSumTransformSkipY;
			int lastPosTransformSkipY = -1;
			uint64_t singleCostY = MAX_INT64;

			int16_t *curResiY = m_qtTempTComYuv[qtlayer].getLumaAddr(absTUPartIdx);
			assert(m_qtTempTComYuv[qtlayer].m_width == MAX_CU_SIZE);

			TCoeff bestCoeffY[32 * 32];
			memcpy(bestCoeffY, coeffCurY, sizeof(TCoeff)* numSamplesLuma);

			int16_t bestResiY[32 * 32];
			for (int i = 0; i < trHeight; ++i)
			{
				memcpy(bestResiY + i * trWidth, curResiY + i * MAX_CU_SIZE, sizeof(int16_t)* trWidth);
			}

			m_rdGoOnSbacCoder->load(m_rdSbacCoders[depth][CI_QT_TRAFO_ROOT]);

			cu->setTransformSkipSubParts(1, TEXT_LUMA, absPartIdx, depth);

			if (m_cfg->bEnableRDOQTS)
			{
				m_entropyCoder->estimateBit(m_trQuant->m_estBitsSbac, trWidth, trHeight, TEXT_LUMA);
			}

			m_trQuant->setQPforQuant(cu->getQP(0), TEXT_LUMA, cu->getSlice()->getSPS()->getQpBDOffsetY(), 0);

			m_trQuant->selectLambda(TEXT_LUMA);
			absSumTransformSkipY = m_trQuant->transformNxN(cu, resiYuv->getLumaAddr(absTUPartIdx), resiYuv->m_width, coeffCurY,
				trWidth, trHeight, TEXT_LUMA, absPartIdx, &lastPosTransformSkipY, true, curuseRDOQ);
			cu->setCbfSubParts(absSumTransformSkipY ? setCbf : 0, TEXT_LUMA, absPartIdx, depth);

			if (absSumTransformSkipY != 0)
			{
				m_entropyCoder->resetBits();
				m_entropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_LUMA, trMode);
				m_entropyCoder->encodeCoeffNxN(cu, coeffCurY, absPartIdx, trWidth, trHeight, depth, TEXT_LUMA);
				const uint32_t skipSingleBitsY = m_entropyCoder->getNumberOfWrittenBits();

				m_trQuant->setQPforQuant(cu->getQP(0), TEXT_LUMA, cu->getSlice()->getSPS()->getQpBDOffsetY(), 0);

				int scalingListType = 3 + g_eTTable[(int)TEXT_LUMA];
				assert(scalingListType < 6);
				assert(m_qtTempTComYuv[qtlayer].m_width == MAX_CU_SIZE);

				m_trQuant->invtransformNxN(cu->getCUTransquantBypass(absPartIdx), REG_DCT, curResiY, MAX_CU_SIZE, coeffCurY, trWidth, trHeight, scalingListType, true, lastPosTransformSkipY);

				nonZeroDistY = primitives.sse_ss[partSize](resiYuv->getLumaAddr(absTUPartIdx), resiYuv->m_width,
					m_qtTempTComYuv[qtlayer].getLumaAddr(absTUPartIdx),
					MAX_CU_SIZE);

				singleCostY = m_rdCost->calcRdCost(nonZeroDistY, skipSingleBitsY);
			}

			if (!absSumTransformSkipY || minCostY < singleCostY)
			{
				cu->setTransformSkipSubParts(0, TEXT_LUMA, absPartIdx, depth);
				memcpy(coeffCurY, bestCoeffY, sizeof(TCoeff)* numSamplesLuma);
				for (int i = 0; i < trHeight; ++i)
				{
					memcpy(curResiY + i * MAX_CU_SIZE, &bestResiY[i * trWidth], sizeof(int16_t)* trWidth);
				}
			}
			else
			{
				distY = nonZeroDistY;
				absSumY = absSumTransformSkipY;
				bestTransformMode[0] = 1;
			}

			cu->setCbfSubParts(absSumY ? setCbf : 0, TEXT_LUMA, absPartIdx, depth);
		}

		if (bCodeChroma && checkTransformSkipUV)
		{
			uint32_t nonZeroDistU = 0, nonZeroDistV = 0, absSumTransformSkipU, absSumTransformSkipV;
			int lastPosTransformSkipU = -1, lastPosTransformSkipV = -1;
			uint64_t singleCostU = MAX_INT64;
			uint64_t singleCostV = MAX_INT64;

			int16_t *curResiU = m_qtTempTComYuv[qtlayer].getCbAddr(absTUPartIdxC);
			int16_t *curResiV = m_qtTempTComYuv[qtlayer].getCrAddr(absTUPartIdxC);
			assert(m_qtTempTComYuv[qtlayer].m_cwidth == MAX_CU_SIZE / 2);

			TCoeff bestCoeffU[32 * 32], bestCoeffV[32 * 32];
			memcpy(bestCoeffU, coeffCurU, sizeof(TCoeff)* numSamplesChroma);
			memcpy(bestCoeffV, coeffCurV, sizeof(TCoeff)* numSamplesChroma);

			int16_t bestResiU[32 * 32], bestResiV[32 * 32];
			for (int i = 0; i < trHeightC; ++i)
			{
				memcpy(&bestResiU[i * trWidthC], curResiU + i * (MAX_CU_SIZE / 2), sizeof(int16_t)* trWidthC);
				memcpy(&bestResiV[i * trWidthC], curResiV + i * (MAX_CU_SIZE / 2), sizeof(int16_t)* trWidthC);
			}

			m_rdGoOnSbacCoder->load(m_rdSbacCoders[depth][CI_QT_TRAFO_ROOT]);

			cu->setTransformSkipSubParts(1, TEXT_CHROMA_U, absPartIdx, cu->getDepth(0) + trModeC);
			cu->setTransformSkipSubParts(1, TEXT_CHROMA_V, absPartIdx, cu->getDepth(0) + trModeC);

			if (m_cfg->bEnableRDOQTS)
			{
				m_entropyCoder->estimateBit(m_trQuant->m_estBitsSbac, trWidthC, trHeightC, TEXT_CHROMA);
			}

			int curChromaQpOffset = cu->getSlice()->getPPS()->getChromaCbQpOffset() + cu->getSlice()->getSliceQpDeltaCb();
			m_trQuant->setQPforQuant(cu->getQP(0), TEXT_CHROMA, cu->getSlice()->getSPS()->getQpBDOffsetC(), curChromaQpOffset);
			m_trQuant->selectLambda(TEXT_CHROMA);

			absSumTransformSkipU = m_trQuant->transformNxN(cu, resiYuv->getCbAddr(absTUPartIdxC), resiYuv->m_cwidth, coeffCurU,
				trWidthC, trHeightC, TEXT_CHROMA_U, absPartIdx, &lastPosTransformSkipU, true, curuseRDOQ);
			curChromaQpOffset = cu->getSlice()->getPPS()->getChromaCrQpOffset() + cu->getSlice()->getSliceQpDeltaCr();
			m_trQuant->setQPforQuant(cu->getQP(0), TEXT_CHROMA, cu->getSlice()->getSPS()->getQpBDOffsetC(), curChromaQpOffset);
			absSumTransformSkipV = m_trQuant->transformNxN(cu, resiYuv->getCrAddr(absTUPartIdxC), resiYuv->m_cwidth, coeffCurV,
				trWidthC, trHeightC, TEXT_CHROMA_V, absPartIdx, &lastPosTransformSkipV, true, curuseRDOQ);

			cu->setCbfSubParts(absSumTransformSkipU ? setCbf : 0, TEXT_CHROMA_U, absPartIdx, cu->getDepth(0) + trModeC);
			cu->setCbfSubParts(absSumTransformSkipV ? setCbf : 0, TEXT_CHROMA_V, absPartIdx, cu->getDepth(0) + trModeC);

			m_entropyCoder->resetBits();
			singleBitsU = 0;
			singleBitsV = 0;

			if (absSumTransformSkipU)
			{
				m_entropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_CHROMA_U, trMode);
				m_entropyCoder->encodeCoeffNxN(cu, coeffCurU, absPartIdx, trWidthC, trHeightC, depth, TEXT_CHROMA_U);
				singleBitsU = m_entropyCoder->getNumberOfWrittenBits();

				curChromaQpOffset = cu->getSlice()->getPPS()->getChromaCbQpOffset() + cu->getSlice()->getSliceQpDeltaCb();
				m_trQuant->setQPforQuant(cu->getQP(0), TEXT_CHROMA, cu->getSlice()->getSPS()->getQpBDOffsetC(), curChromaQpOffset);

				int scalingListType = 3 + g_eTTable[(int)TEXT_CHROMA_U];
				assert(scalingListType < 6);
				assert(m_qtTempTComYuv[qtlayer].m_cwidth == MAX_CU_SIZE / 2);

				m_trQuant->invtransformNxN(cu->getCUTransquantBypass(absPartIdx), REG_DCT, curResiU, MAX_CU_SIZE / 2, coeffCurU, trWidthC, trHeightC, scalingListType, true, lastPosTransformSkipU);

				uint32_t dist = primitives.sse_ss[partSizeC](resiYuv->getCbAddr(absTUPartIdxC), resiYuv->m_cwidth,
					m_qtTempTComYuv[qtlayer].getCbAddr(absTUPartIdxC),
					MAX_CU_SIZE / 2);
				nonZeroDistU = m_rdCost->scaleChromaDistCb(dist);
				singleCostU = m_rdCost->calcRdCost(nonZeroDistU, singleBitsU);
			}

			if (!absSumTransformSkipU || minCostU < singleCostU)
			{
				cu->setTransformSkipSubParts(0, TEXT_CHROMA_U, absPartIdx, cu->getDepth(0) + trModeC);

				memcpy(coeffCurU, bestCoeffU, sizeof(TCoeff)* numSamplesChroma);
				for (int i = 0; i < trHeightC; ++i)
				{
					memcpy(curResiU + i * (MAX_CU_SIZE / 2), &bestResiU[i * trWidthC], sizeof(int16_t)* trWidthC);
				}
			}
			else
			{
				distU = nonZeroDistU;
				absSumU = absSumTransformSkipU;
				bestTransformMode[1] = 1;
			}

			if (absSumTransformSkipV)
			{
				m_entropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_CHROMA_V, trMode);
				m_entropyCoder->encodeCoeffNxN(cu, coeffCurV, absPartIdx, trWidthC, trHeightC, depth, TEXT_CHROMA_V);
				singleBitsV = m_entropyCoder->getNumberOfWrittenBits() - singleBitsU;

				curChromaQpOffset = cu->getSlice()->getPPS()->getChromaCrQpOffset() + cu->getSlice()->getSliceQpDeltaCr();
				m_trQuant->setQPforQuant(cu->getQP(0), TEXT_CHROMA, cu->getSlice()->getSPS()->getQpBDOffsetC(), curChromaQpOffset);

				int scalingListType = 3 + g_eTTable[(int)TEXT_CHROMA_V];
				assert(scalingListType < 6);
				assert(m_qtTempTComYuv[qtlayer].m_cwidth == MAX_CU_SIZE / 2);

				m_trQuant->invtransformNxN(cu->getCUTransquantBypass(absPartIdx), REG_DCT, curResiV, MAX_CU_SIZE / 2, coeffCurV, trWidthC, trHeightC, scalingListType, true, lastPosTransformSkipV);

				uint32_t dist = primitives.sse_ss[partSizeC](resiYuv->getCrAddr(absTUPartIdxC), resiYuv->m_cwidth,
					m_qtTempTComYuv[qtlayer].getCrAddr(absTUPartIdxC),
					MAX_CU_SIZE / 2);
				nonZeroDistV = m_rdCost->scaleChromaDistCr(dist);
				singleCostV = m_rdCost->calcRdCost(nonZeroDistV, singleBitsV);
			}

			if (!absSumTransformSkipV || minCostV < singleCostV)
			{
				cu->setTransformSkipSubParts(0, TEXT_CHROMA_V, absPartIdx, cu->getDepth(0) + trModeC);

				memcpy(coeffCurV, bestCoeffV, sizeof(TCoeff)* numSamplesChroma);
				for (int i = 0; i < trHeightC; ++i)
				{
					memcpy(curResiV + i * (MAX_CU_SIZE / 2), &bestResiV[i * trWidthC], sizeof(int16_t)* trWidthC);
				}
			}
			else
			{
				distV = nonZeroDistV;
				absSumV = absSumTransformSkipV;
				bestTransformMode[2] = 1;
			}

			cu->setCbfSubParts(absSumU ? setCbf : 0, TEXT_CHROMA_U, absPartIdx, cu->getDepth(0) + trModeC);
			cu->setCbfSubParts(absSumV ? setCbf : 0, TEXT_CHROMA_V, absPartIdx, cu->getDepth(0) + trModeC);
		}

		m_rdGoOnSbacCoder->load(m_rdSbacCoders[depth][CI_QT_TRAFO_ROOT]);

		m_entropyCoder->resetBits();

		if (trSizeLog2 > cu->getQuadtreeTULog2MinSizeInCU(absPartIdx))
		{
			m_entropyCoder->encodeTransformSubdivFlag(0, 5 - trSizeLog2);
		}

		if (bCodeChroma)
		{
			m_entropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_CHROMA_U, trMode);
			m_entropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_CHROMA_V, trMode);
		}

		m_entropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_LUMA, trMode);
		m_entropyCoder->encodeCoeffNxN(cu, coeffCurY, absPartIdx, trWidth, trHeight, depth, TEXT_LUMA);

		if (bCodeChroma)
		{
			m_entropyCoder->encodeCoeffNxN(cu, coeffCurU, absPartIdx, trWidthC, trHeightC, depth, TEXT_CHROMA_U);
			m_entropyCoder->encodeCoeffNxN(cu, coeffCurV, absPartIdx, trWidthC, trHeightC, depth, TEXT_CHROMA_V);
		}

		singleBits = m_entropyCoder->getNumberOfWrittenBits();
		singleDist = distY + distU + distV;
		singleCost = m_rdCost->calcRdCost(singleDist, singleBits);
	}

	// code sub-blocks
	if (bCheckSplit)
	{
		if (bCheckFull)
		{
			m_rdGoOnSbacCoder->store(m_rdSbacCoders[depth][CI_QT_TRAFO_TEST]);
			m_rdGoOnSbacCoder->load(m_rdSbacCoders[depth][CI_QT_TRAFO_ROOT]);
		}
		uint32_t subdivDist = 0;
		uint32_t subdivBits = 0;
		uint64_t subDivCost = 0;

		const uint32_t qPartNumSubdiv = cu->getPic()->getNumPartInCU() >> ((depth + 1) << 1);
		for (uint32_t i = 0; i < 4; ++i)
		{
			uint32_t nsAddr = absPartIdx + i * qPartNumSubdiv;
			xEstimateResidualQT(cu, absPartIdx + i * qPartNumSubdiv, nsAddr, resiYuv, depth + 1, subDivCost, subdivBits, subdivDist, bCheckFull ? NULL : outZeroDist);
		}

		uint32_t ycbf = 0;
		uint32_t ucbf = 0;
		uint32_t vcbf = 0;
		for (uint32_t i = 0; i < 4; ++i)
		{
			ycbf |= cu->getCbf(absPartIdx + i * qPartNumSubdiv, TEXT_LUMA, trMode + 1);
			ucbf |= cu->getCbf(absPartIdx + i * qPartNumSubdiv, TEXT_CHROMA_U, trMode + 1);
			vcbf |= cu->getCbf(absPartIdx + i * qPartNumSubdiv, TEXT_CHROMA_V, trMode + 1);
		}

		for (uint32_t i = 0; i < 4 * qPartNumSubdiv; ++i)
		{
			cu->getCbf(TEXT_LUMA)[absPartIdx + i] |= ycbf << trMode;
			cu->getCbf(TEXT_CHROMA_U)[absPartIdx + i] |= ucbf << trMode;
			cu->getCbf(TEXT_CHROMA_V)[absPartIdx + i] |= vcbf << trMode;
		}

		m_rdGoOnSbacCoder->load(m_rdSbacCoders[depth][CI_QT_TRAFO_ROOT]);
		m_entropyCoder->resetBits();

		xEncodeResidualQT(cu, absPartIdx, depth, true, TEXT_LUMA);
		xEncodeResidualQT(cu, absPartIdx, depth, false, TEXT_LUMA);
		xEncodeResidualQT(cu, absPartIdx, depth, false, TEXT_CHROMA_U);
		xEncodeResidualQT(cu, absPartIdx, depth, false, TEXT_CHROMA_V);

		subdivBits = m_entropyCoder->getNumberOfWrittenBits();
		subDivCost = m_rdCost->calcRdCost(subdivDist, subdivBits);

		if (ycbf || ucbf || vcbf || !bCheckFull)
		{
			if (subDivCost < singleCost)
			{
				rdCost += subDivCost;
				outBits += subdivBits;
				outDist += subdivDist;
				return;
			}
		}

		cu->setTransformSkipSubParts(bestTransformMode[0], TEXT_LUMA, absPartIdx, depth);
		if (bCodeChroma)
		{
			cu->setTransformSkipSubParts(bestTransformMode[1], TEXT_CHROMA_U, absPartIdx, cu->getDepth(0) + trModeC);
			cu->setTransformSkipSubParts(bestTransformMode[2], TEXT_CHROMA_V, absPartIdx, cu->getDepth(0) + trModeC);
		}
		assert(bCheckFull);
		m_rdGoOnSbacCoder->load(m_rdSbacCoders[depth][CI_QT_TRAFO_TEST]);
	}

	rdCost += singleCost;
	outBits += singleBits;
	outDist += singleDist;

	cu->setTrIdxSubParts(trMode, absPartIdx, depth);
	cu->setCbfSubParts(absSumY ? setCbf : 0, TEXT_LUMA, absPartIdx, depth);

	if (bCodeChroma)
	{
		cu->setCbfSubParts(absSumU ? setCbf : 0, TEXT_CHROMA_U, absPartIdx, cu->getDepth(0) + trModeC);
		cu->setCbfSubParts(absSumV ? setCbf : 0, TEXT_CHROMA_V, absPartIdx, cu->getDepth(0) + trModeC);
	}
}

void TEncSearch::xEncodeResidualQT(TComDataCU* cu, uint32_t absPartIdx, const uint32_t depth, bool bSubdivAndCbf, TextType ttype)
{
	assert(cu->getDepth(0) == cu->getDepth(absPartIdx));
	const uint32_t curTrMode = depth - cu->getDepth(0);
	const uint32_t trMode = cu->getTransformIdx(absPartIdx);
	const bool bSubdiv = curTrMode != trMode;
	const uint32_t trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> depth] + 2;

	if (bSubdivAndCbf && trSizeLog2 <= cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() && trSizeLog2 > cu->getQuadtreeTULog2MinSizeInCU(absPartIdx))
	{
		m_entropyCoder->encodeTransformSubdivFlag(bSubdiv, 5 - trSizeLog2);
	}

	assert(cu->getPredictionMode(absPartIdx) != MODE_INTRA);
	if (bSubdivAndCbf)
	{
		const bool bFirstCbfOfCU = curTrMode == 0;
		if (bFirstCbfOfCU || trSizeLog2 > 2)
		{
			if (bFirstCbfOfCU || cu->getCbf(absPartIdx, TEXT_CHROMA_U, curTrMode - 1))
			{
				m_entropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_CHROMA_U, curTrMode);
			}
			if (bFirstCbfOfCU || cu->getCbf(absPartIdx, TEXT_CHROMA_V, curTrMode - 1))
			{
				m_entropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_CHROMA_V, curTrMode);
			}
		}
		else if (trSizeLog2 == 2)
		{
			assert(cu->getCbf(absPartIdx, TEXT_CHROMA_U, curTrMode) == cu->getCbf(absPartIdx, TEXT_CHROMA_U, curTrMode - 1));
			assert(cu->getCbf(absPartIdx, TEXT_CHROMA_V, curTrMode) == cu->getCbf(absPartIdx, TEXT_CHROMA_V, curTrMode - 1));
		}
	}

	if (!bSubdiv)
	{
		const uint32_t numCoeffPerAbsPartIdxIncrement = cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight() >> (cu->getSlice()->getSPS()->getMaxCUDepth() << 1);
		//assert( 16 == uiNumCoeffPerAbsPartIdxIncrement ); // check
		const uint32_t qtlayer = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() - trSizeLog2;
		TCoeff *coeffCurY = m_qtTempCoeffY[qtlayer] + numCoeffPerAbsPartIdxIncrement * absPartIdx;
		TCoeff *coeffCurU = m_qtTempCoeffCb[qtlayer] + (numCoeffPerAbsPartIdxIncrement * absPartIdx >> 2);
		TCoeff *coeffCurV = m_qtTempCoeffCr[qtlayer] + (numCoeffPerAbsPartIdxIncrement * absPartIdx >> 2);

		bool  bCodeChroma = true;
		uint32_t  trModeC = trMode;
		uint32_t  trSizeCLog2 = trSizeLog2 - 1;
		if (trSizeLog2 == 2)
		{
			trSizeCLog2++;
			trModeC--;
			uint32_t qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + trModeC) << 1);
			bCodeChroma = ((absPartIdx % qpdiv) == 0);
		}

		if (bSubdivAndCbf)
		{
			m_entropyCoder->encodeQtCbf(cu, absPartIdx, TEXT_LUMA, trMode);
		}
		else
		{
			if (ttype == TEXT_LUMA && cu->getCbf(absPartIdx, TEXT_LUMA, trMode))
			{
				int trWidth = 1 << trSizeLog2;
				int trHeight = 1 << trSizeLog2;
				m_entropyCoder->encodeCoeffNxN(cu, coeffCurY, absPartIdx, trWidth, trHeight, depth, TEXT_LUMA);
			}
			if (bCodeChroma)
			{
				int trWidth = 1 << trSizeCLog2;
				int trHeight = 1 << trSizeCLog2;
				if (ttype == TEXT_CHROMA_U && cu->getCbf(absPartIdx, TEXT_CHROMA_U, trMode))
				{
					m_entropyCoder->encodeCoeffNxN(cu, coeffCurU, absPartIdx, trWidth, trHeight, depth, TEXT_CHROMA_U);
				}
				if (ttype == TEXT_CHROMA_V && cu->getCbf(absPartIdx, TEXT_CHROMA_V, trMode))
				{
					m_entropyCoder->encodeCoeffNxN(cu, coeffCurV, absPartIdx, trWidth, trHeight, depth, TEXT_CHROMA_V);
				}
			}
		}
	}
	else
	{
		if (bSubdivAndCbf || cu->getCbf(absPartIdx, ttype, curTrMode))
		{
			const uint32_t qpartNumSubdiv = cu->getPic()->getNumPartInCU() >> ((depth + 1) << 1);
			for (uint32_t i = 0; i < 4; ++i)
			{
				xEncodeResidualQT(cu, absPartIdx + i * qpartNumSubdiv, depth + 1, bSubdivAndCbf, ttype);
			}
		}
	}
}

void TEncSearch::xSetResidualQTData(TComDataCU* cu, uint32_t absPartIdx, uint32_t absTUPartIdx, TShortYUV* resiYuv, uint32_t depth, bool bSpatial)
{
	assert(cu->getDepth(0) == cu->getDepth(absPartIdx));
	const uint32_t curTrMode = depth - cu->getDepth(0);
	const uint32_t trMode = cu->getTransformIdx(absPartIdx);

	if (curTrMode == trMode)
	{
		const uint32_t trSizeLog2 = g_convertToBit[cu->getSlice()->getSPS()->getMaxCUWidth() >> depth] + 2;
		const uint32_t qtlayer = cu->getSlice()->getSPS()->getQuadtreeTULog2MaxSize() - trSizeLog2;

		bool  bCodeChroma = true;
		uint32_t  trModeC = trMode;
		uint32_t  trSizeCLog2 = trSizeLog2 - 1;
		if (trSizeLog2 == 2)
		{
			trSizeCLog2++;
			trModeC--;
			uint32_t qpdiv = cu->getPic()->getNumPartInCU() >> ((cu->getDepth(0) + trModeC) << 1);
			bCodeChroma = ((absPartIdx % qpdiv) == 0);
		}

		if (bSpatial)
		{
			int trWidth = 1 << trSizeLog2;
			int trHeight = 1 << trSizeLog2;
			m_qtTempTComYuv[qtlayer].copyPartToPartLuma(resiYuv, absTUPartIdx, trWidth, trHeight);

			if (bCodeChroma)
			{
				m_qtTempTComYuv[qtlayer].copyPartToPartChroma(resiYuv, absPartIdx, 1 << trSizeCLog2, 1 << trSizeCLog2);
			}
		}
		else
		{
			uint32_t uiNumCoeffPerAbsPartIdxIncrement = cu->getSlice()->getSPS()->getMaxCUWidth() * cu->getSlice()->getSPS()->getMaxCUHeight() >> (cu->getSlice()->getSPS()->getMaxCUDepth() << 1);
			uint32_t uiNumCoeffY = (1 << (trSizeLog2 << 1));
			TCoeff* pcCoeffSrcY = m_qtTempCoeffY[qtlayer] + uiNumCoeffPerAbsPartIdxIncrement * absPartIdx;
			TCoeff* pcCoeffDstY = cu->getCoeffY() + uiNumCoeffPerAbsPartIdxIncrement * absPartIdx;
			::memcpy(pcCoeffDstY, pcCoeffSrcY, sizeof(TCoeff)* uiNumCoeffY);
			if (bCodeChroma)
			{
				uint32_t    uiNumCoeffC = (1 << (trSizeCLog2 << 1));
				TCoeff* pcCoeffSrcU = m_qtTempCoeffCb[qtlayer] + (uiNumCoeffPerAbsPartIdxIncrement * absPartIdx >> 2);
				TCoeff* pcCoeffSrcV = m_qtTempCoeffCr[qtlayer] + (uiNumCoeffPerAbsPartIdxIncrement * absPartIdx >> 2);
				TCoeff* pcCoeffDstU = cu->getCoeffCb() + (uiNumCoeffPerAbsPartIdxIncrement * absPartIdx >> 2);
				TCoeff* pcCoeffDstV = cu->getCoeffCr() + (uiNumCoeffPerAbsPartIdxIncrement * absPartIdx >> 2);
				::memcpy(pcCoeffDstU, pcCoeffSrcU, sizeof(TCoeff)* uiNumCoeffC);
				::memcpy(pcCoeffDstV, pcCoeffSrcV, sizeof(TCoeff)* uiNumCoeffC);
			}
		}
	}
	else
	{
		const uint32_t qPartNumSubdiv = cu->getPic()->getNumPartInCU() >> ((depth + 1) << 1);
		for (uint32_t i = 0; i < 4; ++i)
		{
			uint32_t nsAddr = absPartIdx + i * qPartNumSubdiv;
			xSetResidualQTData(cu, absPartIdx + i * qPartNumSubdiv, nsAddr, resiYuv, depth + 1, bSpatial);
		}
	}
}

uint32_t TEncSearch::xModeBitsIntra(TComDataCU* cu, uint32_t mode, uint32_t partOffset, uint32_t depth, uint32_t initTrDepth)
{
	// Reload only contexts required for coding intra mode information
	m_rdGoOnSbacCoder->loadIntraDirModeLuma(m_rdSbacCoders[depth][CI_CURR_BEST]);
#if INTRA_PU_4x4_MODIFY
	m_rdGoOnSbacCoder->loadIntraDirModeLuma(m_rdSbacCoders[depth][CI_TEMP_BEST]);
#endif
	cu->setLumaIntraDirSubParts(mode, partOffset, depth + initTrDepth);

	m_entropyCoder->resetBits();
	m_entropyCoder->encodeIntraDirModeLuma(cu, partOffset);

	return m_entropyCoder->getNumberOfWrittenBits();
}

uint32_t TEncSearch::xUpdateCandList(uint32_t mode, uint64_t cost, uint32_t fastCandNum, uint32_t* CandModeList, uint64_t* CandCostList)
{
	uint32_t i;
	uint32_t shift = 0;

	while (shift < fastCandNum && cost < CandCostList[fastCandNum - 1 - shift])
	{
		shift++;
	}

	if (shift != 0)
	{
		for (i = 1; i < shift; i++)
		{
			CandModeList[fastCandNum - i] = CandModeList[fastCandNum - 1 - i];
			CandCostList[fastCandNum - i] = CandCostList[fastCandNum - 1 - i];
		}

		CandModeList[fastCandNum - shift] = mode;
		CandCostList[fastCandNum - shift] = cost;
		return 1;
	}

	return 0;
}

/** add inter-prediction syntax elements for a CU block
 * \param cu
 */
uint32_t TEncSearch::xSymbolBitsInter(TComDataCU* cu)
{
	if (cu->getMergeFlag(0) && cu->getPartitionSize(0) == SIZE_2Nx2N && !cu->getQtRootCbf(0))
	{
		cu->setSkipFlagSubParts(true, 0, cu->getDepth(0));

		m_entropyCoder->resetBits();
		if (cu->getSlice()->getPPS()->getTransquantBypassEnableFlag())
		{
			m_entropyCoder->encodeCUTransquantBypassFlag(cu, 0, true);
		}
		m_entropyCoder->encodeSkipFlag(cu, 0, true);
		m_entropyCoder->encodeMergeIndex(cu, 0, true);
		return m_entropyCoder->getNumberOfWrittenBits();
	}
	else
	{
		m_entropyCoder->resetBits();
		if (cu->getSlice()->getPPS()->getTransquantBypassEnableFlag())
		{
			m_entropyCoder->encodeCUTransquantBypassFlag(cu, 0, true);
		}
		m_entropyCoder->encodeSkipFlag(cu, 0, true);
		m_entropyCoder->encodePredMode(cu, 0, true);
		m_entropyCoder->encodePartSize(cu, 0, cu->getDepth(0), true);
		m_entropyCoder->encodePredInfo(cu, 0, true);
		bool bDummy = false;

#if GET_X265_ORG_DATA_TU
		int64_t temp0 = m_entropyCoder->m_entropyCoderIf->getNumberOfWrittenBits_fraction();
		m_entropyCoder->encodeCoeff(cu, 0, cu->getDepth(0), cu->getWidth(0), cu->getHeight(0), bDummy);
		int64_t temp1 = m_entropyCoder->m_entropyCoderIf->getNumberOfWrittenBits_fraction();

		if (cu->getMergeFlag(0) == 1)
		{
			g_est_bit_tu[2][cu->getDepth(0) ][cu->getZorderIdxInCU() ] = (temp1 - temp0);
		} 
		else
		{
			g_est_bit_tu[0][cu->getDepth(0) ][cu->getZorderIdxInCU() ] = (temp1 - temp0);
		}
		
//		g_est_bit_cu[0][cu->getDepth(0) ][cu->getZorderIdxInCU() ] = (temp1 - temp);

#else
		m_entropyCoder->encodeCoeff(cu, 0, cu->getDepth(0), cu->getWidth(0), cu->getHeight(0), bDummy);
#endif

		return m_entropyCoder->getNumberOfWrittenBits();
	}
}


/**** Function to estimate the header bits ************/
uint32_t  TEncSearch::estimateHeaderBits(TComDataCU* cu, uint32_t absPartIdx)
{
	uint32_t bits = 0;

	m_entropyCoder->resetBits();

	uint32_t lpelx = cu->getCUPelX() + g_rasterToPelX[g_zscanToRaster[absPartIdx]];
	uint32_t rpelx = lpelx + (g_maxCUWidth >> cu->getDepth(0)) - 1;
	uint32_t tpely = cu->getCUPelY() + g_rasterToPelY[g_zscanToRaster[absPartIdx]];
	uint32_t bpely = tpely + (g_maxCUHeight >> cu->getDepth(0)) - 1;

	TComSlice * slice = cu->getPic()->getSlice();
	if ((rpelx < slice->getSPS()->getPicWidthInLumaSamples()) && (bpely < slice->getSPS()->getPicHeightInLumaSamples()))
	{
		m_entropyCoder->encodeSplitFlag(cu, absPartIdx, cu->getDepth(0));
	}

	if (cu->getMergeFlag(0) && cu->getPartitionSize(0) == SIZE_2Nx2N && !cu->getQtRootCbf(0))
	{
		m_entropyCoder->encodeMergeFlag(cu, 0);
		m_entropyCoder->encodeMergeIndex(cu, 0, true);
	}

	if (cu->getSlice()->getPPS()->getTransquantBypassEnableFlag())
	{
		m_entropyCoder->encodeCUTransquantBypassFlag(cu, 0, true);
	}

	if (!cu->getSlice()->isIntra())
	{
		m_entropyCoder->encodeSkipFlag(cu, 0, true);
	}

	m_entropyCoder->encodePredMode(cu, 0, true);

	m_entropyCoder->encodePartSize(cu, 0, cu->getDepth(0), true);
	bits += m_entropyCoder->getNumberOfWrittenBits();

	return bits;
}

void  TEncSearch::setWpScalingDistParam(TComDataCU*, int, int)
{
#if 0 // dead code
	if (refIdx < 0)
	{
		m_distParam.applyWeight = false;
		return;
	}

	TComSlice       *slice = cu->getSlice();
	TComPPS         *pps   = cu->getSlice()->getPPS();
	wpScalingParam  *wp0, *wp1;
	m_distParam.applyWeight = (slice->getSliceType() == P_SLICE && pps->getUseWP()) || (slice->getSliceType() == B_SLICE && pps->getWPBiPred());
	if (!m_distParam.applyWeight) return;

	int refIdx0 = (list == REF_PIC_LIST_0) ? refIdx : (-1);
	int refIdx1 = (list == REF_PIC_LIST_1) ? refIdx : (-1);

	getWpScaling(cu, refIdx0, refIdx1, wp0, wp1);

	if (refIdx0 < 0) wp0 = NULL;
	if (refIdx1 < 0) wp1 = NULL;

	m_distParam.wpCur  = NULL;

	if (list == REF_PIC_LIST_0)
	{
		m_distParam.wpCur = wp0;
	}
	else
	{
		m_distParam.wpCur = wp1;
	}
#endif // if 0
}

//! \}
