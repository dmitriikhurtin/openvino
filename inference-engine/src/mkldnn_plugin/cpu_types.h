// Copyright (C) 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <vector>

namespace MKLDNNPlugin {

using Dim = std::size_t;
using VectorDims = std::vector<Dim>;

enum Type {
    Unknown,
    Generic,
    Reorder,
    Input,
    Output,
    Convolution,
    Deconvolution,
    Lrn,
    Pooling,
    AdaptivePooling,
    FullyConnected,
    Softmax,
    Split,
    Concatenation,
    Eltwise,
    MatMul,
    Reshape,
    Tile,
    ROIAlign,
    ROIPooling,
    PSROIPooling,
    BatchToSpace,
    DepthToSpace,
    Pad,
    Transpose,
    SpaceToBatch,
    SpaceToDepth,
    StridedSlice,
    MemoryOutput,
    MemoryInput,
    RNNCell,
    RNNSeq,
    FakeQuantize,
    BinaryConvolution,
    DeformableConvolution,
    TensorIterator,
    Convert,
    MVN,
    NormalizeL2,
    ScatterUpdate,
    ScatterElementsUpdate,
    ScatterNDUpdate,
    Interpolate,
    Reduce,
    Broadcast,
    EmbeddingSegmentsSum,
    EmbeddingBagPackedSum,
    EmbeddingBagOffsetsSum,
    Gather,
    GatherElements,
    GatherND,
    OneHot,
    RegionYolo,
    Select,
    Roll,
    Reference,
    ShuffleChannels,
    DFT,
    Math,
    CTCLoss,
    Bucketize,
    CTCGreedyDecoder,
    CTCGreedyDecoderSeqLen,
    CumSum,
    DetectionOutput,
    ExperimentalDetectronDetectionOutput,
    LogSoftmax,
    TopK,
    GatherTree,
    GRN,
    Range,
    Proposal,
    ReorgYolo,
    ReverseSequence,
    ExperimentalDetectronTopKROIs,
    ExperimentalDetectronROIFeatureExtractor,
    ExperimentalDetectronPriorGridGenerator,
    ExperimentalDetectronGenerateProposalsSingleImage,
    ExtractImagePatches,
    NonMaxSuppression,
    MatrixNms,
    MulticlassNms
};

enum Algorithm {
    Undefined,

    // Pooling algorithms
    PoolingMax,
    PoolingAvg,

    // Adaptive pooling algorithms
    AdaptivePoolingMax,
    AdaptivePoolingAvg,

    // Convolution algorithms
    ConvolutionCommon,
    ConvolutionGrouped,

    // Convolution algorithms
    DeconvolutionCommon,
    DeconvolutionGrouped,

    // Elementwise algorithms
    EltwiseAdd,
    EltwiseMultiply,
    EltwiseSubtract,
    EltwiseDivide,
    EltwiseFloorMod,
    EltwiseMod,
    EltwiseMaximum,
    EltwiseMinimum,
    EltwiseSquaredDifference,
    EltwisePowerDynamic,
    EltwisePowerStatic,
    EltwiseMulAdd,
    EltwiseEqual,
    EltwiseNotEqual,
    EltwiseGreater,
    EltwiseGreaterEqual,
    EltwiseLess,
    EltwiseLessEqual,
    EltwiseLogicalAnd,
    EltwiseLogicalOr,
    EltwiseLogicalXor,
    EltwiseLogicalNot,
    EltwiseRelu,
    EltwiseGelu,
    EltwiseElu,
    EltwiseTanh,
    EltwiseSigmoid,
    EltwiseAbs,
    EltwiseSqrt,
    EltwiseSoftRelu,
    EltwiseExp,
    EltwiseClamp,
    EltwiseSwish,
    EltwisePrelu,
    EltwiseMish,
    EltwiseHswish,
    EltwiseHsigmoid,
    EltwiseRoundHalfToEven,
    EltwiseRoundHalfAwayFromZero,
    EltwiseErf,

    // FakeQuantize algorithms
    FQCommon,
    FQQuantization,
    FQBinarization,

    // ROIPooling algorithms
    ROIPoolingMax,
    ROIPoolingBilinear,

    // ROIAlign algorithms
    ROIAlignMax,
    ROIAlignAvg,

    // PSROIPooling algorithms
    PSROIPoolingAverage,
    PSROIPoolingBilinear,
    PSROIPoolingBilinearDeformable,

    // Reduce algorithms
    ReduceL1,
    ReduceL2,
    ReduceAnd,
    ReduceOr,
    ReduceMax,
    ReduceMean,
    ReduceMin,
    ReduceProd,
    ReduceSum,
    ReduceLogSum,
    ReduceLogSumExp,
    ReduceSumSquare,

    // Math algorithms
    MathAbs,
    MathAcos,
    MathAcosh,
    MathAsin,
    MathAsinh,
    MathAtan,
    MathAtanh,
    MathCeiling,
    MathCos,
    MathCosh,
    MathErf,
    MathFloor,
    MathHardSigmoid,
    MathLog,
    MathNegative,
    MathReciprocal,
    MathSelu,
    MathSign,
    MathSin,
    MathSinh,
    MathSoftPlus,
    MathSoftsign,
    MathTan
};

} // namespace MKLDNNPlugin
