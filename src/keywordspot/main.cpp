/* Copyright 2018 Canaan Inc.
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
#include <nncase/nncase.hpp>
#include <nncase/datatypes.hpp>
#include <nncase/model.hpp>
#include <nncase/layers/matmul.hpp>
#include <nncase/utils/mfcc.hpp>
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"
#include "tensorflow/lite/optional_debug_tools.h"
#define DR_WAV_IMPLEMENTATION
#include <nncase/utils/dr_wav.h>
#include <array>
#include <cassert>
#include <devices.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

using namespace tflite;

handle_t i2s0;
handle_t i2s2;
const audio_format_t audio = { AUDIO_FMT_PCM, 16, 16000, 2 };

void init_i2s(void)
{
    i2s_stop(i2s0);
    i2s_stop(i2s2);
    i2s_config_as_capture(i2s0, &audio, 1200, I2S_AM_STANDARD, 0x3);
    i2s_config_as_render(i2s2, &audio, 1200, I2S_AM_RIGHT, 0xc);
    i2s_start(i2s2);
    i2s_start(i2s0);
}

struct timeval get_time[2];

using namespace nncase;

constexpr uint32_t SampleRate = 16000;
constexpr uint32_t FftSize = 512;
constexpr uint32_t CepstrumCount = 26;
constexpr uint32_t ContextCount = 5;
constexpr uint32_t WindowSize = uint32_t(SampleRate * 0.025);
constexpr uint32_t StepSize = uint32_t(SampleRate * 0.01);
constexpr uint32_t FeatureNum = CepstrumCount + 2 * ContextCount * CepstrumCount;
constexpr float epsilon = 1e-3f;

struct wavdata
{
    std::unique_ptr<int16_t[]> signal;
    size_t sampleCount;
};

template<uint32_t align>
wavdata read_wav(const char *data, size_t data_size)
{
    drwav wav;
    if (!drwav_init_memory(&wav, data, data_size))
        throw "cannot open wav file.";
    if (wav.channels != 1 || wav.sampleRate != SampleRate)
        throw "invalid sample rate or channels.";

    const auto alignedSize = uint32_t((wav.totalSampleCount + align - 1) / align * align);
    auto waveData = std::make_unique<int16_t[]>(alignedSize);
    drwav_read_s16(&wav, wav.totalSampleCount, waveData.get());
    drwav_uninit(&wav);

    const auto n = alignedSize;
    // Normalize
    double mean = 0;
    for (size_t i = 0; i < n; i++)
        mean += waveData[i];
    mean /= n;

    double sd = 0;
    for (size_t i = 0; i < n; i++)
        sd += std::pow(i - mean, 2);
    sd = std::sqrt(sd / n);

    for (size_t i = 0; i < n; i++)
        waveData[i] = (waveData[i] - mean) / sd * 32767;

    return { std::move(waveData), alignedSize };
}

mfcc<float, SampleRate, WindowSize, CepstrumCount> s_mfcc(0.97);

static const char* s_labels[] =
{
    "blank", "#", "n", "i", "i2", "i3", "in2", "h", "ao", "zh", "i", "ai"
};

std::vector<std::array<float, FeatureNum>> fill_features(const wavdata& wav)
{
    std::vector<std::array<float, CepstrumCount>> originFeatures;
    std::vector<std::array<float, FeatureNum>> result;

    // 提取每2帧的原始特征
    {
        for (size_t i = 0; i < wav.sampleCount - WindowSize; i += StepSize * 2)
        {
            originFeatures.emplace_back();
            s_mfcc.transform(wav.signal.get() + i, originFeatures.back());
        }
    }

    const auto pastMinContext = 0 + int32_t(ContextCount);
    const auto futureMaxContext = int32_t(originFeatures.size()) - 1 - int32_t(ContextCount);
    for (int32_t timeSlice = 0; timeSlice < originFeatures.size(); timeSlice++)
    {
        result.emplace_back();
        auto& features = result.back();
        size_t idx = 0;

        // past
        const auto emptyPastNeed = std::max(0, pastMinContext - timeSlice);
        for (size_t i = 0; i < emptyPastNeed; i++)
        {
            for (size_t j = 0; j < CepstrumCount; j++)
                features[idx++] = 0;
        }

        for (int32_t i = std::max(0, timeSlice - int32_t(ContextCount)); i < timeSlice; i++)
        {
            const auto& src = originFeatures[i];
            for (size_t j = 0; j < CepstrumCount; j++)
                features[idx++] = src[j];
        }

        // now
        for (size_t j = 0; j < CepstrumCount; j++)
            features[idx++] = originFeatures[timeSlice][j];

        const auto emptyFutureNeed = std::max(0, timeSlice - futureMaxContext);
        for (int32_t i = timeSlice + 1; i < std::min(timeSlice + 1 + ContextCount, uint32_t(originFeatures.size())); i++)
        {
            const auto& src = originFeatures[i];
            for (size_t j = 0; j < CepstrumCount; j++)
                features[idx++] = src[j];
        }

        for (size_t i = 0; i < emptyFutureNeed; i++)
        {
            for (size_t j = 0; j < CepstrumCount; j++)
                features[idx++] = 0;
        }

        assert(idx == FeatureNum);
    }

    const auto n = result.size() * FeatureNum;
    // Normalize
    double mean = 0;
    for (auto&& f : result)
        for (auto&& i : f)
            mean += i;
    mean /= n;

    double sd = 0;
    for (auto&& f : result)
        for (auto&& i : f)
            sd += std::pow(i - mean, 2);
    sd = std::sqrt(sd / n);

    for (auto&& f : result)
        for (auto&& i : f)
            i = (i - mean) / sd;

    return result;
}

#define TFLITE_MINIMAL_CHECK(x)                                  \
    if (!(x))                                                    \
    {                                                            \
        fprintf(stderr, "Error at %s:%d\n", __FILE__, __LINE__); \
        while (1)                                                \
            ;                                                    \
    }

extern const char output_graph_tflite[];
extern unsigned int output_graph_tflite_len;

extern const char CHINO_01_wav[];
extern unsigned int CHINO_01_wav_len;

struct spot_ctx
{
    uint32_t last_ = 0;
    uint32_t score = 0;
    bool trigger = false;

    void feed(uint32_t id)
    {
        id = translate(id);
        if (last_ != id)
        {
            if (id)
            {
                add_score(id);
                //printf("[%s] ", s_labels[id]);

                last_ = id;
            }
        }
    }

    int not_ = 0;

    void add_score(uint32_t id)
    {
        if (id == 2)
        {
            not_ = 0;
            score |= 1;
        }
        if (id == 3)
        {
            not_ = 0;
            score |= 2;
        }
        if (id == 7)
        {
            not_ = 0;
            score |= 4;
        }
        if (id == 8)
        {
            not_ = 0;
            score |= 8;
        }
        if (id == 9)
        {
            not_ = 0;
            score |= 16;
        }
        if (id == 11)
        {
            not_ = 0;
            score |= 32;
        }
        else
        {
            if (not_++ == 2)
            {
                not_ = 0;
                score = 0;
            }
        }
        if (score == 0b111111)
        {
            //printf("hello\n");
            not_ = 0;
            score = 0;
            trigger = true;
        }
        else
        {
            trigger = false;
        }
    }

    uint32_t translate(uint32_t id)
    {
        switch (id)
        {
            // i
        case 3:
        case 4:
        case 5:
        case 6:
        case 10:
            return 3;
        default:
            break;
        }

        return id;
    }
};

int main(void)
{
    std::unique_ptr<tflite::FlatBufferModel> model = tflite::FlatBufferModel::BuildFromBuffer(output_graph_tflite, output_graph_tflite_len);
    TFLITE_MINIMAL_CHECK(model != nullptr);

    printf("model built\n");

    // Build the interpreter
    tflite::ops::builtin::BuiltinOpResolver resolver;
    InterpreterBuilder builder(*model.get(), resolver);
    std::unique_ptr<Interpreter> interpreter;
    builder(&interpreter, 1);
    printf("interpreter built\n");
    TFLITE_MINIMAL_CHECK(interpreter != nullptr);

    // Allocate tensor buffers.
    TFLITE_MINIMAL_CHECK(interpreter->AllocateTensors() == kTfLiteOk);

    // Fill input buffers
    int input = interpreter->inputs()[0];

#if 0
    auto wav = read_wav<WindowSize>(__8_wav, __8_wav_len);

    auto features = fill_features(wav);

    spot_ctx ctx;
    for (auto &frame : features)
    {
        std::copy(frame.begin(), frame.end(), interpreter->typed_tensor<float>(input));

        TFLITE_MINIMAL_CHECK(interpreter->Invoke() == kTfLiteOk);

        int output = interpreter->outputs()[0];
        auto cls = interpreter->typed_tensor<int64_t>(output)[0];

        ctx.feed((uint32_t)cls);
    }

    printf("\n");

#else
    i2s0 = io_open("/dev/i2s0");
    i2s2 = io_open("/dev/i2s2");

    configASSERT(i2s0);
    configASSERT(i2s2);

    init_i2s();

    uint8_t *buffer_snd = NULL;
    uint8_t *buffer_rcv = NULL;
    size_t frames_snd = 0;
    size_t frames_rcv = 0;

    spot_ctx ctx;
    bool trigger = false;
    drwav *wav = new drwav;
    configASSERT(drwav_init_memory(wav, CHINO_01_wav, CHINO_01_wav_len));

    while (1)
    {
        std::vector<std::array<float, FeatureNum>> features;

        i2s_get_buffer(i2s0, &buffer_rcv, &frames_rcv);
        i2s_get_buffer(i2s2, &buffer_snd, &frames_snd);

        {
            auto buffer = std::make_unique<int16_t[]>(frames_rcv);
            for (size_t i = 0; i < frames_rcv; i++)
                buffer[i] = reinterpret_cast<const int16_t*>(buffer_rcv)[i * 2 + 1];

            const auto n = frames_rcv;
            // Normalize
            double mean = 0;
            for (size_t i = 0; i < n; i++)
                mean += buffer[i];
            mean /= n;

            double sd = 0;
            for (size_t i = 0; i < n; i++)
                sd += std::pow(i - mean, 2);
            sd = std::sqrt(sd / n);

            for (size_t i = 0; i < n; i++)
                buffer[i] = (buffer[i] - mean) / sd * 32767;

            features = fill_features({ std::move(buffer), frames_rcv });
        }

        i2s_release_buffer(i2s0, frames_rcv);

        for (auto &frame : features)
        {
            std::copy(frame.begin(), frame.end(), interpreter->typed_tensor<float>(input));

            TFLITE_MINIMAL_CHECK(interpreter->Invoke() == kTfLiteOk);

            int output = interpreter->outputs()[0];
            auto cls = interpreter->typed_tensor<int64_t>(output)[0];

            ctx.feed((uint32_t)cls);
            if (ctx.trigger)
                trigger = true;
        }

        if (trigger)
        {
            printf("hello\n");

            drwav_read_s16(wav, frames_snd * 2, (drwav_int16*)buffer_snd);
            drwav_seek_to_first_sample(wav);
            trigger = false;
            printf("stop\n");
        }
        else
        {
            memset(buffer_snd, 0, 4 * frames_snd);
        }

        i2s_release_buffer(i2s2, frames_snd);

        //printf("\nframe used %dms.\n", (int)((tv2.tv_sec * 1000 + tv2.tv_usec / 1000) - (tv.tv_sec * 1000 + tv.tv_usec / 1000)));

    }
#endif
    while (1);
    return 0;
}
