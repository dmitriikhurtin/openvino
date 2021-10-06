// Copyright (C) 2018-2021 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "gtest/gtest.h"
#include "ngraph/ngraph.hpp"
#include "ngraph/ops.hpp"
#include "openvino/core/preprocess/pre_post_process.hpp"
#include "util/all_close.hpp"
#include "util/all_close_f.hpp"
#include "util/test_tools.hpp"

using namespace ov;
using namespace ov::preprocess;
using namespace ngraph::test;

static std::shared_ptr<Function> create_simple_function(element::Type type, const PartialShape& shape) {
    auto data1 = std::make_shared<op::v0::Parameter>(type, shape);
    data1->set_friendly_name("input1");
    data1->get_output_tensor(0).set_names({"tensor_input1"});
    auto res = std::make_shared<op::v0::Result>(data1);
    res->set_friendly_name("Result");
    return std::make_shared<Function>(ResultVector{res}, ParameterVector{data1});
}

static std::shared_ptr<Function> create_2inputs(element::Type type, const PartialShape& shape) {
    auto data1 = std::make_shared<op::v0::Parameter>(type, shape);
    data1->set_friendly_name("input1");
    data1->get_output_tensor(0).set_names({"tensor_input1"});
    auto data2 = std::make_shared<op::v0::Parameter>(type, shape);
    data2->set_friendly_name("input2");
    data1->get_output_tensor(0).set_names({"tensor_input2"});
    auto res1 = std::make_shared<op::v0::Result>(data1);
    res1->set_friendly_name("Result1");
    auto res2 = std::make_shared<op::v0::Result>(data2);
    res2->set_friendly_name("Result2");
    return std::make_shared<Function>(ResultVector{res1, res2}, ParameterVector{data1, data2});
}

TEST(pre_post_process, simple_mean_scale) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 2, 2});
    f = PrePostProcessor().input(InputInfo().preprocess(PreProcessSteps().mean(1.f).scale(2.f))).build(f);
    EXPECT_EQ(f->get_output_element_type(0), element::f32);
}

TEST(pre_post_process, convert_element_type_and_scale) {
    auto f = create_simple_function(element::i8, Shape{1, 3, 2, 2});
    f = PrePostProcessor()
            .input(InputInfo()
                       .tensor(InputTensorInfo().set_element_type(element::i16))
                       .preprocess(PreProcessSteps()
                                       .convert_element_type(element::f32)
                                       .scale(2.f)
                                       .convert_element_type(element::i8)))
            .build(f);
    EXPECT_EQ(f->get_parameters().front()->get_element_type(), element::i16);
    EXPECT_EQ(f->get_output_element_type(0), element::i8);
}

TEST(pre_post_process, empty_preprocess) {
    auto f = create_simple_function(element::i8, Shape{1, 3, 2, 2});
    f = PrePostProcessor().input(InputInfo().tensor(InputTensorInfo().set_element_type(element::i8))).build(f);
    EXPECT_EQ(f->get_parameters().front()->get_element_type(), element::i8);
    EXPECT_EQ(f->get_output_element_type(0), element::i8);
}

TEST(pre_post_process, convert_element_type_from_unknown) {
    auto f = create_simple_function(element::i32, Shape{1, 3, 224, 224});
    ASSERT_THROW(
        f = PrePostProcessor()
                .input(InputInfo().preprocess(
                    PreProcessSteps().convert_element_type(element::dynamic).convert_element_type(element::i32)))
                .build(f),
        ov::AssertFailure);
}

TEST(pre_post_process, convert_element_type_no_match) {
    auto f = create_simple_function(element::i32, Shape{1, 3, 224, 224});
    ASSERT_THROW(f = PrePostProcessor()
                         .input(InputInfo()
                                    .tensor(InputTensorInfo().set_element_type(element::i32))
                                    .preprocess(PreProcessSteps().convert_element_type(element::f32).scale(2.0f)))
                         .build(f),
                 ov::AssertFailure);
}

TEST(pre_post_process, scale_not_float) {
    auto f = create_simple_function(element::i32, Shape{1, 3, 224, 224});
    ASSERT_THROW(
        f = PrePostProcessor()
                .input(InputInfo().preprocess(PreProcessSteps().convert_element_type(element::f32).scale(2.0f)))
                .build(f),
        ov::AssertFailure);
}

TEST(pre_post_process, mean_not_float) {
    auto f = create_simple_function(element::i32, Shape{1, 3, 224, 224});
    ASSERT_THROW(f = PrePostProcessor()
                         .input(InputInfo().preprocess(PreProcessSteps().convert_element_type(element::f32).mean(2.0f)))
                         .build(f),
                 ov::AssertFailure);
}

TEST(pre_post_process, tensor_element_type_and_scale) {
    auto f = create_simple_function(element::i8, Shape{1, 3, 1, 1});
    f = PrePostProcessor()
            .input(InputInfo()
                       .tensor(InputTensorInfo().set_element_type(element::f32))
                       .preprocess(PreProcessSteps().scale(2.0f).convert_element_type(element::i8)))
            .build(f);

    EXPECT_EQ(f->get_parameters().front()->get_element_type(), element::f32);
    EXPECT_EQ(f->get_output_element_type(0), element::i8);
    EXPECT_EQ(f->get_parameters().front()->get_layout(), Layout());
}

TEST(pre_post_process, convert_color_nv12_rgb_single) {
    auto f = create_simple_function(element::f32, PartialShape{Dimension::dynamic(), 2, 2, 3});
    auto name = f->get_parameters()[0]->get_friendly_name();
    auto tensor_names = f->get_parameters().front()->get_output_tensor(0).get_names();
    f = PrePostProcessor()
            .input(
                InputInfo()
                    .tensor(InputTensorInfo()
                                .set_element_type(element::u8)
                                .set_color_format(ColorFormat::NV12_SINGLE_PLANE))
                    .preprocess(PreProcessSteps().convert_color(ColorFormat::RGB).convert_element_type(element::f32)))
            .build(f);

    EXPECT_EQ(f->get_parameters().size(), 1);
    EXPECT_EQ(f->get_parameters().front()->get_element_type(), element::u8);
    EXPECT_EQ(f->get_parameters().front()->get_layout(), "NHWC");
    EXPECT_EQ(f->get_parameters().front()->get_partial_shape(), (PartialShape{Dimension::dynamic(), 3, 2, 1}));
    EXPECT_EQ(f->get_parameters().front()->get_friendly_name(), name);
    EXPECT_EQ(f->get_parameters().front()->get_output_tensor(0).get_names(), tensor_names);
}

TEST(pre_post_process, convert_color_nv12_bgr_single) {
    auto f = create_simple_function(element::f32, PartialShape{Dimension::dynamic(), 2, 2, 3});
    auto name = f->get_parameters()[0]->get_friendly_name();
    auto tensor_names = f->get_parameters().front()->get_output_tensor(0).get_names();
    f = PrePostProcessor()
            .input(InputInfo()
                       .tensor(InputTensorInfo().set_color_format(ColorFormat::NV12_SINGLE_PLANE))
                       .preprocess(PreProcessSteps().convert_color(ColorFormat::BGR)))
            .build(f);

    EXPECT_EQ(f->get_parameters().size(), 1);
    EXPECT_EQ(f->get_parameters().front()->get_element_type(), element::f32);
    EXPECT_EQ(f->get_parameters().front()->get_layout(), "NHWC");
    EXPECT_EQ(f->get_parameters().front()->get_partial_shape(), (PartialShape{Dimension::dynamic(), 3, 2, 1}));
    EXPECT_EQ(f->get_parameters().front()->get_friendly_name(), name);
    EXPECT_EQ(f->get_parameters().front()->get_output_tensor(0).get_names(), tensor_names);
}

TEST(pre_post_process, convert_color_nv12_bgr_2_planes) {
    auto f = create_simple_function(element::f32, Shape{5, 2, 2, 3});
    f = PrePostProcessor()
            .input(InputInfo()
                       .tensor(InputTensorInfo().set_color_format(ColorFormat::NV12_TWO_PLANES, {"TestY", "TestUV"}))
                       .preprocess(PreProcessSteps().convert_color(ColorFormat::BGR)))
            .build(f);

    EXPECT_EQ(f->get_parameters().size(), 2);
    EXPECT_EQ(f->get_parameters()[0]->get_friendly_name(), "input1/TestY");
    EXPECT_EQ(*f->get_parameters()[0]->output(0).get_tensor().get_names().begin(), "tensor_input1/TestY");
    EXPECT_EQ(f->get_parameters()[0]->get_element_type(), element::f32);
    EXPECT_EQ(f->get_parameters()[0]->get_partial_shape(), (PartialShape{5, 2, 2, 1}));

    EXPECT_EQ(f->get_parameters()[1]->get_friendly_name(), "input1/TestUV");
    EXPECT_EQ(*f->get_parameters()[1]->output(0).get_tensor().get_names().begin(), "tensor_input1/TestUV");
    EXPECT_EQ(f->get_parameters()[1]->get_element_type(), element::f32);
    EXPECT_EQ(f->get_parameters()[1]->get_partial_shape(), (PartialShape{5, 1, 1, 2}));
}

TEST(pre_post_process, convert_color_nv12_rgb_2_planes) {
    auto f = create_simple_function(element::f32, Shape{5, 2, 2, 3});
    f = PrePostProcessor()
            .input(InputInfo()
                       .tensor(InputTensorInfo().set_color_format(ColorFormat::NV12_TWO_PLANES))
                       .preprocess(PreProcessSteps().convert_color(ColorFormat::RGB)))
            .build(f);

    EXPECT_EQ(f->get_parameters().size(), 2);
    EXPECT_EQ(f->get_parameters()[0]->get_element_type(), element::f32);
    EXPECT_EQ(f->get_parameters()[1]->get_element_type(), element::f32);
    EXPECT_EQ(f->get_parameters()[0]->get_partial_shape(), (PartialShape{5, 2, 2, 1}));
    EXPECT_EQ(f->get_parameters()[1]->get_partial_shape(), (PartialShape{5, 1, 1, 2}));

    EXPECT_EQ(f->get_parameters()[0]->get_friendly_name(), "input1/Y");
    EXPECT_EQ(*f->get_parameters()[0]->output(0).get_tensor().get_names().begin(), "tensor_input1/Y");

    EXPECT_EQ(f->get_parameters()[1]->get_friendly_name(), "input1/UV");
    EXPECT_EQ(*f->get_parameters()[1]->output(0).get_tensor().get_names().begin(), "tensor_input1/UV");
}

TEST(pre_post_process, convert_color_nv12_bgr_2_planes_u8_lvalue) {
    auto f = create_simple_function(element::u8, Shape{1, 2, 2, 3});
    auto input_tensor_info = InputTensorInfo();
    input_tensor_info.set_color_format(ColorFormat::NV12_TWO_PLANES);
    auto steps = PreProcessSteps();
    steps.convert_color(ColorFormat::BGR);
    f = PrePostProcessor()
            .input(InputInfo().tensor(std::move(input_tensor_info)).preprocess(std::move(steps)))
            .build(f);

    EXPECT_EQ(f->get_parameters().size(), 2);
    EXPECT_EQ(f->get_parameters()[0]->get_element_type(), element::u8);
    EXPECT_EQ(f->get_parameters()[0]->get_partial_shape(), (PartialShape{1, 2, 2, 1}));
    EXPECT_EQ(f->get_parameters()[1]->get_element_type(), element::u8);
    EXPECT_EQ(f->get_parameters()[1]->get_partial_shape(), (PartialShape{1, 1, 1, 2}));
}

TEST(pre_post_process, convert_color_nv12_bgr_2_planes_el_type) {
    auto f = create_simple_function(element::u8, Shape{1, 2, 2, 3});
    EXPECT_NO_THROW(
        f = PrePostProcessor()
                .input(InputInfo()
                           .tensor(InputTensorInfo()
                                       .set_element_type(element::f32)
                                       .set_color_format(ColorFormat::NV12_TWO_PLANES))
                           .preprocess(
                               PreProcessSteps().convert_element_type(element::u8).convert_color(ColorFormat::BGR)))
                .build(f));

    EXPECT_EQ(f->get_parameters().size(), 2);
    EXPECT_EQ(f->get_parameters()[0]->get_element_type(), element::f32);
    EXPECT_EQ(f->get_parameters()[1]->get_element_type(), element::f32);
}

TEST(pre_post_process, convert_color_same_type) {
    auto f = create_simple_function(element::u8, Shape{1, 2, 2, 3});
    EXPECT_NO_THROW(f = PrePostProcessor()
                            .input(InputInfo()
                                       .tensor(InputTensorInfo().set_color_format(ColorFormat::RGB))
                                       .preprocess(PreProcessSteps().convert_color(ColorFormat::RGB)))
                            .build(f));

    EXPECT_EQ(f->get_parameters().size(), 1);
    EXPECT_EQ(f->get_parameters()[0]->get_partial_shape(), (PartialShape{1, 2, 2, 3}));
}

TEST(pre_post_process, convert_color_unsupported) {
    // Feel free to update this test when more color conversions are supported in future
    auto f = create_simple_function(element::f32, PartialShape{1, 4, 4, 3});
    EXPECT_THROW(f = PrePostProcessor()
                         .input(InputInfo()
                                    .tensor(InputTensorInfo().set_color_format(ColorFormat::NV12_SINGLE_PLANE))
                                    .preprocess(PreProcessSteps().convert_color(ColorFormat::UNDEFINED)))
                         .build(f),
                 ov::AssertFailure);

    EXPECT_THROW(f = PrePostProcessor()
                         .input(InputInfo()
                                    .tensor(InputTensorInfo().set_color_format(ColorFormat::NV12_TWO_PLANES))
                                    .preprocess(PreProcessSteps().convert_color(ColorFormat::UNDEFINED)))
                         .build(f),
                 ov::AssertFailure);

    auto colors = {ColorFormat::NV12_TWO_PLANES, ColorFormat::NV12_SINGLE_PLANE, ColorFormat::RGB, ColorFormat::BGR};
    for (const auto& color : colors) {
        EXPECT_THROW(f = PrePostProcessor()
                             .input(InputInfo()
                                        .tensor(InputTensorInfo().set_color_format(ColorFormat::UNDEFINED))
                                        .preprocess(PreProcessSteps().convert_color(color)))
                             .build(f),
                     ov::AssertFailure);

        EXPECT_THROW(f = PrePostProcessor()
                             .input(InputInfo()
                                        .tensor(InputTensorInfo().set_color_format(color))
                                        .preprocess(PreProcessSteps().convert_color(ColorFormat::UNDEFINED)))
                             .build(f),
                     ov::AssertFailure);
    }
}

TEST(pre_post_process, convert_color_incorrect_subnames) {
    auto f = create_simple_function(element::f32, PartialShape{Dimension::dynamic(), 2, 2, 3});
    auto name = f->get_parameters()[0]->get_friendly_name();
    auto tensor_names = f->get_parameters().front()->get_output_tensor(0).get_names();
    EXPECT_THROW(
        f = PrePostProcessor()
                .input(InputInfo()
                           .tensor(InputTensorInfo().set_color_format(ColorFormat::NV12_SINGLE_PLANE, {"Test"}))
                           .preprocess(PreProcessSteps().convert_color(ColorFormat::RGB)))
                .build(f),
        ov::AssertFailure);

    EXPECT_THROW(
        f = PrePostProcessor()
                .input(InputInfo().tensor(InputTensorInfo().set_color_format(ColorFormat::NV12_TWO_PLANES, {"Test"})))
                .build(f),
        ov::AssertFailure);

    EXPECT_THROW(f = PrePostProcessor()
                         .input(InputInfo().tensor(
                             InputTensorInfo().set_color_format(ColorFormat::NV12_TWO_PLANES, {"1", "2", "3"})))
                         .build(f),
                 ov::AssertFailure);
}

TEST(pre_post_process, convert_color_duplicate_subnames) {
    auto f = create_2inputs(element::f32, PartialShape{1, 2, 2, 3});
    f->get_parameters()[0]->get_output_tensor(0).set_names({"tensor_input1"});
    f->get_parameters()[1]->get_output_tensor(0).set_names({"tensor_input1/CustomUV"});
    EXPECT_THROW(f = PrePostProcessor()
                         .input(InputInfo()
                                    .tensor(InputTensorInfo().set_color_format(ColorFormat::NV12_SINGLE_PLANE,
                                                                               {"CustomY", "CustomUV"}))
                                    .preprocess(PreProcessSteps().convert_color(ColorFormat::RGB)))
                         .build(f),
                 ov::AssertFailure);
}

TEST(pre_post_process, convert_color_duplicate_internal_subnames_mean) {
    auto f = create_simple_function(element::f32, PartialShape{1, 2, 2, 3});
    for (int i = 0; i < 10; i++) {
        // Create preprocessing step several times (try to duplicate internal node names this way)
        EXPECT_NO_THROW(f = PrePostProcessor().input(InputInfo().preprocess(PreProcessSteps().mean(0.1f))).build(f));
        EXPECT_NO_THROW(f = PrePostProcessor().input(InputInfo().preprocess(PreProcessSteps().scale(1.1f))).build(f));
        EXPECT_NO_THROW(
            f = PrePostProcessor()
                    .input(InputInfo().preprocess(
                        PreProcessSteps().convert_element_type(element::u8).convert_element_type(element::f32)))
                    .build(f));
        EXPECT_NO_THROW(f = PrePostProcessor()
                                .input(InputInfo()
                                           .tensor(InputTensorInfo().set_layout("NHWC"))
                                           .preprocess(PreProcessSteps().convert_layout("NCHW")))
                                .build(f));
        EXPECT_NO_THROW(
            f = PrePostProcessor()
                    .input(InputInfo()
                               .tensor(InputTensorInfo().set_layout("NHWC").set_spatial_static_shape(480, 640))
                               .preprocess(PreProcessSteps().resize(ResizeAlgorithm::RESIZE_LINEAR)))
                    .build(f));
    }
}

TEST(pre_post_process, unsupported_network_color_format) {
    auto f = create_simple_function(element::f32, PartialShape{1, 4, 4, 3});
    EXPECT_THROW(f = PrePostProcessor()
                         .input(InputInfo().tensor(InputTensorInfo().set_color_format(ColorFormat::NV12_SINGLE_PLANE)))
                         .build(f),
                 ov::AssertFailure);

    EXPECT_THROW(f = PrePostProcessor()
                         .input(InputInfo().tensor(InputTensorInfo().set_color_format(ColorFormat::NV12_TWO_PLANES)))
                         .build(f),
                 ov::AssertFailure);

    EXPECT_THROW(
        f = PrePostProcessor()
                .input(InputInfo()
                           .tensor(InputTensorInfo().set_color_format(ColorFormat::NV12_TWO_PLANES))
                           .preprocess(PreProcessSteps().convert_layout("NCHW").convert_color(ColorFormat::RGB)))
                .build(f),
        ov::AssertFailure);

    EXPECT_THROW(f = PrePostProcessor()
                         .input(InputInfo()
                                    .tensor(InputTensorInfo().set_color_format(ColorFormat::NV12_TWO_PLANES))
                                    .preprocess(PreProcessSteps().mean(0.1f).convert_color(ColorFormat::RGB)))
                         .build(f),
                 ov::AssertFailure);

    EXPECT_THROW(f = PrePostProcessor()
                         .input(InputInfo()
                                    .tensor(InputTensorInfo().set_color_format(ColorFormat::NV12_TWO_PLANES))
                                    .preprocess(PreProcessSteps().scale(2.1f).convert_color(ColorFormat::RGB)))
                         .build(f),
                 ov::AssertFailure);
}

TEST(pre_post_process, custom_preprocessing) {
    auto f = create_simple_function(element::i32, Shape{1, 3, 1, 1});
    f = PrePostProcessor()
            .input(InputInfo().preprocess(PreProcessSteps().custom([](const std::shared_ptr<Node>& node) {
                auto abs = std::make_shared<op::v0::Abs>(node);
                abs->set_friendly_name(node->get_friendly_name() + "/abs");
                return abs;
            })))
            .build(f);
    EXPECT_EQ(f->get_output_element_type(0), element::i32);
}

TEST(pre_post_process, test_lvalue) {
    auto f = create_simple_function(element::i8, Shape{1, 3, 1, 1});
    auto name = f->get_parameters()[0]->get_friendly_name();
    auto tensor_names = f->get_parameters().front()->get_output_tensor(0).get_names();
    auto p = PrePostProcessor();
    auto p1 = std::move(p);
    p = std::move(p1);
    auto inputInfo = InputInfo();
    auto inputInfo2 = std::move(inputInfo);
    inputInfo = std::move(inputInfo2);
    {
        auto inputTensorInfo = InputTensorInfo();
        auto inputTensorInfo2 = std::move(inputTensorInfo);
        inputTensorInfo = std::move(inputTensorInfo2);
        auto& same = inputTensorInfo.set_element_type(element::f32);
        same.set_layout("?CHW");
        inputInfo.tensor(std::move(same));
    }
    {
        auto preprocessSteps = PreProcessSteps();
        auto preprocessSteps2 = std::move(preprocessSteps);
        preprocessSteps = std::move(preprocessSteps2);
        preprocessSteps.mean(1.f);
        preprocessSteps.scale(2.f);
        preprocessSteps.mean({1.f, 2.f, 3.f});
        preprocessSteps.scale({2.f, 3.f, 4.f});
        preprocessSteps.custom([](const std::shared_ptr<Node>& node) {
            auto abs = std::make_shared<op::v0::Abs>(node);
            abs->set_friendly_name(node->get_friendly_name() + "/abs");
            return abs;
        });
        auto& same = preprocessSteps.convert_element_type(element::i8);
        inputInfo.preprocess(std::move(same));
    }
    p.input(std::move(inputInfo));
    f = p.build(f);
    EXPECT_EQ(f->get_parameters().front()->get_element_type(), element::f32);
    EXPECT_EQ(f->get_parameters().front()->get_friendly_name(), name);
    EXPECT_EQ(f->get_parameters().front()->get_layout(), "?CHW");
    EXPECT_EQ(f->get_parameters().front()->get_output_tensor(0).get_names(), tensor_names);
    EXPECT_EQ(f->get_output_element_type(0), element::i8);
}

TEST(pre_post_process, test_2_inputs_basic) {
    auto f = create_2inputs(element::f32, Shape{1, 3, 1, 1});
    { f = PrePostProcessor().input(InputInfo(1).preprocess(PreProcessSteps().mean(1.f).scale(2.0f))).build(f); }
    EXPECT_EQ(f->get_output_element_type(0), element::f32);
    EXPECT_EQ(f->get_output_element_type(1), element::f32);
}

TEST(pre_post_process, reuse_network_layout_no_tensor_info) {
    auto f = create_simple_function(element::f32, PartialShape{Dimension::dynamic(), 3, 2, 1});
    f->get_parameters().front()->set_layout("NC??");
    f = PrePostProcessor()
            .input(InputInfo().preprocess(PreProcessSteps().mean({1.f, 2.f, 3.f}).scale({2.f, 3.f, 4.f})))
            .build(f);
    EXPECT_EQ(f->get_parameters().front()->get_layout(), "NC??");
}

TEST(pre_post_process, reuse_network_layout_tensor_info) {
    auto f = create_simple_function(element::u8, PartialShape{Dimension::dynamic(), 3, 2, 1});
    f->get_parameters().front()->set_layout("NC??");
    f = PrePostProcessor()
            .input(InputInfo()
                       .tensor(InputTensorInfo().set_element_type(element::f32))
                       .preprocess(PreProcessSteps()
                                       .mean({1.f, 2.f, 3.f})
                                       .scale({2.f, 3.f, 4.f})
                                       .convert_element_type(element::u8)))
            .build(f);
    EXPECT_EQ(f->get_parameters().front()->get_layout(), "NC??");
}

TEST(pre_post_process, mean_scale_vector_tensor_layout) {
    auto f = create_simple_function(element::f32, PartialShape{Dimension::dynamic(), 3, 2, 1});
    auto name = f->get_parameters().front()->get_friendly_name();
    auto tensor_names = f->get_parameters().front()->get_output_tensor(0).get_names();
    f = PrePostProcessor()
            .input(InputInfo()
                       .tensor(InputTensorInfo().set_layout("NC??"))
                       .preprocess(PreProcessSteps().mean({1.f, 2.f, 3.f}).scale({2.f, 3.f, 4.f})))
            .build(f);
    EXPECT_EQ(f->get_parameters().front()->get_friendly_name(), name);
    EXPECT_EQ(f->get_parameters().front()->get_layout(), "NC??");
    EXPECT_EQ(f->get_parameters().front()->get_output_tensor(0).get_names(), tensor_names);
    EXPECT_EQ(f->get_output_element_type(0), element::f32);
}

TEST(pre_post_process, mean_scale_dynamic_layout) {
    auto f = create_simple_function(element::f32,
                                    PartialShape{Dimension::dynamic(), Dimension::dynamic(), Dimension::dynamic(), 3});
    auto name = f->get_parameters().front()->get_friendly_name();
    auto tensor_names = f->get_parameters().front()->get_output_tensor(0).get_names();
    f = PrePostProcessor()
            .input(InputInfo()
                       .tensor(InputTensorInfo().set_layout("N...C"))
                       .preprocess(PreProcessSteps().mean({1.f, 2.f, 3.f}).scale({2.f, 3.f, 4.f})))
            .build(f);

    EXPECT_EQ(f->get_parameters().front()->get_friendly_name(), name);
    EXPECT_EQ(f->get_parameters().front()->get_layout(), "N...C");
    EXPECT_EQ(f->get_parameters().front()->get_output_tensor(0).get_names(), tensor_names);
    EXPECT_EQ(f->get_output_element_type(0), element::f32);
}

TEST(pre_post_process, scale_vector_no_channels_layout) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 224, 224});
    EXPECT_EQ(f->get_output_element_type(0), element::f32);
    EXPECT_THROW(f = PrePostProcessor()
                         .input(InputInfo()
                                    .tensor(InputTensorInfo().set_layout("N?HW"))
                                    .preprocess(PreProcessSteps().scale({0.1f, 0.2f, 0.3f})))
                         .build(f),
                 ov::AssertFailure);
}

TEST(pre_post_process, scale_vector_dim_mismatch) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 224, 224});
    EXPECT_EQ(f->get_output_element_type(0), element::f32);
    EXPECT_THROW(f = PrePostProcessor()
                         .input(InputInfo()
                                    .tensor(InputTensorInfo().set_layout("NCHW"))
                                    .preprocess(PreProcessSteps().scale({0.1f, 0.2f, 0.3f, 0.4f})))
                         .build(f),
                 ov::AssertFailure);
}

TEST(pre_post_process, scale_vector_channels_out_of_range) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 224, 224});
    ASSERT_EQ(f->get_output_element_type(0), element::f32);
    ASSERT_THROW(f = PrePostProcessor()
                         .input(InputInfo()
                                    .tensor(InputTensorInfo().set_layout("0123C"))
                                    .preprocess(PreProcessSteps().scale({0.1f, 0.2f, 0.3f})))
                         .build(f),
                 ov::AssertFailure);
}

TEST(pre_post_process, mean_vector_no_layout) {
    auto f = create_simple_function(element::f32, PartialShape{Dimension::dynamic(), 3, 224, 224});
    ASSERT_EQ(f->get_output_element_type(0), element::f32);
    ASSERT_THROW(
        f = PrePostProcessor().input(InputInfo().preprocess(PreProcessSteps().mean({0.1f, 0.2f, 0.3f}))).build(f),
        ov::AssertFailure);
}

TEST(pre_post_process, mean_vector_dynamic_channels_shape) {
    auto f = create_simple_function(
        element::f32,
        PartialShape{Dimension::dynamic(), Dimension::dynamic(), Dimension::dynamic(), Dimension::dynamic()});
    EXPECT_EQ(f->get_output_element_type(0), element::f32);
    EXPECT_NO_THROW(f = PrePostProcessor()
                            .input(InputInfo()
                                       .tensor(InputTensorInfo().set_layout("NCHW"))
                                       .preprocess(PreProcessSteps().mean({0.1f, 0.2f, 0.3f})))
                            .build(f));
    EXPECT_EQ(f->get_output_element_type(0), element::f32);
}

// Error cases for 'resize'
TEST(pre_post_process, resize_no_network_layout) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 224, 224});
    EXPECT_THROW(f = PrePostProcessor()
                         .input(InputInfo()
                                    .tensor(InputTensorInfo().set_layout("NHWC"))
                                    .preprocess(PreProcessSteps().resize(ResizeAlgorithm::RESIZE_CUBIC)))
                         .build(f),
                 ov::AssertFailure);
}

TEST(pre_post_process, tensor_spatial_shape_no_layout_dims) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 224, 224});
    EXPECT_THROW(f = PrePostProcessor()
                         .input(InputInfo()
                                    .tensor(InputTensorInfo().set_layout("NC?W").set_spatial_static_shape(480, 640))
                                    .preprocess(PreProcessSteps().resize(ResizeAlgorithm::RESIZE_CUBIC)))
                         .build(f),
                 ov::AssertFailure);

    EXPECT_THROW(f = PrePostProcessor()
                         .input(InputInfo()
                                    .tensor(InputTensorInfo().set_layout("NCH?").set_spatial_static_shape(480, 640))
                                    .preprocess(PreProcessSteps().resize(ResizeAlgorithm::RESIZE_CUBIC)))
                         .build(f),
                 ov::AssertFailure);
}

TEST(pre_post_process, resize_no_tensor_height) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 224, 224});
    EXPECT_THROW(f = PrePostProcessor()
                         .input(InputInfo()
                                    .tensor(InputTensorInfo().set_layout("N?WC"))
                                    .preprocess(PreProcessSteps().resize(ResizeAlgorithm::RESIZE_LINEAR))
                                    .network(InputNetworkInfo().set_layout("NHWC")))
                         .build(f),
                 ov::AssertFailure);
}

TEST(pre_post_process, resize_no_tensor_width) {
    auto f = create_simple_function(element::f32, Shape{1, 3, 224, 224});
    EXPECT_THROW(f = PrePostProcessor()
                         .input(InputInfo()
                                    .tensor(InputTensorInfo().set_layout("NH?C"))
                                    .preprocess(PreProcessSteps().resize(ResizeAlgorithm::RESIZE_LINEAR))
                                    .network(InputNetworkInfo().set_layout("NHWC")))
                         .build(f),
                 ov::AssertFailure);
}

TEST(pre_post_process, exception_safety) {
    auto f = create_2inputs(element::f32, Shape{1, 3, 224, 224});
    auto name0 = f->get_parameters()[0]->get_friendly_name();
    auto tensor_names0 = f->get_parameters()[0]->get_output_tensor(0).get_names();
    auto name1 = f->get_parameters()[1]->get_friendly_name();
    auto tensor_names1 = f->get_parameters()[1]->get_output_tensor(0).get_names();
    EXPECT_THROW(f = PrePostProcessor()
                         .input(InputInfo(0)  // this one is correct
                                    .tensor(InputTensorInfo().set_element_type(element::u8))
                                    .preprocess(PreProcessSteps().convert_element_type(element::f32)))
                         .input(InputInfo(1)  // This one is not
                                    .tensor(InputTensorInfo().set_color_format(ColorFormat::NV12_TWO_PLANES))
                                    .preprocess(PreProcessSteps().custom(
                                        [](const std::shared_ptr<Node>& node) -> std::shared_ptr<Node> {
                                            throw ngraph::ngraph_error("test error");
                                        })))
                         .build(f),
                 ov::AssertFailure);
    EXPECT_EQ(f->get_parameters().size(), 2);

    EXPECT_EQ(f->get_parameters()[0]->get_element_type(), element::f32);
    EXPECT_EQ(f->get_parameters()[0]->get_partial_shape(), (PartialShape{1, 3, 224, 224}));
    EXPECT_EQ(f->get_parameters()[0]->get_friendly_name(), name0);
    EXPECT_EQ(f->get_parameters()[0]->get_output_tensor(0).get_names(), tensor_names0);

    EXPECT_EQ(f->get_parameters()[1]->get_element_type(), element::f32);
    EXPECT_EQ(f->get_parameters()[1]->get_partial_shape(), (PartialShape{1, 3, 224, 224}));
    EXPECT_EQ(f->get_parameters()[1]->get_friendly_name(), name1);
    EXPECT_EQ(f->get_parameters()[1]->get_output_tensor(0).get_names(), tensor_names1);
}
