/*
    Copyright 2025, 2026 iamlamprey

    This file is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This file is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with This file. If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <stdexcept>
#include <vector>

#include <xsimd/xsimd.hpp>

#include "json/json.hpp"

namespace Altar::DSP
{

namespace NAM
{

using FBatch = xsimd::batch<float>;

static constexpr int batchSize { static_cast<int>(FBatch::size) };

struct ManualModel
{
    virtual ~ManualModel() = default;
    virtual void prepare(int maximumBlockSize) = 0;
    virtual void reset() = 0;
    virtual float forward(float input) = 0;
    virtual void forward(const float* input, float* output, int numSamples) = 0;
    virtual const char* family() const = 0;
};

inline float fastTanh(float x) noexcept
{
    const float x2 { x * x };
    const float p { x * (1.0f + 0.183428244899f * x2) };

    return p / std::sqrt(1.0f + p * p);
}

inline FBatch batchTanh(const FBatch& x) noexcept
{
    const FBatch x2 { x * x };
    const FBatch p { x * (FBatch(1.0f) + FBatch(0.183428244899f) * x2) };

    return p / xsimd::sqrt(FBatch(1.0f) + p * p);
}

inline float simdDot(const float* a, const float* b, int n)
{
    FBatch sum { 0.0f };
    const int numBatches { n / batchSize };

    for (int v { 0 }; v < numBatches; ++v)
    {
        const int base { v * batchSize };
        sum += xsimd::load_unaligned(a + base) * xsimd::load_unaligned(b + base);
    }

    float dot { xsimd::reduce_add(sum) };

    for (int i { numBatches * batchSize }; i < n; ++i)
    {
        dot += a[i] * b[i];
    }

    return dot;
}

struct Dense
{
    int inSize { 0 };
    int outSize { 0 };
    bool hasBias { true };
    std::vector<float> weights;
    std::vector<float> bias;

    void init(int in, int out, bool useBias)
    {
        inSize = in;
        outSize = out;
        hasBias = useBias;
        weights.resize(static_cast<size_t>(out * in), 0.0f);
        bias.resize(static_cast<size_t>(out), 0.0f);
    }

    void setWeights(const float* w, int count)
    {
        std::copy(w, w + std::min(count, static_cast<int>(weights.size())), weights.begin());
    }

    void setBias(const float* b, int count)
    {
        std::copy(b, b + std::min(count, static_cast<int>(bias.size())), bias.begin());
    }

    void forward(const float* input, float* output)
    {
        for (int o { 0 }; o < outSize; ++o)
        {
            const float s { hasBias ? bias[static_cast<size_t>(o)] : 0.0f };
            output[o] = s + simdDot(weights.data() + o * inSize, input, inSize);
        }
    }
};

struct DilatedConv1D
{
    int channels { 0 };
    int kernelSize { 0 };
    int dilation { 0 };
    int stateSize { 0 };
    std::vector<float> weights;
    std::vector<float> bias;
    std::vector<float> state;
    int statePtr { 0 };

    void init(int ch, int ks, int dil)
    {
        channels = ch;
        kernelSize = ks;
        dilation = dil;
        stateSize = (ks - 1) * dil + 1;
        weights.resize(static_cast<size_t>(ch * ch * ks), 0.0f);
        bias.resize(static_cast<size_t>(ch), 0.0f);
        state.resize(static_cast<size_t>(stateSize * ch), 0.0f);
        statePtr = 0;
    }

    void reset()
    {
        std::fill(state.begin(), state.end(), 0.0f);
        statePtr = 0;
    }

    void setWeights(const float* w, int count)
    {
        for (int oc { 0 }; oc < channels && count > 0; ++oc)
        {
            for (int ic { 0 }; ic < channels && count > 0; ++ic)
            {
                const float w0 { *w++ };
                --count;

                const float w1 { count > 0 ? *w++ : 0.0f };
                --count;

                const float w2 { count > 0 ? *w++ : 0.0f };
                --count;

                weights[static_cast<size_t>((oc * kernelSize + 0) * channels + ic)] = w2;
                weights[static_cast<size_t>((oc * kernelSize + 1) * channels + ic)] = w1;
                weights[static_cast<size_t>((oc * kernelSize + 2) * channels + ic)] = w0;
            }
        }
    }

    void forward(const float* input, float* output)
    {
        float* dst { state.data() + statePtr * channels };
        std::copy(input, input + channels, dst);
        std::fill(output, output + channels, 0.0f);

        const int vectorChannels { channels / batchSize };

        for (int tap { 0 }; tap < kernelSize; ++tap)
        {
            const int idx { (statePtr + stateSize - tap * dilation) % stateSize };
            const float* st { state.data() + idx * channels };

            for (int oc { 0 }; oc < channels; ++oc)
            {
                const float* w { weights.data() + (oc * kernelSize + tap) * channels };
                FBatch sum { 0.0f };

                for (int v { 0 }; v < vectorChannels; ++v)
                {
                    const int base { v * batchSize };
                    sum += xsimd::load_unaligned(w + base) * xsimd::load_unaligned(st + base);
                }

                float dot { xsimd::reduce_add(sum) };

                for (int i { vectorChannels * batchSize }; i < channels; ++i)
                {
                    dot += w[i] * st[i];
                }

                output[oc] += dot;
            }
        }

        for (int oc { 0 }; oc < channels; ++oc)
        {
            output[oc] += bias[static_cast<size_t>(oc)];
        }

        statePtr = (statePtr + 1) % stateSize;
    }
};

struct WavenetLayer
{
    DilatedConv1D conv;
    Dense inputMixin;
    Dense conv1x1;
    int channels { 0 };

    void init(int ch, int cond, int ks, int dil)
    {
        channels = ch;
        conv.init(ch, ks, dil);
        inputMixin.init(cond, ch, false);
        conv1x1.init(ch, ch, true);
    }

    void reset()
    {
        conv.reset();
    }

    void forward(const float* input, const float* condition, float* head, float* output)
    {
        alignas(32) float convOut[16] {};
        alignas(32) float mixinOut[16] {};
        alignas(32) float activated[16] {};
        alignas(32) float temp[16] {};

        conv.forward(input, convOut);
        inputMixin.forward(condition, mixinOut);

        const int numBatches { channels / batchSize };

        for (int v { 0 }; v < numBatches; ++v)
        {
            const int base { v * batchSize };
            const FBatch s { xsimd::load_unaligned(convOut + base) + xsimd::load_unaligned(mixinOut + base) };
            const FBatch a { batchTanh(s) };

            xsimd::store_unaligned(head + base, xsimd::load_unaligned(head + base) + a);
            a.store_unaligned(activated + base);
        }

        for (int i { numBatches * batchSize }; i < channels; ++i)
        {
            const float a { fastTanh(convOut[i] + mixinOut[i]) };
            head[i] += a;
            activated[i] = a;
        }

        conv1x1.forward(activated, temp);

        for (int v { 0 }; v < numBatches; ++v)
        {
            const int base { v * batchSize };
            xsimd::store_unaligned(output + base, xsimd::load_unaligned(input + base) + xsimd::load_unaligned(temp + base));
        }

        for (int i { numBatches * batchSize }; i < channels; ++i)
        {
            output[i] = input[i] + temp[i];
        }
    }
};

struct WavenetLayerArray
{
    Dense rechannel;
    std::vector<WavenetLayer> layers;
    Dense headRechannel;
    int inSize { 0 };
    int channels { 0 };
    int headSize { 0 };

    void init(int in, int conditionSize, int head, int ch, int kernel, const int* dilations, int numDilations, bool hasHeadBias)
    {
        inSize = in;
        channels = ch;
        headSize = head;
        rechannel.init(in, ch, false);
        layers.resize(static_cast<size_t>(numDilations));

        for (int i { 0 }; i < numDilations; ++i)
        {
            layers[static_cast<size_t>(i)].init(ch, conditionSize, kernel, dilations[i]);
        }

        headRechannel.init(ch, head, hasHeadBias);
    }

    void reset()
    {
        for (auto& layer : layers)
        {
            layer.reset();
        }
    }

    void forward(const float* input, const float* condition, float* head, float* residual)
    {
        alignas(32) float temp[16] {};

        rechannel.forward(input, residual);

        const float* layerIn { residual };
        float* layerOut { temp };

        for (auto& layer : layers)
        {
            layer.forward(layerIn, condition, head, layerOut);

            if (layerOut == temp)
            {
                layerIn = temp;
                layerOut = residual;
            }
            else
            {
                layerIn = residual;
                layerOut = temp;
            }
        }

        if (layerIn != residual)
        {
            const float* src { layerIn == temp ? temp : residual };

            for (int i { 0 }; i < channels; ++i)
            {
                residual[i] = src[i];
            }
        }

        for (int i { 0 }; i < channels; ++i)
        {
            temp[i] = head[i];
        }

        headRechannel.forward(temp, head);
    }
};

struct ManualWavenet : ManualModel
{
    WavenetLayerArray arrays[2];
    float headScale { 1.0f };

    void load_weights(const nlohmann::json& j)
    {
        const nlohmann::json* weightsJson { nullptr };

        if (j.is_object() && j.contains("weights"))
        {
            weightsJson = &j.at("weights");
        }
        else if (j.is_array())
        {
            if (j.empty())
                throw std::runtime_error("NAM weights JSON array is empty.");

            if (j.front().is_number())
            {
                weightsJson = &j;
            }
            else
            {
                for (const auto& entry : j)
                {
                    if (entry.is_object() && entry.contains("weights"))
                    {
                        weightsJson = &entry.at("weights");
                        break;
                    }
                }
            }
        }

        if (weightsJson == nullptr || !weightsJson->is_array())
            throw std::runtime_error("NAM file does not contain a supported numeric weights array.");

        std::vector<float> w { weightsJson->get<std::vector<float>>() };
        auto it { w.begin() };
        const int dil0[] { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512 };
        const int dil1[] { 1, 2, 4, 8, 16, 32, 64, 128, 256, 512 };

        arrays[0].init(1, 1, 8, 16, 3, dil0, 10, false);
        arrays[1].init(16, 1, 1, 8, 3, dil1, 10, true);

        for (int a { 0 }; a < 2; ++a)
        {
            auto& arr { arrays[a] };

            for (int i { 0 }; i < arr.channels; ++i)
            {
                for (int k { 0 }; k < arr.inSize && it != w.end(); ++k)
                {
                    arr.rechannel.weights[static_cast<size_t>(i * arr.inSize + k)] = *it++;
                }
            }

            for (auto& layer : arr.layers)
            {
                layer.conv.setWeights(&*it, arr.channels * arr.channels * 3);
                it += arr.channels * arr.channels * 3;

                for (int i { 0 }; i < arr.channels; ++i) layer.conv.bias[static_cast<size_t>(i)] = *it++;
                for (int i { 0 }; i < arr.channels; ++i) layer.inputMixin.weights[static_cast<size_t>(i)] = *it++;

                for (int i { 0 }; i < arr.channels; ++i)
                {
                    for (int k { 0 }; k < arr.channels; ++k)
                    {
                        layer.conv1x1.weights[static_cast<size_t>(i * arr.channels + k)] = *it++;
                    }
                }

                for (int i { 0 }; i < arr.channels; ++i) layer.conv1x1.bias[static_cast<size_t>(i)] = *it++;
            }

            for (int i { 0 }; i < arr.headSize; ++i)
            {
                for (int k { 0 }; k < arr.channels; ++k)
                {
                    arr.headRechannel.weights[static_cast<size_t>(i * arr.channels + k)] = *it++;
                }
            }

            if (arr.headRechannel.hasBias)
            {
                for (int i { 0 }; i < arr.headSize; ++i)
                {
                    arr.headRechannel.bias[static_cast<size_t>(i)] = *it++;
                }
            }
        }

        headScale = *it++;
    }

    void prepare(int) override {}

    void reset() override
    {
        arrays[0].reset();
        arrays[1].reset();
    }

    float forward(float input) override
    {
        float condition[1] { 0.0f };
        float head0[16] {};
        float residual0[16] {};
        float in0[1] { input };

        arrays[0].forward(in0, condition, head0, residual0);

        float head1[8] {};
        float residual1[8] {};

        arrays[1].forward(residual0, condition, head1, residual1);

        return head1[0] * headScale;
    }

    void forward(const float* input, float* output, int numSamples) override
    {
        for (int n { 0 }; n < numSamples; ++n)
        {
            output[n] = forward(input[n]);
        }
    }

    const char* family() const override { return "A1 WaveNet"; }
};

template <int Channels>
struct ManualA2Wavenet : ManualModel
{
    static constexpr int numLayers { 23 };
    static constexpr int headKernelSize { 16 };
    static constexpr float leakySlope { 0.01f };
    static constexpr std::array<int, numLayers> kernelSizes {
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 15, 15, 6, 6, 6, 6, 6, 6, 6
    };
    static constexpr std::array<int, numLayers> dilations {
        1, 3, 7, 17, 41, 101, 239, 1, 3, 7, 17, 41, 101, 239, 1, 13, 1, 3, 7, 17, 41, 101, 239
    };

    struct Layer
    {
        int kernelSize { 0 };
        int dilation { 0 };
        int stateSize { 0 };
        int statePtr { 0 };
        std::vector<float> convWeights;
        std::array<float, Channels> convBias {};
        std::array<float, Channels> mixinWeights {};
        std::array<float, Channels * Channels> layer1x1Weights {};
        std::array<float, Channels> layer1x1Bias {};
        std::vector<float> state;
    };

    std::array<float, Channels> rechannelWeights {};
    std::array<Layer, numLayers> layers;
    std::array<std::array<float, Channels>, headKernelSize> headWeights {};
    float headBias { 0.0f };
    float headScale { 1.0f };
    std::vector<float> headState;
    int headStatePtr { 0 };
    std::vector<float> layerValues;
    std::vector<float> headSum;
    std::vector<float> activated;
    std::vector<float> nextValues;

    ManualA2Wavenet()
    {
        for (int i { 0 }; i < numLayers; ++i)
        {
            auto& layer { layers[static_cast<size_t>(i)] };
            layer.kernelSize = kernelSizes[static_cast<size_t>(i)];
            layer.dilation = dilations[static_cast<size_t>(i)];
            layer.stateSize = (layer.kernelSize - 1) * layer.dilation + 1;
            layer.convWeights.resize(static_cast<size_t>(layer.kernelSize * Channels * Channels), 0.0f);
            layer.state.resize(static_cast<size_t>(layer.stateSize * Channels), 0.0f);
        }

        headState.resize(static_cast<size_t>(headKernelSize * Channels), 0.0f);
        layerValues.resize(static_cast<size_t>(Channels), 0.0f);
        headSum.resize(static_cast<size_t>(Channels), 0.0f);
        activated.resize(static_cast<size_t>(Channels), 0.0f);
        nextValues.resize(static_cast<size_t>(Channels), 0.0f);
    }

    void load_weights(const nlohmann::json& j)
    {
        const auto& weightsJson { j.at("weights") };
        if (!weightsJson.is_array())
            throw std::runtime_error("A2 NAM file does not contain a numeric weights array.");

        std::vector<float> weights { weightsJson.get<std::vector<float>>() };
        auto it { weights.begin() };
        const auto end { weights.end() };

        auto take = [&]() -> float
        {
            if (it == end)
                throw std::runtime_error("A2 NAM weight stream ended before the model was complete.");
            return *it++;
        };

        for (int c { 0 }; c < Channels; ++c)
            rechannelWeights[static_cast<size_t>(c)] = take();

        for (int layerIndex { 0 }; layerIndex < numLayers; ++layerIndex)
        {
            auto& layer { layers[static_cast<size_t>(layerIndex)] };

            for (int oc { 0 }; oc < Channels; ++oc)
            {
                for (int ic { 0 }; ic < Channels; ++ic)
                {
                    for (int k { 0 }; k < layer.kernelSize; ++k)
                    {
                        layer.convWeights[static_cast<size_t>((k * Channels + ic) * Channels + oc)] = take();
                    }
                }
            }

            for (int c { 0 }; c < Channels; ++c)
                layer.convBias[static_cast<size_t>(c)] = take();

            for (int c { 0 }; c < Channels; ++c)
                layer.mixinWeights[static_cast<size_t>(c)] = take();

            for (int oc { 0 }; oc < Channels; ++oc)
            {
                for (int ic { 0 }; ic < Channels; ++ic)
                {
                    layer.layer1x1Weights[static_cast<size_t>(ic * Channels + oc)] = take();
                }
            }

            for (int c { 0 }; c < Channels; ++c)
                layer.layer1x1Bias[static_cast<size_t>(c)] = take();
        }

        for (int c { 0 }; c < Channels; ++c)
        {
            for (int k { 0 }; k < headKernelSize; ++k)
            {
                headWeights[static_cast<size_t>(k)][static_cast<size_t>(c)] = take();
            }
        }

        headBias = take();
        headScale = take();

        if (it != end)
            throw std::runtime_error("A2 NAM weight stream contains unexpected trailing values.");
    }

    void prepare(int) override {}

    void reset() override
    {
        for (auto& layer : layers)
        {
            std::fill(layer.state.begin(), layer.state.end(), 0.0f);
            layer.statePtr = 0;
        }

        std::fill(headState.begin(), headState.end(), 0.0f);
        headStatePtr = 0;
    }

    float forward(float input) override
    {
        for (int c { 0 }; c < Channels; ++c)
        {
            layerValues[static_cast<size_t>(c)] = rechannelWeights[static_cast<size_t>(c)] * input;
            headSum[static_cast<size_t>(c)] = 0.0f;
        }

        for (auto& layer : layers)
        {
            std::copy(layerValues.begin(), layerValues.end(), layer.state.begin() + static_cast<size_t>(layer.statePtr * Channels));

            for (int oc { 0 }; oc < Channels; ++oc)
            {
                float sum { layer.convBias[static_cast<size_t>(oc)] + layer.mixinWeights[static_cast<size_t>(oc)] * input };

                for (int k { 0 }; k < layer.kernelSize; ++k)
                {
                    const int stateIndex { (layer.statePtr + layer.stateSize - (layer.kernelSize - 1 - k) * layer.dilation) % layer.stateSize };
                    const float* stateValues { layer.state.data() + static_cast<size_t>(stateIndex * Channels) };

                    for (int ic { 0 }; ic < Channels; ++ic)
                    {
                        const size_t weightIndex { static_cast<size_t>((k * Channels + ic) * Channels + oc) };
                        sum += layer.convWeights[weightIndex] * stateValues[ic];
                    }
                }

                const float value { sum >= 0.0f ? sum : sum * leakySlope };
                activated[static_cast<size_t>(oc)] = value;
                headSum[static_cast<size_t>(oc)] += value;
            }

            for (int oc { 0 }; oc < Channels; ++oc)
            {
                float sum { layerValues[static_cast<size_t>(oc)] + layer.layer1x1Bias[static_cast<size_t>(oc)] };

                for (int ic { 0 }; ic < Channels; ++ic)
                {
                    sum += layer.layer1x1Weights[static_cast<size_t>(ic * Channels + oc)] * activated[static_cast<size_t>(ic)];
                }

                nextValues[static_cast<size_t>(oc)] = sum;
            }

            std::copy(nextValues.begin(), nextValues.end(), layerValues.begin());
            layer.statePtr = (layer.statePtr + 1) % layer.stateSize;
        }

        std::copy(headSum.begin(), headSum.end(), headState.begin() + static_cast<size_t>(headStatePtr * Channels));

        float output { headBias };
        for (int k { 0 }; k < headKernelSize; ++k)
        {
            const int stateIndex { (headStatePtr + headKernelSize - (headKernelSize - 1 - k)) % headKernelSize };
            const float* stateValues { headState.data() + static_cast<size_t>(stateIndex * Channels) };

            for (int c { 0 }; c < Channels; ++c)
                output += headWeights[static_cast<size_t>(k)][static_cast<size_t>(c)] * stateValues[c];
        }

        headStatePtr = (headStatePtr + 1) % headKernelSize;
        return output * headScale;
    }

    void forward(const float* input, float* output, int numSamples) override
    {
        for (int n { 0 }; n < numSamples; ++n)
            output[n] = forward(input[n]);
    }

    const char* family() const override
    {
        if constexpr (Channels == 3)
            return "A2 WaveNet 3ch";
        else
            return "A2 WaveNet 8ch";
    }
};

struct LoadedManualModel
{
    std::shared_ptr<ManualModel> model;
    double sampleRate { 48000.0 };
    std::string family;
};

inline double readSampleRate(const nlohmann::json& modelJson)
{
    if (modelJson.is_object() && modelJson.contains("sample_rate") && modelJson.at("sample_rate").is_number())
        return modelJson.at("sample_rate").get<double>();

    return 48000.0;
}

inline bool isA1WavenetShape(const nlohmann::json& modelJson)
{
    if (!modelJson.is_object() || modelJson.value("architecture", std::string()) != "WaveNet")
        return false;

    const auto& config { modelJson.at("config") };
    if (!config.is_object() || !config.contains("layers") || !config.at("layers").is_array() || config.at("layers").size() != 2)
        return false;

    const int expectedChannels[] { 16, 8 };
    const int expectedInput[] { 1, 16 };

    for (int i { 0 }; i < 2; ++i)
    {
        const auto& layer { config.at("layers").at(static_cast<size_t>(i)) };
        if (layer.value("input_size", 0) != expectedInput[i] || layer.value("condition_size", 0) != 1 || layer.value("channels", 0) != expectedChannels[i])
            return false;
        if (layer.value("kernel_size", 0) != 3 || layer.value("activation", std::string()) != "Tanh")
            return false;
        if (!layer.contains("dilations") || !layer.at("dilations").is_array() || layer.at("dilations").size() != 10)
            return false;
    }

    return true;
}

inline bool allStringsEqual(const nlohmann::json& array, const std::string& expected)
{
    if (!array.is_array())
        return false;

    for (const auto& entry : array)
        if (!entry.is_string() || entry.get<std::string>() != expected)
            return false;

    return true;
}

inline bool allNull(const nlohmann::json& array)
{
    if (!array.is_array())
        return false;

    for (const auto& entry : array)
        if (!entry.is_null())
            return false;

    return true;
}

inline bool isInactiveFilm(const nlohmann::json& layer, const char* key)
{
    if (!layer.contains(key) || layer.at(key).is_null())
        return true;
    if (layer.at(key).is_boolean())
        return !layer.at(key).get<bool>();
    if (layer.at(key).is_object())
        return !layer.at(key).value("active", false);
    return false;
}

inline bool isA2WavenetShape(const nlohmann::json& modelJson, int& channels)
{
    if (!modelJson.is_object() || modelJson.value("architecture", std::string()) != "WaveNet")
        return false;

    const auto& config { modelJson.at("config") };
    if (!config.is_object() || !config.contains("layers") || !config.at("layers").is_array() || config.at("layers").size() != 1)
        return false;
    if (config.contains("head") && !config.at("head").is_null())
        return false;
    if (!config.contains("head_scale") || !config.at("head_scale").is_number())
        return false;
    if (config.value("in_channels", 1) != 1)
        return false;

    const auto& layer { config.at("layers").front() };
    if (layer.value("input_size", 0) != 1 || layer.value("condition_size", 0) != 1)
        return false;

    channels = layer.value("channels", 0);
    if (layer.value("bottleneck", channels) != channels || (channels != 3 && channels != 8))
        return false;

    if (!layer.contains("kernel_sizes") || !layer.contains("dilations"))
        return false;

    const auto& kernelSizesJson { layer.at("kernel_sizes") };
    const auto& dilationsJson { layer.at("dilations") };
    if (!kernelSizesJson.is_array() || !dilationsJson.is_array() || kernelSizesJson.size() != ManualA2Wavenet<3>::numLayers || dilationsJson.size() != ManualA2Wavenet<3>::numLayers)
        return false;

    for (int i { 0 }; i < ManualA2Wavenet<3>::numLayers; ++i)
    {
        if (kernelSizesJson.at(static_cast<size_t>(i)).get<int>() != ManualA2Wavenet<3>::kernelSizes[static_cast<size_t>(i)])
            return false;
        if (dilationsJson.at(static_cast<size_t>(i)).get<int>() != ManualA2Wavenet<3>::dilations[static_cast<size_t>(i)])
            return false;
    }

    if (!layer.contains("activation"))
        return false;

    const auto& activation { layer.at("activation") };
    if (!activation.is_array() || activation.size() != ManualA2Wavenet<3>::numLayers)
        return false;

    for (const auto& entry : activation)
    {
        if (!entry.is_object() || entry.value("type", std::string()) != "LeakyReLU")
            return false;
        if (std::abs(entry.value("negative_slope", 0.0f) - ManualA2Wavenet<3>::leakySlope) > 1.0e-7f)
            return false;
    }

    if (layer.contains("gating_mode") && !layer.at("gating_mode").is_null() && (!allStringsEqual(layer.at("gating_mode"), "none") || layer.at("gating_mode").size() != ManualA2Wavenet<3>::numLayers))
        return false;
    if (layer.contains("secondary_activation") && !layer.at("secondary_activation").is_null() && (!allNull(layer.at("secondary_activation")) || layer.at("secondary_activation").size() != ManualA2Wavenet<3>::numLayers))
        return false;

    if (!layer.contains("layer1x1") || !layer.at("layer1x1").is_object() || !layer.at("layer1x1").value("active", false) || layer.at("layer1x1").value("groups", 1) != 1)
        return false;
    if (layer.contains("head1x1") && layer.at("head1x1").is_object() && layer.at("head1x1").value("active", false))
        return false;

    if (!layer.contains("head") || !layer.at("head").is_object())
        return false;
    if (layer.at("head").value("out_channels", 0) != 1 || layer.at("head").value("kernel_size", 0) != ManualA2Wavenet<3>::headKernelSize || !layer.at("head").value("bias", false))
        return false;

    for (const char* key : { "conv_pre_film", "conv_post_film", "input_mixin_pre_film", "input_mixin_post_film", "activation_pre_film", "activation_post_film", "layer1x1_post_film", "head1x1_post_film" })
    {
        if (!isInactiveFilm(layer, key))
            return false;
    }

    if (layer.value("groups_input", 1) != 1 || layer.value("groups_input_mixin", 1) != 1)
        return false;
    if (layer.contains("slimmable") && !layer.at("slimmable").is_null())
        return false;

    return true;
}

inline LoadedManualModel createManualModel(const nlohmann::json& modelJson);

inline LoadedManualModel createFromSlimmableContainer(const nlohmann::json& modelJson)
{
    const auto& submodels { modelJson.at("config").at("submodels") };
    if (!submodels.is_array() || submodels.empty())
        throw std::runtime_error("SlimmableContainer has no submodels.");

    nlohmann::json selectedModel;
    bool hasSelectedModel { false };
    double selectedMaxValue { -1.0 };

    for (const auto& entry : submodels)
    {
        if (!entry.is_object() || !entry.contains("model"))
            continue;

        const double maxValue { entry.at("max_value").get<double>() };
        if (!hasSelectedModel || maxValue > selectedMaxValue)
        {
            selectedMaxValue = maxValue;
            selectedModel = entry.at("model");
            hasSelectedModel = true;
        }
    }

    if (!hasSelectedModel)
        throw std::runtime_error("SlimmableContainer did not contain a selectable submodel.");

    if (!selectedModel.is_object())
        throw std::runtime_error("SlimmableContainer selected submodel is not a JSON object.");

    LoadedManualModel loaded { createManualModel(selectedModel) };
    loaded.family = "SlimmableContainer -> " + loaded.family;
    return loaded;
}

inline LoadedManualModel createManualModel(const nlohmann::json& modelJson)
{
    if (!modelJson.is_object())
        throw std::runtime_error("NAM file root must be a JSON object.");

    const std::string architecture { modelJson.value("architecture", std::string()) };

    if (architecture == "SlimmableContainer")
        return createFromSlimmableContainer(modelJson);

    if (architecture != "WaveNet")
        throw std::runtime_error("Unsupported NAM architecture: " + architecture);

    LoadedManualModel loaded;
    loaded.sampleRate = readSampleRate(modelJson);

    int a2Channels { 0 };
    if (isA2WavenetShape(modelJson, a2Channels))
    {
        if (a2Channels == 3)
        {
            auto model { std::make_shared<ManualA2Wavenet<3>>() };
            model->load_weights(modelJson);
            loaded.model = std::move(model);
        }
        else
        {
            auto model { std::make_shared<ManualA2Wavenet<8>>() };
            model->load_weights(modelJson);
            loaded.model = std::move(model);
        }

        loaded.family = loaded.model->family();
        return loaded;
    }

    if (isA1WavenetShape(modelJson))
    {
        auto model { std::make_shared<ManualWavenet>() };
        model->load_weights(modelJson);
        loaded.model = std::move(model);
        loaded.family = loaded.model->family();
        return loaded;
    }

    throw std::runtime_error("Unsupported WaveNet shape. Altar supports A1 WaveNet, strict A2 WaveNet 3ch/8ch, and SlimmableContainer files that contain those models.");
}

} // namespace NAM

} // namespace Altar::DSP
