// Copyright (C) 2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "transpose_to_pwl.hpp"
#include "transformations/utils/utils.hpp"
#include "ops/pwl.hpp"
#include "ops/reference/pwl.hpp"

#include <memory>
#include <vector>
#include <numeric>
#include <iostream>

#include <openvino/cc/ngraph/itt.hpp>
#include <ngraph/rt_info.hpp>
#include <ngraph/pattern/op/wrap_type.hpp>
#include <ngraph/pattern/op/or.hpp>

static constexpr double EXP_BREAK = 0.045;

namespace GNAPluginNS {

NGRAPH_RTTI_DEFINITION(TransposeToPwl, "TransposeToPwl", 0);

template<typename T>
double get_break_bound() {
    if (std::is_same<T, ngraph::opset8::Exp>::value) {
        return EXP_BREAK;
    }

    return 0;
}

template<typename T>
bool split_search(double lower_bound, double upper_bound) {
    if (lower_bound > upper_bound) {
        return false;
    }

    double break_bound = get_break_bound<T>();
    if (std::is_same<T, ngraph::opset8::Sigmoid>::value ||
        std::is_same<T, ngraph::opset8::Tanh>::value ||
        std::is_same<T, SoftSign>::value ||
        std::is_same<T, ngraph::opset8::Exp>::value ||
        std::is_same<T, ngraph::opset8::Power>::value) {
        return lower_bound < break_bound && upper_bound > break_bound;
    }
    return false;
}

template <typename T>
double pivot_search(const details::Function<T>& activation_function,
                    std::vector<details::Pwl>& result,
                    uint32_t N,
                    double alpha_0,
                    double alpha_N,
                    bool negative,
                    double max_error,
                    double threshold = 0.1) {
    std::vector<std::vector<double>> t(N + 1);
    std::vector<std::vector<double>> alpha(N + 1);
    std::vector<std::vector<double>> epsilon(N + 1);
    std::vector<std::vector<double>> d(N + 1);
    bool same_epsilon = false;
    double Delta;
    double epsilon_final = 0.0;
    double max_epsilon = 0.0;
    double max_epsilon_prev;
    double min_epsilon;
    double sgn = (negative) ? -1.0 : 1.0;
    int j;

    // Figure 4:  Box #1
    j = 0;
    Delta = 1.0;

    for (int i = 0; i < N; i++) {
        t[i].push_back(alpha_0 + (static_cast<double>((i + 1)) / static_cast<double>((N + 1))) * (alpha_N - alpha_0));
    }

    while (true) {
        // Figure 4:  Box #2
        alpha[0].resize(j + 1);
        alpha[0][j] = alpha_0;
        for (int i = 1; i < N; i++) {
            alpha[i].resize(j + 1);
            alpha[i][j] = (activation_function.get_value(t[i - 1][j]) - activation_function.get_value(t[i][j]) +
                activation_function.first_derivative(t[i][j]) * t[i][j] - activation_function.first_derivative(t[i - 1][j]) * t[i - 1][j])
                / (activation_function.first_derivative(t[i][j]) - activation_function.first_derivative(t[i - 1][j]));
        }
        alpha[N].resize(j + 1);
        alpha[N][j] = alpha_N;

        // Figure 4:  Box #3
        for (int i = 0; i < N; i++) {
            epsilon[i].resize(j + 1);
            epsilon[i][j] = sgn * (activation_function.first_derivative(t[i][j]) * (alpha[i][j] - t[i][j]) +
                activation_function.get_value(t[i][j]) - activation_function.get_value(alpha[i][j]));
            if (std::isnan(epsilon[i][j])) {
                throw std::runtime_error("The value is out of range.");
            }
        }
        epsilon[N].resize(j + 1);
        epsilon[N][j] = sgn * (activation_function.first_derivative(t[N - 1][j]) * (alpha[N][j] - t[N - 1][j]) +
            activation_function.get_value(t[N - 1][j]) - activation_function.get_value(alpha[N][j]));
        if (std::isnan(epsilon[N][j])) {
            throw std::runtime_error("The value is out of range.");
        }

        // Figure 4:  Test for completion
        max_epsilon_prev = max_epsilon;
        max_epsilon = fabs(epsilon[0][j]);
        min_epsilon = fabs(epsilon[0][j]);
        for (int i = 1; i < N + 1; i++) {
            if (fabs(epsilon[i][j]) > max_epsilon) max_epsilon = fabs(epsilon[i][j]);
            if (fabs(epsilon[i][j]) < min_epsilon) min_epsilon = fabs(epsilon[i][j]);
        }
        if (j == details::max_iterations<T>() || max_epsilon - min_epsilon < threshold * min_epsilon) {
            details::Pwl value;
            result.resize(0);
            epsilon_final = (max_epsilon + min_epsilon) / 4.0;  // Andrzej's modification
            for (int i = 0; i < N; i++) {
                double val, val_next;
                double value_t = t[i][j];
                value.alpha = alpha[i][j];
                val = sgn * activation_function.first_derivative(value_t) * (value.alpha - value_t) +
                    sgn * activation_function.get_value(value_t) - epsilon_final;
                val_next = sgn * activation_function.first_derivative(value_t) * (alpha[i + 1][j] - value_t) +
                    sgn * activation_function.get_value(value_t) - epsilon_final;
                value.m = (val_next - val) / (alpha[i + 1][j] - value.alpha);
                value.b = (val - value.m * value.alpha);
                result.push_back(value);
            }
            value.m = value.b = 0.0;
            value.alpha = alpha[N][j];
            result.push_back(value);
            if (j == details::max_iterations<T>()) {
                throw std::runtime_error("Failed to converge in pivot_search!");
            }
            return (epsilon_final);
        }

        if (j > 0) {
            if (max_epsilon > max_epsilon_prev) {
                j = j - 1;
                Delta = Delta / 2;
            } else if (max_epsilon == max_epsilon_prev) {
                if (!same_epsilon) {
                    same_epsilon = true;
                } else {
                    j = j - 1;
                    Delta = Delta / 2;
                    same_epsilon = false;
                }
            }
        }

        // Figure 4:  Box #4
        for (int i = 0; i < N; i++) {
            d[i].resize(j + 1);
            d[i][j] = Delta * (epsilon[i + 1][j] - epsilon[i][j]) /
                ((epsilon[i + 1][j] / (alpha[i + 1][j] - t[i][j])) + (epsilon[i][j] / (t[i][j] - alpha[i][j])));
        }

        // Figure 4:  Box #5
        for (int i = 0; i < N; i++) {
            t[i].resize(j + 2);
            t[i][j + 1] = t[i][j] + d[i][j];
        }
        t[N].resize(j + 2);

        j = j + 1;
    }
}

template<typename T>
double calculate_error_pct(const details::Function<T>& activation_function,
                           const std::vector<details::Pwl>& segments,
                           double lower_bound,
                           double upper_bound,
                           const double offset,
                           bool negative,
                           int samples = 500) {
    double sgn = negative ? -1 : 1;
    double delta = (upper_bound - lower_bound) / (samples + 1);
    if (delta < 0) {
        return 0.0;
    }

    std::vector<double> m(segments.size() - 1);
    std::vector<double> b(segments.size() - 1);
    std::vector<double> alpha(segments.size());
    for (size_t i = 0; i < segments.size() - 1; i++) {
        m[i] = segments[i].m;
        b[i] = segments[i].b;
        alpha[i] = segments[i].alpha;
    }
    alpha[segments.size() - 1] = segments[segments.size() - 1].alpha;

    std::vector<double> in(samples);
    for (int i = 0; i < samples; i++)
        in[i] = lower_bound + i * delta;
    std::vector<double> out(samples);
    runtime::reference::pwl<double, double>(in.data(), out.data(), in.size(), m.data(), b.data(), alpha.data(), m.size());
    double max_err = std::fabs(activation_function.get_value(lower_bound) - sgn * out[0]);
    for (int i = 0; i < samples; i++) {
        double err = std::fabs(activation_function.get_value(lower_bound + i * delta) - sgn * out[i]);
        if (err > max_err)
            max_err = err;
    }

    return max_err;
}

template<typename T>
bool is_negative(const details::Function<T>& activation_function, double upper_bound) {
    if (std::is_same<T, ngraph::opset8::Sigmoid>::value ||
        std::is_same<T, ngraph::opset8::Tanh>::value ||
        std::is_same<T, SoftSign>::value) {
        return upper_bound == 0;
    }

    if (std::is_same<T, ngraph::opset8::Exp>::value) {
        return true;
    }

    return false;
}

template<>
bool is_negative<ngraph::opset8::Power>(const details::Function<ngraph::opset8::Power>& activation_function, double upper_bound) {
    return std::fmod(activation_function.m_exponent, 1.0) == 0;
}

template<typename T>
double max_error(const details::Function<T>& activation_function, double allowed_err_pct) {
    return allowed_err_pct;
}

template<>
double max_error<ngraph::opset8::Power>(const details::Function<ngraph::opset8::Power>& activation_function, double allowed_err_pct) {
    if (activation_function.m_exponent == 0) {
        return allowed_err_pct;
    }

    return allowed_err_pct;
}

template<typename T>
std::vector<details::Pwl> pwl_search(const details::Function<T>& activation_function,
                                     double lower_bound,
                                     double upper_bound,
                                     double allowed_err_pct,
                                     double& err_pct) {
    std::vector<details::Pwl> pwl;
    if (lower_bound > upper_bound) {
        return pwl;
    }

    if (split_search<T>(lower_bound, upper_bound)) {
        auto negative_pwl = [](std::vector<details::Pwl>& data) {
            for (auto& e : data) {
                e.m = -e.m;
                e.b = -e.b;
            }
        };

        double err_pct1 = 0.0;
        double err_pct2 = 0.0;
        double break_bound = get_break_bound<T>();
        pwl = pwl_search<T>(activation_function, lower_bound, break_bound, allowed_err_pct, err_pct1);
        negative_pwl(pwl);
        std::vector<details::Pwl> pwl2 = pwl_search<T>(activation_function, break_bound, upper_bound, allowed_err_pct, err_pct2);
        if (std::is_same<T, ngraph::opset8::Exp>::value ||
            std::is_same<T, ngraph::opset8::Power>::value) {
            negative_pwl(pwl2);
        }

        // merge
        if (!pwl.empty())
            pwl.pop_back();  // remove final alpha from first half
        pwl.insert(pwl.end(), pwl2.begin(), pwl2.end());  // concatenate the two halves
        err_pct = (err_pct1 + err_pct2) / 2;  // this is not quite correct but should give an indication
    } else {
        int segments_number = 1;
        bool negative = is_negative<T>(activation_function, upper_bound);
        auto err = pivot_search<T>(activation_function, pwl, segments_number, lower_bound, upper_bound, negative,
            max_error<T>(activation_function, allowed_err_pct));
        err_pct = calculate_error_pct<T>(activation_function, pwl, lower_bound, upper_bound, err, negative);
        while (segments_number < details::max_segments_number<T>() && max_error<T>(activation_function, allowed_err_pct) < err_pct) {
            segments_number++;
            err = pivot_search<T>(activation_function, pwl, segments_number, lower_bound, upper_bound, negative,
                max_error<T>(activation_function, allowed_err_pct));
            err_pct = calculate_error_pct<T>(activation_function, pwl, lower_bound, upper_bound, err, negative);
        }

        if (segments_number >= details::max_segments_number<T>()) {
            throw std::runtime_error("Failed to converge in pwl_search!");
        }
    }

    return pwl;
}

template<typename T>
std::vector<details::Pwl> pwl_search(const std::shared_ptr<T>& node,
                                     double allowed_err_pct,
                                     double& err_pct) {
    return pwl_search<T>(details::Function<T>(),
                         details::lower_bound<T>(),
                         details::upper_bound<T>(),
                         allowed_err_pct,
                         err_pct);
}

template <typename T>
bool get_exponent(const std::shared_ptr<ngraph::opset8::Constant>& constant, double& exponent) {
    using A = typename ov::element_type_traits<T::value>::value_type;
    const auto& exponents = constant->get_vector<A>();
    if (exponents.empty() || exponents.size() > 1) {
        throw std::runtime_error("The size of exponents is more than 1.");
    }

    exponent = exponents[0];
    return true;
}

template<typename T>
bool get_exponent(const std::tuple<T>& args,
                  const std::shared_ptr<ngraph::opset8::Constant>& constant, double& exponent) {
    return constant->get_element_type() == T::value &&
           get_exponent<T>(constant, exponent);
}

template<typename T, typename ...Types>
bool get_exponent(const std::tuple<T, Types...>&,
                  const std::shared_ptr<ngraph::opset8::Constant>& constant, double& exponent) {
    return constant->get_element_type() == T::value &&
           get_exponent<T>(constant, exponent) ||
           get_exponent<Types...>(std::tuple<Types...>(), constant, exponent);
}

template<>
std::vector<details::Pwl> pwl_search<ngraph::opset8::Power>(const std::shared_ptr<ngraph::opset8::Power>& node,
                                                            double allowed_err_pct,
                                                            double& err_pct) {
    auto constant = std::dynamic_pointer_cast<ngraph::opset8::Constant>(node->get_input_node_shared_ptr(1));
    double exponent = 0;
    if (!get_exponent(std::tuple<std::integral_constant<ov::element::Type_t, ov::element::i32>,
                                 std::integral_constant<ov::element::Type_t, ov::element::i64>,
                                 std::integral_constant<ov::element::Type_t, ov::element::u32>,
                                 std::integral_constant<ov::element::Type_t, ov::element::u64>,
                                 std::integral_constant<ov::element::Type_t, ov::element::f16>,
                                 std::integral_constant<ov::element::Type_t, ov::element::f32>,
                                 std::integral_constant<ov::element::Type_t, ov::element::f64>>(),
                      constant,
                      exponent)) {
        throw std::runtime_error("The unsupported type of element.");
    }

    if (details::are_floats_equal(exponent, 1.0)) {
        std::vector<details::Pwl> pwl;
        pwl.emplace_back(1., 0., static_cast<double>(std::numeric_limits<int32_t>::min()));
        pwl.emplace_back(0., 0., static_cast<double>(std::numeric_limits<int32_t>::max()));
        return pwl;
    }

    return pwl_search<ngraph::opset8::Power>(details::Function<ngraph::opset8::Power>(exponent, 1, 0),
                                             details::lower_bound<ngraph::opset8::Power>(exponent),
                                             details::upper_bound<ngraph::opset8::Power>(),
                                             allowed_err_pct,
                                             err_pct);
}

template<>
std::vector<details::Pwl> pwl_search<ngraph::op::PowerIE>(const std::shared_ptr<ngraph::op::PowerIE>& node,
                                                          double allowed_err_pct,
                                                          double& err_pct) {
    auto power = std::dynamic_pointer_cast<ngraph::op::PowerIE>(node);
    return pwl_search<ngraph::opset8::Power>(details::Function<ngraph::opset8::Power>(power->power, power->scale, power->shift),
                                             details::lower_bound<ngraph::opset8::Power>(power->power),
                                             details::upper_bound<ngraph::opset8::Power>(),
                                             allowed_err_pct,
                                             err_pct);
}

template<typename T>
bool transpose_to_pwl(const std::shared_ptr<T>& node, double allowed_err_pct) {
    double err_pct = 0;
    auto segments = pwl_search<T>(node, allowed_err_pct, err_pct);
    if (segments.size() < 2) {
        return false;
    }

    std::vector<double> m(segments.size() - 1);
    std::vector<double> b(segments.size() - 1);
    std::vector<double> alpha(segments.size());
    for (size_t i = 0; i < segments.size() - 1; i++) {
        m[i] = segments[i].m;
        b[i] = segments[i].b;
        alpha[i] = segments[i].alpha;
    }
    alpha[segments.size() - 1] = segments[segments.size() - 1].alpha;

    auto m_constant = std::make_shared<ngraph::opset8::Constant>(ngraph::element::Type_t::f64,
        ngraph::Shape{segments.size() - 1}, m);
    auto b_constant = std::make_shared<ngraph::opset8::Constant>(ngraph::element::Type_t::f64,
        ngraph::Shape{segments.size() - 1}, b);
    auto alpha_constant = std::make_shared<ngraph::opset8::Constant>(ngraph::element::Type_t::f64,
        ngraph::Shape{segments.size()}, alpha);
    auto pwl = std::make_shared<Pwl>(node->input(0).get_source_output(), m_constant, b_constant, alpha_constant);
    pwl->set_friendly_name(node->get_friendly_name());
    ngraph::copy_runtime_info(node, pwl);
    replace_node(node, pwl);
    return true;
}

template<typename T>
bool transpose_to_pwl(const std::tuple<T>& args,
                      const std::shared_ptr<ngraph::Node>& node,
                      double allowed_err_pct);

template<typename T, typename ...Types>
bool transpose_to_pwl(const std::tuple<T, Types...>& args,
                      const std::shared_ptr<ngraph::Node>& node,
                      double allowed_err_pct) {
    auto op = std::dynamic_pointer_cast<T>(node);
    if (op) {
        return transpose_to_pwl(op, allowed_err_pct);
    }
    return transpose_to_pwl<Types...>(std::tuple<Types...>(), node, allowed_err_pct);
}

template<typename T>
bool transpose_to_pwl(const std::tuple<T>& args,
                      const std::shared_ptr<ngraph::Node>& node,
                      double allowed_err_pct) {
    auto op = std::dynamic_pointer_cast<T>(node);
    if (op) {
        return transpose_to_pwl(op, allowed_err_pct);
    }
    return false;
}

TransposeToPwl::TransposeToPwl(double allowed_err_pct) {
    MATCHER_SCOPE(TransposeToPwl);
    auto sigmoid = ngraph::pattern::wrap_type<ngraph::opset8::Sigmoid>({ ngraph::pattern::any_input() });
    auto tanh = ngraph::pattern::wrap_type<ngraph::opset8::Tanh>({ ngraph::pattern::any_input() });
    auto exp = ngraph::pattern::wrap_type<ngraph::opset8::Exp>({ ngraph::pattern::any_input() });
    auto power = ngraph::pattern::wrap_type<ngraph::opset8::Power>({ ngraph::pattern::any_input(), ngraph::pattern::any_input() });
    auto powerIE = ngraph::pattern::wrap_type<ngraph::op::PowerIE>({ ngraph::pattern::any_input() });
    auto log = ngraph::pattern::wrap_type<ngraph::opset8::Log>({ ngraph::pattern::any_input() });
    auto softsign = ngraph::pattern::wrap_type<SoftSign>({ ngraph::pattern::any_input() });
    const auto activation_functions =
        std::make_shared<ngraph::pattern::op::Or>(ov::OutputVector{ sigmoid, tanh, exp, power, powerIE, log, softsign });

    auto callback = [sigmoid,
                     tanh,
                     exp,
                     power,
                     powerIE,
                     log,
                     softsign,
                     allowed_err_pct](ngraph::pattern::Matcher & m) -> bool {
        const auto& pattern_to_output = m.get_pattern_value_map();
        auto iter = pattern_to_output.find(sigmoid);
        if (iter == pattern_to_output.end() &&
            (iter = pattern_to_output.find(tanh)) == pattern_to_output.end() &&
            (iter = pattern_to_output.find(exp)) == pattern_to_output.end() &&
            (iter = pattern_to_output.find(power)) == pattern_to_output.end() &&
            (iter = pattern_to_output.find(powerIE)) == pattern_to_output.end() &&
            (iter = pattern_to_output.find(log)) == pattern_to_output.end() &&
            (iter = pattern_to_output.find(softsign)) == pattern_to_output.end()) {
            return false;
        }
        return transpose_to_pwl(
            std::tuple<
                ngraph::opset8::Sigmoid,
                ngraph::opset8::Tanh,
                ngraph::opset8::Exp,
                ngraph::opset8::Power,
                ngraph::op::PowerIE,
                ngraph::opset8::Log,
                SoftSign>(),
            iter->second.get_node_shared_ptr(),
            allowed_err_pct);
    };

    auto m = std::make_shared<ngraph::pattern::Matcher>(activation_functions, matcher_name);
    register_matcher(m, callback);
}

} // namespace GNAPluginNS
