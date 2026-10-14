#include "openclspatialprocessingbackend.h"

#include <QByteArray>
#include <QLibrary>
#include <QMutex>
#include <QMutexLocker>
#include <QSysInfo>
#include <QStringList>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

#ifdef Q_OS_WIN
#define ONPOINT_CL_API_CALL __stdcall
#define ONPOINT_CL_CALLBACK __stdcall
#else
#define ONPOINT_CL_API_CALL
#define ONPOINT_CL_CALLBACK
#endif

namespace
{
using cl_uchar = std::uint8_t;
using cl_int = std::int32_t;
using cl_uint = std::uint32_t;
using cl_ulong = std::uint64_t;
using cl_double = double;
using cl_bool = cl_uint;
using cl_bitfield = cl_ulong;
using cl_device_type = cl_bitfield;
using cl_platform_info = cl_uint;
using cl_device_info = cl_uint;
using cl_command_queue_properties = cl_bitfield;
using cl_mem_flags = cl_bitfield;
using cl_program_build_info = cl_uint;
using cl_context_properties = std::intptr_t;

struct _cl_platform_id;
struct _cl_device_id;
struct _cl_context;
struct _cl_command_queue;
struct _cl_mem;
struct _cl_program;
struct _cl_kernel;
struct _cl_event;

typedef _cl_platform_id *cl_platform_id;
typedef _cl_device_id *cl_device_id;
typedef _cl_context *cl_context;
typedef _cl_command_queue *cl_command_queue;
typedef _cl_mem *cl_mem;
typedef _cl_program *cl_program;
typedef _cl_kernel *cl_kernel;
typedef _cl_event *cl_event;

typedef void (ONPOINT_CL_CALLBACK *ClContextCallback)(
    const char *,
    const void *,
    std::size_t,
    void *
    );

constexpr cl_int CL_SUCCESS = 0;
constexpr cl_bool CL_TRUE = 1;

constexpr cl_device_type CL_DEVICE_TYPE_GPU =
    static_cast<cl_device_type>(1) << 2;

constexpr cl_mem_flags CL_MEM_READ_WRITE =
    static_cast<cl_mem_flags>(1) << 0;
constexpr cl_mem_flags CL_MEM_READ_ONLY =
    static_cast<cl_mem_flags>(1) << 2;
constexpr cl_mem_flags CL_MEM_COPY_HOST_PTR =
    static_cast<cl_mem_flags>(1) << 5;

constexpr cl_platform_info CL_PLATFORM_NAME = 0x0902;
constexpr cl_platform_info CL_PLATFORM_VENDOR = 0x0903;

constexpr cl_device_info CL_DEVICE_NAME = 0x102B;
constexpr cl_device_info CL_DEVICE_VENDOR = 0x102C;
constexpr cl_device_info CL_DEVICE_AVAILABLE = 0x1027;
constexpr cl_device_info CL_DEVICE_COMPILER_AVAILABLE = 0x1028;
constexpr cl_device_info CL_DEVICE_GLOBAL_MEM_SIZE = 0x101F;
constexpr cl_device_info CL_DEVICE_MAX_MEM_ALLOC_SIZE = 0x1010;
constexpr cl_device_info CL_DEVICE_DOUBLE_FP_CONFIG = 0x1032;
constexpr cl_device_info CL_DEVICE_EXTENSIONS = 0x1030;

constexpr cl_program_build_info CL_PROGRAM_BUILD_LOG = 0x1183;

using ClGetPlatformIDs = cl_int (ONPOINT_CL_API_CALL *)(
    cl_uint,
    cl_platform_id *,
    cl_uint *
    );
using ClGetPlatformInfo = cl_int (ONPOINT_CL_API_CALL *)(
    cl_platform_id,
    cl_platform_info,
    std::size_t,
    void *,
    std::size_t *
    );
using ClGetDeviceIDs = cl_int (ONPOINT_CL_API_CALL *)(
    cl_platform_id,
    cl_device_type,
    cl_uint,
    cl_device_id *,
    cl_uint *
    );
using ClGetDeviceInfo = cl_int (ONPOINT_CL_API_CALL *)(
    cl_device_id,
    cl_device_info,
    std::size_t,
    void *,
    std::size_t *
    );
using ClCreateContext = cl_context (ONPOINT_CL_API_CALL *)(
    const cl_context_properties *,
    cl_uint,
    const cl_device_id *,
    ClContextCallback,
    void *,
    cl_int *
    );
using ClReleaseContext = cl_int (ONPOINT_CL_API_CALL *)(cl_context);
using ClCreateCommandQueue = cl_command_queue (ONPOINT_CL_API_CALL *)(
    cl_context,
    cl_device_id,
    cl_command_queue_properties,
    cl_int *
    );
using ClReleaseCommandQueue = cl_int (ONPOINT_CL_API_CALL *)(
    cl_command_queue
    );
using ClCreateProgramWithSource = cl_program (ONPOINT_CL_API_CALL *)(
    cl_context,
    cl_uint,
    const char **,
    const std::size_t *,
    cl_int *
    );
using ClBuildProgram = cl_int (ONPOINT_CL_API_CALL *)(
    cl_program,
    cl_uint,
    const cl_device_id *,
    const char *,
    void (ONPOINT_CL_CALLBACK *)(cl_program, void *),
    void *
    );
using ClGetProgramBuildInfo = cl_int (ONPOINT_CL_API_CALL *)(
    cl_program,
    cl_device_id,
    cl_program_build_info,
    std::size_t,
    void *,
    std::size_t *
    );
using ClReleaseProgram = cl_int (ONPOINT_CL_API_CALL *)(cl_program);
using ClCreateKernel = cl_kernel (ONPOINT_CL_API_CALL *)(
    cl_program,
    const char *,
    cl_int *
    );
using ClReleaseKernel = cl_int (ONPOINT_CL_API_CALL *)(cl_kernel);
using ClCreateBuffer = cl_mem (ONPOINT_CL_API_CALL *)(
    cl_context,
    cl_mem_flags,
    std::size_t,
    void *,
    cl_int *
    );
using ClReleaseMemObject = cl_int (ONPOINT_CL_API_CALL *)(cl_mem);
using ClSetKernelArg = cl_int (ONPOINT_CL_API_CALL *)(
    cl_kernel,
    cl_uint,
    std::size_t,
    const void *
    );
using ClEnqueueNDRangeKernel = cl_int (ONPOINT_CL_API_CALL *)(
    cl_command_queue,
    cl_kernel,
    cl_uint,
    const std::size_t *,
    const std::size_t *,
    const std::size_t *,
    cl_uint,
    const cl_event *,
    cl_event *
    );
using ClEnqueueReadBuffer = cl_int (ONPOINT_CL_API_CALL *)(
    cl_command_queue,
    cl_mem,
    cl_bool,
    std::size_t,
    std::size_t,
    void *,
    cl_uint,
    const cl_event *,
    cl_event *
    );
using ClFinish = cl_int (ONPOINT_CL_API_CALL *)(cl_command_queue);

QString openClErrorText(cl_int errorCode)
{
    switch (errorCode) {
    case 0: return "CL_SUCCESS";
    case -1: return "CL_DEVICE_NOT_FOUND";
    case -2: return "CL_DEVICE_NOT_AVAILABLE";
    case -3: return "CL_COMPILER_NOT_AVAILABLE";
    case -4: return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
    case -5: return "CL_OUT_OF_RESOURCES";
    case -6: return "CL_OUT_OF_HOST_MEMORY";
    case -11: return "CL_BUILD_PROGRAM_FAILURE";
    case -30: return "CL_INVALID_VALUE";
    case -34: return "CL_INVALID_CONTEXT";
    case -36: return "CL_INVALID_COMMAND_QUEUE";
    case -38: return "CL_INVALID_MEM_OBJECT";
    case -48: return "CL_INVALID_KERNEL";
    case -49: return "CL_INVALID_ARG_INDEX";
    case -50: return "CL_INVALID_ARG_VALUE";
    case -51: return "CL_INVALID_ARG_SIZE";
    case -52: return "CL_INVALID_KERNEL_ARGS";
    case -54: return "CL_INVALID_WORK_GROUP_SIZE";
    case -61: return "CL_INVALID_BUFFER_SIZE";
    case -1001: return "CL_PLATFORM_NOT_FOUND_KHR";
    default:
        return QString("OpenCL error %1").arg(errorCode);
    }
}

QString cleanedInfoString(const QByteArray &bytes)
{
    QByteArray cleaned = bytes;

    while (!cleaned.isEmpty() &&
           (cleaned.endsWith('\0') ||
            cleaned.endsWith('\n') ||
            cleaned.endsWith('\r'))) {
        cleaned.chop(1);
    }

    return QString::fromLocal8Bit(cleaned).trimmed();
}

std::array<cl_int, 256> makeRangeWeights(double sigma)
{
    sigma = std::max(1.0, sigma);

    std::array<cl_int, 256> weights {};

    for (int difference = 0;
         difference < 256;
         ++difference) {
        const double normalized = difference / sigma;
        const double weight =
            1.0 /
            (1.0 +
             (4.0 * normalized * normalized));

        weights[static_cast<std::size_t>(difference)] =
            std::max(
                1,
                static_cast<int>(
                    std::round(weight * 1024.0)
                    )
                );
    }

    return weights;
}

std::vector<cl_int> makeGaussianKernel(
    double sigma,
    int &kernelRadius
    )
{
    sigma = std::clamp(sigma, 0.35, 3.20);

    kernelRadius = std::clamp(
        static_cast<int>(std::ceil(sigma * 2.5)),
        1,
        8
        );

    const int kernelSize =
        (kernelRadius * 2) + 1;

    std::vector<double> rawKernel(
        static_cast<std::size_t>(kernelSize)
        );

    double rawSum = 0.0;

    for (int offset = -kernelRadius;
         offset <= kernelRadius;
         ++offset) {
        const double normalized = offset / sigma;
        const double value =
            std::exp(
                -0.5 * normalized * normalized
                );

        rawKernel[static_cast<std::size_t>(
            offset + kernelRadius
            )] = value;
        rawSum += value;
    }

    std::vector<cl_int> kernel(
        static_cast<std::size_t>(kernelSize)
        );

    int integerSum = 0;

    for (int index = 0;
         index < kernelSize;
         ++index) {
        kernel[static_cast<std::size_t>(index)] =
            std::max(
                1,
                static_cast<int>(
                    std::round(
                        (rawKernel[static_cast<std::size_t>(index)] /
                         rawSum) *
                        65536.0
                        )
                    )
                );

        integerSum +=
            kernel[static_cast<std::size_t>(index)];
    }

    kernel[static_cast<std::size_t>(kernelRadius)] +=
        65536 - integerSum;

    return kernel;
}

const char *spatialKernelSource()
{
    return R"CLC(
#pragma OPENCL EXTENSION cl_khr_fp64 : enable
#pragma OPENCL FP_CONTRACT OFF

inline int spatial_clamp_byte_int(const int value)
{
    return min(255, max(0, value));
}

inline double spatial_clamp_unit(const double value)
{
    return fmin(1.0, fmax(0.0, value));
}

inline double spatial_smooth_step(
    const double edge0,
    const double edge1,
    const double value)
{
    if (edge1 <= edge0) {
        return value >= edge1 ? 1.0 : 0.0;
    }

    const double normalized = spatial_clamp_unit(
        (value - edge0) / (edge1 - edge0));

    return normalized * normalized *
        (3.0 - (2.0 * normalized));
}

inline int spatial_luminance_bytes(
    const uchar blue,
    const uchar green,
    const uchar red)
{
    return spatial_clamp_byte_int(
        ((77 * (int)red) +
         (150 * (int)green) +
         (29 * (int)blue) +
         128) >> 8);
}

inline int spatial_cb_bytes(
    const uchar blue,
    const uchar green,
    const uchar red)
{
    return spatial_clamp_byte_int(
        128 +
        (((-43 * (int)red) -
          (85 * (int)green) +
          (128 * (int)blue) +
          128) >> 8));
}

inline int spatial_cr_bytes(
    const uchar blue,
    const uchar green,
    const uchar red)
{
    return spatial_clamp_byte_int(
        128 +
        (((128 * (int)red) -
          (107 * (int)green) -
          (21 * (int)blue) +
          128) >> 8));
}

inline int spatial_channel_from_double(const double value)
{
    return spatial_clamp_byte_int((int)round(value));
}

inline int spatial_weight_for_distance(const int distance)
{
    switch (min(4, max(0, distance))) {
    case 0: return 1024;
    case 1: return 820;
    case 2: return 610;
    case 3: return 420;
    default: return 270;
    }
}

__kernel void spatial_encode_ycbcr(
    __global const uchar *sourcePixelBytes,
    __global uchar *encodedPixelBytes,
    const uint pixelCount)
{
    const uint pixelIndex = get_global_id(0);

    if (pixelIndex >= pixelCount) {
        return;
    }

    const uint byteIndex = pixelIndex * 4U;
    const uchar blue = sourcePixelBytes[byteIndex + 0U];
    const uchar green = sourcePixelBytes[byteIndex + 1U];
    const uchar red = sourcePixelBytes[byteIndex + 2U];

    // Encoded QImage-compatible BGRA byte storage:
    // B = Cr, G = Cb, R = luminance, A = original alpha.
    encodedPixelBytes[byteIndex + 0U] =
        (uchar)spatial_cr_bytes(blue, green, red);
    encodedPixelBytes[byteIndex + 1U] =
        (uchar)spatial_cb_bytes(blue, green, red);
    encodedPixelBytes[byteIndex + 2U] =
        (uchar)spatial_luminance_bytes(blue, green, red);
    encodedPixelBytes[byteIndex + 3U] =
        sourcePixelBytes[byteIndex + 3U];
}

__kernel void spatial_noise_edge_pass(
    __global const uchar *inputPixelBytes,
    __global uchar *outputPixelBytes,
    const int width,
    const int height,
    const int horizontal,
    const int radius,
    __constant int *rangeWeights,
    const int filterLuminance,
    const int filterChroma)
{
    const uint pixelIndex = get_global_id(0);
    const uint pixelCount = (uint)(width * height);

    if (pixelIndex >= pixelCount) {
        return;
    }

    const int x = (int)(pixelIndex % (uint)width);
    const int y = (int)(pixelIndex / (uint)width);
    const uint centerByte = pixelIndex * 4U;

    const int centerLuminance =
        (int)inputPixelBytes[centerByte + 2U];

    long luminanceSum = 0;
    long blueDifferenceSum = 0;
    long redDifferenceSum = 0;
    long weightSum = 0;

    for (int offset = -radius;
         offset <= radius;
         ++offset) {
        const int sampleX = horizontal != 0
            ? min(width - 1, max(0, x + offset))
            : x;

        const int sampleY = horizontal != 0
            ? y
            : min(height - 1, max(0, y + offset));

        const uint sampleIndex =
            (uint)((sampleY * width) + sampleX);
        const uint sampleByte = sampleIndex * 4U;

        const int sampleLuminance =
            (int)inputPixelBytes[sampleByte + 2U];

        const int luminanceDifference =
            abs(sampleLuminance - centerLuminance);

        const int combinedWeight = max(
            1,
            (rangeWeights[luminanceDifference] *
             spatial_weight_for_distance(abs(offset))) >> 10);

        weightSum += (long)combinedWeight;
        luminanceSum +=
            (long)sampleLuminance *
            (long)combinedWeight;
        blueDifferenceSum +=
            (long)inputPixelBytes[sampleByte + 1U] *
            (long)combinedWeight;
        redDifferenceSum +=
            (long)inputPixelBytes[sampleByte + 0U] *
            (long)combinedWeight;
    }

    const int filteredLuminance = weightSum > 0
        ? (int)((luminanceSum + (weightSum / 2)) / weightSum)
        : centerLuminance;

    const int filteredBlueDifference = weightSum > 0
        ? (int)((blueDifferenceSum + (weightSum / 2)) / weightSum)
        : (int)inputPixelBytes[centerByte + 1U];

    const int filteredRedDifference = weightSum > 0
        ? (int)((redDifferenceSum + (weightSum / 2)) / weightSum)
        : (int)inputPixelBytes[centerByte + 0U];

    outputPixelBytes[centerByte + 0U] = filterChroma != 0
        ? (uchar)spatial_clamp_byte_int(filteredRedDifference)
        : inputPixelBytes[centerByte + 0U];

    outputPixelBytes[centerByte + 1U] = filterChroma != 0
        ? (uchar)spatial_clamp_byte_int(filteredBlueDifference)
        : inputPixelBytes[centerByte + 1U];

    outputPixelBytes[centerByte + 2U] = filterLuminance != 0
        ? (uchar)spatial_clamp_byte_int(filteredLuminance)
        : inputPixelBytes[centerByte + 2U];

    outputPixelBytes[centerByte + 3U] =
        inputPixelBytes[centerByte + 3U];
}

__kernel void spatial_noise_finish(
    __global uchar *resultPixelBytes,
    __global const uchar *encodedOriginal,
    __global const uchar *encodedFiltered,
    const int width,
    const int height,
    const double luminanceMixBase,
    const double colorMixBase,
    const double detailProtection)
{
    const uint pixelIndex = get_global_id(0);
    const uint pixelCount = (uint)(width * height);

    if (pixelIndex >= pixelCount) {
        return;
    }

    const int x = (int)(pixelIndex % (uint)width);
    const int y = (int)(pixelIndex / (uint)width);
    const uint byteIndex = pixelIndex * 4U;

    const int originalLuminance =
        (int)encodedOriginal[byteIndex + 2U];

    const int leftX = max(0, x - 1);
    const int rightX = min(width - 1, x + 1);
    const int topY = max(0, y - 1);
    const int bottomY = min(height - 1, y + 1);

    const int leftLuminance =
        (int)encodedOriginal[
            ((uint)((y * width) + leftX) * 4U) + 2U];
    const int rightLuminance =
        (int)encodedOriginal[
            ((uint)((y * width) + rightX) * 4U) + 2U];
    const int topLuminance =
        (int)encodedOriginal[
            ((uint)((topY * width) + x) * 4U) + 2U];
    const int bottomLuminance =
        (int)encodedOriginal[
            ((uint)((bottomY * width) + x) * 4U) + 2U];

    const int gradient = max(
        max(abs(originalLuminance - leftLuminance),
            abs(originalLuminance - rightLuminance)),
        max(abs(originalLuminance - topLuminance),
            abs(originalLuminance - bottomLuminance)));

    const double edgeFactor = spatial_clamp_unit(
        ((double)gradient - 2.0) / 26.0);

    const double luminanceMix =
        luminanceMixBase *
        (1.0 -
         (0.86 * detailProtection * edgeFactor));

    const double colorMix =
        colorMixBase *
        (1.0 -
         (0.58 * detailProtection * edgeFactor));

    const int filteredLuminance =
        (int)encodedFiltered[byteIndex + 2U];
    const int originalCb =
        (int)encodedOriginal[byteIndex + 1U];
    const int filteredCb =
        (int)encodedFiltered[byteIndex + 1U];
    const int originalCr =
        (int)encodedOriginal[byteIndex + 0U];
    const int filteredCr =
        (int)encodedFiltered[byteIndex + 0U];

    const int luminanceValue = spatial_channel_from_double(
        (double)originalLuminance +
        (((double)filteredLuminance -
          (double)originalLuminance) *
         luminanceMix));

    const int cbValue = spatial_channel_from_double(
        (double)originalCb +
        (((double)filteredCb -
          (double)originalCb) *
         colorMix));

    const int crValue = spatial_channel_from_double(
        (double)originalCr +
        (((double)filteredCr -
          (double)originalCr) *
         colorMix));

    const double cb = (double)cbValue - 128.0;
    const double cr = (double)crValue - 128.0;

    const int red = spatial_channel_from_double(
        (double)luminanceValue + (1.402 * cr));
    const int green = spatial_channel_from_double(
        (double)luminanceValue -
        (0.344136 * cb) -
        (0.714136 * cr));
    const int blue = spatial_channel_from_double(
        (double)luminanceValue + (1.772 * cb));

    resultPixelBytes[byteIndex + 0U] = (uchar)blue;
    resultPixelBytes[byteIndex + 1U] = (uchar)green;
    resultPixelBytes[byteIndex + 2U] = (uchar)red;
    resultPixelBytes[byteIndex + 3U] =
        encodedOriginal[byteIndex + 3U];
}

__kernel void spatial_focus_horizontal(
    __global const uchar *sourcePixelBytes,
    __global uchar *horizontalLuminance,
    __global uchar *horizontalAlpha,
    const int width,
    const int height,
    __constant int *kernelWeights,
    const int kernelRadius)
{
    const uint pixelIndex = get_global_id(0);
    const uint pixelCount = (uint)(width * height);

    if (pixelIndex >= pixelCount) {
        return;
    }

    const int x = (int)(pixelIndex % (uint)width);
    const int y = (int)(pixelIndex / (uint)width);

    ulong luminanceSum = 0UL;
    ulong alphaWeightSum = 0UL;
    ulong alphaSum = 0UL;
    ulong kernelSum = 0UL;

    for (int offset = -kernelRadius;
         offset <= kernelRadius;
         ++offset) {
        const int sampleX =
            min(width - 1, max(0, x + offset));
        const uint sampleIndex =
            (uint)((y * width) + sampleX);
        const uint sampleByte = sampleIndex * 4U;

        const int kernelWeight =
            kernelWeights[offset + kernelRadius];
        const int alpha =
            (int)sourcePixelBytes[sampleByte + 3U];
        const ulong visibleWeight =
            (ulong)kernelWeight * (ulong)alpha;

        const int luminanceValue = spatial_luminance_bytes(
            sourcePixelBytes[sampleByte + 0U],
            sourcePixelBytes[sampleByte + 1U],
            sourcePixelBytes[sampleByte + 2U]);

        luminanceSum +=
            (ulong)luminanceValue * visibleWeight;
        alphaWeightSum += visibleWeight;
        alphaSum +=
            (ulong)alpha * (ulong)kernelWeight;
        kernelSum += (ulong)kernelWeight;
    }

    const uint centerByte = pixelIndex * 4U;
    const int centerLuminance = spatial_luminance_bytes(
        sourcePixelBytes[centerByte + 0U],
        sourcePixelBytes[centerByte + 1U],
        sourcePixelBytes[centerByte + 2U]);

    horizontalLuminance[pixelIndex] =
        alphaWeightSum > 0UL
        ? (uchar)spatial_clamp_byte_int(
              (int)((luminanceSum +
                     (alphaWeightSum / 2UL)) /
                    alphaWeightSum))
        : (uchar)centerLuminance;

    horizontalAlpha[pixelIndex] =
        kernelSum > 0UL
        ? (uchar)spatial_clamp_byte_int(
              (int)((alphaSum +
                     (kernelSum / 2UL)) /
                    kernelSum))
        : sourcePixelBytes[centerByte + 3U];
}

__kernel void spatial_focus_vertical(
    __global const uchar *horizontalLuminance,
    __global const uchar *horizontalAlpha,
    __global uchar *blurredLuminance,
    const int width,
    const int height,
    __constant int *kernelWeights,
    const int kernelRadius)
{
    const uint pixelIndex = get_global_id(0);
    const uint pixelCount = (uint)(width * height);

    if (pixelIndex >= pixelCount) {
        return;
    }

    const int x = (int)(pixelIndex % (uint)width);
    const int y = (int)(pixelIndex / (uint)width);

    ulong luminanceSum = 0UL;
    ulong alphaWeightSum = 0UL;

    for (int offset = -kernelRadius;
         offset <= kernelRadius;
         ++offset) {
        const int sampleY =
            min(height - 1, max(0, y + offset));
        const uint sampleIndex =
            (uint)((sampleY * width) + x);

        const int kernelWeight =
            kernelWeights[offset + kernelRadius];
        const int alpha =
            (int)horizontalAlpha[sampleIndex];
        const ulong visibleWeight =
            (ulong)kernelWeight * (ulong)alpha;

        luminanceSum +=
            (ulong)horizontalLuminance[sampleIndex] *
            visibleWeight;
        alphaWeightSum += visibleWeight;
    }

    blurredLuminance[pixelIndex] =
        alphaWeightSum > 0UL
        ? (uchar)spatial_clamp_byte_int(
              (int)((luminanceSum +
                     (alphaWeightSum / 2UL)) /
                    alphaWeightSum))
        : horizontalLuminance[pixelIndex];
}

__kernel void spatial_focus_finish(
    __global const uchar *sourcePixelBytes,
    __global uchar *resultPixelBytes,
    __global const uchar *blurredLuminance,
    const int width,
    const int height,
    const double amountScale,
    const double threshold,
    const double thresholdSoftness,
    const double edgeProtectionStrength,
    const double maximumDelta,
    const double blend)
{
    const uint pixelIndex = get_global_id(0);
    const uint pixelCount = (uint)(width * height);

    if (pixelIndex >= pixelCount) {
        return;
    }

    const int x = (int)(pixelIndex % (uint)width);
    const int y = (int)(pixelIndex / (uint)width);
    const uint byteIndex = pixelIndex * 4U;

    const uchar blueByte = sourcePixelBytes[byteIndex + 0U];
    const uchar greenByte = sourcePixelBytes[byteIndex + 1U];
    const uchar redByte = sourcePixelBytes[byteIndex + 2U];

    const int originalLuminance = spatial_luminance_bytes(
        blueByte,
        greenByte,
        redByte);

    const double highPass =
        (double)originalLuminance -
        (double)blurredLuminance[pixelIndex];

    const double detailGate = spatial_smooth_step(
        threshold,
        threshold + thresholdSoftness,
        fabs(highPass));

    const int leftX = max(0, x - 1);
    const int rightX = min(width - 1, x + 1);
    const int topY = max(0, y - 1);
    const int bottomY = min(height - 1, y + 1);

    const uint leftByte =
        (uint)((y * width) + leftX) * 4U;
    const uint rightByte =
        (uint)((y * width) + rightX) * 4U;
    const uint topByte =
        (uint)((topY * width) + x) * 4U;
    const uint bottomByte =
        (uint)((bottomY * width) + x) * 4U;

    const int leftLuminance = spatial_luminance_bytes(
        sourcePixelBytes[leftByte + 0U],
        sourcePixelBytes[leftByte + 1U],
        sourcePixelBytes[leftByte + 2U]);
    const int rightLuminance = spatial_luminance_bytes(
        sourcePixelBytes[rightByte + 0U],
        sourcePixelBytes[rightByte + 1U],
        sourcePixelBytes[rightByte + 2U]);
    const int topLuminance = spatial_luminance_bytes(
        sourcePixelBytes[topByte + 0U],
        sourcePixelBytes[topByte + 1U],
        sourcePixelBytes[topByte + 2U]);
    const int bottomLuminance = spatial_luminance_bytes(
        sourcePixelBytes[bottomByte + 0U],
        sourcePixelBytes[bottomByte + 1U],
        sourcePixelBytes[bottomByte + 2U]);

    const int localGradient = max(
        max(abs(originalLuminance - leftLuminance),
            abs(originalLuminance - rightLuminance)),
        max(abs(originalLuminance - topLuminance),
            abs(originalLuminance - bottomLuminance)));

    const double strongEdge = spatial_smooth_step(
        20.0,
        92.0,
        (double)localGradient);

    const double edgeGuard =
        1.0 -
        (0.78 * edgeProtectionStrength * strongEdge);

    const double shadowGuard = spatial_clamp_unit(
        (double)originalLuminance / 18.0);
    const double highlightGuard = spatial_clamp_unit(
        (255.0 - (double)originalLuminance) / 18.0);
    const double toneGuard =
        fmin(shadowGuard, highlightGuard);

    double luminanceDelta =
        highPass *
        amountScale *
        detailGate *
        edgeGuard *
        toneGuard *
        blend;

    luminanceDelta = fmin(
        maximumDelta,
        fmax(-maximumDelta, luminanceDelta));

    const int newLuminance = spatial_channel_from_double(
        (double)originalLuminance + luminanceDelta);

    // Focus changes luminance only. Apply the same luminance delta to all
    // three source channels instead of round-tripping through YCbCr.
    // This preserves the source chroma, avoids color casts on some OpenCL
    // drivers, and remains within two code values of the CPU reconstruction.
    const double channelDelta =
        (double)newLuminance -
        (double)originalLuminance;

    const int red = spatial_channel_from_double(
        (double)redByte + channelDelta);
    const int green = spatial_channel_from_double(
        (double)greenByte + channelDelta);
    const int blue = spatial_channel_from_double(
        (double)blueByte + channelDelta);

    resultPixelBytes[byteIndex + 0U] = (uchar)blue;
    resultPixelBytes[byteIndex + 1U] = (uchar)green;
    resultPixelBytes[byteIndex + 2U] = (uchar)red;
    resultPixelBytes[byteIndex + 3U] =
        sourcePixelBytes[byteIndex + 3U];
}

inline double spatial_photo_luminance(
    const double red,
    const double green,
    const double blue)
{
    return (0.299 * red) +
        (0.587 * green) +
        (0.114 * blue);
}

inline double spatial_region_radius(
    const int rectLeft,
    const int rectTop,
    const int rectWidth,
    const int rectHeight,
    const int x,
    const int y,
    const int shape)
{
    const double centerX =
        (double)rectLeft +
        (((double)rectWidth - 1.0) / 2.0);
    const double centerY =
        (double)rectTop +
        (((double)rectHeight - 1.0) / 2.0);

    const double radiusX = fmax(1.0, (double)rectWidth / 2.0);
    const double radiusY = fmax(1.0, (double)rectHeight / 2.0);

    const double dx = fabs((double)x - centerX) / radiusX;
    const double dy = fabs((double)y - centerY) / radiusY;

    return shape == 0
        ? sqrt((dx * dx) + (dy * dy))
        : fmax(dx, dy);
}

inline double spatial_region_weight(
    const int rectLeft,
    const int rectTop,
    const int rectWidth,
    const int rectHeight,
    const int x,
    const int y,
    const int shape,
    const int softEdge)
{
    const double radius = spatial_region_radius(
        rectLeft,
        rectTop,
        rectWidth,
        rectHeight,
        x,
        y,
        shape);

    if (radius > 1.0) {
        return 0.0;
    }

    const double soft =
        (double)min(100, max(0, softEdge)) / 100.0;
    const double fadeSize = 0.08 + (soft * 0.46);
    const double fadeStart =
        fmin(0.95, fmax(0.05, 1.0 - fadeSize));

    return 1.0 - spatial_smooth_step(
        fadeStart,
        1.0,
        radius);
}

__kernel void spatial_overexposure_recover(
    __global const uchar *sourcePixelBytes,
    __global uchar *resultPixelBytes,
    const int imageWidth,
    const int imageHeight,
    const int rectLeft,
    const int rectTop,
    const int rectWidth,
    const int rectHeight,
    const int shape,
    const int softEdge,
    const double brightCutoff,
    const double recoveryAmount,
    const double colorBleed,
    const double transparency,
    const double preserveShine,
    __constant double *sectorColors,
    __constant int *sectorValid)
{
    const uint localIndex = get_global_id(0);
    const uint selectionPixelCount =
        (uint)(rectWidth * rectHeight);

    if (localIndex >= selectionPixelCount) {
        return;
    }

    const int localX =
        (int)(localIndex % (uint)rectWidth);
    const int localY =
        (int)(localIndex / (uint)rectWidth);
    const int x = rectLeft + localX;
    const int y = rectTop + localY;

    const double maskWeight = spatial_region_weight(
        rectLeft,
        rectTop,
        rectWidth,
        rectHeight,
        x,
        y,
        shape,
        softEdge);

    if (maskWeight <= 0.0) {
        return;
    }

    const uint pixelIndex = (uint)((y * imageWidth) + x);
    const uint byteIndex = pixelIndex * 4U;

    const double oldBlue =
        (double)sourcePixelBytes[byteIndex + 0U];
    const double oldGreen =
        (double)sourcePixelBytes[byteIndex + 1U];
    const double oldRed =
        (double)sourcePixelBytes[byteIndex + 2U];

    const double oldLuminance = spatial_photo_luminance(
        oldRed,
        oldGreen,
        oldBlue);

    const double hotness = spatial_smooth_step(
        brightCutoff - 12.0,
        255.0,
        oldLuminance);

    if (hotness <= 0.0) {
        return;
    }

    const double radius = spatial_region_radius(
        rectLeft,
        rectTop,
        rectWidth,
        rectHeight,
        x,
        y,
        shape);

    const double centerWeight = spatial_clamp_unit(
        1.0 - radius);
    const double shineProtection =
        preserveShine * centerWeight * hotness;

    const double correctionWeight =
        hotness *
        maskWeight *
        recoveryAmount *
        transparency *
        (1.0 - (shineProtection * 0.55));

    if (correctionWeight <= 0.0) {
        return;
    }

    double averageRed = 0.0;
    double averageGreen = 0.0;
    double averageBlue = 0.0;
    int validCount = 0;

    for (int sampleIndex = 0;
         sampleIndex < 8;
         ++sampleIndex) {
        if (sectorValid[sampleIndex] == 0) {
            continue;
        }

        averageRed += sectorColors[(sampleIndex * 3) + 0];
        averageGreen += sectorColors[(sampleIndex * 3) + 1];
        averageBlue += sectorColors[(sampleIndex * 3) + 2];
        ++validCount;
    }

    if (validCount > 0) {
        averageRed /= (double)validCount;
        averageGreen /= (double)validCount;
        averageBlue /= (double)validCount;
    } else {
        averageRed = oldRed;
        averageGreen = oldGreen;
        averageBlue = oldBlue;
    }

    const double centerX =
        (double)rectLeft +
        (((double)rectWidth - 1.0) / 2.0);
    const double centerY =
        (double)rectTop +
        (((double)rectHeight - 1.0) / 2.0);
    const double radiusX = fmax(1.0, (double)rectWidth / 2.0);
    const double radiusY = fmax(1.0, (double)rectHeight / 2.0);

    const double normalizedX = fmin(
        1.0,
        fmax(-1.0, ((double)x - centerX) / radiusX));
    const double normalizedY = fmin(
        1.0,
        fmax(-1.0, ((double)y - centerY) / radiusY));

    const double anchorRadius = 1.18;
    const double anchorX[8] = {
        -anchorRadius, 0.0, anchorRadius, anchorRadius,
         anchorRadius, 0.0, -anchorRadius, -anchorRadius
    };
    const double anchorY[8] = {
        -anchorRadius, -anchorRadius, -anchorRadius, 0.0,
         anchorRadius, anchorRadius, anchorRadius, 0.0
    };

    double localRed = 0.0;
    double localGreen = 0.0;
    double localBlue = 0.0;
    double totalWeight = 0.0;

    for (int sampleIndex = 0;
         sampleIndex < 8;
         ++sampleIndex) {
        if (sectorValid[sampleIndex] == 0) {
            continue;
        }

        const double dx = normalizedX - anchorX[sampleIndex];
        const double dy = normalizedY - anchorY[sampleIndex];
        const double distanceSquared = (dx * dx) + (dy * dy);
        const double weight =
            1.0 / pow(distanceSquared + 0.32, 1.20);

        localRed +=
            sectorColors[(sampleIndex * 3) + 0] * weight;
        localGreen +=
            sectorColors[(sampleIndex * 3) + 1] * weight;
        localBlue +=
            sectorColors[(sampleIndex * 3) + 2] * weight;
        totalWeight += weight;
    }

    if (totalWeight > 0.0) {
        localRed /= totalWeight;
        localGreen /= totalWeight;
        localBlue /= totalWeight;
    } else {
        localRed = averageRed;
        localGreen = averageGreen;
        localBlue = averageBlue;
    }

    const double edgeDistance = spatial_clamp_unit(
        fmax(fabs(normalizedX), fabs(normalizedY)));
    const double localInfluence =
        0.20 +
        (0.80 * spatial_smooth_step(
            0.05,
            0.92,
            edgeDistance));

    const double surroundingRed =
        averageRed +
        ((localRed - averageRed) * localInfluence);
    const double surroundingGreen =
        averageGreen +
        ((localGreen - averageGreen) * localInfluence);
    const double surroundingBlue =
        averageBlue +
        ((localBlue - averageBlue) * localInfluence);

    double targetRed =
        oldRed + ((surroundingRed - oldRed) * colorBleed);
    double targetGreen =
        oldGreen + ((surroundingGreen - oldGreen) * colorBleed);
    double targetBlue =
        oldBlue + ((surroundingBlue - oldBlue) * colorBleed);

    const double surroundingLuminance = spatial_photo_luminance(
        surroundingRed,
        surroundingGreen,
        surroundingBlue);

    double targetLuminance =
        surroundingLuminance +
        ((oldLuminance - surroundingLuminance) *
         (0.20 + (shineProtection * 0.50)));

    double neighborhoodRed = 0.0;
    double neighborhoodGreen = 0.0;
    double neighborhoodBlue = 0.0;
    double neighborhoodWeight = 0.0;

    for (int offsetY = -2;
         offsetY <= 2;
         ++offsetY) {
        const int sampleY =
            min(imageHeight - 1, max(0, y + offsetY));

        for (int offsetX = -2;
             offsetX <= 2;
             ++offsetX) {
            const int sampleX =
                min(imageWidth - 1, max(0, x + offsetX));
            const double distanceSquared =
                (double)(offsetX * offsetX) +
                (double)(offsetY * offsetY);
            const double sampleWeight =
                1.0 / (1.0 + distanceSquared);
            const uint sampleByte =
                (uint)((sampleY * imageWidth) + sampleX) * 4U;

            neighborhoodBlue +=
                (double)sourcePixelBytes[sampleByte + 0U] *
                sampleWeight;
            neighborhoodGreen +=
                (double)sourcePixelBytes[sampleByte + 1U] *
                sampleWeight;
            neighborhoodRed +=
                (double)sourcePixelBytes[sampleByte + 2U] *
                sampleWeight;
            neighborhoodWeight += sampleWeight;
        }
    }

    double neighborhoodLuminance = oldLuminance;

    if (neighborhoodWeight > 0.0) {
        neighborhoodLuminance = spatial_photo_luminance(
            neighborhoodRed / neighborhoodWeight,
            neighborhoodGreen / neighborhoodWeight,
            neighborhoodBlue / neighborhoodWeight);
    }

    const double fineDetail = fmin(
        18.0,
        fmax(-18.0,
             oldLuminance - neighborhoodLuminance));

    targetLuminance = fmin(
        255.0,
        fmax(0.0,
             targetLuminance + (fineDetail * 0.18)));

    const double currentTargetLuminance = spatial_photo_luminance(
        targetRed,
        targetGreen,
        targetBlue);
    const double luminanceDelta =
        targetLuminance - currentTargetLuminance;

    targetRed = fmin(255.0, fmax(0.0, targetRed + luminanceDelta));
    targetGreen = fmin(255.0, fmax(0.0, targetGreen + luminanceDelta));
    targetBlue = fmin(255.0, fmax(0.0, targetBlue + luminanceDelta));

    const double finalRed =
        oldRed + ((targetRed - oldRed) * correctionWeight);
    const double finalGreen =
        oldGreen + ((targetGreen - oldGreen) * correctionWeight);
    const double finalBlue =
        oldBlue + ((targetBlue - oldBlue) * correctionWeight);

    resultPixelBytes[byteIndex + 0U] =
        (uchar)spatial_channel_from_double(finalBlue);
    resultPixelBytes[byteIndex + 1U] =
        (uchar)spatial_channel_from_double(finalGreen);
    resultPixelBytes[byteIndex + 2U] =
        (uchar)spatial_channel_from_double(finalRed);
    resultPixelBytes[byteIndex + 3U] =
        sourcePixelBytes[byteIndex + 3U];
}
)CLC";
}

bool imageBufferInformation(
    const QImage &image,
    std::size_t *byteCount,
    cl_uint *pixelCount,
    QString *errorMessage
    )
{
    if (image.isNull() ||
        (image.format() != QImage::Format_ARGB32 &&
         image.format() != QImage::Format_RGB32)) {
        if (errorMessage) {
            *errorMessage =
                "OpenCL spatial processing requires a 32-bit QImage buffer.";
        }
        return false;
    }

    const qsizetype byteCountValue = image.sizeInBytes();

    if (byteCountValue <= 0 ||
        (byteCountValue % 4) != 0) {
        if (errorMessage) {
            *errorMessage =
                "The image buffer has an unsupported byte size.";
        }
        return false;
    }

    const quint64 pixelCount64 =
        static_cast<quint64>(image.width()) *
        static_cast<quint64>(image.height());

    if (pixelCount64 >
        static_cast<quint64>(
            std::numeric_limits<cl_uint>::max()
            )) {
        if (errorMessage) {
            *errorMessage =
                "The image is too large for this OpenCL spatial kernel.";
        }
        return false;
    }

    if (byteCount) {
        *byteCount =
            static_cast<std::size_t>(byteCountValue);
    }

    if (pixelCount) {
        *pixelCount =
            static_cast<cl_uint>(pixelCount64);
    }

    return true;
}
}

class OpenClSpatialProcessingBackend::Implementation
{
public:
    ~Implementation()
    {
        shutdownUnlocked();
    }

    void initialize()
    {
        QMutexLocker locker(&mutex);

        if (initializationAttempted) {
            return;
        }

        initializationAttempted = true;

        if (QSysInfo::ByteOrder != QSysInfo::LittleEndian) {
            status =
                "OpenCL spatial processing is disabled on big-endian systems.";
            return;
        }

        if (!loadLibrary() ||
            !resolveFunctions() ||
            !selectGpuDevice() ||
            !supportsFp64 ||
            !createContextAndKernels() ||
            !runSmokeTest()) {
            const QString failureStatus = status;
            shutdownUnlocked();
            status = failureStatus;
            return;
        }

        available = true;
        status = QString(
            "OpenCL spatial processing ready on %1"
            ).arg(deviceDisplayName);
    }

    void shutdown()
    {
        QMutexLocker locker(&mutex);
        shutdownUnlocked();
        initializationAttempted = false;
        status =
            "OpenCL spatial processing has not been initialized.";
    }

    bool isAvailable() const
    {
        QMutexLocker locker(&mutex);
        return available;
    }

    QString deviceName() const
    {
        QMutexLocker locker(&mutex);
        return deviceDisplayName;
    }

    QString statusText() const
    {
        QMutexLocker locker(&mutex);
        return status;
    }

    bool applyNoiseReduction(
        QImage &image,
        const OpenClSpatialProcessingBackend::NoiseSettings &settings,
        QString *errorMessage
        )
    {
        QMutexLocker locker(&mutex);

        if (!available) {
            if (errorMessage) {
                *errorMessage = status;
            }
            return false;
        }

        QString executionError;

        if (!executeNoiseReduction(
                image,
                settings,
                &executionError
                )) {
            if (isNonFatalFallback(executionError)) {
                if (errorMessage) {
                    *errorMessage = executionError;
                }
                return false;
            }

            disableAfterFailure(
                "Noise Reduction",
                executionError,
                errorMessage
                );
            return false;
        }

        return true;
    }

    bool applyFocus(
        QImage &image,
        const OpenClSpatialProcessingBackend::FocusSettings &settings,
        QString *errorMessage
        )
    {
        QMutexLocker locker(&mutex);

        if (!available) {
            if (errorMessage) {
                *errorMessage = status;
            }
            return false;
        }

        QString executionError;

        if (!executeFocus(
                image,
                settings,
                &executionError
                )) {
            if (isNonFatalFallback(executionError)) {
                if (errorMessage) {
                    *errorMessage = executionError;
                }
                return false;
            }

            disableAfterFailure(
                "Focus/Sharpen",
                executionError,
                errorMessage
                );
            return false;
        }

        return true;
    }

    bool applyOverexposureRecovery(
        QImage &image,
        const QRect &selection,
        const OpenClSpatialProcessingBackend::OverexposureSettings &settings,
        QString *errorMessage
        )
    {
        QMutexLocker locker(&mutex);

        if (!available) {
            if (errorMessage) {
                *errorMessage = status;
            }
            return false;
        }

        QString executionError;

        if (!executeOverexposureRecovery(
                image,
                selection,
                settings,
                &executionError
                )) {
            if (isNonFatalFallback(executionError)) {
                if (errorMessage) {
                    *errorMessage = executionError;
                }
                return false;
            }

            disableAfterFailure(
                "Overexposure Recovery",
                executionError,
                errorMessage
                );
            return false;
        }

        return true;
    }

private:
    template<typename FunctionPointer>
    bool resolve(
        FunctionPointer *functionPointer,
        const char *symbolName
        )
    {
        if (!functionPointer) {
            return false;
        }

        *functionPointer = reinterpret_cast<FunctionPointer>(
            library.resolve(symbolName)
            );

        if (!*functionPointer) {
            status = QString(
                "The OpenCL loader is missing required symbol %1."
                ).arg(QString::fromLatin1(symbolName));
            return false;
        }

        return true;
    }

    bool loadLibrary()
    {
        QStringList candidates;

#ifdef Q_OS_WIN
        candidates << "OpenCL";
#elif defined(Q_OS_MACOS) || defined(Q_OS_MAC)
        candidates
            << "/System/Library/Frameworks/OpenCL.framework/OpenCL"
            << "OpenCL";
#else
        candidates
            << "OpenCL"
            << "libOpenCL.so.1"
            << "/lib/x86_64-linux-gnu/libOpenCL.so.1"
            << "/usr/lib/x86_64-linux-gnu/libOpenCL.so.1";
#endif

        for (const QString &candidate : candidates) {
            library.setFileName(candidate);

            if (library.load()) {
                return true;
            }
        }

        status =
            "No OpenCL runtime library was found for spatial processing.";
        return false;
    }

    bool resolveFunctions()
    {
        return
            resolve(&clGetPlatformIDs, "clGetPlatformIDs") &&
            resolve(&clGetPlatformInfo, "clGetPlatformInfo") &&
            resolve(&clGetDeviceIDs, "clGetDeviceIDs") &&
            resolve(&clGetDeviceInfo, "clGetDeviceInfo") &&
            resolve(&clCreateContext, "clCreateContext") &&
            resolve(&clReleaseContext, "clReleaseContext") &&
            resolve(&clCreateCommandQueue, "clCreateCommandQueue") &&
            resolve(&clReleaseCommandQueue, "clReleaseCommandQueue") &&
            resolve(&clCreateProgramWithSource, "clCreateProgramWithSource") &&
            resolve(&clBuildProgram, "clBuildProgram") &&
            resolve(&clGetProgramBuildInfo, "clGetProgramBuildInfo") &&
            resolve(&clReleaseProgram, "clReleaseProgram") &&
            resolve(&clCreateKernel, "clCreateKernel") &&
            resolve(&clReleaseKernel, "clReleaseKernel") &&
            resolve(&clCreateBuffer, "clCreateBuffer") &&
            resolve(&clReleaseMemObject, "clReleaseMemObject") &&
            resolve(&clSetKernelArg, "clSetKernelArg") &&
            resolve(&clEnqueueNDRangeKernel, "clEnqueueNDRangeKernel") &&
            resolve(&clEnqueueReadBuffer, "clEnqueueReadBuffer") &&
            resolve(&clFinish, "clFinish");
    }

    QByteArray queryPlatformBytes(
        cl_platform_id platform,
        cl_platform_info parameter
        ) const
    {
        if (!platform ||
            !clGetPlatformInfo) {
            return QByteArray();
        }

        std::size_t byteCount = 0;

        if (clGetPlatformInfo(
                platform,
                parameter,
                0,
                nullptr,
                &byteCount
                ) != CL_SUCCESS ||
            byteCount == 0) {
            return QByteArray();
        }

        QByteArray result(
            static_cast<qsizetype>(byteCount),
            '\0'
            );

        if (clGetPlatformInfo(
                platform,
                parameter,
                byteCount,
                result.data(),
                nullptr
                ) != CL_SUCCESS) {
            return QByteArray();
        }

        return result;
    }

    static int candidatePreferenceScore(
        const QString &platformName,
        const QString &platformVendor,
        const QString &deviceName,
        const QString &deviceVendor
        )
    {
        const QString identity =
            (platformName + " " + platformVendor + " " +
             deviceName + " " + deviceVendor).toLower();

        // Prefer vendor-supported compute runtimes first, then Rusticl. The
        // older Mesa/Clover provider remains a last-resort candidate because
        // some versions have returned incorrect byte/alpha results in the
        // exact On Point self-tests. Intel's official compute runtime is
        // preferred over Rusticl when both expose the same Intel GPU.
        if (identity.contains("nvidia")) {
            return 600;
        }

        if (platformName.contains(
                "AMD Accelerated",
                Qt::CaseInsensitive
                ) ||
            platformName.contains(
                "ROCm",
                Qt::CaseInsensitive
                )) {
            return 550;
        }

        if (identity.contains("intel") &&
            !platformName.contains(
                "rusticl",
                Qt::CaseInsensitive
                ) &&
            !platformName.contains(
                "Clover",
                Qt::CaseInsensitive
                ) &&
            !platformName.contains(
                "Mesa",
                Qt::CaseInsensitive
                )) {
            return 500;
        }

        if (platformName.contains(
                "rusticl",
                Qt::CaseInsensitive
                )) {
            return 400;
        }

        if (platformName.contains(
                "Clover",
                Qt::CaseInsensitive
                ) ||
            (platformName.contains(
                 "Mesa",
                 Qt::CaseInsensitive
                 ) &&
             !platformName.contains(
                 "rusticl",
                 Qt::CaseInsensitive
                 ))) {
            return 100;
        }

        return 200;
    }

    QByteArray queryDeviceBytes(
        cl_device_info parameter
        ) const
    {
        if (!selectedDevice ||
            !clGetDeviceInfo) {
            return QByteArray();
        }

        std::size_t byteCount = 0;

        if (clGetDeviceInfo(
                selectedDevice,
                parameter,
                0,
                nullptr,
                &byteCount
                ) != CL_SUCCESS ||
            byteCount == 0) {
            return QByteArray();
        }

        QByteArray result(
            static_cast<qsizetype>(byteCount),
            '\0'
            );

        if (clGetDeviceInfo(
                selectedDevice,
                parameter,
                byteCount,
                result.data(),
                nullptr
                ) != CL_SUCCESS) {
            return QByteArray();
        }

        return result;
    }

    template<typename ValueType>
    bool queryDeviceValue(
        cl_device_info parameter,
        ValueType *value
        ) const
    {
        if (!selectedDevice ||
            !clGetDeviceInfo ||
            !value) {
            return false;
        }

        return clGetDeviceInfo(
            selectedDevice,
            parameter,
            sizeof(ValueType),
            value,
            nullptr
            ) == CL_SUCCESS;
    }

    bool selectGpuDevice()
    {
        cl_uint platformCount = 0;
        cl_int error = clGetPlatformIDs(
            0,
            nullptr,
            &platformCount
            );

        if (error != CL_SUCCESS ||
            platformCount == 0) {
            status =
                "OpenCL did not report a spatial-compute platform. On Debian/LMDE, install an OpenCL ICD: nvidia-opencl-icd for NVIDIA or mesa-opencl-icd for AMD or Intel Mesa/Rusticl.";
            return false;
        }

        std::vector<cl_platform_id> platforms(platformCount);

        error = clGetPlatformIDs(
            platformCount,
            platforms.data(),
            nullptr
            );

        if (error != CL_SUCCESS) {
            status =
                "OpenCL spatial platform enumeration failed: " +
                openClErrorText(error);
            return false;
        }

        struct Candidate
        {
            cl_device_id device = nullptr;
            cl_ulong globalMemory = 0;
            QString name;
            QString vendor;
            QString platformName;
            QString platformVendor;
            int preferenceScore = 0;
        };

        std::vector<Candidate> candidates;

        for (cl_platform_id platform : platforms) {
            const QString platformName = cleanedInfoString(
                queryPlatformBytes(platform, CL_PLATFORM_NAME)
                );
            const QString platformVendor = cleanedInfoString(
                queryPlatformBytes(platform, CL_PLATFORM_VENDOR)
                );

            cl_uint deviceCount = 0;

            error = clGetDeviceIDs(
                platform,
                CL_DEVICE_TYPE_GPU,
                0,
                nullptr,
                &deviceCount
                );

            if (error != CL_SUCCESS ||
                deviceCount == 0) {
                continue;
            }

            std::vector<cl_device_id> devices(deviceCount);

            error = clGetDeviceIDs(
                platform,
                CL_DEVICE_TYPE_GPU,
                deviceCount,
                devices.data(),
                nullptr
                );

            if (error != CL_SUCCESS) {
                continue;
            }

            for (cl_device_id device : devices) {
                selectedDevice = device;

                cl_bool deviceAvailable = 0;
                cl_bool compilerAvailable = 0;
                cl_ulong globalMemory = 0;

                queryDeviceValue(
                    CL_DEVICE_AVAILABLE,
                    &deviceAvailable
                    );
                queryDeviceValue(
                    CL_DEVICE_COMPILER_AVAILABLE,
                    &compilerAvailable
                    );
                queryDeviceValue(
                    CL_DEVICE_GLOBAL_MEM_SIZE,
                    &globalMemory
                    );

                if (!deviceAvailable ||
                    !compilerAvailable) {
                    continue;
                }

                Candidate candidate;
                candidate.device = device;
                candidate.globalMemory = globalMemory;
                candidate.name = cleanedInfoString(
                    queryDeviceBytes(CL_DEVICE_NAME)
                    );
                candidate.vendor = cleanedInfoString(
                    queryDeviceBytes(CL_DEVICE_VENDOR)
                    );
                candidate.platformName = platformName;
                candidate.platformVendor = platformVendor;
                candidate.preferenceScore = candidatePreferenceScore(
                    candidate.platformName,
                    candidate.platformVendor,
                    candidate.name,
                    candidate.vendor
                    );

                candidates.push_back(candidate);
            }
        }

        selectedDevice = nullptr;

        if (candidates.empty()) {
            status =
                "No usable OpenCL GPU was found for spatial processing.";
            return false;
        }

        const auto preferredCandidate =
            std::max_element(
                candidates.begin(),
                candidates.end(),
                [](const Candidate &left,
                   const Candidate &right) {
                    if (left.preferenceScore !=
                        right.preferenceScore) {
                        return left.preferenceScore <
                            right.preferenceScore;
                    }

                    return left.globalMemory <
                        right.globalMemory;
                }
                );

        selectedDevice = preferredCandidate->device;
        globalMemoryBytes =
            static_cast<std::size_t>(
                std::min<cl_ulong>(
                    preferredCandidate->globalMemory,
                    static_cast<cl_ulong>(
                        std::numeric_limits<std::size_t>::max()
                        )
                    )
                );

        const QString rawDeviceName =
            preferredCandidate->name.isEmpty()
            ? QString("Unnamed GPU")
            : preferredCandidate->name;

        const QString vendorName =
            preferredCandidate->vendor;

        deviceDisplayName =
            vendorName.isEmpty() ||
            rawDeviceName.contains(
                vendorName,
                Qt::CaseInsensitive
                )
            ? rawDeviceName
            : vendorName + " " + rawDeviceName;

        if (!preferredCandidate->platformName.isEmpty() &&
            !deviceDisplayName.contains(
                preferredCandidate->platformName,
                Qt::CaseInsensitive
                )) {
            deviceDisplayName +=
                " via " + preferredCandidate->platformName;
        }

        cl_ulong maximumAllocation = 0;

        if (queryDeviceValue(
                CL_DEVICE_MAX_MEM_ALLOC_SIZE,
                &maximumAllocation
                )) {
            maximumAllocationBytes =
                static_cast<std::size_t>(
                    std::min<cl_ulong>(
                        maximumAllocation,
                        static_cast<cl_ulong>(
                            std::numeric_limits<std::size_t>::max()
                            )
                        )
                    );
        }

        cl_ulong doublePrecisionConfig = 0;
        const bool reportsDoubleConfig =
            queryDeviceValue(
                CL_DEVICE_DOUBLE_FP_CONFIG,
                &doublePrecisionConfig
                ) &&
            doublePrecisionConfig != 0;

        const QString extensionText =
            cleanedInfoString(
                queryDeviceBytes(CL_DEVICE_EXTENSIONS)
                );

        supportsFp64 =
            reportsDoubleConfig ||
            extensionText.contains(
                "cl_khr_fp64",
                Qt::CaseInsensitive
                );

        if (!supportsFp64) {
            status =
                "The OpenCL GPU does not report double-precision support; "
                "exact spatial image math remains on the CPU.";
            return false;
        }

        return true;
    }

    bool createContextAndKernels()
    {
        cl_int error = CL_SUCCESS;

        context = clCreateContext(
            nullptr,
            1,
            &selectedDevice,
            nullptr,
            nullptr,
            &error
            );

        if (!context ||
            error != CL_SUCCESS) {
            status =
                "OpenCL spatial context creation failed: " +
                openClErrorText(error);
            return false;
        }

        commandQueue = clCreateCommandQueue(
            context,
            selectedDevice,
            0,
            &error
            );

        if (!commandQueue ||
            error != CL_SUCCESS) {
            status =
                "OpenCL spatial command queue creation failed: " +
                openClErrorText(error);
            return false;
        }

        const char *source = spatialKernelSource();
        const std::size_t sourceLength =
            std::strlen(source);

        program = clCreateProgramWithSource(
            context,
            1,
            &source,
            &sourceLength,
            &error
            );

        if (!program ||
            error != CL_SUCCESS) {
            status =
                "OpenCL spatial program creation failed: " +
                openClErrorText(error);
            return false;
        }

        error = clBuildProgram(
            program,
            1,
            &selectedDevice,
            nullptr,
            nullptr,
            nullptr
            );

        if (error != CL_SUCCESS) {
            std::size_t logSize = 0;
            clGetProgramBuildInfo(
                program,
                selectedDevice,
                CL_PROGRAM_BUILD_LOG,
                0,
                nullptr,
                &logSize
                );

            QByteArray buildLog(
                static_cast<qsizetype>(logSize),
                '\0'
                );

            if (logSize > 0) {
                clGetProgramBuildInfo(
                    program,
                    selectedDevice,
                    CL_PROGRAM_BUILD_LOG,
                    logSize,
                    buildLog.data(),
                    nullptr
                    );
            }

            status =
                "OpenCL spatial kernel build failed: " +
                openClErrorText(error);

            const QString cleanedLog =
                cleanedInfoString(buildLog);

            if (!cleanedLog.isEmpty()) {
                status += " — " + cleanedLog;
            }

            return false;
        }

        struct KernelRequest
        {
            const char *name = nullptr;
            cl_kernel *destination = nullptr;
        };

        const KernelRequest requests[] = {
            {"spatial_encode_ycbcr", &encodeKernel},
            {"spatial_noise_edge_pass", &noiseEdgeKernel},
            {"spatial_noise_finish", &noiseFinishKernel},
            {"spatial_focus_horizontal", &focusHorizontalKernel},
            {"spatial_focus_vertical", &focusVerticalKernel},
            {"spatial_focus_finish", &focusFinishKernel},
            {"spatial_overexposure_recover", &overexposureKernel}
        };

        for (const KernelRequest &request : requests) {
            error = CL_SUCCESS;
            *request.destination = clCreateKernel(
                program,
                request.name,
                &error
                );

            if (!*request.destination ||
                error != CL_SUCCESS) {
                status = QString(
                    "OpenCL spatial kernel creation failed for %1: %2"
                    )
                    .arg(QString::fromLatin1(request.name))
                    .arg(openClErrorText(error));
                return false;
            }
        }

        return true;
    }

    bool runSmokeTest()
    {
        QImage focusImage(
            8,
            8,
            QImage::Format_ARGB32
            );

        for (int y = 0;
             y < focusImage.height();
             ++y) {
            QRgb *line = reinterpret_cast<QRgb *>(
                focusImage.scanLine(y)
                );

            for (int x = 0;
                 x < focusImage.width();
                 ++x) {
                line[x] = qRgba(
                    30 + (x * 20),
                    40 + (y * 18),
                    70 + ((x + y) * 8),
                    80 + ((x * 13 + y * 7) % 176)
                    );
            }
        }

        const QImage original = focusImage;

        OpenClSpatialProcessingBackend::FocusSettings settings;
        settings.amount = 18;
        settings.radius = 10;
        settings.threshold = 24;
        settings.edgeProtection = 72;
        settings.blend = 85;

        QString errorMessage;

        if (!executeFocus(
                focusImage,
                settings,
                &errorMessage
                )) {
            status =
                "OpenCL spatial self-test failed: " +
                errorMessage;
            return false;
        }

        for (int y = 0;
             y < focusImage.height();
             ++y) {
            const QRgb *originalLine =
                reinterpret_cast<const QRgb *>(
                    original.constScanLine(y)
                    );
            const QRgb *resultLine =
                reinterpret_cast<const QRgb *>(
                    focusImage.constScanLine(y)
                    );

            for (int x = 0;
                 x < focusImage.width();
                 ++x) {
                if (qAlpha(originalLine[x]) !=
                    qAlpha(resultLine[x])) {
                    status =
                        "OpenCL spatial self-test changed alpha data.";
                    return false;
                }

                // Focus is a luminance-only operation. The red-green and
                // blue-green channel differences must remain stable. This
                // catches catastrophic color casts even when a kernel runs
                // successfully and preserves alpha.
                const int originalRedGreen =
                    qRed(originalLine[x]) -
                    qGreen(originalLine[x]);
                const int resultRedGreen =
                    qRed(resultLine[x]) -
                    qGreen(resultLine[x]);
                const int originalBlueGreen =
                    qBlue(originalLine[x]) -
                    qGreen(originalLine[x]);
                const int resultBlueGreen =
                    qBlue(resultLine[x]) -
                    qGreen(resultLine[x]);

                if (std::abs(resultRedGreen - originalRedGreen) > 2 ||
                    std::abs(resultBlueGreen - originalBlueGreen) > 2) {
                    status =
                        "OpenCL spatial self-test detected a Focus color cast.";
                    return false;
                }
            }
        }

        return true;
    }

    bool ensureMemoryBudget(
        std::size_t largestAllocation,
        std::size_t estimatedTotal,
        QString *errorMessage
        ) const
    {
        if (maximumAllocationBytes > 0 &&
            largestAllocation > maximumAllocationBytes) {
            if (errorMessage) {
                *errorMessage = QString(
                    "The largest spatial buffer requires %1 MiB, exceeding "
                    "the GPU's single-allocation limit of %2 MiB."
                    )
                    .arg(largestAllocation / (1024.0 * 1024.0), 0, 'f', 1)
                    .arg(maximumAllocationBytes / (1024.0 * 1024.0), 0, 'f', 1);
            }
            return false;
        }

        if (globalMemoryBytes > 0) {
            const std::size_t safeBudget =
                static_cast<std::size_t>(
                    globalMemoryBytes * 0.70
                    );

            if (estimatedTotal > safeBudget) {
                if (errorMessage) {
                    *errorMessage = QString(
                        "Spatial processing estimates %1 MiB of GPU memory, "
                        "above the safe %2 MiB budget for this device."
                        )
                        .arg(estimatedTotal / (1024.0 * 1024.0), 0, 'f', 1)
                        .arg(safeBudget / (1024.0 * 1024.0), 0, 'f', 1);
                }
                return false;
            }
        }

        return true;
    }

    bool setKernelArgument(
        cl_kernel kernel,
        cl_uint index,
        std::size_t size,
        const void *value,
        cl_int *error
        )
    {
        if (!error ||
            *error != CL_SUCCESS) {
            return false;
        }

        *error = clSetKernelArg(
            kernel,
            index,
            size,
            value
            );

        return *error == CL_SUCCESS;
    }

    bool launchKernel(
        cl_kernel kernel,
        std::size_t globalWorkSize,
        QString *errorMessage
        )
    {
        const cl_int error = clEnqueueNDRangeKernel(
            commandQueue,
            kernel,
            1,
            nullptr,
            &globalWorkSize,
            nullptr,
            0,
            nullptr,
            nullptr
            );

        if (error != CL_SUCCESS) {
            if (errorMessage) {
                *errorMessage =
                    "Kernel launch failed: " +
                    openClErrorText(error);
            }
            return false;
        }

        return true;
    }

    bool readResultBuffer(
        cl_mem buffer,
        std::size_t byteCount,
        void *destination,
        QString *errorMessage
        )
    {
        std::vector<cl_uchar> resultBytes;

        try {
            resultBytes.resize(byteCount);
        } catch (const std::bad_alloc &) {
            if (errorMessage) {
                *errorMessage =
                    "Not enough host memory was available for the GPU result buffer.";
            }
            return false;
        }

        cl_int error = clEnqueueReadBuffer(
            commandQueue,
            buffer,
            CL_TRUE,
            0,
            byteCount,
            resultBytes.data(),
            0,
            nullptr,
            nullptr
            );

        if (error == CL_SUCCESS) {
            error = clFinish(commandQueue);
        }

        if (error != CL_SUCCESS) {
            if (errorMessage) {
                *errorMessage =
                    "GPU result transfer failed: " +
                    openClErrorText(error);
            }
            return false;
        }

        std::memcpy(
            destination,
            resultBytes.data(),
            byteCount
            );

        return true;
    }

    bool executeNoiseReduction(
        QImage &image,
        const OpenClSpatialProcessingBackend::NoiseSettings &inputSettings,
        QString *errorMessage
        )
    {
        std::size_t byteCount = 0;
        cl_uint pixelCount = 0;

        if (!imageBufferInformation(
                image,
                &byteCount,
                &pixelCount,
                errorMessage
                )) {
            return false;
        }

        const int luminanceAmount = std::clamp(
            inputSettings.luminanceNoise,
            0,
            100
            );
        const int colorAmount = std::clamp(
            inputSettings.colorNoise,
            0,
            100
            );
        const int detailProtection = std::clamp(
            inputSettings.detailProtection,
            0,
            100
            );
        const int blendAmount = std::clamp(
            inputSettings.blend,
            0,
            100
            );

        if (blendAmount <= 0 ||
            (luminanceAmount <= 0 &&
             colorAmount <= 0)) {
            return true;
        }

        if (!ensureMemoryBudget(
                byteCount,
                byteCount * 4U + 4096U,
                errorMessage
                )) {
            return false;
        }

        const int colorRadius = std::clamp(
            1 + (colorAmount / 26),
            1,
            4
            );
        const double colorRangeSigma =
            14.0 +
            ((100 - detailProtection) * 0.34) +
            (colorAmount * 0.12);

        const int luminanceRadius = std::clamp(
            1 + (luminanceAmount / 26),
            1,
            4
            );
        const double luminanceRangeSigma =
            4.5 +
            ((100 - detailProtection) * 0.18) +
            (luminanceAmount * 0.10);

        const auto amountToMix = [](int amount) {
            amount = std::clamp(amount, 0, 100);

            if (amount == 0) {
                return 0.0;
            }

            return 1.0 -
                std::exp(-amount / 36.0);
        };

        const double overallBlend =
            blendAmount / 100.0;
        const double luminanceMixBase =
            amountToMix(luminanceAmount) *
            overallBlend;
        const double colorMixBase =
            amountToMix(colorAmount) *
            overallBlend;
        const double detail =
            detailProtection / 100.0;

        const std::array<cl_int, 256> colorWeights =
            makeRangeWeights(colorRangeSigma);
        const std::array<cl_int, 256> luminanceWeights =
            makeRangeWeights(luminanceRangeSigma);

        cl_int error = CL_SUCCESS;
        cl_mem resultBuffer = nullptr;
        cl_mem originalEncodedBuffer = nullptr;
        cl_mem workBufferA = nullptr;
        cl_mem workBufferB = nullptr;
        cl_mem colorWeightsBuffer = nullptr;
        cl_mem luminanceWeightsBuffer = nullptr;

        auto releaseBuffers = [this,
                               &resultBuffer,
                               &originalEncodedBuffer,
                               &workBufferA,
                               &workBufferB,
                               &colorWeightsBuffer,
                               &luminanceWeightsBuffer]() {
            cl_mem *buffers[] = {
                &luminanceWeightsBuffer,
                &colorWeightsBuffer,
                &workBufferB,
                &workBufferA,
                &originalEncodedBuffer,
                &resultBuffer
            };

            for (cl_mem *buffer : buffers) {
                if (*buffer) {
                    clReleaseMemObject(*buffer);
                    *buffer = nullptr;
                }
            }
        };

        resultBuffer = clCreateBuffer(
            context,
            CL_MEM_READ_WRITE |
                CL_MEM_COPY_HOST_PTR,
            byteCount,
            image.bits(),
            &error
            );

        if (!resultBuffer ||
            error != CL_SUCCESS) {
            if (errorMessage) {
                *errorMessage =
                    "Noise result buffer creation failed: " +
                    openClErrorText(error);
            }
            releaseBuffers();
            return false;
        }

        auto createImageBuffer = [this,
                                  byteCount,
                                  &error]() {
            return clCreateBuffer(
                context,
                CL_MEM_READ_WRITE,
                byteCount,
                nullptr,
                &error
                );
        };

        originalEncodedBuffer = createImageBuffer();
        workBufferA = createImageBuffer();
        workBufferB = createImageBuffer();

        if (!originalEncodedBuffer ||
            !workBufferA ||
            !workBufferB ||
            error != CL_SUCCESS) {
            if (errorMessage) {
                *errorMessage =
                    "Noise working-buffer creation failed: " +
                    openClErrorText(error);
            }
            releaseBuffers();
            return false;
        }

        if (colorAmount > 0) {
            colorWeightsBuffer = clCreateBuffer(
                context,
                CL_MEM_READ_ONLY |
                    CL_MEM_COPY_HOST_PTR,
                sizeof(colorWeights),
                const_cast<cl_int *>(colorWeights.data()),
                &error
                );
        }

        if (error == CL_SUCCESS &&
            luminanceAmount > 0) {
            luminanceWeightsBuffer = clCreateBuffer(
                context,
                CL_MEM_READ_ONLY |
                    CL_MEM_COPY_HOST_PTR,
                sizeof(luminanceWeights),
                const_cast<cl_int *>(luminanceWeights.data()),
                &error
                );
        }

        if (error != CL_SUCCESS ||
            (colorAmount > 0 && !colorWeightsBuffer) ||
            (luminanceAmount > 0 && !luminanceWeightsBuffer)) {
            if (errorMessage) {
                *errorMessage =
                    "Noise weight-buffer creation failed: " +
                    openClErrorText(error);
            }
            releaseBuffers();
            return false;
        }

        error = CL_SUCCESS;
        setKernelArgument(
            encodeKernel,
            0,
            sizeof(cl_mem),
            &resultBuffer,
            &error
            );
        setKernelArgument(
            encodeKernel,
            1,
            sizeof(cl_mem),
            &originalEncodedBuffer,
            &error
            );
        setKernelArgument(
            encodeKernel,
            2,
            sizeof(cl_uint),
            &pixelCount,
            &error
            );

        if (error != CL_SUCCESS ||
            !launchKernel(
                encodeKernel,
                static_cast<std::size_t>(pixelCount),
                errorMessage
                )) {
            if (error != CL_SUCCESS && errorMessage) {
                *errorMessage =
                    "Noise encode argument setup failed: " +
                    openClErrorText(error);
            }
            releaseBuffers();
            return false;
        }

        const int width = image.width();
        const int height = image.height();

        cl_mem currentBuffer = originalEncodedBuffer;

        auto runNoisePass = [this,
                             width,
                             height,
                             pixelCount,
                             errorMessage](
                                cl_mem inputBuffer,
                                cl_mem outputBuffer,
                                int horizontal,
                                int radius,
                                cl_mem weightsBuffer,
                                int filterLuminance,
                                int filterChroma) {
            cl_int passError = CL_SUCCESS;

            setKernelArgument(noiseEdgeKernel, 0, sizeof(cl_mem), &inputBuffer, &passError);
            setKernelArgument(noiseEdgeKernel, 1, sizeof(cl_mem), &outputBuffer, &passError);
            setKernelArgument(noiseEdgeKernel, 2, sizeof(cl_int), &width, &passError);
            setKernelArgument(noiseEdgeKernel, 3, sizeof(cl_int), &height, &passError);
            setKernelArgument(noiseEdgeKernel, 4, sizeof(cl_int), &horizontal, &passError);
            setKernelArgument(noiseEdgeKernel, 5, sizeof(cl_int), &radius, &passError);
            setKernelArgument(noiseEdgeKernel, 6, sizeof(cl_mem), &weightsBuffer, &passError);
            setKernelArgument(noiseEdgeKernel, 7, sizeof(cl_int), &filterLuminance, &passError);
            setKernelArgument(noiseEdgeKernel, 8, sizeof(cl_int), &filterChroma, &passError);

            if (passError != CL_SUCCESS) {
                if (errorMessage) {
                    *errorMessage =
                        "Noise-pass argument setup failed: " +
                        openClErrorText(passError);
                }
                return false;
            }

            return launchKernel(
                noiseEdgeKernel,
                static_cast<std::size_t>(pixelCount),
                errorMessage
                );
        };

        if (colorAmount > 0) {
            if (!runNoisePass(
                    currentBuffer,
                    workBufferA,
                    1,
                    colorRadius,
                    colorWeightsBuffer,
                    0,
                    1
                    ) ||
                !runNoisePass(
                    workBufferA,
                    workBufferB,
                    0,
                    colorRadius,
                    colorWeightsBuffer,
                    0,
                    1
                    )) {
                releaseBuffers();
                return false;
            }

            currentBuffer = workBufferB;
        }

        if (luminanceAmount > 0) {
            if (!runNoisePass(
                    currentBuffer,
                    workBufferA,
                    1,
                    luminanceRadius,
                    luminanceWeightsBuffer,
                    1,
                    0
                    ) ||
                !runNoisePass(
                    workBufferA,
                    workBufferB,
                    0,
                    luminanceRadius,
                    luminanceWeightsBuffer,
                    1,
                    0
                    )) {
                releaseBuffers();
                return false;
            }

            currentBuffer = workBufferB;
        }

        error = CL_SUCCESS;
        setKernelArgument(noiseFinishKernel, 0, sizeof(cl_mem), &resultBuffer, &error);
        setKernelArgument(noiseFinishKernel, 1, sizeof(cl_mem), &originalEncodedBuffer, &error);
        setKernelArgument(noiseFinishKernel, 2, sizeof(cl_mem), &currentBuffer, &error);
        setKernelArgument(noiseFinishKernel, 3, sizeof(cl_int), &width, &error);
        setKernelArgument(noiseFinishKernel, 4, sizeof(cl_int), &height, &error);
        setKernelArgument(noiseFinishKernel, 5, sizeof(cl_double), &luminanceMixBase, &error);
        setKernelArgument(noiseFinishKernel, 6, sizeof(cl_double), &colorMixBase, &error);
        setKernelArgument(noiseFinishKernel, 7, sizeof(cl_double), &detail, &error);

        if (error != CL_SUCCESS) {
            if (errorMessage) {
                *errorMessage =
                    "Noise finish argument setup failed: " +
                    openClErrorText(error);
            }
            releaseBuffers();
            return false;
        }

        if (!launchKernel(
                noiseFinishKernel,
                static_cast<std::size_t>(pixelCount),
                errorMessage
                )) {
            releaseBuffers();
            return false;
        }

        const bool success = readResultBuffer(
            resultBuffer,
            byteCount,
            image.bits(),
            errorMessage
            );

        releaseBuffers();
        return success;
    }

    bool executeFocus(
        QImage &image,
        const OpenClSpatialProcessingBackend::FocusSettings &inputSettings,
        QString *errorMessage
        )
    {
        std::size_t byteCount = 0;
        cl_uint pixelCount = 0;

        if (!imageBufferInformation(
                image,
                &byteCount,
                &pixelCount,
                errorMessage
                )) {
            return false;
        }

        const int amount = std::clamp(
            inputSettings.amount,
            0,
            100
            );
        const int radiusControl = std::clamp(
            inputSettings.radius,
            1,
            50
            );
        const int thresholdControl = std::clamp(
            inputSettings.threshold,
            0,
            100
            );
        const int edgeProtection = std::clamp(
            inputSettings.edgeProtection,
            0,
            100
            );
        const int blendAmount = std::clamp(
            inputSettings.blend,
            0,
            100
            );

        if (amount <= 0 ||
            blendAmount <= 0) {
            return true;
        }

        const std::size_t oneChannelBytes =
            static_cast<std::size_t>(pixelCount);

        if (!ensureMemoryBudget(
                byteCount,
                (byteCount * 2U) +
                    (oneChannelBytes * 3U) +
                    4096U,
                errorMessage
                )) {
            return false;
        }

        const double sigma =
            0.42 +
            ((radiusControl - 1) / 49.0) *
            2.58;

        int kernelRadius = 1;
        const std::vector<cl_int> kernelWeights =
            makeGaussianKernel(
                sigma,
                kernelRadius
                );

        const double amountScale =
            (amount / 100.0) *
            3.40;
        const double threshold =
            thresholdControl *
            0.14;
        const double thresholdSoftness =
            1.8 +
            (sigma * 1.4);
        const double edgeProtectionStrength =
            edgeProtection / 100.0;
        const double maximumDelta =
            4.0 +
            ((100 - edgeProtection) / 100.0) *
            12.0 +
            (amount / 100.0) *
            10.0;
        const double blend =
            blendAmount / 100.0;

        cl_int error = CL_SUCCESS;
        cl_mem sourceBuffer = nullptr;
        cl_mem resultBuffer = nullptr;
        cl_mem horizontalLuminanceBuffer = nullptr;
        cl_mem horizontalAlphaBuffer = nullptr;
        cl_mem blurredLuminanceBuffer = nullptr;
        cl_mem kernelBuffer = nullptr;

        auto releaseBuffers = [this,
                               &sourceBuffer,
                               &resultBuffer,
                               &horizontalLuminanceBuffer,
                               &horizontalAlphaBuffer,
                               &blurredLuminanceBuffer,
                               &kernelBuffer]() {
            cl_mem *buffers[] = {
                &kernelBuffer,
                &blurredLuminanceBuffer,
                &horizontalAlphaBuffer,
                &horizontalLuminanceBuffer,
                &resultBuffer,
                &sourceBuffer
            };

            for (cl_mem *buffer : buffers) {
                if (*buffer) {
                    clReleaseMemObject(*buffer);
                    *buffer = nullptr;
                }
            }
        };

        sourceBuffer = clCreateBuffer(
            context,
            CL_MEM_READ_ONLY |
                CL_MEM_COPY_HOST_PTR,
            byteCount,
            image.bits(),
            &error
            );

        if (error == CL_SUCCESS) {
            resultBuffer = clCreateBuffer(
                context,
                CL_MEM_READ_WRITE |
                    CL_MEM_COPY_HOST_PTR,
                byteCount,
                image.bits(),
                &error
                );
        }

        if (error == CL_SUCCESS) {
            horizontalLuminanceBuffer = clCreateBuffer(
                context,
                CL_MEM_READ_WRITE,
                oneChannelBytes,
                nullptr,
                &error
                );
        }

        if (error == CL_SUCCESS) {
            horizontalAlphaBuffer = clCreateBuffer(
                context,
                CL_MEM_READ_WRITE,
                oneChannelBytes,
                nullptr,
                &error
                );
        }

        if (error == CL_SUCCESS) {
            blurredLuminanceBuffer = clCreateBuffer(
                context,
                CL_MEM_READ_WRITE,
                oneChannelBytes,
                nullptr,
                &error
                );
        }

        if (error == CL_SUCCESS) {
            kernelBuffer = clCreateBuffer(
                context,
                CL_MEM_READ_ONLY |
                    CL_MEM_COPY_HOST_PTR,
                kernelWeights.size() * sizeof(cl_int),
                const_cast<cl_int *>(kernelWeights.data()),
                &error
                );
        }

        if (error != CL_SUCCESS ||
            !sourceBuffer ||
            !resultBuffer ||
            !horizontalLuminanceBuffer ||
            !horizontalAlphaBuffer ||
            !blurredLuminanceBuffer ||
            !kernelBuffer) {
            if (errorMessage) {
                *errorMessage =
                    "Focus working-buffer creation failed: " +
                    openClErrorText(error);
            }
            releaseBuffers();
            return false;
        }

        const int width = image.width();
        const int height = image.height();

        error = CL_SUCCESS;
        setKernelArgument(focusHorizontalKernel, 0, sizeof(cl_mem), &sourceBuffer, &error);
        setKernelArgument(focusHorizontalKernel, 1, sizeof(cl_mem), &horizontalLuminanceBuffer, &error);
        setKernelArgument(focusHorizontalKernel, 2, sizeof(cl_mem), &horizontalAlphaBuffer, &error);
        setKernelArgument(focusHorizontalKernel, 3, sizeof(cl_int), &width, &error);
        setKernelArgument(focusHorizontalKernel, 4, sizeof(cl_int), &height, &error);
        setKernelArgument(focusHorizontalKernel, 5, sizeof(cl_mem), &kernelBuffer, &error);
        setKernelArgument(focusHorizontalKernel, 6, sizeof(cl_int), &kernelRadius, &error);

        if (error != CL_SUCCESS ||
            !launchKernel(
                focusHorizontalKernel,
                static_cast<std::size_t>(pixelCount),
                errorMessage
                )) {
            if (error != CL_SUCCESS && errorMessage) {
                *errorMessage =
                    "Focus horizontal-pass argument setup failed: " +
                    openClErrorText(error);
            }
            releaseBuffers();
            return false;
        }

        error = CL_SUCCESS;
        setKernelArgument(focusVerticalKernel, 0, sizeof(cl_mem), &horizontalLuminanceBuffer, &error);
        setKernelArgument(focusVerticalKernel, 1, sizeof(cl_mem), &horizontalAlphaBuffer, &error);
        setKernelArgument(focusVerticalKernel, 2, sizeof(cl_mem), &blurredLuminanceBuffer, &error);
        setKernelArgument(focusVerticalKernel, 3, sizeof(cl_int), &width, &error);
        setKernelArgument(focusVerticalKernel, 4, sizeof(cl_int), &height, &error);
        setKernelArgument(focusVerticalKernel, 5, sizeof(cl_mem), &kernelBuffer, &error);
        setKernelArgument(focusVerticalKernel, 6, sizeof(cl_int), &kernelRadius, &error);

        if (error != CL_SUCCESS ||
            !launchKernel(
                focusVerticalKernel,
                static_cast<std::size_t>(pixelCount),
                errorMessage
                )) {
            if (error != CL_SUCCESS && errorMessage) {
                *errorMessage =
                    "Focus vertical-pass argument setup failed: " +
                    openClErrorText(error);
            }
            releaseBuffers();
            return false;
        }

        error = CL_SUCCESS;
        setKernelArgument(focusFinishKernel, 0, sizeof(cl_mem), &sourceBuffer, &error);
        setKernelArgument(focusFinishKernel, 1, sizeof(cl_mem), &resultBuffer, &error);
        setKernelArgument(focusFinishKernel, 2, sizeof(cl_mem), &blurredLuminanceBuffer, &error);
        setKernelArgument(focusFinishKernel, 3, sizeof(cl_int), &width, &error);
        setKernelArgument(focusFinishKernel, 4, sizeof(cl_int), &height, &error);
        setKernelArgument(focusFinishKernel, 5, sizeof(cl_double), &amountScale, &error);
        setKernelArgument(focusFinishKernel, 6, sizeof(cl_double), &threshold, &error);
        setKernelArgument(focusFinishKernel, 7, sizeof(cl_double), &thresholdSoftness, &error);
        setKernelArgument(focusFinishKernel, 8, sizeof(cl_double), &edgeProtectionStrength, &error);
        setKernelArgument(focusFinishKernel, 9, sizeof(cl_double), &maximumDelta, &error);
        setKernelArgument(focusFinishKernel, 10, sizeof(cl_double), &blend, &error);

        if (error != CL_SUCCESS ||
            !launchKernel(
                focusFinishKernel,
                static_cast<std::size_t>(pixelCount),
                errorMessage
                )) {
            if (error != CL_SUCCESS && errorMessage) {
                *errorMessage =
                    "Focus finish argument setup failed: " +
                    openClErrorText(error);
            }
            releaseBuffers();
            return false;
        }

        const bool success = readResultBuffer(
            resultBuffer,
            byteCount,
            image.bits(),
            errorMessage
            );

        releaseBuffers();
        return success;
    }

    bool executeOverexposureRecovery(
        QImage &image,
        const QRect &inputSelection,
        const OpenClSpatialProcessingBackend::OverexposureSettings &inputSettings,
        QString *errorMessage
        )
    {
        std::size_t byteCount = 0;
        cl_uint pixelCount = 0;

        if (!imageBufferInformation(
                image,
                &byteCount,
                &pixelCount,
                errorMessage
                )) {
            return false;
        }

        const QRect imageRect(
            0,
            0,
            image.width(),
            image.height()
            );
        const QRect selection =
            inputSelection.normalized().intersected(imageRect);

        if (selection.isEmpty()) {
            return true;
        }

        if (!ensureMemoryBudget(
                byteCount,
                byteCount * 2U + 4096U,
                errorMessage
                )) {
            return false;
        }

        std::array<cl_double, 24> sectorColors {};
        std::array<cl_int, 8> sectorValid {};

        for (std::size_t index = 0;
             index < inputSettings.sectorSamples.size();
             ++index) {
            const auto &sample =
                inputSettings.sectorSamples[index];

            sectorValid[index] =
                sample.valid ? 1 : 0;
            sectorColors[(index * 3) + 0] = sample.red;
            sectorColors[(index * 3) + 1] = sample.green;
            sectorColors[(index * 3) + 2] = sample.blue;
        }

        const int imageWidth = image.width();
        const int imageHeight = image.height();
        const int rectLeft = selection.left();
        const int rectTop = selection.top();
        const int rectWidth = selection.width();
        const int rectHeight = selection.height();
        const int shape = inputSettings.shape == 0 ? 0 : 1;
        const int softEdge = std::clamp(
            inputSettings.softEdge,
            0,
            100
            );
        const double brightCutoff = std::clamp(
            inputSettings.brightCutoff,
            0.0,
            255.0
            );
        const double recoveryAmount = std::clamp(
            inputSettings.recoveryAmount,
            0.0,
            1.0
            );
        const double colorBleed = std::clamp(
            inputSettings.colorBleed,
            0.0,
            1.0
            );
        const double transparency = std::clamp(
            inputSettings.transparency,
            0.0,
            1.0
            );
        const double preserveShine = std::clamp(
            inputSettings.preserveShine,
            0.0,
            1.0
            );

        const quint64 selectionPixelCount64 =
            static_cast<quint64>(rectWidth) *
            static_cast<quint64>(rectHeight);

        if (selectionPixelCount64 == 0 ||
            selectionPixelCount64 >
                static_cast<quint64>(
                    std::numeric_limits<cl_uint>::max()
                    )) {
            if (errorMessage) {
                *errorMessage =
                    "The overexposure selection is too large for OpenCL.";
            }
            return false;
        }

        cl_int error = CL_SUCCESS;
        cl_mem sourceBuffer = nullptr;
        cl_mem resultBuffer = nullptr;
        cl_mem sectorColorBuffer = nullptr;
        cl_mem sectorValidBuffer = nullptr;

        auto releaseBuffers = [this,
                               &sourceBuffer,
                               &resultBuffer,
                               &sectorColorBuffer,
                               &sectorValidBuffer]() {
            cl_mem *buffers[] = {
                &sectorValidBuffer,
                &sectorColorBuffer,
                &resultBuffer,
                &sourceBuffer
            };

            for (cl_mem *buffer : buffers) {
                if (*buffer) {
                    clReleaseMemObject(*buffer);
                    *buffer = nullptr;
                }
            }
        };

        sourceBuffer = clCreateBuffer(
            context,
            CL_MEM_READ_ONLY |
                CL_MEM_COPY_HOST_PTR,
            byteCount,
            image.bits(),
            &error
            );

        if (error == CL_SUCCESS) {
            resultBuffer = clCreateBuffer(
                context,
                CL_MEM_READ_WRITE |
                    CL_MEM_COPY_HOST_PTR,
                byteCount,
                image.bits(),
                &error
                );
        }

        if (error == CL_SUCCESS) {
            sectorColorBuffer = clCreateBuffer(
                context,
                CL_MEM_READ_ONLY |
                    CL_MEM_COPY_HOST_PTR,
                sizeof(sectorColors),
                sectorColors.data(),
                &error
                );
        }

        if (error == CL_SUCCESS) {
            sectorValidBuffer = clCreateBuffer(
                context,
                CL_MEM_READ_ONLY |
                    CL_MEM_COPY_HOST_PTR,
                sizeof(sectorValid),
                sectorValid.data(),
                &error
                );
        }

        if (error != CL_SUCCESS ||
            !sourceBuffer ||
            !resultBuffer ||
            !sectorColorBuffer ||
            !sectorValidBuffer) {
            if (errorMessage) {
                *errorMessage =
                    "Overexposure working-buffer creation failed: " +
                    openClErrorText(error);
            }
            releaseBuffers();
            return false;
        }

        error = CL_SUCCESS;
        setKernelArgument(overexposureKernel, 0, sizeof(cl_mem), &sourceBuffer, &error);
        setKernelArgument(overexposureKernel, 1, sizeof(cl_mem), &resultBuffer, &error);
        setKernelArgument(overexposureKernel, 2, sizeof(cl_int), &imageWidth, &error);
        setKernelArgument(overexposureKernel, 3, sizeof(cl_int), &imageHeight, &error);
        setKernelArgument(overexposureKernel, 4, sizeof(cl_int), &rectLeft, &error);
        setKernelArgument(overexposureKernel, 5, sizeof(cl_int), &rectTop, &error);
        setKernelArgument(overexposureKernel, 6, sizeof(cl_int), &rectWidth, &error);
        setKernelArgument(overexposureKernel, 7, sizeof(cl_int), &rectHeight, &error);
        setKernelArgument(overexposureKernel, 8, sizeof(cl_int), &shape, &error);
        setKernelArgument(overexposureKernel, 9, sizeof(cl_int), &softEdge, &error);
        setKernelArgument(overexposureKernel, 10, sizeof(cl_double), &brightCutoff, &error);
        setKernelArgument(overexposureKernel, 11, sizeof(cl_double), &recoveryAmount, &error);
        setKernelArgument(overexposureKernel, 12, sizeof(cl_double), &colorBleed, &error);
        setKernelArgument(overexposureKernel, 13, sizeof(cl_double), &transparency, &error);
        setKernelArgument(overexposureKernel, 14, sizeof(cl_double), &preserveShine, &error);
        setKernelArgument(overexposureKernel, 15, sizeof(cl_mem), &sectorColorBuffer, &error);
        setKernelArgument(overexposureKernel, 16, sizeof(cl_mem), &sectorValidBuffer, &error);

        if (error != CL_SUCCESS) {
            if (errorMessage) {
                *errorMessage =
                    "Overexposure argument setup failed: " +
                    openClErrorText(error);
            }
            releaseBuffers();
            return false;
        }

        if (!launchKernel(
                overexposureKernel,
                static_cast<std::size_t>(selectionPixelCount64),
                errorMessage
                )) {
            releaseBuffers();
            return false;
        }

        const bool success = readResultBuffer(
            resultBuffer,
            byteCount,
            image.bits(),
            errorMessage
            );

        releaseBuffers();
        return success;
    }

    bool isNonFatalFallback(
        const QString &message
        ) const
    {
        return
            message.contains("requires a 32-bit", Qt::CaseInsensitive) ||
            message.contains("unsupported byte size", Qt::CaseInsensitive) ||
            message.contains("too large", Qt::CaseInsensitive) ||
            message.contains("single-allocation limit", Qt::CaseInsensitive) ||
            message.contains("safe", Qt::CaseInsensitive) &&
                message.contains("budget", Qt::CaseInsensitive) ||
            message.contains("selection is too large", Qt::CaseInsensitive);
    }

    void disableAfterFailure(
        const QString &operation,
        const QString &executionError,
        QString *errorMessage
        )
    {
        const QString failureStatus = QString(
            "OpenCL spatial processing was disabled for this session after "
            "%1 failed: %2"
            )
            .arg(operation)
            .arg(executionError);

        shutdownUnlocked();
        status = failureStatus;

        if (errorMessage) {
            *errorMessage = status;
        }
    }

    void shutdownUnlocked()
    {
        available = false;
        supportsFp64 = false;

        cl_kernel *kernels[] = {
            &overexposureKernel,
            &focusFinishKernel,
            &focusVerticalKernel,
            &focusHorizontalKernel,
            &noiseFinishKernel,
            &noiseEdgeKernel,
            &encodeKernel
        };

        for (cl_kernel *kernel : kernels) {
            if (*kernel &&
                clReleaseKernel) {
                clReleaseKernel(*kernel);
                *kernel = nullptr;
            }
        }

        if (program &&
            clReleaseProgram) {
            clReleaseProgram(program);
            program = nullptr;
        }

        if (commandQueue &&
            clReleaseCommandQueue) {
            clReleaseCommandQueue(commandQueue);
            commandQueue = nullptr;
        }

        if (context &&
            clReleaseContext) {
            clReleaseContext(context);
            context = nullptr;
        }

        selectedDevice = nullptr;
        maximumAllocationBytes = 0;
        globalMemoryBytes = 0;

        if (library.isLoaded()) {
            library.unload();
        }
    }

    mutable QMutex mutex;
    bool initializationAttempted = false;
    bool available = false;
    bool supportsFp64 = false;
    QString deviceDisplayName;
    QString status =
        "OpenCL spatial processing has not been initialized.";
    std::size_t maximumAllocationBytes = 0;
    std::size_t globalMemoryBytes = 0;

    QLibrary library;
    cl_device_id selectedDevice = nullptr;
    cl_context context = nullptr;
    cl_command_queue commandQueue = nullptr;
    cl_program program = nullptr;
    cl_kernel encodeKernel = nullptr;
    cl_kernel noiseEdgeKernel = nullptr;
    cl_kernel noiseFinishKernel = nullptr;
    cl_kernel focusHorizontalKernel = nullptr;
    cl_kernel focusVerticalKernel = nullptr;
    cl_kernel focusFinishKernel = nullptr;
    cl_kernel overexposureKernel = nullptr;

    ClGetPlatformIDs clGetPlatformIDs = nullptr;
    ClGetPlatformInfo clGetPlatformInfo = nullptr;
    ClGetDeviceIDs clGetDeviceIDs = nullptr;
    ClGetDeviceInfo clGetDeviceInfo = nullptr;
    ClCreateContext clCreateContext = nullptr;
    ClReleaseContext clReleaseContext = nullptr;
    ClCreateCommandQueue clCreateCommandQueue = nullptr;
    ClReleaseCommandQueue clReleaseCommandQueue = nullptr;
    ClCreateProgramWithSource clCreateProgramWithSource = nullptr;
    ClBuildProgram clBuildProgram = nullptr;
    ClGetProgramBuildInfo clGetProgramBuildInfo = nullptr;
    ClReleaseProgram clReleaseProgram = nullptr;
    ClCreateKernel clCreateKernel = nullptr;
    ClReleaseKernel clReleaseKernel = nullptr;
    ClCreateBuffer clCreateBuffer = nullptr;
    ClReleaseMemObject clReleaseMemObject = nullptr;
    ClSetKernelArg clSetKernelArg = nullptr;
    ClEnqueueNDRangeKernel clEnqueueNDRangeKernel = nullptr;
    ClEnqueueReadBuffer clEnqueueReadBuffer = nullptr;
    ClFinish clFinish = nullptr;
};

OpenClSpatialProcessingBackend::OpenClSpatialProcessingBackend()
    : m_implementation(
          std::make_unique<Implementation>()
          )
{
}

OpenClSpatialProcessingBackend::~OpenClSpatialProcessingBackend() = default;

void OpenClSpatialProcessingBackend::initialize()
{
    m_implementation->initialize();
}

void OpenClSpatialProcessingBackend::shutdown()
{
    m_implementation->shutdown();
}

bool OpenClSpatialProcessingBackend::isAvailable() const
{
    return m_implementation->isAvailable();
}

QString OpenClSpatialProcessingBackend::deviceName() const
{
    return m_implementation->deviceName();
}

QString OpenClSpatialProcessingBackend::statusText() const
{
    return m_implementation->statusText();
}

bool OpenClSpatialProcessingBackend::applyNoiseReduction(
    QImage &image,
    const NoiseSettings &settings,
    QString *errorMessage
    )
{
    return m_implementation->applyNoiseReduction(
        image,
        settings,
        errorMessage
        );
}

bool OpenClSpatialProcessingBackend::applyFocus(
    QImage &image,
    const FocusSettings &settings,
    QString *errorMessage
    )
{
    return m_implementation->applyFocus(
        image,
        settings,
        errorMessage
        );
}

bool OpenClSpatialProcessingBackend::applyOverexposureRecovery(
    QImage &image,
    const QRect &selection,
    const OverexposureSettings &settings,
    QString *errorMessage
    )
{
    return m_implementation->applyOverexposureRecovery(
        image,
        selection,
        settings,
        errorMessage
        );
}
