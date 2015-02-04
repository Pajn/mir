/*
 * Copyright © 2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#define MIR_INCLUDE_DEPRECATED_EVENT_HEADER

#include "mir/input/input_device_info.h"

#include "mir_test_framework/connected_client_with_a_surface.h"
#include "mir_test_framework/fake_input_device.h"
#include "mir_test_framework/stub_server_platform_factory.h"
#include "mir_test/wait_condition.h"
#include "mir_test/spin_wait.h"
#include "mir_test/event_matchers.h"
#include "mir_test/event_factory.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <linux/input.h>

#include <condition_variable>
#include <chrono>
#include <mutex>

namespace mi = mir::input;
namespace mt = mir::test;
namespace mis = mir::input::synthesis;
namespace mtf = mir_test_framework;

namespace
{

struct MockInputHandler
{
    MOCK_METHOD1(handle_input, void(MirEvent const*));
};

struct TestClientInputNew : mtf::ConnectedClientWithASurface
{
    void SetUp() override
    {
        ConnectedClientWithASurface::SetUp();

        MirEventDelegate const event_delegate { handle_input, this };
        mir_surface_set_event_handler(surface, &event_delegate);
        mir_surface_swap_buffers_sync(surface);

        wait_for_surface_to_become_focused_and_exposed();
        ready_to_accept_events.wake_up_everyone();
    }

    static void handle_input(MirSurface*, MirEvent const* ev, void* context)
    {
        auto const client = static_cast<TestClientInputNew*>(context);
        auto type = mir_event_get_type(ev);

        if (type == mir_event_type_key ||
            type == mir_event_type_motion )
            client->handler.handle_input(ev);
    }

    void wait_for_surface_to_become_focused_and_exposed()
    {
        bool success = mt::spin_wait_for_condition_or_timeout(
            [&]
            {
                return mir_surface_get_visibility(surface) == mir_surface_visibility_exposed &&
                       mir_surface_get_focus(surface) == mir_surface_focused;
            },
            std::chrono::seconds{5});

        if (!success)
            throw std::runtime_error("Timeout waiting for surface to become focused and exposed");
    }


    MockInputHandler handler;
    mir::test::WaitCondition all_events_received;
    mir::test::WaitCondition ready_to_accept_events;
    std::unique_ptr<mtf::FakeInputDevice> fake_keyboard{
        mtf::add_fake_input_device(mi::InputDeviceInfo{ 0, "keyboard", "keyboard-uid" , mi::DeviceCapability::keyboard})
        };
};

}


TEST_F(TestClientInputNew, new_clients_receive_us_english_mapped_keys)
{
    using namespace testing;

    InSequence seq;

    EXPECT_CALL(handler,
                handle_input(
                    AllOf(mt::KeyDownEvent(), mt::KeyOfSymbol(XKB_KEY_M))));
    EXPECT_CALL(handler,
                handle_input(
                    AllOf(mt::KeyDownEvent(), mt::KeyOfSymbol(XKB_KEY_I))));
    EXPECT_CALL(handler,
                handle_input(
                    AllOf(mt::KeyDownEvent(), mt::KeyOfSymbol(XKB_KEY_R))))
        .WillOnce(mt::WakeUp(&all_events_received));

    //client.start();

    fake_keyboard->emit_event(
        mis::a_key_down_event().of_scancode(KEY_RIGHTSHIFT));
    fake_keyboard->emit_event(
        mis::a_key_down_event().of_scancode(KEY_M));
    fake_keyboard->emit_event(
        mis::a_key_down_event().of_scancode(KEY_I));
    fake_keyboard->emit_event(
        mis::a_key_down_event().of_scancode(KEY_R));
}
