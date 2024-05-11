/*
 * Copyright (c) 2022-2024, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "tensorrt_llm/common/quantization.h"
#include "tensorrt_llm/runtime/common.h"
#include "tensorrt_llm/runtime/loraModule.h"
#include "tensorrt_llm/runtime/medusaModule.h"
#include <NvInferRuntime.h>

namespace tensorrt_llm::runtime
{

class ModelConfig
{
public:
    enum class ModelVariant : std::int32_t
    {
        kGpt = 0,
        kGlm = 1,            // https://github.com/THUDM/GLM and https://github.com/THUDM/ChatGLM-6B
        kMamba = 2,          // https://github.com/state-spaces/mamba
        kRecurrentGemma = 3, // https://github.com/google-deepmind/recurrentgemma
    };

    struct MambaConfig
    {
        SizeType dState = 0;
        SizeType dConv = 0;
        SizeType expand = 0;
    };

    struct RnnConfig
    {
        SizeType dConv = 0;
        SizeType hiddenSize = 0;
    };

    enum class LayerType : std::int32_t
    {
        kATTENTION,
        kRECURRENT,
    };

    explicit ModelConfig(SizeType vocabSize, SizeType nbAttentionLayers, SizeType nbSsmLayers, SizeType nbHeads,
        SizeType hiddenSize, nvinfer1::DataType dtype)
        : mVocabSize(vocabSize)
        , mNbAttentionLayers(nbAttentionLayers)
        , mNbSsmLayers(nbSsmLayers)
        , mNbHeads(nbHeads)
        , mNbKvHeads(nbHeads)
        , mHiddenSize(hiddenSize)
        , mSizePerHead(mHiddenSize / mNbHeads)
        , mDataType(dtype)
        , mUseGptAttentionPlugin(false)
        , mUseMambaConv1dPlugin(false)
        , mInputPacked{false}
        , mPagedKvCache{false}
        , mPagedState{false}
        , mTokensPerBlock{64}
        , mQuantMode{common::QuantMode::none()}
        , mMaxBatchSize(0)
        , mMaxBeamWidth(0)
        , mMaxInputLen(0)
        , mMaxSequenceLen(0)
        , mMaxNumTokens(std::nullopt)
        , mComputeContextLogits(false)
        , mComputeGenerationLogits(false)
        , mModelVariant(ModelVariant::kGpt)
        , mUseCustomAllReduce(false)
        , mMaxPromptEmbeddingTableSize(0)
        , mMaxDraftLen(0)
        , mUseContextFMHAForGeneration(false)
        , mPagedContextFMHA(false)
        , mUseXQA{false}
        , mUseLoraPlugin(false)
        , mMlpHiddenSize(0)
        , mMedusaModule(std::nullopt)
        , mUseCrossAttention(false)
        , mUsePositionEmbedding(true) // TODO: remove these two properties?
        , mUseTokenTypeEmbedding(false)
    {
    }

    [[nodiscard]] SizeType constexpr getVocabSize() const noexcept
    {
        return mVocabSize;
    }

    [[nodiscard]] SizeType constexpr getVocabSizePadded(SizeType worldSize) const noexcept
    {
        return (mVocabSize + worldSize - 1) / worldSize * worldSize;
    }

    [[nodiscard]] SizeType constexpr getNbAttentionLayers(SizeType pipelineParallelism = 1) const
    {
        TLLM_CHECK(mNbAttentionLayers % pipelineParallelism == 0);
        return mNbAttentionLayers / pipelineParallelism;
    }

    [[nodiscard]] SizeType constexpr getNbSsmLayers(SizeType pipelineParallelism = 1) const
    {
        TLLM_CHECK(mNbSsmLayers % pipelineParallelism == 0);
        return mNbSsmLayers / pipelineParallelism;
    }

    [[nodiscard]] SizeType constexpr getNbHeads() const noexcept
    {
        return mNbHeads;
    }

    [[nodiscard]] SizeType constexpr getNbKvHeads() const noexcept
    {
        return mNbKvHeads;
    }

    void constexpr setNbKvHeads(SizeType nbKvHeads) noexcept
    {
        mNbKvHeads = nbKvHeads;
    }

    [[nodiscard]] SizeType constexpr getHiddenSize() const noexcept
    {
        return mHiddenSize;
    }

    [[nodiscard]] SizeType constexpr getSizePerHead() const noexcept
    {
        return mSizePerHead;
    }

    void constexpr setSizePerHead(SizeType sizePerHead) noexcept
    {
        mSizePerHead = sizePerHead;
    }

    [[nodiscard]] nvinfer1::DataType constexpr getDataType() const noexcept
    {
        return mDataType;
    }

    [[nodiscard]] bool constexpr useGptAttentionPlugin() const noexcept
    {
        return mUseGptAttentionPlugin;
    }

    void constexpr useGptAttentionPlugin(bool useGptAttentionPlugin) noexcept
    {
        mUseGptAttentionPlugin = useGptAttentionPlugin;
    }

    [[nodiscard]] bool constexpr useMambaConv1dPlugin() const noexcept
    {
        return mUseMambaConv1dPlugin;
    }

    void constexpr useMambaConv1dPlugin(bool useMambaConv1dPlugin) noexcept
    {
        mUseMambaConv1dPlugin = useMambaConv1dPlugin;
    }

    [[nodiscard]] bool constexpr usePackedInput() const noexcept
    {
        return mInputPacked;
    }

    void constexpr usePackedInput(bool inputPacked) noexcept
    {
        mInputPacked = inputPacked;
    }

    [[nodiscard]] bool constexpr usePagedKvCache() const noexcept
    {
        return mPagedKvCache;
    }

    void constexpr usePagedKvCache(bool pagedKvCache) noexcept
    {
        mPagedKvCache = pagedKvCache;
    }

    [[nodiscard]] bool constexpr usePagedState() const noexcept
    {
        return mPagedState;
    }

    void constexpr usePagedState(bool pagedState) noexcept
    {
        mPagedState = pagedState;
    }

    [[nodiscard]] SizeType constexpr getTokensPerBlock() const noexcept
    {
        return mTokensPerBlock;
    }

    void constexpr setTokensPerBlock(SizeType TokensPerBlock) noexcept
    {
        mTokensPerBlock = TokensPerBlock;
    }

    [[nodiscard]] common::QuantMode constexpr getQuantMode() const noexcept
    {
        return mQuantMode;
    }

    void constexpr setQuantMode(common::QuantMode QuantMode) noexcept
    {
        mQuantMode = QuantMode;
    }

    [[nodiscard]] bool constexpr supportsInflightBatching() const noexcept
    {
        return (isTransformerBased() && mUseGptAttentionPlugin && mInputPacked && mPagedKvCache)
            || (isSsmBased() && mUseMambaConv1dPlugin && mInputPacked && mPagedState);
    }

    [[nodiscard]] SizeType constexpr getMaxBatchSize() const noexcept
    {
        return mMaxBatchSize;
    }

    void constexpr setMaxBatchSize(SizeType maxBatchSize) noexcept
    {
        mMaxBatchSize = maxBatchSize;
    }

    [[nodiscard]] SizeType constexpr getMaxBeamWidth() const noexcept
    {
        return mMaxBeamWidth;
    }

    void constexpr setMaxBeamWidth(SizeType maxBeamWidth) noexcept
    {
        mMaxBeamWidth = maxBeamWidth;
    }

    [[nodiscard]] SizeType constexpr getMaxInputLen() const noexcept
    {
        return mMaxInputLen;
    }

    void constexpr setMaxInputLen(SizeType maxInputLen) noexcept
    {
        mMaxInputLen = maxInputLen;
    }

    [[nodiscard]] SizeType constexpr getMaxSequenceLen() const noexcept
    {
        return mMaxSequenceLen;
    }

    void constexpr setMaxSequenceLen(SizeType maxSequenceLen) noexcept
    {
        mMaxSequenceLen = maxSequenceLen;
    }

    [[nodiscard]] std::optional<SizeType> constexpr getMaxNumTokens() const noexcept
    {
        return mMaxNumTokens;
    }

    void constexpr setMaxNumTokens(std::optional<SizeType> maxNumTokens) noexcept
    {
        mMaxNumTokens = maxNumTokens;
    }

    [[nodiscard]] bool constexpr usePromptTuning() const noexcept
    {
        return mMaxPromptEmbeddingTableSize > 0;
    }

    [[nodiscard]] SizeType constexpr getMaxPromptEmbeddingTableSize() const noexcept
    {
        return mMaxPromptEmbeddingTableSize;
    }

    void constexpr setMaxPromptEmbeddingTableSize(SizeType maxPromptEmbeddingTableSize) noexcept
    {
        mMaxPromptEmbeddingTableSize = maxPromptEmbeddingTableSize;
    }

    [[nodiscard]] bool constexpr computeContextLogits() const noexcept
    {
        return mComputeContextLogits;
    }

    void constexpr computeContextLogits(bool computeContextLogits) noexcept
    {
        mComputeContextLogits = computeContextLogits;
    }

    [[nodiscard]] bool constexpr computeGenerationLogits() const noexcept
    {
        return mComputeGenerationLogits;
    }

    void constexpr computeGenerationLogits(bool computeGenerationLogits) noexcept
    {
        mComputeGenerationLogits = computeGenerationLogits;
    }

    [[nodiscard]] ModelVariant getModelVariant() const
    {
        return mModelVariant;
    }

    void setModelVariant(ModelVariant modelVariant)
    {
        mModelVariant = modelVariant;
    }

    [[nodiscard]] bool constexpr useCustomAllReduce() const noexcept
    {
        return mUseCustomAllReduce;
    }

    void constexpr useCustomAllReduce(bool customAllReduce) noexcept
    {
        mUseCustomAllReduce = customAllReduce;
    }

    void constexpr setMaxDraftLen(SizeType maxDraftLen) noexcept
    {
        mMaxDraftLen = maxDraftLen;
    }

    [[nodiscard]] SizeType getMaxDraftLen() const
    {
        return mMaxDraftLen;
    }

    [[nodiscard]] SizeType constexpr getMaxTokensPerStep() const noexcept
    {
        return mMaxDraftLen + 1;
    }

    void constexpr setUseContextFMHAForGeneration(bool useContextFMHAForGeneration) noexcept
    {
        mUseContextFMHAForGeneration = useContextFMHAForGeneration;
    }

    [[nodiscard]] bool constexpr getContextFMHAForGeneration() const noexcept
    {
        return mUseContextFMHAForGeneration;
    }

    void constexpr setPagedContextFMHA(bool pagedContextFMHA) noexcept
    {
        mPagedContextFMHA = pagedContextFMHA;
    }

    [[nodiscard]] bool constexpr getPagedContextFMHA() const noexcept
    {
        return mPagedContextFMHA;
    }

    void constexpr useXQA(bool useXQA) noexcept
    {
        mUseXQA = useXQA;
    }

    [[nodiscard]] bool constexpr useXQA() const noexcept
    {
        return mUseXQA;
    }

    [[nodiscard]] bool constexpr useLoraPlugin() const noexcept
    {
        return mUseLoraPlugin;
    }

    void constexpr useLoraPlugin(bool useLoraPlugin) noexcept
    {
        mUseLoraPlugin = useLoraPlugin;
    }

    [[nodiscard]] std::vector<LoraModule> const& getLoraModules() const noexcept
    {
        return mLoraModules;
    }

    void setLoraModules(std::vector<LoraModule> const& loraModules) noexcept
    {
        mLoraModules = loraModules;
    }

    [[nodiscard]] SizeType constexpr getMlpHiddenSize() const noexcept
    {
        return mMlpHiddenSize;
    }

    void constexpr setMlpHiddenSize(SizeType mlpHiddenSize) noexcept
    {
        mMlpHiddenSize = mlpHiddenSize;
    }

    [[nodiscard]] bool constexpr useCrossAttention() const noexcept
    {
        return mUseCrossAttention;
    }

    void constexpr useCrossAttention(bool newCrossAttention) noexcept
    {
        mUseCrossAttention = newCrossAttention;
    }

    [[nodiscard]] bool constexpr usePositionEmbedding() const noexcept
    {
        return mUsePositionEmbedding;
    }

    void constexpr usePositionEmbedding(bool newPositionEmbedding) noexcept
    {
        mUsePositionEmbedding = newPositionEmbedding;
    }

    [[nodiscard]] bool constexpr useTokenTypeEmbedding() const noexcept
    {
        return mUseTokenTypeEmbedding;
    }

    void constexpr useTokenTypeEmbedding(bool newTokenTypeEmbedding) noexcept
    {
        mUseTokenTypeEmbedding = newTokenTypeEmbedding;
    }

    [[nodiscard]] SizeType constexpr getFfnHiddenSize() const noexcept
    {
        return mFfnHiddenSize;
    }

    void constexpr setFfnHiddenSize(SizeType ffnHiddenSize) noexcept
    {
        mFfnHiddenSize = ffnHiddenSize;
    }

    [[nodiscard]] SizeType constexpr getMaxLoraRank() const noexcept
    {
        return mMaxLoraRank;
    }

    void constexpr setMaxLoraRank(SizeType maxLoraRank) noexcept
    {
        mMaxLoraRank = maxLoraRank;
    }

    [[nodiscard]] bool constexpr useMedusa() const noexcept
    {
        return mMedusaModule.has_value();
    }

    [[nodiscard]] std::optional<MedusaModule> getMedusaModule() const noexcept
    {
        return mMedusaModule;
    }

    void setMedusaModule(MedusaModule const& medusaModule) noexcept
    {
        mMedusaModule = medusaModule;
    }

    [[nodiscard]] nvinfer1::DataType getKvDataType() const noexcept
    {
        if (getQuantMode().hasFp8KvCache())
        {
            return nvinfer1::DataType::kFP8;
        }
        else if (getQuantMode().hasInt8KvCache())
        {
            return nvinfer1::DataType::kINT8;
        }
        else
        {
            return getDataType();
        }
    }

    [[nodiscard]] bool constexpr isTransformerBased() const noexcept
    {
        return mModelVariant == ModelVariant::kGpt || mModelVariant == ModelVariant::kGlm
            || mModelVariant == ModelVariant::kRecurrentGemma;
    }

    [[nodiscard]] bool hasMambaConfig() const noexcept
    {
        return mMambaConfig.has_value();
    }

    [[nodiscard]] std::optional<MambaConfig> getMambaConfig() const noexcept
    {
        return mMambaConfig;
    }

    void setMambaConfig(MambaConfig const& mambaConfig) noexcept
    {
        mMambaConfig = mambaConfig;
    }

    [[nodiscard]] bool constexpr isSsmBased() const noexcept
    {
        return mModelVariant == ModelVariant::kMamba || mModelVariant == ModelVariant::kRecurrentGemma;
    }

    [[nodiscard]] bool hasRnnConfig() const noexcept
    {
        return mRnnConfig.has_value();
    }

    [[nodiscard]] std::optional<RnnConfig> getRnnConfig() const noexcept
    {
        return mRnnConfig;
    }

    void setRnnConfig(RnnConfig const& rnnConfig) noexcept
    {
        mRnnConfig = rnnConfig;
    }

    [[nodiscard]] std::vector<LayerType> const& getLayerTypes() const noexcept
    {
        return mLayerTypes;
    }

    void setLayerTypes(std::vector<LayerType> const& layerTypes) noexcept
    {
        mLayerTypes = layerTypes;
    }

private:
    SizeType mVocabSize;
    SizeType mNbAttentionLayers;
    SizeType mNbSsmLayers;
    SizeType mNbHeads;
    SizeType mNbKvHeads;
    SizeType mHiddenSize;
    SizeType mSizePerHead;
    nvinfer1::DataType mDataType;
    bool mUseGptAttentionPlugin;
    bool mUseMambaConv1dPlugin;
    bool mInputPacked;
    bool mPagedKvCache;
    bool mPagedState;
    SizeType mTokensPerBlock;
    common::QuantMode mQuantMode;
    SizeType mMaxBatchSize;
    SizeType mMaxBeamWidth;
    SizeType mMaxInputLen;
    SizeType mMaxSequenceLen;
    std::optional<SizeType> mMaxNumTokens;

    bool mComputeContextLogits;
    bool mComputeGenerationLogits;
    ModelVariant mModelVariant;
    bool mUseCustomAllReduce;

    SizeType mMaxPromptEmbeddingTableSize;
    SizeType mMaxDraftLen;

    bool mUseContextFMHAForGeneration;
    bool mPagedContextFMHA;
    bool mUseXQA;

    bool mUseLoraPlugin;
    std::vector<LoraModule> mLoraModules;
    SizeType mMlpHiddenSize;
    SizeType mMaxLoraRank;

    std::optional<MedusaModule> mMedusaModule;
    std::optional<MambaConfig> mMambaConfig;

    // Configs related to encoder / enc-dec models
    bool mUseCrossAttention;
    bool mUsePositionEmbedding;
    bool mUseTokenTypeEmbedding;
    SizeType mFfnHiddenSize; // indicates encoder output hidden size

    std::optional<RnnConfig> mRnnConfig;

    std::vector<LayerType> mLayerTypes;
};

} // namespace tensorrt_llm::runtime
