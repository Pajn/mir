/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "src/client/mir_output_capture.h"
#include "src/client/client_buffer_factory.h"
#include "src/client/client_platform.h"

#include "mir_test_doubles/null_client_buffer.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <thread>

namespace mcl = mir::client;
namespace mp = mir::protobuf;
namespace mtd = mir::test::doubles;

namespace google
{
namespace protobuf
{
class RpcController;
}
}

namespace
{

struct MockProtobufServer : mir::protobuf::DisplayServer
{
    MOCK_METHOD4(create_capture,
                 void(google::protobuf::RpcController* /*controller*/,
                      mp::CaptureParameters const* /*request*/,
                      mp::Capture* /*response*/,
                      google::protobuf::Closure* /*done*/));
    MOCK_METHOD4(release_capture,
                 void(google::protobuf::RpcController* /*controller*/,
                      mp::CaptureId const* /*request*/,
                      mp::Void* /*response*/,
                      google::protobuf::Closure* /*done*/));
    MOCK_METHOD4(capture_output,
                 void(google::protobuf::RpcController* /*controller*/,
                      mp::CaptureId const* /*request*/,
                      mp::Buffer* /*response*/,
                      google::protobuf::Closure* /*done*/));
};

class StubProtobufServer : public mir::protobuf::DisplayServer
{
public:
    void create_capture(
        google::protobuf::RpcController* /*controller*/,
        mp::CaptureParameters const* /*request*/,
        mp::Capture* /*response*/,
        google::protobuf::Closure* done) override
    {
        if (server_thread.joinable())
            server_thread.join();
        server_thread = std::thread{[done, this] { done->Run(); }};
    }

    void release_capture(
        google::protobuf::RpcController* /*controller*/,
        mp::CaptureId const* /*request*/,
        mp::Void* /*response*/,
        google::protobuf::Closure* done) override
    {
        if (server_thread.joinable())
            server_thread.join();
        server_thread = std::thread{[done, this] { done->Run(); }};
    }

    void capture_output(
        google::protobuf::RpcController* /*controller*/,
        mp::CaptureId const* /*request*/,
        mp::Buffer* /*response*/,
        google::protobuf::Closure* done) override
    {
        if (server_thread.joinable())
            server_thread.join();
        server_thread = std::thread{[done, this] { done->Run(); }};
    }

    ~StubProtobufServer()
    {
        if (server_thread.joinable())
            server_thread.join();
    }

private:
    std::thread server_thread;
};

struct StubEGLNativeWindowFactory : mcl::EGLNativeWindowFactory
{
    std::shared_ptr<EGLNativeWindowType>
        create_egl_native_window(mcl::ClientSurface*)
    {
        return std::make_shared<EGLNativeWindowType>(egl_native_window);
    }

    static EGLNativeWindowType egl_native_window;
};

EGLNativeWindowType StubEGLNativeWindowFactory::egl_native_window{
    reinterpret_cast<EGLNativeWindowType>(&StubEGLNativeWindowFactory::egl_native_window)};

class StubClientBufferFactory : public mcl::ClientBufferFactory
{
    std::shared_ptr<mcl::ClientBuffer> create_buffer(
        std::shared_ptr<MirBufferPackage> const& /*package*/,
        mir::geometry::Size /*size*/, MirPixelFormat /*pf*/)
    {
        return std::make_shared<mtd::NullClientBuffer>();
    }
};

struct MockClientBufferFactory : mcl::ClientBufferFactory
{
    MOCK_METHOD3(create_buffer,
                 std::shared_ptr<mcl::ClientBuffer>(
                    std::shared_ptr<MirBufferPackage> const& /*package*/,
                    mir::geometry::Size /*size*/, MirPixelFormat /*pf*/));
};

MATCHER_P(WithOutputId, value, "")
{
    return arg->output_id() == value;
}

MATCHER_P(WithCaptureId, value, "")
{
    return arg->value() == value;
}

ACTION_P(SetCreateCaptureId, capture_id)
{
    arg2->mutable_capture_id()->set_value(capture_id);
}

ACTION_P(SetCreateBufferId, buffer_id)
{
    arg2->mutable_buffer()->set_buffer_id(buffer_id);
}

ACTION_P(SetBufferId, buffer_id)
{
    arg2->set_buffer_id(buffer_id);
}

ACTION_P(SetCreateBufferFromPackage, package)
{
    auto buffer = arg2->mutable_buffer();
    for (int i = 0; i != package.data_items; ++i)
    {
        buffer->add_data(package.data[i]);
    }

    for (int i = 0; i != package.fd_items; ++i)
    {
        buffer->add_fd(package.fd[i]);
    }

    buffer->set_stride(package.stride);
}

ACTION(RunClosure)
{
    arg3->Run();
}

MATCHER_P(BufferPackageSharedPtrMatches, package, "")
{
    if (package.data_items != arg->data_items)
        return false;
    if (package.fd_items != arg->fd_items)
        return false;
    if (memcmp(package.data, arg->data, sizeof(package.data[0]) * package.data_items))
        return false;
    if (package.stride != arg->stride)
        return false;
    return true;
}

struct MockCallback
{
    MOCK_METHOD2(call, void(void*, void*));
};

void mock_callback_func(MirOutputCapture* capture, void* context)
{
    auto mock_cb = static_cast<MockCallback*>(context);
    mock_cb->call(capture, context);
}

void null_callback_func(MirOutputCapture*, void*)
{
}

struct CustomMirDisplayOutput : MirDisplayOutput
{
    CustomMirDisplayOutput(uint32_t output_id_arg,
                           uint32_t num_modes_arg,
                           uint32_t current_mode_arg,
                           MirPixelFormat current_format_arg)
        : MirDisplayOutput(),
          modes_uptr{new MirDisplayMode[num_modes_arg]}
    {
        used = true;
        connected = true;
        output_id = output_id_arg;
        num_modes = num_modes_arg;
        current_mode = current_mode_arg;
        current_format = current_format_arg;
        modes = modes_uptr.get();
        for (uint32_t i = 0; i != num_modes; ++i)
        {
            modes[i] = {50 * i, 60 * i, 10.0 * i};
        }
    }

    mir::geometry::Size current_size() const
    {
        return {modes[current_mode].horizontal_resolution,
                modes[current_mode].vertical_resolution};
    }

    std::unique_ptr<MirDisplayMode[]> const modes_uptr;
};

class MirOutputCaptureTest : public testing::Test
{
public:
    MirOutputCaptureTest()
        : default_output_id{5},
          default_mir_output{default_output_id, 3, 1, mir_pixel_format_xbgr_8888},
          stub_egl_native_window_factory{std::make_shared<StubEGLNativeWindowFactory>()},
          stub_client_buffer_factory{std::make_shared<StubClientBufferFactory>()},
          mock_client_buffer_factory{std::make_shared<MockClientBufferFactory>()}
    {
    }

    testing::NiceMock<MockProtobufServer> mock_server;
    StubProtobufServer stub_server;
    uint32_t const default_output_id;
    CustomMirDisplayOutput const default_mir_output;
    std::shared_ptr<StubEGLNativeWindowFactory> const stub_egl_native_window_factory;
    std::shared_ptr<StubClientBufferFactory> const stub_client_buffer_factory;
    std::shared_ptr<MockClientBufferFactory> const mock_client_buffer_factory;
};

}

TEST_F(MirOutputCaptureTest, creates_capture_on_construction)
{
    using namespace testing;

    EXPECT_CALL(mock_server,
                create_capture(_,WithOutputId(default_output_id),_,_))
        .WillOnce(RunClosure());

    MirOutputCapture capture{
        default_mir_output, mock_server,
        stub_egl_native_window_factory,
        stub_client_buffer_factory,
        null_callback_func, nullptr};
}

TEST_F(MirOutputCaptureTest, releases_capture_on_release)
{
    using namespace testing;

    uint32_t const capture_id{77};

    InSequence seq;

    EXPECT_CALL(mock_server,
                create_capture(_,WithOutputId(default_output_id),_,_))
        .WillOnce(DoAll(SetCreateCaptureId(capture_id), RunClosure()));

    EXPECT_CALL(mock_server,
                release_capture(_,WithCaptureId(capture_id),_,_))
        .WillOnce(RunClosure());

    MirOutputCapture capture{
        default_mir_output, mock_server,
        stub_egl_native_window_factory,
        stub_client_buffer_factory,
        null_callback_func, nullptr};

    capture.release(null_callback_func, nullptr);
}

TEST_F(MirOutputCaptureTest, requests_capture_on_next_buffer)
{
    using namespace testing;
    uint32_t const capture_id{77};

    InSequence seq;

    EXPECT_CALL(mock_server,
                create_capture(_,WithOutputId(default_output_id),_,_))
        .WillOnce(DoAll(SetCreateCaptureId(capture_id), RunClosure()));

    EXPECT_CALL(mock_server,
                capture_output(_,WithCaptureId(capture_id),_,_))
        .WillOnce(RunClosure());

    MirOutputCapture capture{
        default_mir_output, mock_server,
        stub_egl_native_window_factory,
        stub_client_buffer_factory,
        null_callback_func, nullptr};

    capture.next_buffer(null_callback_func, nullptr);
}

TEST_F(MirOutputCaptureTest, executes_callback_on_creation)
{
    using namespace testing;

    MockCallback mock_cb;
    EXPECT_CALL(mock_cb, call(_, &mock_cb));

    MirOutputCapture capture{
        default_mir_output, stub_server,
        stub_egl_native_window_factory,
        stub_client_buffer_factory,
        mock_callback_func, &mock_cb};

    capture.creation_wait_handle()->wait_for_all();
}

TEST_F(MirOutputCaptureTest, executes_callback_on_release)
{
    using namespace testing;

    MirOutputCapture capture{
        default_mir_output, stub_server,
        stub_egl_native_window_factory,
        stub_client_buffer_factory,
        null_callback_func, nullptr};

    capture.creation_wait_handle()->wait_for_all();

    MockCallback mock_cb;
    EXPECT_CALL(mock_cb, call(&capture, &mock_cb));

    auto wh = capture.release(mock_callback_func, &mock_cb);
    wh->wait_for_all();
}

TEST_F(MirOutputCaptureTest, executes_callback_on_next_buffer)
{
    using namespace testing;

    MirOutputCapture capture{
        default_mir_output, stub_server,
        stub_egl_native_window_factory,
        stub_client_buffer_factory,
        null_callback_func, nullptr};

    capture.creation_wait_handle()->wait_for_all();

    MockCallback mock_cb;
    EXPECT_CALL(mock_cb, call(&capture, &mock_cb));

    auto wh = capture.next_buffer(mock_callback_func, &mock_cb);
    wh->wait_for_all();
}

TEST_F(MirOutputCaptureTest, construction_throws_on_invalid_output)
{
    uint32_t const num_modes{2};
    uint32_t const invalid_current_mode{3};
    uint32_t const valid_current_mode{1};

    CustomMirDisplayOutput invalid_modes_output{
        default_output_id, num_modes, invalid_current_mode, mir_pixel_format_xbgr_8888};

    EXPECT_THROW({
        MirOutputCapture capture(
            invalid_modes_output, stub_server,
            stub_egl_native_window_factory,
            stub_client_buffer_factory,
            null_callback_func, nullptr);
    }, std::runtime_error);

    CustomMirDisplayOutput unused_output{
        default_output_id, num_modes, valid_current_mode, mir_pixel_format_xbgr_8888};
    unused_output.used = false;

    EXPECT_THROW({
        MirOutputCapture capture(
            unused_output, stub_server,
            stub_egl_native_window_factory,
            stub_client_buffer_factory,
            null_callback_func, nullptr);
    }, std::runtime_error);
}

TEST_F(MirOutputCaptureTest, returns_correct_surface_parameters)
{
    MirOutputCapture capture{
        default_mir_output, stub_server,
        stub_egl_native_window_factory,
        stub_client_buffer_factory,
        null_callback_func, nullptr};

    capture.creation_wait_handle()->wait_for_all();

    auto params = capture.get_parameters();

    EXPECT_STREQ("", params.name);
    EXPECT_EQ(default_mir_output.current_size().width.as_int(), params.width);
    EXPECT_EQ(default_mir_output.current_size().height.as_int(), params.height);
    EXPECT_EQ(default_mir_output.current_format, params.pixel_format);
    EXPECT_EQ(mir_buffer_usage_hardware, params.buffer_usage);
    EXPECT_EQ(default_mir_output.output_id, params.output_id);
}

TEST_F(MirOutputCaptureTest, uses_buffer_message_from_server)
{
    using namespace testing;

    auto const client_buffer1 = std::make_shared<mtd::NullClientBuffer>();
    MirBufferPackage buffer_package;
    buffer_package.fd_items = 1;
    buffer_package.fd[0] = 16;
    buffer_package.data_items = 2;
    buffer_package.data[0] = 100;
    buffer_package.data[1] = 234;
    buffer_package.stride = 768;

    EXPECT_CALL(mock_server,
                create_capture(_,WithOutputId(default_output_id),_,_))
        .WillOnce(DoAll(SetCreateBufferFromPackage(buffer_package), RunClosure()));

    EXPECT_CALL(*mock_client_buffer_factory,
                create_buffer(BufferPackageSharedPtrMatches(buffer_package),_,_))
        .WillOnce(Return(client_buffer1));

    MirOutputCapture capture{
        default_mir_output, mock_server,
        stub_egl_native_window_factory,
        mock_client_buffer_factory,
        null_callback_func, nullptr};

    capture.creation_wait_handle()->wait_for_all();
}

TEST_F(MirOutputCaptureTest, returns_current_client_buffer)
{
    using namespace testing;

    uint32_t const capture_id = 88;
    int const buffer_id1 = 5;
    int const buffer_id2 = 6;
    auto const client_buffer1 = std::make_shared<mtd::NullClientBuffer>();
    auto const client_buffer2 = std::make_shared<mtd::NullClientBuffer>();

    EXPECT_CALL(mock_server,
                create_capture(_,WithOutputId(default_output_id),_,_))
        .WillOnce(DoAll(SetCreateBufferId(buffer_id1),
                        SetCreateCaptureId(capture_id),
                        RunClosure()));

    EXPECT_CALL(mock_server,
                capture_output(_,WithCaptureId(capture_id),_,_))
        .WillOnce(DoAll(SetBufferId(buffer_id2), RunClosure()));

    EXPECT_CALL(*mock_client_buffer_factory, create_buffer(_,_,_))
        .WillOnce(Return(client_buffer1))
        .WillOnce(Return(client_buffer2));

    MirOutputCapture capture{
        default_mir_output, mock_server,
        stub_egl_native_window_factory,
        mock_client_buffer_factory,
        null_callback_func, nullptr};

    capture.creation_wait_handle()->wait_for_all();

    EXPECT_EQ(client_buffer1, capture.get_current_buffer());

    auto wh = capture.next_buffer(null_callback_func, nullptr);
    wh->wait_for_all();

    EXPECT_EQ(client_buffer2, capture.get_current_buffer());
}

TEST_F(MirOutputCaptureTest, gets_egl_native_window)
{
    using namespace testing;

    MirOutputCapture capture{
        default_mir_output, stub_server,
        stub_egl_native_window_factory,
        stub_client_buffer_factory,
        null_callback_func, nullptr};

    capture.creation_wait_handle()->wait_for_all();

    auto egl_native_window = capture.egl_native_window();

    EXPECT_EQ(StubEGLNativeWindowFactory::egl_native_window, egl_native_window);
}
