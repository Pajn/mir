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
 * Authored By: Nick Dedekind <nick.dedekind@canonical.com>
 */

#ifndef MIR_SCENE_TRUST_SESSION_IMPL_H_
#define MIR_SCENE_TRUST_SESSION_IMPL_H_

#include "mir/scene/trust_session.h"

#include <atomic>
#include <mutex>

namespace mir
{

namespace scene
{
class SessionContainer;
class TrustSessionCreationParameters;
class TrustSessionListener;
class TrustSessionContainer;
class TrustSessionTrustedParticipants;

class TrustSessionImpl : public TrustSession
{
public:
    TrustSessionImpl(std::weak_ptr<Session> const& session,
                 TrustSessionCreationParameters const& parameters,
                 std::shared_ptr<TrustSessionListener> const& trust_session_listener);
    ~TrustSessionImpl();

    std::weak_ptr<Session> get_trusted_helper() const override;

    void stop() override;

protected:
    TrustSessionImpl(const TrustSessionImpl&) = delete;
    TrustSessionImpl& operator=(const TrustSessionImpl&) = delete;

private:
    std::weak_ptr<Session> const trusted_helper;
    std::shared_ptr<TrustSessionListener> const trust_session_listener;

    MirTrustSessionState state;
    std::string cookie;
    std::recursive_mutex mutable mutex;
};
}
}

#endif // MIR_SCENE_TRUST_SESSION_IMPL_H_
