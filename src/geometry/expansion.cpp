module;

#include <cmath>
#include <utility>
#include <vector>

module hw3d.geometry;

namespace hw3d {
namespace {

struct TwoTerm {
    double low;
    double high;
};

TwoTerm two_sum(const double a, const double b) noexcept
{
    const double high = a + b;
    const double b_virtual = high - a;
    const double a_virtual = high - b_virtual;
    const double b_roundoff = b - b_virtual;
    const double a_roundoff = a - a_virtual;
    return {a_roundoff + b_roundoff, high};
}

TwoTerm two_diff(const double a, const double b) noexcept
{
    const double high = a - b;
    const double b_virtual = a - high;
    const double a_virtual = high + b_virtual;
    const double b_roundoff = b_virtual - b;
    const double a_roundoff = a - a_virtual;
    return {a_roundoff + b_roundoff, high};
}

TwoTerm two_product(const double a, const double b) noexcept
{
    const double high = a * b;
    return {std::fma(a, b, -high), high};
}

std::vector<double> grow_expansion_zeroelim(
    const std::vector<double>& expansion,
    const double value)
{
    std::vector<double> result;
    result.reserve(expansion.size() + 1);

    double accumulator = value;
    for (const double component : expansion) {
        const TwoTerm sum = two_sum(accumulator, component);
        if (sum.low != 0.0) {
            result.push_back(sum.low);
        }
        accumulator = sum.high;
    }
    if (accumulator != 0.0) {
        result.push_back(accumulator);
    }
    return result;
}

std::vector<double> compress(const std::vector<double>& components)
{
    std::vector<double> result;
    result.reserve(components.size());
    for (const double component : components) {
        if (component != 0.0) {
            result = grow_expansion_zeroelim(result, component);
        }
    }
    return result;
}

std::vector<double> fast_expansion_sum_zeroelim(
    const std::vector<double>& left,
    const std::vector<double>& right)
{
    std::vector<double> result = left;
    result.reserve(left.size() + right.size());
    for (const double component : right) {
        result = grow_expansion_zeroelim(result, component);
    }
    return result;
}

std::vector<double> scale_expansion_zeroelim(
    const std::vector<double>& expansion,
    const double scale)
{
    if (scale == 0.0 || expansion.empty()) {
        return {};
    }

    std::vector<double> result;
    result.reserve(expansion.size() * 2);
    for (const double component : expansion) {
        const TwoTerm product = two_product(component, scale);
        if (product.low != 0.0) {
            result = grow_expansion_zeroelim(result, product.low);
        }
        if (product.high != 0.0) {
            result = grow_expansion_zeroelim(result, product.high);
        }
    }
    return result;
}

}

Expansion::Expansion(const float exact_value)
{
    if (exact_value != 0.0F) {
        components_.push_back(exact_value);
    }
}

Expansion::Expansion(std::vector<double> components)
    : components_(compress(components))
{
}

bool Expansion::is_zero() const noexcept
{
    return components_.empty();
}

Sign Expansion::sign() const noexcept
{
    if (components_.empty()) {
        return Sign::zero;
    }
    return components_.back() < 0.0 ? Sign::negative : Sign::positive;
}

bool operator==(const Expansion& left, const Expansion& right)
{
    return (left - right).is_zero();
}

Expansion operator+(const Expansion& left, const Expansion& right)
{
    return Expansion(fast_expansion_sum_zeroelim(
        left.components_,
        right.components_));
}

Expansion operator-(const Expansion& left, const Expansion& right)
{
    if (left.components_.size() <= 1 && right.components_.size() <= 1) {
        const double left_value = left.components_.empty()
            ? 0.0 : left.components_.front();
        const double right_value = right.components_.empty()
            ? 0.0 : right.components_.front();
        const TwoTerm difference = two_diff(left_value, right_value);
        std::vector<double> result;
        result.reserve(2);
        if (difference.low != 0.0) {
            result.push_back(difference.low);
        }
        if (difference.high != 0.0) {
            result.push_back(difference.high);
        }
        return Expansion(std::move(result));
    }
    return left + (-right);
}

Expansion operator-(const Expansion& value)
{
    std::vector<double> result;
    result.reserve(value.components_.size());
    for (const double component : value.components_) {
        result.push_back(-component);
    }
    return Expansion(std::move(result));
}

Expansion operator*(const Expansion& left, const Expansion& right)
{
    std::vector<double> result;
    result.reserve(left.components_.size() * right.components_.size() * 2);
    for (const double component : right.components_) {
        result = fast_expansion_sum_zeroelim(
            result,
            scale_expansion_zeroelim(left.components_, component));
    }
    return Expansion(std::move(result));
}

}
