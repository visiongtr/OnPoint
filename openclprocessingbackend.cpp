#include "openclprocessingbackend.h"

#include <QByteArray>
#include <QLibrary>
#include <QMutex>
#include <QMutexLocker>
#include <QSysInfo>
#include <QStringList>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
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
using cl_char = std::int8_t;
using cl_uchar = std::uint8_t;
using cl_short = std::int16_t;
using cl_ushort = std::uint16_t;
using cl_int = std::int32_t;
using cl_uint = std::uint32_t;
using cl_long = std::int64_t;
using cl_ulong = std::uint64_t;
using cl_half = std::uint16_t;
using cl_double = double;
using cl_bool = cl_uint;
using cl_bitfield = cl_ulong;
using cl_device_type = cl_bitfield;
using cl_platform_info = cl_uint;
using cl_device_info = cl_uint;
using cl_context_info = cl_uint;
using cl_command_queue_properties = cl_bitfield;
using cl_mem_flags = cl_bitfield;
using cl_program_build_info = cl_uint;
using cl_context_properties = std::intptr_t;
using cl_queue_properties = std::intptr_t;

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
constexpr cl_device_info CL_DRIVER_VERSION = 0x102D;
constexpr cl_device_info CL_DEVICE_VERSION = 0x102F;
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
using ClReleaseContext = cl_int (ONPOINT_CL_API_CALL *)(
    cl_context
    );
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
using ClReleaseProgram = cl_int (ONPOINT_CL_API_CALL *)(
    cl_program
    );
using ClCreateKernel = cl_kernel (ONPOINT_CL_API_CALL *)(
    cl_program,
    const char *,
    cl_int *
    );
using ClReleaseKernel = cl_int (ONPOINT_CL_API_CALL *)(
    cl_kernel
    );
using ClCreateBuffer = cl_mem (ONPOINT_CL_API_CALL *)(
    cl_context,
    cl_mem_flags,
    std::size_t,
    void *,
    cl_int *
    );
using ClReleaseMemObject = cl_int (ONPOINT_CL_API_CALL *)(
    cl_mem
    );
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
using ClFinish = cl_int (ONPOINT_CL_API_CALL *)(
    cl_command_queue
    );

QString openClErrorText(
    cl_int errorCode
    )
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

QString cleanedInfoString(
    const QByteArray &bytes
    )
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

const char *channelLookupKernelSource()
{
    return R"CLC(
__kernel void apply_channel_lookups(
    __global uchar *pixelBytes,
    __constant uchar *lookups,
    const uint pixelCount)
{
    const uint pixelIndex = get_global_id(0);

    if (pixelIndex >= pixelCount) {
        return;
    }

    const uint byteIndex = pixelIndex * 4U;

    // QImage::Format_ARGB32 stores BGRA bytes on little-endian systems.
    const uchar blue = pixelBytes[byteIndex + 0U];
    const uchar green = pixelBytes[byteIndex + 1U];
    const uchar red = pixelBytes[byteIndex + 2U];

    pixelBytes[byteIndex + 0U] = lookups[512U + (uint)blue];
    pixelBytes[byteIndex + 1U] = lookups[256U + (uint)green];
    pixelBytes[byteIndex + 2U] = lookups[(uint)red];
    // Alpha at byteIndex + 3U remains untouched.
}
)CLC";
}

const char *adjustmentPipelineKernelSource()
{
    return R"CLC(
#pragma OPENCL EXTENSION cl_khr_fp64 : enable
#pragma OPENCL FP_CONTRACT OFF

inline double onpoint_clamp_double(
    const double value,
    const double minimumValue,
    const double maximumValue)
{
    return fmin(maximumValue, fmax(minimumValue, value));
}

inline uchar onpoint_channel_from_double(
    const double value)
{
    const double roundedValue = round(value);
    return (uchar)onpoint_clamp_double(
        roundedValue,
        0.0,
        255.0);
}

inline double onpoint_weighted_tone_shift(
    const double tone,
    const double highlightShift,
    const double midtoneShift,
    const double shadowShift)
{
    const double shadowWeight = onpoint_clamp_double(
        (0.55 - tone) / 0.55,
        0.0,
        1.0);

    const double highlightWeight = onpoint_clamp_double(
        (tone - 0.45) / 0.55,
        0.0,
        1.0);

    const double midtoneWeight = onpoint_clamp_double(
        1.0 - (fabs(tone - 0.50) / 0.50),
        0.0,
        1.0);

    return
        (shadowShift * shadowWeight) +
        (midtoneShift * midtoneWeight) +
        (highlightShift * highlightWeight);
}

__kernel void apply_adjustment_pipeline(
    __global uchar *pixelBytes,
    __constant uchar *lookups,
    const uint pixelCount,
    const int useBasicLookups,
    const int useBaseHms,
    const int useColorHms,
    const int useSaturation,
    const double baseHighlightShift,
    const double baseMidtoneShift,
    const double baseShadowShift,
    const double cyanRedHighlightShift,
    const double cyanRedMidtoneShift,
    const double cyanRedShadowShift,
    const double magentaGreenHighlightShift,
    const double magentaGreenMidtoneShift,
    const double magentaGreenShadowShift,
    const double yellowBlueHighlightShift,
    const double yellowBlueMidtoneShift,
    const double yellowBlueShadowShift,
    const int saturationAmount)
{
    const uint pixelIndex = get_global_id(0);

    if (pixelIndex >= pixelCount) {
        return;
    }

    const uint byteIndex = pixelIndex * 4U;

    double blue = (double)pixelBytes[byteIndex + 0U];
    double green = (double)pixelBytes[byteIndex + 1U];
    double red = (double)pixelBytes[byteIndex + 2U];

    if (useBasicLookups != 0) {
        blue = (double)lookups[512U + (uint)blue];
        green = (double)lookups[256U + (uint)green];
        red = (double)lookups[(uint)red];
    }

    if (useBaseHms != 0) {
        const double luminance =
            (0.299 * red) +
            (0.587 * green) +
            (0.114 * blue);

        const double tone = luminance / 255.0;

        const double totalShift = onpoint_weighted_tone_shift(
            tone,
            baseHighlightShift,
            baseMidtoneShift,
            baseShadowShift);

        red = (double)onpoint_channel_from_double(red + totalShift);
        green = (double)onpoint_channel_from_double(green + totalShift);
        blue = (double)onpoint_channel_from_double(blue + totalShift);
    }

    if (useColorHms != 0) {
        const double luminance =
            (0.299 * red) +
            (0.587 * green) +
            (0.114 * blue);

        const double tone = luminance / 255.0;

        const double redShift = onpoint_weighted_tone_shift(
            tone,
            cyanRedHighlightShift,
            cyanRedMidtoneShift,
            cyanRedShadowShift);

        const double greenShift = onpoint_weighted_tone_shift(
            tone,
            magentaGreenHighlightShift,
            magentaGreenMidtoneShift,
            magentaGreenShadowShift);

        const double blueShift = onpoint_weighted_tone_shift(
            tone,
            yellowBlueHighlightShift,
            yellowBlueMidtoneShift,
            yellowBlueShadowShift);

        red = (double)onpoint_channel_from_double(red + redShift);
        green = (double)onpoint_channel_from_double(green + greenShift);
        blue = (double)onpoint_channel_from_double(blue + blueShift);
    }

    if (useSaturation != 0) {
        const double factor =
            1.0 + ((double)saturationAmount / 100.0);

        const double gray =
            (0.299 * red) +
            (0.587 * green) +
            (0.114 * blue);

        red = (double)onpoint_channel_from_double(
            gray + ((red - gray) * factor));

        green = (double)onpoint_channel_from_double(
            gray + ((green - gray) * factor));

        blue = (double)onpoint_channel_from_double(
            gray + ((blue - gray) * factor));
    }

    pixelBytes[byteIndex + 0U] = (uchar)blue;
    pixelBytes[byteIndex + 1U] = (uchar)green;
    pixelBytes[byteIndex + 2U] = (uchar)red;
    // Alpha at byteIndex + 3U remains untouched.
}
)CLC";
}
}

class OpenClProcessingBackend::Implementation
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
                "OpenCL GPU processing is disabled on big-endian systems.";
            return;
        }

        if (!loadLibrary()) {
            return;
        }

        if (!resolveFunctions()) {
            shutdownUnlocked();
            return;
        }

        if (!selectGpuDevice()) {
            shutdownUnlocked();
            return;
        }

        if (!createContextAndKernel()) {
            shutdownUnlocked();
            return;
        }

        if (!runSelfTest()) {
            shutdownUnlocked();
            return;
        }

        available = true;
        status = QString(
            "OpenCL ready on %1"
            ).arg(deviceDisplayName);
    }

    void shutdown()
    {
        QMutexLocker locker(&mutex);
        shutdownUnlocked();
        initializationAttempted = false;
        status =
            "OpenCL GPU processing has not been initialized.";
    }

    bool applyChannelLookups(
        QImage &image,
        const OpenClProcessingBackend::ChannelLookup &redLookup,
        const OpenClProcessingBackend::ChannelLookup &greenLookup,
        const OpenClProcessingBackend::ChannelLookup &blueLookup,
        QString *errorMessage
        )
    {
        QMutexLocker locker(&mutex);

        if (!available ||
            image.isNull()) {
            if (errorMessage) {
                *errorMessage = status;
            }
            return false;
        }

        if (image.format() != QImage::Format_ARGB32 &&
            image.format() != QImage::Format_RGB32) {
            if (errorMessage) {
                *errorMessage =
                    "OpenCL requires a 32-bit QImage processing buffer.";
            }
            return false;
        }

        const qsizetype byteCountValue =
            image.sizeInBytes();

        if (byteCountValue <= 0 ||
            (byteCountValue % 4) != 0) {
            if (errorMessage) {
                *errorMessage =
                    "The image buffer has an unsupported byte size.";
            }
            return false;
        }

        const std::size_t byteCount =
            static_cast<std::size_t>(byteCountValue);

        const quint64 pixelCount64 =
            static_cast<quint64>(byteCount / 4U);

        if (pixelCount64 >
            static_cast<quint64>(
                std::numeric_limits<cl_uint>::max()
                )) {
            if (errorMessage) {
                *errorMessage =
                    "The image is too large for this OpenCL kernel.";
            }
            return false;
        }

        if (maximumAllocationBytes > 0 &&
            byteCount > maximumAllocationBytes) {
            if (errorMessage) {
                *errorMessage = QString(
                    "The image buffer requires %1 MiB, exceeding the GPU's "
                    "single-allocation limit of %2 MiB."
                    )
                    .arg(byteCount / (1024.0 * 1024.0), 0, 'f', 1)
                    .arg(maximumAllocationBytes / (1024.0 * 1024.0), 0, 'f', 1);
            }
            return false;
        }

        std::array<cl_uchar, 768> combinedLookups {};

        std::copy(
            redLookup.begin(),
            redLookup.end(),
            combinedLookups.begin()
            );
        std::copy(
            greenLookup.begin(),
            greenLookup.end(),
            combinedLookups.begin() + 256
            );
        std::copy(
            blueLookup.begin(),
            blueLookup.end(),
            combinedLookups.begin() + 512
            );

        QString executionError;
        const bool success = executeLookups(
            image.bits(),
            byteCount,
            static_cast<cl_uint>(pixelCount64),
            combinedLookups.data(),
            combinedLookups.size(),
            &executionError
            );

        if (!success) {
            const QString failureStatus =
                "OpenCL was disabled for this session after a processing "
                "failure: " + executionError;

            shutdownUnlocked();
            status = failureStatus;

            if (errorMessage) {
                *errorMessage = status;
            }

            return false;
        }

        return true;
    }

    bool applyAdjustmentPipeline(
        QImage &image,
        const OpenClProcessingBackend::ChannelLookup &redLookup,
        const OpenClProcessingBackend::ChannelLookup &greenLookup,
        const OpenClProcessingBackend::ChannelLookup &blueLookup,
        const OpenClProcessingBackend::AdjustmentPipelineSettings &settings,
        QString *errorMessage
        )
    {
        QMutexLocker locker(&mutex);

        if (!available ||
            !advancedColorAvailable ||
            !adjustmentKernel ||
            image.isNull()) {
            if (errorMessage) {
                *errorMessage = advancedColorStatus.isEmpty()
                    ? status
                    : advancedColorStatus;
            }
            return false;
        }

        if (image.format() != QImage::Format_ARGB32 &&
            image.format() != QImage::Format_RGB32) {
            if (errorMessage) {
                *errorMessage =
                    "OpenCL requires a 32-bit QImage processing buffer.";
            }
            return false;
        }

        const qsizetype byteCountValue =
            image.sizeInBytes();

        if (byteCountValue <= 0 ||
            (byteCountValue % 4) != 0) {
            if (errorMessage) {
                *errorMessage =
                    "The image buffer has an unsupported byte size.";
            }
            return false;
        }

        const std::size_t byteCount =
            static_cast<std::size_t>(byteCountValue);

        const quint64 pixelCount64 =
            static_cast<quint64>(byteCount / 4U);

        if (pixelCount64 >
            static_cast<quint64>(
                std::numeric_limits<cl_uint>::max()
                )) {
            if (errorMessage) {
                *errorMessage =
                    "The image is too large for this OpenCL kernel.";
            }
            return false;
        }

        if (maximumAllocationBytes > 0 &&
            byteCount > maximumAllocationBytes) {
            if (errorMessage) {
                *errorMessage = QString(
                    "The image buffer requires %1 MiB, exceeding the GPU's "
                    "single-allocation limit of %2 MiB."
                    )
                    .arg(byteCount / (1024.0 * 1024.0), 0, 'f', 1)
                    .arg(maximumAllocationBytes / (1024.0 * 1024.0), 0, 'f', 1);
            }
            return false;
        }

        std::array<cl_uchar, 768> combinedLookups {};

        std::copy(
            redLookup.begin(),
            redLookup.end(),
            combinedLookups.begin()
            );
        std::copy(
            greenLookup.begin(),
            greenLookup.end(),
            combinedLookups.begin() + 256
            );
        std::copy(
            blueLookup.begin(),
            blueLookup.end(),
            combinedLookups.begin() + 512
            );

        QString executionError;
        const bool success = executeAdjustmentPipeline(
            image.bits(),
            byteCount,
            static_cast<cl_uint>(pixelCount64),
            combinedLookups.data(),
            combinedLookups.size(),
            settings,
            &executionError
            );

        if (!success) {
            const QString failureStatus =
                "OpenCL was disabled for this session after an adjustment "
                "pipeline failure: " + executionError;

            shutdownUnlocked();
            status = failureStatus;

            if (errorMessage) {
                *errorMessage = status;
            }

            return false;
        }

        return true;
    }

    bool isAvailable() const
    {
        QMutexLocker locker(&mutex);
        return available;
    }

    bool adjustmentPipelineAvailable() const
    {
        QMutexLocker locker(&mutex);
        return available &&
            advancedColorAvailable &&
            adjustmentKernel;
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
            "No OpenCL runtime library was found. The CPU engine remains active.";
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
                "OpenCL did not report any compute platforms. The CPU engine remains active. On Debian/LMDE, install an OpenCL ICD: nvidia-opencl-icd for NVIDIA or mesa-opencl-icd for AMD or Intel Mesa/Rusticl.";
            return false;
        }

        std::vector<cl_platform_id> platforms(
            platformCount
            );

        error = clGetPlatformIDs(
            platformCount,
            platforms.data(),
            nullptr
            );

        if (error != CL_SUCCESS) {
            status =
                "OpenCL platform enumeration failed: " +
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

            std::vector<cl_device_id> devices(
                deviceCount
                );

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
                "No usable OpenCL GPU device was found. The CPU engine remains active.";
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

        const QString deviceName =
            preferredCandidate->name.isEmpty()
            ? QString("Unnamed GPU")
            : preferredCandidate->name;

        const QString vendorName =
            preferredCandidate->vendor;

        deviceDisplayName =
            vendorName.isEmpty() ||
            deviceName.contains(
                vendorName,
                Qt::CaseInsensitive
                )
            ? deviceName
            : vendorName + " " + deviceName;

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
                queryDeviceBytes(
                    CL_DEVICE_EXTENSIONS
                    )
                );

        supportsFp64 =
            reportsDoubleConfig ||
            extensionText.contains(
                "cl_khr_fp64",
                Qt::CaseInsensitive
                );

        return true;
    }

    bool createContextAndKernel()
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
                "OpenCL context creation failed: " +
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
                "OpenCL command queue creation failed: " +
                openClErrorText(error);
            return false;
        }

        auto releaseProgramObjects = [this]() {
            if (adjustmentKernel &&
                clReleaseKernel) {
                clReleaseKernel(adjustmentKernel);
                adjustmentKernel = nullptr;
            }

            if (lookupKernel &&
                clReleaseKernel) {
                clReleaseKernel(lookupKernel);
                lookupKernel = nullptr;
            }

            if (program &&
                clReleaseProgram) {
                clReleaseProgram(program);
                program = nullptr;
            }
        };

        auto buildProgram = [this,
                             &releaseProgramObjects](
                                bool includeAdjustmentPipeline,
                                QString *buildFailure) {
            releaseProgramObjects();

            const char *sources[2] = {
                channelLookupKernelSource(),
                adjustmentPipelineKernelSource()
            };

            const std::size_t sourceLengths[2] = {
                std::strlen(sources[0]),
                std::strlen(sources[1])
            };

            const cl_uint sourceCount =
                includeAdjustmentPipeline ? 2U : 1U;

            cl_int buildError = CL_SUCCESS;

            program = clCreateProgramWithSource(
                context,
                sourceCount,
                sources,
                sourceLengths,
                &buildError
                );

            if (!program ||
                buildError != CL_SUCCESS) {
                if (buildFailure) {
                    *buildFailure =
                        "OpenCL program creation failed: " +
                        openClErrorText(buildError);
                }
                return false;
            }

            buildError = clBuildProgram(
                program,
                1,
                &selectedDevice,
                nullptr,
                nullptr,
                nullptr
                );

            if (buildError != CL_SUCCESS) {
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

                QString message =
                    "OpenCL kernel build failed: " +
                    openClErrorText(buildError);

                const QString cleanedLog =
                    cleanedInfoString(buildLog);

                if (!cleanedLog.isEmpty()) {
                    message += " — " + cleanedLog;
                }

                if (buildFailure) {
                    *buildFailure = message;
                }

                return false;
            }

            lookupKernel = clCreateKernel(
                program,
                "apply_channel_lookups",
                &buildError
                );

            if (!lookupKernel ||
                buildError != CL_SUCCESS) {
                if (buildFailure) {
                    *buildFailure =
                        "OpenCL lookup-kernel creation failed: " +
                        openClErrorText(buildError);
                }
                return false;
            }

            if (includeAdjustmentPipeline) {
                adjustmentKernel = clCreateKernel(
                    program,
                    "apply_adjustment_pipeline",
                    &buildError
                    );

                if (!adjustmentKernel ||
                    buildError != CL_SUCCESS) {
                    if (buildFailure) {
                        *buildFailure =
                            "OpenCL adjustment-kernel creation failed: " +
                            openClErrorText(buildError);
                    }
                    return false;
                }
            }

            return true;
        };

        advancedColorAvailable = false;
        advancedColorStatus.clear();

        if (supportsFp64) {
            QString advancedFailure;

            if (buildProgram(
                    true,
                    &advancedFailure
                    )) {
                advancedColorAvailable = true;
                advancedColorStatus =
                    "OpenCL advanced color pipeline is ready.";
                return true;
            }

            // Preserve the proven lookup engine even when a vendor cannot
            // compile the optional double-precision color kernel.
            QString lookupFailure;

            if (!buildProgram(
                    false,
                    &lookupFailure
                    )) {
                status = lookupFailure;
                return false;
            }

            advancedColorStatus =
                "OpenCL advanced color pipeline is unavailable; "
                "the CPU color engine remains active. " +
                advancedFailure;
            return true;
        }

        QString lookupFailure;

        if (!buildProgram(
                false,
                &lookupFailure
                )) {
            status = lookupFailure;
            return false;
        }

        advancedColorStatus =
            "This OpenCL device does not report double-precision support; "
            "advanced color processing remains on the CPU.";
        return true;
    }

    bool runSelfTest()
    {
        std::array<cl_uchar, 8> pixels = {
            10, 20, 30, 255,
            40, 50, 60, 128
        };

        std::array<cl_uchar, 768> lookups {};

        for (std::size_t index = 0;
             index < 256;
             ++index) {
            lookups[index] =
                static_cast<cl_uchar>(
                    std::min<std::size_t>(255, index + 1)
                    );
            lookups[256 + index] =
                static_cast<cl_uchar>(
                    std::min<std::size_t>(255, index + 2)
                    );
            lookups[512 + index] =
                static_cast<cl_uchar>(
                    std::min<std::size_t>(255, index + 3)
                    );
        }

        QString errorMessage;

        if (!executeLookups(
                pixels.data(),
                pixels.size(),
                2,
                lookups.data(),
                lookups.size(),
                &errorMessage
                )) {
            status =
                "OpenCL self-test failed: " +
                errorMessage;
            return false;
        }

        const std::array<cl_uchar, 8> expected = {
            13, 22, 31, 255,
            43, 52, 61, 128
        };

        if (pixels != expected) {
            status =
                "OpenCL self-test returned incorrect pixel data. The CPU engine remains active.";
            return false;
        }

        if (advancedColorAvailable &&
            adjustmentKernel &&
            !runAdjustmentSelfTest()) {
            clReleaseKernel(adjustmentKernel);
            adjustmentKernel = nullptr;
            advancedColorAvailable = false;

            if (advancedColorStatus.isEmpty()) {
                advancedColorStatus =
                    "The OpenCL advanced color self-test failed; "
                    "the CPU color engine remains active.";
            }
        }

        return true;
    }

    bool runAdjustmentSelfTest()
    {
        std::array<cl_uchar, 16> pixels = {
            12, 34, 56, 255,
            70, 120, 190, 200,
            210, 170, 40, 128,
            250, 245, 230, 64
        };

        std::array<cl_uchar, 768> lookups {};

        for (std::size_t index = 0;
             index < 256;
             ++index) {
            lookups[index] =
                static_cast<cl_uchar>(
                    std::min<std::size_t>(255, index + 3)
                    );
            lookups[256 + index] =
                static_cast<cl_uchar>(index);
            lookups[512 + index] =
                static_cast<cl_uchar>(
                    index > 1 ? index - 2 : 0
                    );
        }

        OpenClProcessingBackend::AdjustmentPipelineSettings testSettings;
        testSettings.baseHighlightShift = 10.24;
        testSettings.baseMidtoneShift = -6.40;
        testSettings.baseShadowShift = 3.84;
        testSettings.cyanRedHighlightShift = 7.68;
        testSettings.cyanRedMidtoneShift = -2.56;
        testSettings.cyanRedShadowShift = 5.12;
        testSettings.magentaGreenHighlightShift = -3.84;
        testSettings.magentaGreenMidtoneShift = 6.40;
        testSettings.magentaGreenShadowShift = -1.28;
        testSettings.yellowBlueHighlightShift = 2.56;
        testSettings.yellowBlueMidtoneShift = -5.12;
        testSettings.yellowBlueShadowShift = 8.96;
        testSettings.saturationAmount = 35;
        testSettings.applyBasicLookups = true;
        testSettings.applyBaseHms = true;
        testSettings.applyColorHms = true;
        testSettings.applySaturation = true;

        std::array<cl_uchar, 16> expected = pixels;

        const auto clampChannel = [](double value) {
            return static_cast<cl_uchar>(
                std::clamp(
                    static_cast<int>(std::round(value)),
                    0,
                    255
                    )
                );
        };

        const auto weightedToneShift = [](
            double tone,
            double highlightShift,
            double midtoneShift,
            double shadowShift) {
            const double shadowWeight =
                std::clamp(
                    (0.55 - tone) / 0.55,
                    0.0,
                    1.0
                    );

            const double highlightWeight =
                std::clamp(
                    (tone - 0.45) / 0.55,
                    0.0,
                    1.0
                    );

            const double midtoneWeight =
                std::clamp(
                    1.0 -
                        (std::abs(tone - 0.50) / 0.50),
                    0.0,
                    1.0
                    );

            return
                (shadowShift * shadowWeight) +
                (midtoneShift * midtoneWeight) +
                (highlightShift * highlightWeight);
        };

        for (std::size_t pixelIndex = 0;
             pixelIndex < expected.size() / 4;
             ++pixelIndex) {
            const std::size_t byteIndex = pixelIndex * 4;

            double blue = lookups[
                512 + expected[byteIndex + 0]
                ];
            double green = lookups[
                256 + expected[byteIndex + 1]
                ];
            double red = lookups[
                expected[byteIndex + 2]
                ];

            double luminance =
                (0.299 * red) +
                (0.587 * green) +
                (0.114 * blue);
            double tone = luminance / 255.0;

            const double baseShift = weightedToneShift(
                tone,
                testSettings.baseHighlightShift,
                testSettings.baseMidtoneShift,
                testSettings.baseShadowShift
                );

            red = clampChannel(red + baseShift);
            green = clampChannel(green + baseShift);
            blue = clampChannel(blue + baseShift);

            luminance =
                (0.299 * red) +
                (0.587 * green) +
                (0.114 * blue);
            tone = luminance / 255.0;

            red = clampChannel(
                red + weightedToneShift(
                    tone,
                    testSettings.cyanRedHighlightShift,
                    testSettings.cyanRedMidtoneShift,
                    testSettings.cyanRedShadowShift
                    )
                );

            green = clampChannel(
                green + weightedToneShift(
                    tone,
                    testSettings.magentaGreenHighlightShift,
                    testSettings.magentaGreenMidtoneShift,
                    testSettings.magentaGreenShadowShift
                    )
                );

            blue = clampChannel(
                blue + weightedToneShift(
                    tone,
                    testSettings.yellowBlueHighlightShift,
                    testSettings.yellowBlueMidtoneShift,
                    testSettings.yellowBlueShadowShift
                    )
                );

            const double saturationFactor =
                1.0 +
                (testSettings.saturationAmount / 100.0);

            const double gray =
                (0.299 * red) +
                (0.587 * green) +
                (0.114 * blue);

            red = clampChannel(
                gray + ((red - gray) * saturationFactor)
                );
            green = clampChannel(
                gray + ((green - gray) * saturationFactor)
                );
            blue = clampChannel(
                gray + ((blue - gray) * saturationFactor)
                );

            expected[byteIndex + 0] =
                static_cast<cl_uchar>(blue);
            expected[byteIndex + 1] =
                static_cast<cl_uchar>(green);
            expected[byteIndex + 2] =
                static_cast<cl_uchar>(red);
        }

        QString errorMessage;

        if (!executeAdjustmentPipeline(
                pixels.data(),
                pixels.size(),
                4,
                lookups.data(),
                lookups.size(),
                testSettings,
                &errorMessage
                )) {
            advancedColorStatus =
                "The OpenCL advanced color self-test could not run: " +
                errorMessage;
            return false;
        }

        for (std::size_t index = 0;
             index < pixels.size();
             ++index) {
            if ((index % 4) == 3) {
                if (pixels[index] != expected[index]) {
                    advancedColorStatus =
                        "The OpenCL advanced color self-test changed alpha data.";
                    return false;
                }
                continue;
            }

            const int difference =
                std::abs(
                    static_cast<int>(pixels[index]) -
                    static_cast<int>(expected[index])
                    );

            if (difference > 1) {
                advancedColorStatus =
                    "The OpenCL advanced color self-test exceeded its "
                    "one-level channel tolerance.";
                return false;
            }
        }

        return true;
    }

    bool executeAdjustmentPipeline(
        void *pixelBytes,
        std::size_t byteCount,
        cl_uint pixelCount,
        const cl_uchar *lookups,
        std::size_t lookupByteCount,
        const OpenClProcessingBackend::AdjustmentPipelineSettings &settings,
        QString *errorMessage
        )
    {
        if (!pixelBytes ||
            byteCount == 0 ||
            pixelCount == 0 ||
            !lookups ||
            lookupByteCount != 768 ||
            !adjustmentKernel) {
            if (errorMessage) {
                *errorMessage =
                    "Invalid OpenCL adjustment-pipeline arguments.";
            }
            return false;
        }

        cl_int error = CL_SUCCESS;

        cl_mem pixelBuffer = clCreateBuffer(
            context,
            CL_MEM_READ_WRITE |
                CL_MEM_COPY_HOST_PTR,
            byteCount,
            pixelBytes,
            &error
            );

        if (!pixelBuffer ||
            error != CL_SUCCESS) {
            if (errorMessage) {
                *errorMessage =
                    "Pixel buffer creation failed: " +
                    openClErrorText(error);
            }
            return false;
        }

        cl_mem lookupBuffer = clCreateBuffer(
            context,
            CL_MEM_READ_ONLY |
                CL_MEM_COPY_HOST_PTR,
            lookupByteCount,
            const_cast<cl_uchar *>(lookups),
            &error
            );

        if (!lookupBuffer ||
            error != CL_SUCCESS) {
            clReleaseMemObject(pixelBuffer);

            if (errorMessage) {
                *errorMessage =
                    "Lookup buffer creation failed: " +
                    openClErrorText(error);
            }
            return false;
        }

        auto releaseBuffers = [this,
                               &pixelBuffer,
                               &lookupBuffer]() {
            if (lookupBuffer) {
                clReleaseMemObject(lookupBuffer);
                lookupBuffer = nullptr;
            }

            if (pixelBuffer) {
                clReleaseMemObject(pixelBuffer);
                pixelBuffer = nullptr;
            }
        };

        const cl_int useBasicLookups =
            settings.applyBasicLookups ? 1 : 0;
        const cl_int useBaseHms =
            settings.applyBaseHms ? 1 : 0;
        const cl_int useColorHms =
            settings.applyColorHms ? 1 : 0;
        const cl_int useSaturation =
            settings.applySaturation ? 1 : 0;
        const cl_int saturationAmount =
            std::clamp(settings.saturationAmount, -100, 100);

        const cl_double scalarArguments[] = {
            settings.baseHighlightShift,
            settings.baseMidtoneShift,
            settings.baseShadowShift,
            settings.cyanRedHighlightShift,
            settings.cyanRedMidtoneShift,
            settings.cyanRedShadowShift,
            settings.magentaGreenHighlightShift,
            settings.magentaGreenMidtoneShift,
            settings.magentaGreenShadowShift,
            settings.yellowBlueHighlightShift,
            settings.yellowBlueMidtoneShift,
            settings.yellowBlueShadowShift
        };

        error = clSetKernelArg(
            adjustmentKernel,
            0,
            sizeof(cl_mem),
            &pixelBuffer
            );

        if (error == CL_SUCCESS) {
            error = clSetKernelArg(
                adjustmentKernel,
                1,
                sizeof(cl_mem),
                &lookupBuffer
                );
        }

        if (error == CL_SUCCESS) {
            error = clSetKernelArg(
                adjustmentKernel,
                2,
                sizeof(cl_uint),
                &pixelCount
                );
        }

        const cl_int flags[] = {
            useBasicLookups,
            useBaseHms,
            useColorHms,
            useSaturation
        };

        for (cl_uint index = 0;
             error == CL_SUCCESS && index < 4;
             ++index) {
            error = clSetKernelArg(
                adjustmentKernel,
                3 + index,
                sizeof(cl_int),
                &flags[index]
                );
        }

        for (cl_uint index = 0;
             error == CL_SUCCESS && index < 12;
             ++index) {
            error = clSetKernelArg(
                adjustmentKernel,
                7 + index,
                sizeof(cl_double),
                &scalarArguments[index]
                );
        }

        if (error == CL_SUCCESS) {
            error = clSetKernelArg(
                adjustmentKernel,
                19,
                sizeof(cl_int),
                &saturationAmount
                );
        }

        if (error != CL_SUCCESS) {
            releaseBuffers();

            if (errorMessage) {
                *errorMessage =
                    "Adjustment-kernel argument setup failed: " +
                    openClErrorText(error);
            }
            return false;
        }

        const std::size_t globalWorkSize =
            static_cast<std::size_t>(pixelCount);

        error = clEnqueueNDRangeKernel(
            commandQueue,
            adjustmentKernel,
            1,
            nullptr,
            &globalWorkSize,
            nullptr,
            0,
            nullptr,
            nullptr
            );

        if (error != CL_SUCCESS) {
            releaseBuffers();

            if (errorMessage) {
                *errorMessage =
                    "Adjustment-kernel launch failed: " +
                    openClErrorText(error);
            }
            return false;
        }

        std::vector<cl_uchar> resultBytes;

        try {
            resultBytes.resize(byteCount);
        } catch (const std::bad_alloc &) {
            releaseBuffers();

            if (errorMessage) {
                *errorMessage =
                    "Not enough host memory was available for the GPU result buffer.";
            }
            return false;
        }

        error = clEnqueueReadBuffer(
            commandQueue,
            pixelBuffer,
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

        releaseBuffers();

        if (error != CL_SUCCESS) {
            if (errorMessage) {
                *errorMessage =
                    "GPU result transfer failed: " +
                    openClErrorText(error);
            }
            return false;
        }

        std::memcpy(
            pixelBytes,
            resultBytes.data(),
            byteCount
            );

        return true;
    }

    bool executeLookups(
        void *pixelBytes,
        std::size_t byteCount,
        cl_uint pixelCount,
        const cl_uchar *lookups,
        std::size_t lookupByteCount,
        QString *errorMessage
        )
    {
        if (!pixelBytes ||
            byteCount == 0 ||
            pixelCount == 0 ||
            !lookups ||
            lookupByteCount != 768) {
            if (errorMessage) {
                *errorMessage =
                    "Invalid OpenCL lookup processing arguments.";
            }
            return false;
        }

        cl_int error = CL_SUCCESS;

        cl_mem pixelBuffer = clCreateBuffer(
            context,
            CL_MEM_READ_WRITE |
                CL_MEM_COPY_HOST_PTR,
            byteCount,
            pixelBytes,
            &error
            );

        if (!pixelBuffer ||
            error != CL_SUCCESS) {
            if (errorMessage) {
                *errorMessage =
                    "Pixel buffer creation failed: " +
                    openClErrorText(error);
            }
            return false;
        }

        cl_mem lookupBuffer = clCreateBuffer(
            context,
            CL_MEM_READ_ONLY |
                CL_MEM_COPY_HOST_PTR,
            lookupByteCount,
            const_cast<cl_uchar *>(lookups),
            &error
            );

        if (!lookupBuffer ||
            error != CL_SUCCESS) {
            clReleaseMemObject(pixelBuffer);

            if (errorMessage) {
                *errorMessage =
                    "Lookup buffer creation failed: " +
                    openClErrorText(error);
            }
            return false;
        }

        auto releaseBuffers = [this,
                               &pixelBuffer,
                               &lookupBuffer]() {
            if (lookupBuffer) {
                clReleaseMemObject(lookupBuffer);
                lookupBuffer = nullptr;
            }

            if (pixelBuffer) {
                clReleaseMemObject(pixelBuffer);
                pixelBuffer = nullptr;
            }
        };

        error = clSetKernelArg(
            lookupKernel,
            0,
            sizeof(cl_mem),
            &pixelBuffer
            );

        if (error == CL_SUCCESS) {
            error = clSetKernelArg(
                lookupKernel,
                1,
                sizeof(cl_mem),
                &lookupBuffer
                );
        }

        if (error == CL_SUCCESS) {
            error = clSetKernelArg(
                lookupKernel,
                2,
                sizeof(cl_uint),
                &pixelCount
                );
        }

        if (error != CL_SUCCESS) {
            releaseBuffers();

            if (errorMessage) {
                *errorMessage =
                    "Kernel argument setup failed: " +
                    openClErrorText(error);
            }
            return false;
        }

        const std::size_t globalWorkSize =
            static_cast<std::size_t>(pixelCount);

        error = clEnqueueNDRangeKernel(
            commandQueue,
            lookupKernel,
            1,
            nullptr,
            &globalWorkSize,
            nullptr,
            0,
            nullptr,
            nullptr
            );

        if (error != CL_SUCCESS) {
            releaseBuffers();

            if (errorMessage) {
                *errorMessage =
                    "Kernel launch failed: " +
                    openClErrorText(error);
            }
            return false;
        }

        // Read into temporary host memory first. If a driver reports a
        // transfer failure, the QImage remains untouched and the established
        // CPU path can safely process the original pixels exactly once.
        std::vector<cl_uchar> resultBytes;

        try {
            resultBytes.resize(byteCount);
        } catch (const std::bad_alloc &) {
            releaseBuffers();

            if (errorMessage) {
                *errorMessage =
                    "Not enough host memory was available for the GPU result buffer.";
            }
            return false;
        }

        error = clEnqueueReadBuffer(
            commandQueue,
            pixelBuffer,
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

        releaseBuffers();

        if (error != CL_SUCCESS) {
            if (errorMessage) {
                *errorMessage =
                    "GPU result transfer failed: " +
                    openClErrorText(error);
            }
            return false;
        }

        std::memcpy(
            pixelBytes,
            resultBytes.data(),
            byteCount
            );

        return true;
    }

    void shutdownUnlocked()
    {
        available = false;
        advancedColorAvailable = false;
        supportsFp64 = false;
        advancedColorStatus.clear();

        if (adjustmentKernel &&
            clReleaseKernel) {
            clReleaseKernel(adjustmentKernel);
            adjustmentKernel = nullptr;
        }

        if (lookupKernel &&
            clReleaseKernel) {
            clReleaseKernel(lookupKernel);
            lookupKernel = nullptr;
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

        if (library.isLoaded()) {
            library.unload();
        }
    }

    mutable QMutex mutex;
    bool initializationAttempted = false;
    bool available = false;
    bool supportsFp64 = false;
    bool advancedColorAvailable = false;
    QString deviceDisplayName;
    QString status =
        "OpenCL GPU processing has not been initialized.";
    QString advancedColorStatus;
    std::size_t maximumAllocationBytes = 0;

    QLibrary library;
    cl_device_id selectedDevice = nullptr;
    cl_context context = nullptr;
    cl_command_queue commandQueue = nullptr;
    cl_program program = nullptr;
    cl_kernel lookupKernel = nullptr;
    cl_kernel adjustmentKernel = nullptr;

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

OpenClProcessingBackend::OpenClProcessingBackend()
    : m_implementation(
          std::make_unique<Implementation>()
          )
{
}

OpenClProcessingBackend::~OpenClProcessingBackend() = default;

void OpenClProcessingBackend::initialize()
{
    m_implementation->initialize();
}

void OpenClProcessingBackend::shutdown()
{
    m_implementation->shutdown();
}

bool OpenClProcessingBackend::isAvailable() const
{
    return m_implementation->isAvailable();
}

bool OpenClProcessingBackend::adjustmentPipelineAvailable() const
{
    return m_implementation->adjustmentPipelineAvailable();
}

QString OpenClProcessingBackend::deviceName() const
{
    return m_implementation->deviceName();
}

QString OpenClProcessingBackend::statusText() const
{
    return m_implementation->statusText();
}

bool OpenClProcessingBackend::applyChannelLookups(
    QImage &image,
    const ChannelLookup &redLookup,
    const ChannelLookup &greenLookup,
    const ChannelLookup &blueLookup,
    QString *errorMessage
    )
{
    return m_implementation->applyChannelLookups(
        image,
        redLookup,
        greenLookup,
        blueLookup,
        errorMessage
        );
}

bool OpenClProcessingBackend::applyAdjustmentPipeline(
    QImage &image,
    const ChannelLookup &redLookup,
    const ChannelLookup &greenLookup,
    const ChannelLookup &blueLookup,
    const AdjustmentPipelineSettings &settings,
    QString *errorMessage
    )
{
    return m_implementation->applyAdjustmentPipeline(
        image,
        redLookup,
        greenLookup,
        blueLookup,
        settings,
        errorMessage
        );
}

