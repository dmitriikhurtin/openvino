// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "openvino/core/shape.hpp"

#include <algorithm>
#include <iostream>
#include <vector>

#include "ngraph/check.hpp"

ov::Shape::Shape() : Shape(std::initializer_list<Dimension>{}) {}

ov::Shape::Shape(std::initializer_list<Dimension> init) : Shape(true, init) {}

ov::Shape::Shape(const std::vector<Dimension::value_type>& dimensions)
    : m_rank_is_static(true),
      m_dimensions(dimensions.begin(), dimensions.end()) {}

ov::Shape::Shape(const StaticShape& shape)
    : m_rank_is_static(true),
      m_shape_type(ShapeType::SHAPE_IS_STATIC),
      m_dimensions(shape.begin(), shape.end()) {}

ov::Shape::Shape(bool rank_is_static, std::vector<Dimension> dimensions)
    : m_rank_is_static(rank_is_static),
      m_dimensions(std::move(dimensions)) {}

ov::Shape::Shape(std::vector<Dimension> dimensions) : m_rank_is_static(true), m_dimensions(std::move(dimensions)) {}

bool ov::Shape::is_static() const {
    ShapeType shape_type = m_shape_type;

    if (m_shape_type == ShapeType::SHAPE_IS_UNKNOWN || m_shape_type == ShapeType::SHAPE_IS_UPDATED) {
        shape_type = m_rank_is_static && std::all_of(m_dimensions.begin(),
                                                     m_dimensions.end(),
                                                     [](const Dimension& d) {
                                                         return d.is_static();
                                                     })
                         ? ShapeType::SHAPE_IS_STATIC
                         : ShapeType::SHAPE_IS_DYNAMIC;

        if (m_shape_type == ShapeType::SHAPE_IS_UNKNOWN)
            m_shape_type = shape_type;
    }

    return shape_type == ShapeType::SHAPE_IS_STATIC;
}

bool ov::Shape::operator==(const Shape& partial_shape) const {
    if (rank() != partial_shape.rank()) {
        return false;
    }
    if (rank().is_dynamic()) {
        return true;
    }
    for (auto i = 0; i < rank().get_length(); ++i) {
        if (m_dimensions[i] != partial_shape.m_dimensions[i]) {
            return false;
        }
    }
    return true;
}

bool ov::Shape::operator!=(const Shape& partial_shape) const {
    return !(*this == partial_shape);
}

ov::StaticShape ov::Shape::get_max_shape() const {
    if (rank().is_dynamic()) {
        return StaticShape();
    } else {
        StaticShape shape;
        for (auto dimension : m_dimensions) {
            shape.push_back(dimension.get_interval().get_max_val());
        }
        return shape;
    }
}

ov::StaticShape ov::Shape::get_min_shape() const {
    if (rank().is_dynamic()) {
        return StaticShape();
    } else {
        StaticShape shape;
        for (auto dimension : m_dimensions) {
            shape.push_back(dimension.get_interval().get_min_val());
        }
        return shape;
    }
}

ov::StaticShape ov::Shape::get_shape() const {
    NGRAPH_CHECK(rank().is_static(), "get_shape() must be called on a static shape");
    StaticShape shape;
    for (auto dimension : m_dimensions) {
        auto min_val = dimension.get_interval().get_min_val();
        auto max_val = dimension.get_interval().get_max_val();
        NGRAPH_CHECK(min_val == max_val, "get_shape() must be called on a static shape");
        shape.push_back(min_val);
    }
    return shape;
}

ov::Shape ov::operator+(const Shape& s1, const Shape& s2) {
    if (s1.rank().is_dynamic() || s2.rank().is_dynamic()) {
        return Shape::dynamic();
    }

    if (!s1.rank().compatible(s2.rank())) {
        throw std::invalid_argument("rank mismatch");
    }

    Shape result{};
    result.m_rank_is_static = true;
    for (size_t i = 0; i < s1.m_dimensions.size(); i++) {
        result.m_dimensions.push_back(s1.m_dimensions[i] + s2.m_dimensions[i]);
    }
    return result;
}

std::ostream& ov::operator<<(std::ostream& str, const Shape& shape) {
    if (shape.m_rank_is_static) {
        str << "{";
        bool first = true;
        for (auto& d : shape.m_dimensions) {
            if (!first) {
                str << ",";
            }
            str << d;
            first = false;
        }
        return (str << "}");
    } else {
        return (str << "?");
    }
}

ov::Shape ov::Shape::dynamic(Rank r) {
    return Shape(r.is_static(), std::vector<Dimension>(r.is_static() ? r.get_length() : 0, Dimension::dynamic()));
}

bool ov::Shape::compatible(const Shape& s) const {
    // If we don't know *this's rank, or we don't know s's rank, they are compatible.
    if (!m_rank_is_static || s.rank().is_dynamic()) {
        return true;
    }
    // If we do know *this's rank and s's rank, and they are unequal, they are incompatible.
    else if (rank().get_length() != s.rank().get_length()) {
        return false;
    }
    // If we know both the ranks and they are equal, then *this and s are compatible iff they
    // are elementwise compatible everywhere.
    else {
        for (int64_t i = 0; i < rank().get_length(); i++) {
            if (!m_dimensions[i].compatible(s.m_dimensions[i])) {
                return false;
            }
        }
        // If we are still here, we know that s1 and s2 have the same rank and are elementwise
        // compatible everywhere.
        return true;
    }
}

bool ov::Shape::same_scheme(const Shape& s) const {
    if (rank().is_dynamic() && s.rank().is_dynamic()) {
        return true;
    } else if (rank().is_static() && s.rank().is_static()) {
        if (rank().get_length() != s.rank().get_length()) {
            return false;
        }

        bool success = true;

        for (int64_t i = 0; i < rank().get_length(); i++) {
            success &= (*this)[i].same_scheme(s[i]);
        }

        return success;
    } else {
        return false;
    }
}

bool ov::Shape::relaxes(const Shape& s) const {
    if (rank().is_dynamic()) {
        return true;
    } else if (s.rank().is_static() && rank().get_length() == s.rank().get_length()) {
        bool all_relax = true;

        for (int64_t i = 0; i < rank().get_length(); i++) {
            all_relax &= ((*this)[i].relaxes(s[i]));
        }

        return all_relax;
    } else {
        return false;
    }
}

bool ov::Shape::refines(const Shape& s) const {
    if (s.rank().is_dynamic()) {
        return true;
    } else if (rank().is_static() && rank().get_length() == s.rank().get_length()) {
        bool all_refine = true;

        for (int64_t i = 0; i < rank().get_length(); i++) {
            all_refine &= ((*this)[i].refines(s[i]));
        }

        return all_refine;
    } else {
        return false;
    }
}

bool ov::Shape::merge_rank(Rank r) {
    if (r.is_dynamic()) {
        return true;
    } else if (!m_rank_is_static) {
        m_rank_is_static = true;
        m_dimensions = std::vector<Dimension>(r.get_length(), Dimension::dynamic());
        m_shape_type = ShapeType::SHAPE_IS_UNKNOWN;
        return true;
    } else {
        return (static_cast<int64_t>(m_dimensions.size()) == r.get_length());
    }
}

ov::StaticShape ov::Shape::to_shape() const {
    if (is_dynamic()) {
        throw std::invalid_argument("to_shape was called on a dynamic shape.");
    }

    std::vector<size_t> shape_dimensions(m_dimensions.size());
    std::transform(m_dimensions.begin(), m_dimensions.end(), shape_dimensions.begin(), [](const Dimension& d) {
        return d.get_length();
    });

    return shape_dimensions;
}

bool ov::Shape::merge_into(Shape& dst, const Shape& src) {
    if (dst.rank().is_dynamic()) {
        dst = src;
        return true;
    } else if (src.rank().is_dynamic()) {
        // No change to dst.
        return true;
    } else if (dst.rank().get_length() != src.rank().get_length()) {
        // Mismatching static ranks, cannot merge.
        return false;
    } else {
        // Ranks are both static, and they match.
        bool success = true;
        for (int64_t i = 0; i < dst.rank().get_length(); i++) {
            success &= Dimension::merge(dst[i], dst[i], src[i]);
        }
        return success;
    }
}

bool ov::Shape::broadcast_merge_into(Shape& dst, const Shape& src, const ngraph::op::AutoBroadcastSpec& autob) {
    switch (autob.m_type) {
    case ngraph::op::AutoBroadcastType::NONE:
        return true;
    case ngraph::op::AutoBroadcastType::NUMPY: {
        if (dst.rank().is_dynamic() || src.rank().is_dynamic()) {
            dst = Shape::dynamic();
            return true;
        } else {
            // Ranks are both static.
            auto dst_rank = dst.rank().get_length();
            auto src_rank = src.rank().get_length();
            auto new_rank = std::max(dst_rank, src_rank);
            std::vector<Dimension> dims(new_rank);
            bool success = true;
            for (int64_t i = 0; i < new_rank; i++) {
                auto dsti = i < (new_rank - dst_rank) ? Dimension(1) : dst[i - (new_rank - dst_rank)];
                auto srci = i < (new_rank - src_rank) ? Dimension(1) : src[i - (new_rank - src_rank)];
                success &= Dimension::broadcast_merge(dims[i], dsti, srci);
            }
            dst = Shape(std::move(dims));
            return success;
        }
    }
    case ngraph::op::AutoBroadcastType::PDPD: {
        if (dst.rank().is_dynamic() || src.rank().is_dynamic()) {
            return true;
        } else {
            // Ranks are both static.
            auto dst_rank = dst.rank().get_length();
            auto src_rank = src.rank().get_length();
            if (dst_rank == src_rank && dst.compatible(src))
                return true;

            int64_t axis = autob.m_axis;
            if (axis < -1) {
                return false;
            }
            if (axis == -1) {
                axis = dst_rank - src_rank;
            }

            size_t len = src_rank;
            while (len > 0 && src[len - 1].is_static() && src[len - 1].get_length() == 1) {
                --len;
            }

            for (size_t i = axis; i < axis + len; ++i) {
                if (!(dst[i].compatible(src[i - axis]))) {
                    return false;
                }
            }

            return true;
        }
    }
    default:
        NGRAPH_CHECK(false, "Unsupported auto broadcast type: ", autob.m_type);
    }

    return false;
}

bool ov::Shape::all_non_negative() const {
    for (auto& d : m_dimensions) {
        if (d.is_static() && d.get_length() < 0) {
            return false;
        }
    }

    return true;
}

const ov::Dimension& ov::Shape::operator[](size_t i) const {
    if (i >= m_dimensions.size()) {
        throw std::out_of_range("Accessing out-of-range dimension in Dimension[]");
    }
    return m_dimensions[i];
}

ov::Dimension& ov::Shape::operator[](size_t i) {
    if (i >= m_dimensions.size()) {
        throw std::out_of_range("Accessing out-of-range dimension in Dimension[]");
    }
    m_shape_type = ShapeType::SHAPE_IS_UPDATED;  // We can't guarantee that the shape remains static or dynamic.
    return m_dimensions[i];
}

const std::vector<int64_t>& ov::AttributeAdapter<ov::Shape>::get() {
    if (!m_buffer_valid) {
        m_buffer.clear();
        if (m_ref.rank().is_dynamic()) {
            m_buffer.push_back(-2);
        } else {
            for (int64_t i = 0; i < m_ref.rank().get_length(); ++i) {
                const auto& elt = static_cast<const ov::Shape&>(m_ref)[i];
                m_buffer.push_back(elt.is_dynamic() ? -1 : elt.get_length());
            }
        }
        m_buffer_valid = true;
    }
    return m_buffer;
}

void ov::AttributeAdapter<ov::Shape>::set(const std::vector<int64_t>& value) {
    m_ref = ov::Shape();
    if (value.size() == 1 && value[0] == -2) {
        m_ref = ov::Shape::dynamic();
    } else {
        std::vector<Dimension> dims;
        dims.reserve(value.size());
        for (auto elt : value) {
            dims.push_back(elt == -1 ? Dimension::dynamic() : elt);
        }
        m_ref = ov::Shape(dims);
    }
    m_buffer_valid = false;
}

OPENVINO_API constexpr ov::DiscreteTypeInfo ov::AttributeAdapter<ov::Shape>::type_info;
