/*
 * Copyright © 2013-2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 or 3,
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
 * Authored By: Robert Carr <racarr@canonical.com>
 */

#ifndef MIR_SCENE_APPLICATION_SESSION_H_
#define MIR_SCENE_APPLICATION_SESSION_H_

#include "mir/scene/session.h"

#include "output_properties_cache.h"
#include "mir/observer_registrar.h"

#include <atomic>
#include <map>
#include <mutex>

namespace mir
{
class Executor;
namespace frontend
{
class EventSink;
class ClientBuffers;
}
namespace compositor { class BufferStream; }
namespace graphics
{
class DisplayConfiguration;
class GraphicBufferAllocator;
class BufferAttribute;
class DisplayConfigurationObserver;
}
namespace shell { class SurfaceStack; }
namespace scene
{
class SessionListener;
class Surface;
class SnapshotStrategy;
class BufferStreamFactory;
class SurfaceFactory;

class ApplicationSession
    : public Session
{
public:
    ApplicationSession(
        std::shared_ptr<shell::SurfaceStack> const& surface_stack,
        std::shared_ptr<SurfaceFactory> const& surface_factory,
        std::shared_ptr<BufferStreamFactory> const& buffer_stream_factory,
        pid_t pid,
        std::string const& session_name,
        std::shared_ptr<SnapshotStrategy> const& snapshot_strategy,
        std::shared_ptr<SessionListener> const& session_listener,
        graphics::DisplayConfiguration const& initial_config,
        std::shared_ptr<frontend::EventSink> const& sink,
        std::shared_ptr<graphics::GraphicBufferAllocator> const& allocator,
        std::shared_ptr<ObserverRegistrar<graphics::DisplayConfigurationObserver>> const& display_config_registrar);

    ~ApplicationSession();

    auto create_surface(
        SurfaceCreationParameters const& params,
        std::shared_ptr<frontend::EventSink> const& surface_sink) -> std::shared_ptr<scene::Surface> override;
    void destroy_surface(frontend::SurfaceId surface) override;
    auto surface(frontend::SurfaceId surface) const -> std::shared_ptr<Surface> override;
    auto surface_id(std::shared_ptr<scene::Surface> const& surface) const -> frontend::SurfaceId override;
    std::shared_ptr<Surface> surface_after(std::shared_ptr<Surface> const&) const override;

    void take_snapshot(SnapshotCallback const& snapshot_taken) override;
    std::shared_ptr<Surface> default_surface() const override;

    std::string name() const override;
    pid_t process_id() const override;

    void hide() override;
    void show() override;

    void send_display_config(graphics::DisplayConfiguration const& info) override;
    void send_error(ClientVisibleError const& error) override;
    void send_input_config(MirInputConfig const& devices) override;

    void set_lifecycle_state(MirLifecycleState state) override;

    void start_prompt_session() override;
    void stop_prompt_session() override;
    void suspend_prompt_session() override;
    void resume_prompt_session() override;

    std::shared_ptr<frontend::BufferStream> get_buffer_stream(frontend::BufferStreamId stream) const override;
    frontend::BufferStreamId create_buffer_stream(graphics::BufferProperties const& params) override;
    void destroy_buffer_stream(frontend::BufferStreamId stream) override;
    void configure_streams(Surface& surface, std::vector<shell::StreamSpecification> const& config) override;
    void destroy_surface(std::weak_ptr<Surface> const& surface) override;
protected:
    ApplicationSession(ApplicationSession const&) = delete;
    ApplicationSession& operator=(ApplicationSession const&) = delete;

private:
    class DisplayConfigurationObserver;

    std::shared_ptr<shell::SurfaceStack> const surface_stack;
    std::shared_ptr<SurfaceFactory> const surface_factory;
    std::shared_ptr<BufferStreamFactory> const buffer_stream_factory;
    pid_t const pid;
    std::string const session_name;
    std::shared_ptr<SnapshotStrategy> const snapshot_strategy;
    std::shared_ptr<SessionListener> const session_listener;
    std::shared_ptr<frontend::EventSink> const event_sink;
    std::shared_ptr<graphics::GraphicBufferAllocator> const gralloc;
    std::weak_ptr<ObserverRegistrar<graphics::DisplayConfigurationObserver>> const display_config_registrar;
    std::shared_ptr<DisplayConfigurationObserver> const display_config_observer;
    std::unique_ptr<Executor> const display_config_observer_executor;

    frontend::SurfaceId next_id();

    std::atomic<int> next_surface_id;

    OutputPropertiesCache output_cache;

    typedef std::map<frontend::SurfaceId, std::shared_ptr<Surface>> Surfaces;
    typedef std::map<frontend::BufferStreamId, std::shared_ptr<compositor::BufferStream>> Streams;
    auto checked_find(frontend::SurfaceId id) const -> Surfaces::const_iterator;
    auto checked_find(std::shared_ptr<scene::Surface> const& surface) const -> Surfaces::const_iterator;
    auto checked_find(frontend::BufferStreamId id) const -> Streams::const_iterator;
    std::mutex mutable surfaces_and_streams_mutex;
    Surfaces surfaces;
    Streams streams;

    std::map<frontend::SurfaceId, frontend::BufferStreamId> default_content_map;

    void destroy_surface(std::unique_lock<std::mutex>& lock, Surfaces::const_iterator in_surfaces);
};

}
} // namespace mir

#endif // MIR_SCENE_APPLICATION_SESSION_H_
