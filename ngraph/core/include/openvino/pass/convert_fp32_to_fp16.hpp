// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "openvino/pass/graph_rewrite.hpp"

namespace ov {
namespace pass {
class OPENVINO_API ConvertFP32ToFP16 : public FunctionPass {
public:
    OPENVINO_RTTI_DECLARATION;
    bool run_on_function(std::shared_ptr<ngraph::Function>) override;
};
}  // namespace pass
}  // namespace ov
