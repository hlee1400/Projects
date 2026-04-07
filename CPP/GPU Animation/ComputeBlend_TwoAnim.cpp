//----------------------------------------------------------------------------
// Copyright 2026, Ed Keenan, all rights reserved.
//----------------------------------------------------------------------------

#include "ComputeBlend_TwoAnim.h"
#include "Anim.h"
#include "HierarchyTableMan.h"
#include "StateDirectXMan.h"
#include "ShaderObjectNodeMan.h"
#include "Clip.h"

namespace Azul
{
	ComputeBlend_TwoAnim::ComputeBlend_TwoAnim(Anim *pAnim_A, Anim *pAnim_B)
		:ComputeBlend(),
		poMixerA{nullptr},
		poMixerB{nullptr},
		poMixerC{nullptr},
		poWorldComputeC{nullptr}
	{
		// --- Mixter A ----
		assert(pAnim_A);
		Clip *pClipA = pAnim_A->GetClip();
		assert(pClipA);

		this->poMixerA = new MixerA(pClipA);
		assert(this->poMixerA);

		// --- Mixer B ----
		assert(pAnim_B);
		Clip *pClipB = pAnim_B->GetClip();
		assert(pClipB);


		this->poMixerB = new MixerB(pClipB);
		assert(this->poMixerB);

		// --- Mixer C ----
		HierarchyTable *pHierarchyTable0 = HierarchyTableMan::Find(pClipA->GetHierarchyName());
		assert(pHierarchyTable0);

		HierarchyTable *pHierarchyTable1 = HierarchyTableMan::Find(pClipB->GetHierarchyName());
		assert(pHierarchyTable1);

		assert(pHierarchyTable0 == pHierarchyTable1);

		this->poMixerC = new MixerC(pClipA);
		assert(this->poMixerC);

		// --- World Compute ----
		this->poWorldComputeC = new WorldComputeC(this->poMixerC, pHierarchyTable0);
		assert(this->poWorldComputeC);

	}

	ComputeBlend_TwoAnim::~ComputeBlend_TwoAnim()
	{
		delete this->poMixerA;
		this->poMixerA = nullptr;

		delete this->poMixerB;
		this->poMixerB = nullptr;

		delete this->poMixerC;
		this->poMixerC = nullptr;

		delete this->poWorldComputeC;
		this->poWorldComputeC = nullptr;
	}

	void ComputeBlend_TwoAnim::Execute()
	{
		this->privMixerExecute();
		this->privWorldComputeExecute();
	}

	void ComputeBlend_TwoAnim::AnimateMixerA(Clip *pClip, AnimTime time)
	{
		pClip->AnimateBones(time, this->poMixerA);
	}

	void ComputeBlend_TwoAnim::AnimateMixerB(Clip *pClip, AnimTime time)
	{
		pClip->AnimateBones(time, this->poMixerB);
	}

	void ComputeBlend_TwoAnim::AnimateMixerC(float blendWeight)
	{
		this->poMixerC->pKeyCa = &this->poMixerA->mMixerResult;
		this->poMixerC->pKeyCb = &this->poMixerB->mMixerResult;
		this->poMixerC->poMixerConstant->ts = blendWeight;
	}

	void ComputeBlend_TwoAnim::BindWorldBoneArray()
	{
		BufferSRV_cs *pBoneWorld = this->poWorldComputeC->GetBoneWorld();
		pBoneWorld->BindComputeToVS(ShaderResourceBufferSlot::BoneWorldIn);
	}

	void ComputeBlend_TwoAnim::privMixerExecute()
	{
		// ------------------------------------------------
		//  execute Compute Shader 
		// ------------------------------------------------

		ShaderObject *pShaderObj = nullptr;
		unsigned int count = 0;

		// --- MixerA ---
		pShaderObj = ShaderObjectNodeMan::Find(ShaderObject::Name::MixerACompute);
		pShaderObj->ActivateShader();

		assert(poMixerA->pKeyAa);
		poMixerA->pKeyAa->BindComputeToCS(ShaderResourceBufferSlot::KeyAa);

		assert(poMixerA->pKeyAb);
		poMixerA->pKeyAb->BindComputeToCS(ShaderResourceBufferSlot::KeyAb);

		assert(poMixerA->GetMixerResult());
		poMixerA->GetMixerResult()->BindCompute(UnorderedAccessBufferSlot::MixerOutAx);

		assert(poMixerA->GetMixerConstBuff());
		poMixerA->GetMixerConstBuff()->Transfer(poMixerA->GetRawConstBuffer());
		poMixerA->GetMixerConstBuff()->BindCompute(ConstantCSBufferSlot::csMixerA);

		count = (unsigned int)ceil((float)poMixerA->GetNumNodes() / 1.0f);
		StateDirectXMan::GetContext()->Dispatch(count, 1, 1);

		// --- MixerB ---
		pShaderObj = ShaderObjectNodeMan::Find(ShaderObject::Name::MixerBCompute);
		pShaderObj->ActivateShader();

		assert(poMixerB->pKeyBa);
		poMixerB->pKeyBa->BindComputeToCS(ShaderResourceBufferSlot::KeyBa);

		assert(poMixerB->pKeyBb);
		poMixerB->pKeyBb->BindComputeToCS(ShaderResourceBufferSlot::KeyBb);

		assert(poMixerB->GetMixerResult());
		poMixerB->GetMixerResult()->BindCompute(UnorderedAccessBufferSlot::MixerOutBx);

		assert(poMixerB->GetMixerConstBuff());
		poMixerB->GetMixerConstBuff()->Transfer(poMixerB->GetRawConstBuffer());
		poMixerB->GetMixerConstBuff()->BindCompute(ConstantCSBufferSlot::csMixerB);

		count = (unsigned int)ceil((float)poMixerB->GetNumNodes() / 1.0f);
		StateDirectXMan::GetContext()->Dispatch(count, 1, 1);

		// --- MixerC ---
		pShaderObj = ShaderObjectNodeMan::Find(ShaderObject::Name::MixerCCompute);
		pShaderObj->ActivateShader();

		assert(poMixerC->pKeyCa);
		poMixerC->pKeyCa->BindCompute(UnorderedAccessBufferSlot::MixerOutAx);

		assert(poMixerC->pKeyCb);
		poMixerC->pKeyCb->BindCompute(UnorderedAccessBufferSlot::MixerOutBx);

		assert(poMixerC->GetMixerResult());
		poMixerC->GetMixerResult()->BindCompute(UnorderedAccessBufferSlot::MixerOutCx);

		assert(poMixerC->GetMixerConstBuff());
		poMixerC->GetMixerConstBuff()->Transfer(poMixerC->GetRawConstBuffer());
		poMixerC->GetMixerConstBuff()->BindCompute(ConstantCSBufferSlot::csMixerC);

		count = (unsigned int)ceil((float)poMixerC->GetNumNodes() / 1.0f);
		StateDirectXMan::GetContext()->Dispatch(count, 1, 1);
	}

	void ComputeBlend_TwoAnim::privWorldComputeExecute()
	{
		// ------------------------------------------------
		//  execute Compute Shader 
		// ------------------------------------------------

		ShaderObject *pShaderObj = ShaderObjectNodeMan::Find(ShaderObject::Name::WorldComputeC);
		pShaderObj->ActivateShader();

		assert(poWorldComputeC->GetLocalBone());
		poWorldComputeC->GetLocalBone()->BindCompute(UnorderedAccessBufferSlot::MixerOutCx);

		assert(poWorldComputeC->GetHierarchy());
		poWorldComputeC->GetHierarchy()->BindComputeToCS(ShaderResourceBufferSlot::HierarchyTable);

		assert(poWorldComputeC->GetUAVWorldMat());
		poWorldComputeC->GetUAVWorldMat()->BindCompute(UnorderedAccessBufferSlot::BoneWorldOut);

		assert(poWorldComputeC->GetWorldConstBuffer());
		poWorldComputeC->GetWorldConstBuffer()->Transfer(poWorldComputeC->GetRawConstBuffer());
		poWorldComputeC->GetWorldConstBuffer()->BindCompute(ConstantCSBufferSlot::csWorld);

		// Dispatch - BANANA - adjust dispatch/shader numbers
		unsigned int count = (unsigned int)ceil((float)poWorldComputeC->GetNumJoints() / 1.0f);
		StateDirectXMan::GetContext()->Dispatch(count, 1, 1);

		// UAV buffers are not allowed in Vertex shaders
		// so copy the UAV buffer into an SRV buffer
		assert(poWorldComputeC->GetBoneWorld());
		StateDirectXMan::GetContext()->CopyResource(poWorldComputeC->GetBoneWorld()->GetD3DBuffer(),
													poWorldComputeC->GetUAVWorldMat()->GetD3DBuffer());

	}


}

// --- End of File ---
