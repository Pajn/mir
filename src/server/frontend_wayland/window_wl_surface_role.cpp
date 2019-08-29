/*
 * Copyright © 2015-2018 Canonical Ltd.
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
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#include "window_wl_surface_role.h"

#include "output_manager.h"
#include "wayland_utils.h"
#include "wl_surface.h"
#include "wayland_surface_observer.h"
#include "wl_seat.h"

#include "mir/shell/surface_specification.h"
#include "mir/shell/shell.h"

#include "mir/frontend/wayland.h"
#include "mir/frontend/mir_client_session.h"
#include "null_event_sink.h"

#include "mir/scene/surface.h"
#include "mir/scene/surface_creation_parameters.h"

#include <boost/throw_exception.hpp>

namespace mf = mir::frontend;
namespace ms = mir::scene;
namespace msh = mir::shell;
namespace geom = mir::geometry;

mf::WindowWlSurfaceRole::WindowWlSurfaceRole(
    WlSeat* seat,
    wl_client* client,
    WlSurface* surface,
    std::shared_ptr<msh::Shell> const& shell,
    OutputManager* output_manager)
    : destroyed{std::make_shared<bool>(false)},
      surface{surface},
      client{client},
      weak_shell{shell},
      weak_session{get_session(client)},
      output_manager{output_manager},
      observer{std::make_shared<WaylandSurfaceObserver>(seat, client, surface, this)},
      params{std::make_unique<scene::SurfaceCreationParameters>(
          scene::SurfaceCreationParameters().of_type(mir_window_type_freestyle))}
{
    surface->set_role(this);
}

mf::WindowWlSurfaceRole::~WindowWlSurfaceRole()
{
    surface->clear_role();
    observer->disconnect();
    *destroyed = true;
    if (auto const scene_surface = weak_scene_surface.lock())
    {
        auto const shell = shell_checked();
        auto const session = session_checked();
        shell->destroy_surface(session, scene_surface);
        weak_scene_surface.reset();
    }
}

auto mf::WindowWlSurfaceRole::scene_surface() const -> std::experimental::optional<std::shared_ptr<scene::Surface>>
{
    auto shared = weak_scene_surface.lock();
    if (shared)
        return shared;
    else
        return std::experimental::nullopt;
}

void mf::WindowWlSurfaceRole::populate_spec_with_surface_data(shell::SurfaceSpecification& spec)
{
    spec.streams = std::vector<shell::StreamSpecification>();
    spec.input_shape = std::vector<geom::Rectangle>();
    surface->populate_surface_data(spec.streams.value(), spec.input_shape.value(), {});
}

void mf::WindowWlSurfaceRole::refresh_surface_data_now()
{
    if (auto const scene_surface = weak_scene_surface.lock())
    {
        shell::SurfaceSpecification surface_data_spec;
        populate_spec_with_surface_data(surface_data_spec);
        auto const shell = shell_checked();
        auto const session = session_checked();
        shell->modify_surface(session, scene_surface, surface_data_spec);
    }
}

void mf::WindowWlSurfaceRole::apply_spec(mir::shell::SurfaceSpecification const& new_spec)
{
    if (new_spec.width.is_set())
        pending_explicit_width = new_spec.width.value();
    if (new_spec.height.is_set())
        pending_explicit_height = new_spec.height.value();

    if (weak_scene_surface.lock())
    {
        spec().update_from(new_spec);
    }
    else
    {
        params->update_from(new_spec);
    }
}

void mf::WindowWlSurfaceRole::set_pending_offset(std::experimental::optional<geom::Displacement> const& offset)
{
    surface->set_pending_offset(offset);
}

void mf::WindowWlSurfaceRole::set_pending_width(std::experimental::optional<geometry::Width> const& width)
{
    pending_explicit_width = width;
}

void mf::WindowWlSurfaceRole::set_pending_height(std::experimental::optional<geometry::Height> const& height)
{
    pending_explicit_height = height;
}

void mf::WindowWlSurfaceRole::set_title(std::string const& title)
{
    if (weak_scene_surface.lock())
    {
        spec().name = title;
    }
    else
    {
        params->name = title;
    }
}

void mf::WindowWlSurfaceRole::initiate_interactive_move()
{
    if (auto const scene_surface = weak_scene_surface.lock())
    {
        auto const shell = shell_checked();
        auto const session = session_checked();
        shell->request_move(session, scene_surface, observer->latest_timestamp().count());
    }
}

void mf::WindowWlSurfaceRole::initiate_interactive_resize(MirResizeEdge edge)
{
    if (auto const scene_surface = weak_scene_surface.lock())
    {
        auto const shell = shell_checked();
        auto const session = session_checked();
        shell->request_resize(session, scene_surface, observer->latest_timestamp().count(), edge);
    }
}

void mf::WindowWlSurfaceRole::set_parent(std::experimental::optional<std::weak_ptr<scene::Surface>> const& parent)
{
    if (weak_scene_surface.lock())
    {
        if (parent)
            spec().parent = parent.value();
        else
            spec().parent.consume();
    }
    else
    {
        if (parent)
            params->parent = parent.value();
        else
            params->parent = {};
    }
}

void mf::WindowWlSurfaceRole::set_max_size(int32_t width, int32_t height)
{
    if (weak_scene_surface.lock())
    {
        if (width == 0) width = std::numeric_limits<int>::max();
        if (height == 0) height = std::numeric_limits<int>::max();

        auto& mods = spec();
        mods.max_width = geom::Width{width};
        mods.max_height = geom::Height{height};
    }
    else
    {
        if (width == 0)
        {
            if (params->max_width.is_set())
                params->max_width.consume();
        }
        else
            params->max_width = geom::Width{width};

        if (height == 0)
        {
            if (params->max_height.is_set())
                params->max_height.consume();
        }
        else
            params->max_height = geom::Height{height};
    }
}

void mf::WindowWlSurfaceRole::set_min_size(int32_t width, int32_t height)
{
    if (weak_scene_surface.lock())
    {
        auto& mods = spec();
        mods.min_width = geom::Width{width};
        mods.min_height = geom::Height{height};
    }
    else
    {
        params->min_width = geom::Width{width};
        params->min_height = geom::Height{height};
    }
}

void mf::WindowWlSurfaceRole::set_fullscreen(std::experimental::optional<struct wl_resource*> const& output)
{
    // We must process this request immediately (i.e. don't defer until commit())
    if (auto const scene_surface = weak_scene_surface.lock())
    {
        shell::SurfaceSpecification mods;
        mods.state = mir_window_state_fullscreen;
        mods.output_id = output_manager->output_id_for(client, output);
        auto const shell = shell_checked();
        auto const session = session_checked();
        shell->modify_surface(session, scene_surface, mods);
    }
    else
    {
        params->state = mir_window_state_fullscreen;
        if (output)
            params->output_id = output_manager->output_id_for(client, output.value());
        create_mir_window();
    }
}

void mf::WindowWlSurfaceRole::set_state_now(MirWindowState state)
{
    if (auto const scene_surface = weak_scene_surface.lock())
    {
        shell::SurfaceSpecification mods;
        mods.state = state;
        auto const shell = shell_checked();
        auto const session = session_checked();
        shell->modify_surface(session, scene_surface, mods);
    }
    else
    {
        params->state = state;
        create_mir_window();
    }
}

auto mf::WindowWlSurfaceRole::pending_size() const -> geom::Size
{
    auto size = current_size();
    if (pending_explicit_width)
        size.width = pending_explicit_width.value();
    if (pending_explicit_height)
        size.height = pending_explicit_height.value();
    return size;
}

auto mf::WindowWlSurfaceRole::current_size() const -> geom::Size
{
    auto size = committed_size.value_or(geom::Size{640, 480});
    if (surface->buffer_size())
    {
        if (!committed_width_set_explicitly)
            size.width = surface->buffer_size().value().width;
        if (!committed_height_set_explicitly)
            size.height = surface->buffer_size().value().height;
    }
    return size;
}

std::experimental::optional<geom::Size> mf::WindowWlSurfaceRole::requested_window_size()
{
    return observer->requested_window_size();
}

auto mf::WindowWlSurfaceRole::window_state() -> MirWindowState
{
    return observer->state();
}

auto mf::WindowWlSurfaceRole::is_active() -> bool
{
    return observer->is_active();
}

std::chrono::nanoseconds mf::WindowWlSurfaceRole::latest_timestamp()
{
    return observer->latest_timestamp();
}

void mf::WindowWlSurfaceRole::commit(WlSurfaceState const& state)
{
    surface->commit(state);

    handle_commit();

    auto size = pending_size();
    observer->latest_client_size(size);

    if (auto const scene_surface = weak_scene_surface.lock())
    {
        if (!committed_size || size != committed_size.value())
        {
            spec().width = size.width;
            spec().height = size.height;
        }

        if (state.surface_data_needs_refresh())
        {
            populate_spec_with_surface_data(spec());
        }

        auto const shell = shell_checked();
        auto const session = session_checked();

        if (pending_changes)
            shell->modify_surface(session, scene_surface, *pending_changes);

        pending_changes.reset();
    }
    else
    {
        create_mir_window();
    }

    committed_size = size;
    if (pending_explicit_width)
        committed_width_set_explicitly = true;
    if (pending_explicit_height)
        committed_height_set_explicitly = true;
    pending_explicit_width = std::experimental::nullopt;
    pending_explicit_height = std::experimental::nullopt;
}

auto mf::WindowWlSurfaceRole::shell_checked() -> std::shared_ptr<msh::Shell>
{
    auto locked = weak_shell.lock();
    if (locked)
        return locked;
    else
        BOOST_THROW_EXCEPTION(std::logic_error("weak_shell is null"));
}

auto mf::WindowWlSurfaceRole::session_checked() -> std::shared_ptr<ms::Session>
{
    auto locked = weak_session.lock();
    if (locked)
        return locked;
    else
        BOOST_THROW_EXCEPTION(std::logic_error("weak_session is null"));
}

void mf::WindowWlSurfaceRole::visiblity(bool visible)
{
    auto const scene_surface = weak_scene_surface.lock();
    if (!scene_surface)
        return;

    if (scene_surface->visible() == visible)
        return;

    if (visible)
    {
        if (scene_surface->state() == mir_window_state_hidden)
            spec().state = mir_window_state_restored;
    }
    else
    {
        if (scene_surface->state() != mir_window_state_hidden)
            spec().state = mir_window_state_hidden;
    }
}

mir::shell::SurfaceSpecification& mf::WindowWlSurfaceRole::spec()
{
    if (!pending_changes)
        pending_changes = std::make_unique<mir::shell::SurfaceSpecification>();

    return *pending_changes;
}

void mf::WindowWlSurfaceRole::create_mir_window()
{
    params->size = pending_size();
    params->streams = std::vector<shell::StreamSpecification>{};
    params->input_shape = std::vector<geom::Rectangle>{};
    surface->populate_surface_data(params->streams.value(), params->input_shape.value(), {});

    auto const shell = shell_checked();
    auto const session = session_checked();

    auto const scene_surface = shell->create_surface(session, *params, std::make_shared<NullEventSink>());
    weak_scene_surface = scene_surface;

    scene_surface->add_observer(observer);

    // HACK: This is needed because the surface observer is added after the surface is created, and placed_relative() is
    // called during creation. It will go away once the plumbing is in place to send the observer to the shell
    if (params->aux_rect.is_set() && params->placement_hints.is_set())
    {
        shell::SurfaceSpecification mods;
        mods.aux_rect = params->aux_rect;
        mods.placement_hints = params->placement_hints;
        mods.placement_hints = params->placement_hints;
        mods.surface_placement_gravity = params->surface_placement_gravity;
        mods.aux_rect_placement_gravity = params->aux_rect_placement_gravity;
        shell->modify_surface(session, scene_surface, mods);
    }

    // The shell isn't guaranteed to respect the requested size
    auto const client_size = scene_surface->client_size();
    if (client_size != params->size)
        observer->resized_to(scene_surface.get(), client_size);
}

