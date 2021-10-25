// Copyright (C) 2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <gtest/gtest.h>

#include "transformations/common_optimizations/transpose_to_pwl.hpp"

#include "common_test_utils/data_utils.hpp"
#include "common_test_utils/ngraph_test_utils.hpp"
#include <ngraph/function.hpp>
#include <ngraph/opsets/opset1.hpp>
#include <ngraph/pass/manager.hpp>
#include <transformations/init_node_info.hpp>

namespace pwl_test {
std::shared_ptr<ngraph::Function> CreateSigmoid(const ngraph::Shape& input_shape) {
    auto input_params = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, input_shape);
    auto sigmoid = std::make_shared<ngraph::opset1::Sigmoid>(input_params);
    auto result = std::make_shared<ngraph::opset1::Result>(sigmoid);
    return std::make_shared<ngraph::Function>(ngraph::ResultVector{result}, ngraph::ParameterVector{input_params});
}

std::shared_ptr<ngraph::Function> CreateTanh(const ngraph::Shape& input_shape) {
    auto input_params = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, input_shape);
    auto tanh = std::make_shared<ngraph::opset1::Tanh>(input_params);
    auto result = std::make_shared<ngraph::opset1::Result>(tanh);
    return std::make_shared<ngraph::Function>(ngraph::ResultVector{result}, ngraph::ParameterVector{input_params});
}

std::shared_ptr<ngraph::Function> CreateExp(const ngraph::Shape& input_shape) {
    auto input_params = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, input_shape);
    auto exp = std::make_shared<ngraph::opset1::Exp>(input_params);
    auto result = std::make_shared<ngraph::opset1::Result>(exp);
    return std::make_shared<ngraph::Function>(ngraph::ResultVector{result}, ngraph::ParameterVector{input_params});
}

std::shared_ptr<ngraph::Function> CreateAbs(const ngraph::Shape& input_shape) {
    auto input_params = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, input_shape);
    auto abs = std::make_shared<ngraph::opset1::Abs>(input_params);
    auto result = std::make_shared<ngraph::opset1::Result>(abs);
    return std::make_shared<ngraph::Function>(ngraph::ResultVector{result}, ngraph::ParameterVector{input_params});
}

std::shared_ptr<ngraph::Function> CreateSign(const ngraph::Shape& input_shape) {
    auto input_params = std::make_shared<ngraph::opset1::Parameter>(ngraph::element::f32, input_shape);
    auto sign = std::make_shared<ngraph::opset1::Sign>(input_params);
    auto result = std::make_shared<ngraph::opset1::Result>(sign);
    return std::make_shared<ngraph::Function>(ngraph::ResultVector{result}, ngraph::ParameterVector{input_params});
}
} // namespace pwl_test

namespace {
void RunTest(const std::shared_ptr<ngraph::Function>& func,
    const std::shared_ptr<ngraph::Function>& reference_func,
    float lower_bound,
    float upper_bound) {
    {
        ngraph::pass::Manager m;
        m.register_pass<ngraph::pass::InitNodeInfo>();
        m.register_pass<ngraph::pass::TransposeToPwl>();
        m.run_passes(func);
        ASSERT_NO_THROW(check_rt_info(func));
    }

    auto shape = func->input().get_node_shared_ptr()->get_output_shape(0);

    ov::HostTensorVector output_tensors;
    ov::HostTensorVector input_tensors;
    output_tensors.emplace_back(std::make_shared<ngraph::runtime::HostTensor>(ngraph::element::f32, shape));
    std::vector<float> data = CommonTestUtils::generate_float_numbers(ov::shape_size(shape), lower_bound, upper_bound);
    input_tensors.emplace_back(std::make_shared<ngraph::runtime::HostTensor>(ngraph::element::f32, shape, data.data()));

    OPENVINO_SUPPRESS_DEPRECATED_START
    ASSERT_NO_THROW(func->evaluate(output_tensors, input_tensors));
    OPENVINO_SUPPRESS_DEPRECATED_END

    ov::HostTensorVector output_tensors_ref;
    output_tensors_ref.emplace_back(std::make_shared<ngraph::runtime::HostTensor>(ngraph::element::f32, shape));

    OPENVINO_SUPPRESS_DEPRECATED_START
    ASSERT_NO_THROW(reference_func->evaluate(output_tensors_ref, input_tensors));
    OPENVINO_SUPPRESS_DEPRECATED_END

    for (size_t i = 0; i < ov::shape_size(shape); i++) {
        double delta = std::abs(
            output_tensors[0]->get_data_ptr<float>()[i] - output_tensors_ref[0]->get_data_ptr<float>()[i]);
        std::cout << "delta: " << delta << " " << output_tensors[0]->get_data_ptr<float>()[i] << " "
            << output_tensors_ref[0]->get_data_ptr<float>()[i] << '\n';
        ASSERT_TRUE(delta <= 0.005);
    }
}
} // namespace

TEST(PwlTest, Sigmoid) {
    RunTest(
        pwl_test::CreateSigmoid({1, 32}),
        pwl_test::CreateSigmoid({1, 32}),
        -10,
        10);
}

TEST(PwlTest, Tanh) {
    RunTest(
        pwl_test::CreateTanh({1, 32}),
        pwl_test::CreateTanh({1, 32}),
        -5,
        5);
}

/*TEST(PwlTest, Exp) {
    RunTest(
        pwl_test::CreateExp({1, 32}),
        pwl_test::CreateExp({1, 32}),
        0,
        log(INT16_MAX));
}*/

TEST(PwlTest, Abs) {
    RunTest(
        pwl_test::CreateAbs({1, 32}),
        pwl_test::CreateAbs({1, 32}),
        -1,
        1);
}

TEST(PwlTest, Sign) {
    RunTest(
        pwl_test::CreateSign({1, 32}),
        pwl_test::CreateSign({1, 32}),
        -1,
        1);
}
