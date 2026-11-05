/*
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 3
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the license, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <QTest>
#include <opencv2/imgproc.hpp>

#include "adapters/imageadapter.h"
#include "decoders/rawdecoder.h"
#include "decoders/scoresdecoder.h"
#include "ortengine.h"
#include "recipe.h"
#include "units.h"

using namespace OnnxInf;
namespace fs = std::filesystem;

// QVERIFY2 evaluates its message even on success, which must not touch error() then
#define VERIFY_OK(expected)              \
    do {                                 \
        const auto &res_ = (expected);   \
        if (!res_.has_value())           \
            QFAIL(res_.error().c_str()); \
    } while (false)

namespace
{

fs::path testModel(const char *name)
{
    return fs::path(ONNXINFER_TEST_MODEL_DIR) / name;
}

TensorInfo imageInput(std::vector<int64_t> shape, DType dtype = DType::Float32)
{
    return TensorInfo{.name = "images", .dtype = dtype, .shape = std::move(shape), .dimNames = {}};
}

ParamSet imageParams(std::initializer_list<std::pair<std::string, ParamValue>> values)
{
    ParamSet params(ImageAdapter::paramSpecs());
    for (const auto &[key, value] : values) {
        const auto r = params.set(key, value);
        if (!r)
            qFatal("Invalid test parameter: %s", r.error().c_str());
    }
    return params;
}

/**
 * Number in a table cell. std::stod would follow the C locale QtTest sets up.
 */
double cellValue(const std::string &cell)
{
    double value = std::nan("");
    std::from_chars(cell.data(), cell.data() + cell.size(), value);
    return value;
}

/**
 * Value of the float NCHW tensor at channel c, row y, column x.
 */
float nchwAt(const Tensor &t, int c, int y, int x)
{
    const auto &s = t.shape();
    return t.dataAs<float>()[(c * s[2] + y) * s[3] + x];
}

} // namespace

class TestOnnxInference : public QObject
{
    Q_OBJECT
private slots:
    void paramSetToml()
    {
        ParamSet params(ImageAdapter::paramSpecs());
        QCOMPARE(params.getString("layout"), std::string("nchw"));
        QCOMPARE(params.getSize("size"), (std::vector<int64_t>{640, 640}));

        auto tab = toml::parse(R"(
            layout = "nhwc"
            size = [320, 240]
            pad_value = 0
            mean = [0.5, 1]
            color = "purple"
            resize = 7
            bogus = true
        )");
        const auto warnings = params.loadToml(tab);
        QCOMPARE(warnings.size(), 3U);
        QCOMPARE(params.getString("layout"), std::string("nhwc"));
        QCOMPARE(params.getSize("size"), (std::vector<int64_t>{320, 240}));
        QCOMPARE(params.getInt("pad_value"), 0);
        QCOMPARE(params.getFloatList("mean"), (std::vector<double>{0.5, 1.0}));
        // invalid values keep their previous value
        QCOMPARE(params.getString("color"), std::string("rgb"));
        QCOMPARE(params.getString("resize"), std::string("letterbox"));

        // out-of-range values are refused
        QVERIFY(!params.set("pad_value", int64_t{300}).has_value());
        QVERIFY(!params.set("size", std::vector<int64_t>{0, 10}).has_value());
        QVERIFY(!params.set("layout", std::string("nchwx")).has_value());

        toml::table out;
        params.writeToml(out);
        ParamSet reloaded(ImageAdapter::paramSpecs());
        QVERIFY(reloaded.loadToml(out).empty());
        QVERIFY(reloaded == params);
    }

    void recipeRoundtrip()
    {
        Recipe recipe;
        recipe.family = "generic";
        recipe.modelFile = "models/classifier.onnx";
        QVERIFY(recipe.setDecoder("scores"));
        QVERIFY(recipe.output.set("activation", std::string("softmax")).has_value());
        QVERIFY(recipe.output.set("labels", std::vector<std::string>{"cat", "dog"}).has_value());
        QVERIFY(recipe.input.set("resize", std::string("crop")).has_value());

        const auto text = recipeToString(recipe);
        const auto res = recipeFromToml(toml::parse(text));
        VERIFY_OK(res);
        QVERIFY(res->warnings.empty());
        QVERIFY(res->recipe == recipe);

        // switching the decoder resets its parameters
        auto other = res->recipe;
        QVERIFY(other.setDecoder("raw"));
        QCOMPARE(other.output.getString("tensor"), std::string());
        QVERIFY(!other.setDecoder("nonexistent"));

        const auto bad = recipeFromToml(toml::parse("[input]\nadapter = \"lidar\"\n"));
        QVERIFY(!bad.has_value());

        const auto warn = recipeFromToml(toml::parse("[model]\ncolour = 1\n[output]\ndecoder = \"raw\"\nfoo = 2\n"));
        QVERIFY(warn.has_value());
        QCOMPARE(warn->warnings.size(), 2U);
    }

    void recipeSidecarPath()
    {
        QCOMPARE(sidecarRecipePath("/data/yolo11n.onnx"), fs::path("/data/yolo11n.recipe.toml"));
    }

    void recipeSkeletonFromShapes()
    {
        ModelInfo nchw;
        nchw.inputs = {imageInput({1, 3, 224, 320})};
        nchw.outputs = {
            TensorInfo{.name = "logits", .dtype = DType::Float32, .shape = {1, 10}, .dimNames = {}}
        };
        auto r = recipeSkeleton(nchw);
        QCOMPARE(r.adapter, std::string("image"));
        QCOMPARE(r.input.getString("layout"), std::string("nchw"));
        QCOMPARE(r.input.getString("color"), std::string("rgb"));
        QCOMPARE(r.input.getSize("size"), (std::vector<int64_t>{320, 224}));
        QCOMPARE(r.input.getString("normalize"), std::string("0-1"));
        QCOMPARE(r.decoder, std::string("raw"));
        QCOMPARE(r.output.getString("tensor"), std::string("logits"));

        ModelInfo nhwc;
        nhwc.inputs = {imageInput({1, 48, 64, 1}, DType::UInt8)};
        r = recipeSkeleton(nhwc);
        QCOMPARE(r.input.getString("layout"), std::string("nhwc"));
        QCOMPARE(r.input.getString("color"), std::string("gray"));
        QCOMPARE(r.input.getSize("size"), (std::vector<int64_t>{64, 48}));
        QCOMPARE(r.input.getString("normalize"), std::string("none"));

        // dynamic dimensions keep the default size
        ModelInfo dyn;
        dyn.inputs = {imageInput({-1, 3, -1, -1})};
        r = recipeSkeleton(dyn);
        QCOMPARE(r.input.getSize("size"), (std::vector<int64_t>{640, 640}));
    }

    void imageAdapterLetterbox()
    {
        ImageAdapter adapter(imageParams({}));
        QVERIFY(adapter.configure(imageInput({1, 3, 64, 64})).has_value());
        QCOMPARE(adapter.inputShape(), (std::vector<int64_t>{1, 3, 64, 64}));

        // 200x100 BGR frame: left half pure blue, right half pure red
        cv::Mat frame(100, 200, CV_8UC3, cv::Scalar(255, 0, 0));
        frame(cv::Rect(100, 0, 100, 100)).setTo(cv::Scalar(0, 0, 255));

        Tensor t;
        ImageGeometry geom;
        const auto r = adapter.process(frame, t, geom);
        VERIFY_OK(r);
        QCOMPARE(t.shape(), (std::vector<int64_t>{1, 3, 64, 64}));

        // scaled to 64x32, centered vertically
        QCOMPARE(geom.scaleX, 0.32);
        QCOMPARE(geom.scaleY, 0.32);
        QCOMPARE(geom.padX, 0.0);
        QCOMPARE(geom.padY, 16.0);
        QCOMPARE(geom.toSourceX(32.0), 100.0);
        QCOMPARE(geom.toSourceY(16.0), 0.0);
        QCOMPARE(geom.toSourceY(48.0), 100.0);

        // padding rows are 114 gray in all channels
        for (int c = 0; c < 3; ++c) {
            QCOMPARE(nchwAt(t, c, 0, 10), 114.0f / 255.0f);
            QCOMPARE(nchwAt(t, c, 63, 50), 114.0f / 255.0f);
        }
        // channel 0 is red in RGB order: the right half
        QCOMPARE(nchwAt(t, 0, 32, 10), 0.0f);
        QCOMPARE(nchwAt(t, 2, 32, 10), 1.0f);
        QCOMPARE(nchwAt(t, 0, 32, 54), 1.0f);
        QCOMPARE(nchwAt(t, 2, 32, 54), 0.0f);
    }

    void imageAdapterStretchCrop()
    {
        cv::Mat frame(100, 200, CV_8UC3, cv::Scalar(10, 20, 30));
        Tensor t;
        ImageGeometry geom;

        ImageAdapter stretch(imageParams({
            {"resize", std::string("stretch")}
        }));
        QVERIFY(stretch.configure(imageInput({1, 3, 50, 50})).has_value());
        QVERIFY(stretch.process(frame, t, geom).has_value());
        QCOMPARE(geom.toSourceX(50.0), 200.0);
        QCOMPARE(geom.toSourceY(50.0), 100.0);

        // a center crop of 100x100 out of 200x100, scaled by 0.5
        ImageAdapter crop(imageParams({
            {"resize", std::string("crop")}
        }));
        QVERIFY(crop.configure(imageInput({1, 3, 50, 50})).has_value());
        QVERIFY(crop.process(frame, t, geom).has_value());
        QCOMPARE(geom.scaleX, 0.5);
        QCOMPARE(geom.toSourceX(0.0), 50.0);
        QCOMPARE(geom.toSourceX(50.0), 150.0);
        QCOMPARE(geom.toSourceY(25.0), 50.0);

        // no resizing requires matching sizes
        ImageAdapter none(imageParams({
            {"resize", std::string("none")}
        }));
        QVERIFY(none.configure(imageInput({1, 3, 50, 50})).has_value());
        QVERIFY(!none.process(frame, t, geom).has_value());

        // with dynamic dims, "none" takes the stream's frame size
        QVERIFY(none.configure(imageInput({1, 3, -1, -1}), cv::Size(200, 100)).has_value());
        QCOMPARE(none.inputShape(), (std::vector<int64_t>{1, 3, 100, 200}));
        QVERIFY(none.process(frame, t, geom).has_value());
        QCOMPARE(geom.scaleX, 1.0);
    }

    void imageAdapterNormalization()
    {
        cv::Mat frame(8, 8, CV_8UC3, cv::Scalar(0, 128, 255)); // BGR

        // raw values, BGR order kept, channels last
        ImageAdapter raw(imageParams({
            {"normalize", std::string("none")},
            {"color",     std::string("bgr") },
            {"layout",    std::string("nhwc")}
        }));
        QVERIFY(raw.configure(imageInput({1, 8, 8, 3})).has_value());
        Tensor t;
        ImageGeometry geom;
        QVERIFY(raw.process(frame, t, geom).has_value());
        QCOMPARE(t.dataAs<float>()[0], 0.0f);
        QCOMPARE(t.dataAs<float>()[1], 128.0f);
        QCOMPARE(t.dataAs<float>()[2], 255.0f);

        // mean-std in RGB order: (x/255 - mean) / std
        ImageAdapter ms(imageParams({
            {"normalize", std::string("mean-std")            },
            {"mean",      std::vector<double>{0.5}           },
            {"std",       std::vector<double>{0.25, 0.5, 1.0}}
        }));
        QVERIFY(ms.configure(imageInput({1, 3, 8, 8})).has_value());
        QVERIFY(ms.process(frame, t, geom).has_value());
        QVERIFY(std::abs(nchwAt(t, 0, 3, 3) - (1.0f - 0.5f) / 0.25f) < 1e-5);
        QVERIFY(std::abs(nchwAt(t, 1, 3, 3) - (128.0f / 255.0f - 0.5f) / 0.5f) < 1e-5);
        QVERIFY(std::abs(nchwAt(t, 2, 3, 3) - (0.0f - 0.5f) / 1.0f) < 1e-5);

        // a 12-bit camera frame in 16-bit storage, fed to a uint8 grayscale model
        cv::Mat frame16(8, 8, CV_16UC1, cv::Scalar(4095));
        frame16.at<uint16_t>(0, 0) = 0;
        ImageAdapter u8(imageParams({
            {"normalize", std::string("none")},
            {"color",     std::string("gray")},
            {"layout",    std::string("nhwc")},
            {"bit_depth", int64_t{12}        }
        }));
        QVERIFY(u8.configure(imageInput({1, 8, 8, 1}, DType::UInt8)).has_value());
        QVERIFY(u8.process(frame16, t, geom).has_value());
        QCOMPARE(t.dtype(), DType::UInt8);
        QCOMPARE(int(t.dataAs<uint8_t>()[0]), 0);
        QCOMPARE(int(t.dataAs<uint8_t>()[1]), 255);
    }

    void imageAdapterConfigErrors()
    {
        ImageAdapter adapter(imageParams({}));
        QVERIFY(!adapter.configure(imageInput({1, 3, 64})).has_value());
        QVERIFY(!adapter.configure(imageInput({4, 3, 64, 64})).has_value());
        QVERIFY(!adapter.configure(imageInput({1, 1, 64, 64})).has_value());
        QVERIFY(!adapter.configure(imageInput({1, 3, 64, 64}, DType::Int64)).has_value());
        // uint8 inputs need raw values
        QVERIFY(!adapter.configure(imageInput({1, 3, 64, 64}, DType::UInt8)).has_value());
        // wrong layout: the channel dimension would be 64
        QVERIFY(!adapter.configure(imageInput({1, 64, 64, 3})).has_value());

        // static dims win over the recipe size, with a note
        QVERIFY(adapter.configure(imageInput({1, 3, 32, 48})).has_value());
        QCOMPARE(adapter.inputShape(), (std::vector<int64_t>{1, 3, 32, 48}));
        QCOMPARE(adapter.notes().size(), 1U);

        // dynamic dims come from the recipe
        ImageAdapter dyn(imageParams({
            {"size", std::vector<int64_t>{96, 64}}
        }));
        QVERIFY(dyn.configure(imageInput({-1, 3, -1, -1})).has_value());
        QCOMPARE(dyn.inputShape(), (std::vector<int64_t>{1, 3, 64, 96}));
        QVERIFY(dyn.notes().empty());
    }

    void rawDecoder()
    {
        Tensor out(DType::Float32, {1, 3});
        out.dataAs<float>()[0] = 0.5f;
        out.dataAs<float>()[1] = -2.0f;
        out.dataAs<float>()[2] = 1e-8f;
        TensorSet set{.tensors = {out.view("y")}, .storage = nullptr};

        ParamSet params(RawDecoder::paramSpecs());
        RawDecoder dec(params);
        QVERIFY(dec.configure(set.tensors, "frame_index").has_value());
        QCOMPARE(dec.header(), (std::vector<std::string>{"time_usec", "frame_index", "v0", "v1", "v2"}));

        std::vector<Row> rows;
        QVERIFY(dec.decode(set, DecodeInput{.index = 7, .timeUsec = 123456, .image = std::nullopt}, rows).has_value());
        QCOMPARE(rows.size(), 1U);
        QCOMPARE(rows[0], (Row{"123456", "7", "0.5", "-2", "1e-08"}));

        // too many values for one row
        QVERIFY(params.set("max_values", int64_t{2}).has_value());
        RawDecoder small(params);
        QVERIFY(!small.configure(set.tensors, "frame_index").has_value());

        // unknown tensor name
        QVERIFY(params.set("tensor", std::string("nope")).has_value());
        RawDecoder named(params);
        QVERIFY(!named.configure(set.tensors, "frame_index").has_value());
    }

    void scoresDecoder()
    {
        // two steps of three logits each
        Tensor out(DType::Float32, {1, 2, 3});
        const float logits[] = {0.0f, 2.0f, 0.0f, 5.0f, 5.0f, 9.0f};
        std::copy(std::begin(logits), std::end(logits), out.dataAs<float>());
        TensorSet set{.tensors = {out.view("scores")}, .storage = nullptr};

        ParamSet params(ScoresDecoder::paramSpecs());
        QVERIFY(params.set("activation", std::string("softmax")).has_value());
        QVERIFY(params.set("labels", std::vector<std::string>{"rest", "groom", "run"}).has_value());
        ScoresDecoder dec(params);
        QVERIFY(dec.configure(set.tensors, "frame_index").has_value());
        QCOMPARE(
            dec.header(),
            (std::vector<std::string>{
                "time_usec",
                "frame_index",
                "step",
                "argmax",
                "label",
                "score_rest",
                "score_groom",
                "score_run"}));

        std::vector<Row> rows;
        QVERIFY(dec.decode(set, DecodeInput{.index = 3, .timeUsec = 1000, .image = std::nullopt}, rows).has_value());
        QCOMPARE(rows.size(), 2U);
        QCOMPARE(rows[0][2], std::string("0"));
        QCOMPARE(rows[0][3], std::string("1"));
        QCOMPARE(rows[0][4], std::string("groom"));
        QCOMPARE(rows[1][2], std::string("1"));
        QCOMPARE(rows[1][4], std::string("run"));

        double sum = 0;
        for (size_t i = 5; i < 8; ++i)
            sum += cellValue(rows[0][i]);
        QVERIFY(std::abs(sum - 1.0) < 1e-5);
        QVERIFY(std::abs(cellValue(rows[0][6]) - std::exp(2.0) / (2.0 + std::exp(2.0))) < 1e-5);

        // label count must match the classes
        QVERIFY(params.set("labels", std::vector<std::string>{"a", "b"}).has_value());
        ScoresDecoder mismatch(params);
        QVERIFY(!mismatch.configure(set.tensors, "frame_index").has_value());
    }

    void decoderRegistry()
    {
        for (const auto &unit : decoderUnits()) {
            const auto dec = createDecoder(unit.id, ParamSet(unit.paramSpecs()));
            QVERIFY(dec.has_value());
        }
        QVERIFY(!createDecoder("nonexistent", ParamSet()).has_value());
        QVERIFY(findAdapterUnit("image") != nullptr);
    }

    void engineInspect()
    {
        if (!fs::exists(testModel("channel-mean.onnx")))
            QSKIP("Test models were not generated (python3-onnx missing?)");

        const auto info = OrtEngine::inspectModel(testModel("channel-mean.onnx"));
        VERIFY_OK(info);
        QCOMPARE(info->inputs.size(), 1U);
        QCOMPARE(info->inputs[0].name, std::string("images"));
        QCOMPARE(info->inputs[0].dtype, DType::Float32);
        QCOMPARE(info->inputs[0].shape, (std::vector<int64_t>{1, 3, -1, -1}));
        QCOMPARE(info->inputs[0].dimNames[2], std::string("height"));
        QCOMPARE(info->outputs[0].name, std::string("scores"));
        QCOMPARE(info->producerName, std::string("syntalos-tests"));
        QCOMPARE(info->metadata.at("description"), std::string("Syntalos test model"));

        QVERIFY(!OrtEngine::inspectModel(testModel("does-not-exist.onnx")).has_value());
        QVERIFY(!OrtEngine::availableProviders().empty());
        QVERIFY(!OrtEngine::ortVersion().empty());
    }

    void engineIdentityImage()
    {
        if (!fs::exists(testModel("identity-image.onnx")))
            QSKIP("Test models were not generated (python3-onnx missing?)");

        OrtEngine engine;
        const EngineOptions options;
        auto r = engine.load(testModel("identity-image.onnx"), options);
        VERIFY_OK(r);
        QVERIFY(engine.isLoadedWith(testModel("identity-image.onnx"), options));
        QVERIFY(!engine.isLoadedWith(testModel("identity-image.onnx"), EngineOptions{.intraOpThreads = 1}));

        ImageAdapter adapter(imageParams({}));
        QVERIFY(adapter.configure(engine.modelInfo().inputs[0]).has_value());

        cv::Mat frame(120, 160, CV_8UC3);
        cv::randu(frame, cv::Scalar::all(0), cv::Scalar::all(256));
        Tensor input;
        ImageGeometry geom;
        QVERIFY(adapter.process(frame, input, geom).has_value());

        const auto out = engine.run(input);
        VERIFY_OK(out);
        const auto y = out->find("output");
        QVERIFY(y != nullptr);
        QCOMPARE(y->shape, input.shape());
        QCOMPARE(y->dtype, DType::Float32);
        QVERIFY(std::memcmp(y->data, input.data(), input.byteSize()) == 0);

        // a tensor of the wrong type is refused
        Tensor wrong(DType::UInt8, {1, 3, 64, 64});
        QVERIFY(!engine.run(wrong).has_value());
    }

    void engineIdentityUInt8()
    {
        if (!fs::exists(testModel("identity-u8-nhwc.onnx")))
            QSKIP("Test models were not generated (python3-onnx missing?)");

        OrtEngine engine;
        QVERIFY(engine.load(testModel("identity-u8-nhwc.onnx"), EngineOptions{}).has_value());
        const auto recipe = recipeSkeleton(engine.modelInfo());

        ImageAdapter adapter(recipe.input);
        QVERIFY(adapter.configure(engine.modelInfo().inputs[0]).has_value());
        QCOMPARE(adapter.inputShape(), (std::vector<int64_t>{1, 32, 48, 1}));

        // same size as the model input, so the pixels pass through unchanged
        cv::Mat frame(32, 48, CV_8UC1);
        cv::randu(frame, cv::Scalar::all(0), cv::Scalar::all(256));
        Tensor input;
        ImageGeometry geom;
        QVERIFY(adapter.process(frame, input, geom).has_value());
        const auto out = engine.run(input);
        QVERIFY(out.has_value());
        QCOMPARE(out->tensors[0].dtype, DType::UInt8);
        QVERIFY(std::memcmp(out->tensors[0].data, frame.data, frame.total()) == 0);
    }

    void engineChannelMeanScores()
    {
        if (!fs::exists(testModel("channel-mean.onnx")))
            QSKIP("Test models were not generated (python3-onnx missing?)");

        OrtEngine engine;
        QVERIFY(engine.load(testModel("channel-mean.onnx"), EngineOptions{}).has_value());

        ImageAdapter adapter(imageParams({
            {"resize", std::string("stretch")      },
            {"size",   std::vector<int64_t>{40, 30}}
        }));
        QVERIFY(adapter.configure(engine.modelInfo().inputs[0]).has_value());
        QCOMPARE(adapter.inputShape(), (std::vector<int64_t>{1, 3, 30, 40}));

        // a warm-up run with zeros gives the decoder the output shapes
        Tensor zeros(adapter.inputDType(), adapter.inputShape());
        zeros.fillZero();
        const auto warmup = engine.run(zeros);
        QVERIFY(warmup.has_value());

        ParamSet params(ScoresDecoder::paramSpecs());
        QVERIFY(params.set("labels", std::vector<std::string>{"red", "green", "blue"}).has_value());
        ScoresDecoder dec(params);
        QVERIFY(dec.configure(warmup->tensors, ImageAdapter::indexColumn()).has_value());

        // a uniform BGR frame: every channel mean is the channel value
        cv::Mat frame(100, 100, CV_8UC3, cv::Scalar(51, 102, 255));
        Tensor input;
        ImageGeometry geom;
        QVERIFY(adapter.process(frame, input, geom).has_value());
        const auto out = engine.run(input);
        QVERIFY(out.has_value());

        std::vector<Row> rows;
        QVERIFY(dec.decode(*out, DecodeInput{.index = 42, .timeUsec = 987654321, .image = geom}, rows).has_value());
        QCOMPARE(rows.size(), 1U);
        QCOMPARE(rows[0][0], std::string("987654321"));
        QCOMPARE(rows[0][1], std::string("42"));
        QCOMPARE(rows[0][4], std::string("red"));
        QVERIFY(std::abs(cellValue(rows[0][5]) - 1.0) < 1e-6);
        QVERIFY(std::abs(cellValue(rows[0][6]) - 0.4) < 1e-6);
        QVERIFY(std::abs(cellValue(rows[0][7]) - 0.2) < 1e-6);
    }
};

QTEST_GUILESS_MAIN(TestOnnxInference)
#include "test-onnxinfer.moc"
