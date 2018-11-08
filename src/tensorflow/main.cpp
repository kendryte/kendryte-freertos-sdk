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
#include "bitmap_helpers.h"
#include "get_top_n.h"
#include "tensorflow/lite/interpreter.h"
#include "tensorflow/lite/kernels/register.h"
#include "tensorflow/lite/model.h"
#include "tensorflow/lite/optional_debug_tools.h"
#include <devices.h>
#include <filesystem.h>
#include <memory>
#include <stdio.h>
#include <sys/time.h>
#include <vector>

using namespace tflite;
using namespace tflite::label_image;

#define TFLITE_MINIMAL_CHECK(x)                                  \
    if (!(x))                                                    \
    {                                                            \
        fprintf(stderr, "Error at %s:%d\n", __FILE__, __LINE__); \
        while (1)                                                \
            ;                                                    \
    }

extern const char mobilenet_v1_0_5_128_quant_tflite[];
extern unsigned int mobilenet_v1_0_5_128_quant_tflite_len;

extern const uint8_t grace_hopper_bmp[];
extern unsigned int grace_hopper_bmp_len;
extern const char *labels[];

int main()
{
    Settings s;

    int image_width = 128;
    int image_height = 128;
    int image_channels = 3;
    std::vector<uint8_t> in = read_bmp(grace_hopper_bmp, grace_hopper_bmp_len, &image_width, &image_height, &image_channels, &s);
    printf("image read\n");

    std::unique_ptr<tflite::FlatBufferModel> model = tflite::FlatBufferModel::BuildFromBuffer(mobilenet_v1_0_5_128_quant_tflite, mobilenet_v1_0_5_128_quant_tflite_len);
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
    printf("=== Pre-invoke Interpreter State ===\n");
    tflite::PrintInterpreterState(interpreter.get());

    // Fill input buffers
    int input = interpreter->inputs()[0];
    TfLiteIntArray *dims = interpreter->tensor(input)->dims;
    int wanted_height = dims->data[1];
    int wanted_width = dims->data[2];
    int wanted_channels = dims->data[3];

    memcpy(interpreter->typed_tensor<uint8_t>(input), in.data(), image_width * image_height * image_channels);

    printf("input loaded\n");

    timeval tv, tv2;
    gettimeofday(&tv, NULL);
    // Run inference
    TFLITE_MINIMAL_CHECK(interpreter->Invoke() == kTfLiteOk);
    gettimeofday(&tv2, NULL);
    printf("\n\n=== Post-invoke Interpreter State ===\n");
    tflite::PrintInterpreterState(interpreter.get());

    printf("Infer used %dms.\n", (int)((tv2.tv_sec * 1000 + tv2.tv_usec / 1e3) - (tv.tv_sec * 1000 + tv.tv_usec / 1e3)));

    const float threshold = 0.001f;

    std::vector<std::pair<float, int>> top_results;

    int output = interpreter->outputs()[0];
    TfLiteIntArray *output_dims = interpreter->tensor(output)->dims;
    // assume output dims to be something like (1, 1, ... ,size)
    auto output_size = output_dims->data[output_dims->size - 1];
    get_top_n<uint8_t>(interpreter->typed_output_tensor<uint8_t>(0),
        output_size, s.number_of_results, threshold,
        &top_results, false);

    printf("Top 5:\n");
    for (auto &p : top_results)
    {
        printf("%s: %f\n", labels[p.second], p.first);
    }

    while (1)
        ;
}
