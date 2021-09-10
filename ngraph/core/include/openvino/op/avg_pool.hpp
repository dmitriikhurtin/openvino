// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "openvino/op/op.hpp"
#include "openvino/op/util/attr_types.hpp"

namespace ov {
namespace op {
namespace v1 {
/// \brief Batched average pooling operation.
///
class OPENVINO_API AvgPool : public Op {
public:
    OPENVINO_RTTI_DECLARATION;

    /// \brief Constructs a batched average pooling operation.
    AvgPool() = default;

    ///
    /// \brief      Constructs a batched average pooling operation.
    ///
    /// \param      arg            The output producing the input data batch tensor.<br>
    ///                            `[d1, dn]`
    /// \param      strides        The strides.<br> `[n]`
    /// \param      pads_begin     The beginning of padding shape.<br> `[n]`
    /// \param      pads_end       The end of padding shape.<br> `[n]`
    /// \param      kernel         The kernel shape.<br> `[n]`
    /// \param      exclude_pad    If false then averages include padding elements, each
    ///                            treated as the number zero.  If true, padding
    ///                            elements
    ///                            are entirely ignored when computing averages.
    /// \param      rounding_type  Whether to use ceiling or floor rounding type while
    ///                            computing output shape.
    /// \param      auto_pad       Padding type to use for additional padded dimensions
    ///
    AvgPool(const Output<Node>& arg,
            const Strides& strides,
            const StaticShape& pads_begin,
            const StaticShape& pads_end,
            const StaticShape& kernel,
            bool exclude_pad,
            op::RoundingType rounding_type = op::RoundingType::FLOOR,
            const PadType& auto_pad = op::PadType::EXPLICIT);

    void validate_and_infer_types() override;
    bool visit_attributes(AttributeVisitor& visitor) override;

    std::shared_ptr<Node> clone_with_new_inputs(const OutputVector& new_args) const override;

    /// \return The kernel shape.
    const StaticShape& get_kernel() const;
    void set_kernel(const StaticShape& kernel);
    /// \return The strides.
    const Strides& get_strides() const;
    void set_strides(const Strides& strides);
    /// \return The beginning of padding shape.
    const StaticShape& get_pads_begin() const;
    void set_pads_begin(const StaticShape& pads_begin);
    /// \return The end of padding shape.
    const StaticShape& get_pads_end() const;
    void set_pads_end(const StaticShape& pads_end);
    bool get_exclude_pad() const;
    void set_exclude_pad(bool exclude_pad);
    /// \return The pad type for pooling.
    const PadType& get_auto_pad() const;
    void set_auto_pad(const PadType& auto_pad);
    op::RoundingType get_rounding_type() const;
    void set_rounding_type(op::RoundingType rounding_type);
    /// \return The default value for AvgPool.
    OPENVINO_SUPPRESS_DEPRECATED_START
    std::shared_ptr<Node> get_default_value() const override;
    OPENVINO_SUPPRESS_DEPRECATED_END

protected:
    StaticShape m_kernel;
    Strides m_strides;
    StaticShape m_pads_begin;
    StaticShape m_pads_end;
    bool m_exclude_pad{true};
    PadType m_auto_pad{PadType::EXPLICIT};
    op::RoundingType m_rounding_type{op::RoundingType::FLOOR};
};
}  // namespace v1
}  // namespace op
}  // namespace ov
