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

#ifndef MIR_SCENE_TRUST_SESSION_PARTICIPANT_CONTAINER_H_
#define MIR_SCENE_TRUST_SESSION_PARTICIPANT_CONTAINER_H_

#include <sys/types.h>
#include <mutex>
#include <unordered_map>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/composite_key.hpp>
#include <boost/multi_index/mem_fun.hpp>

namespace mir
{
namespace frontend
{
class TrustSession;
class Session;
}

using boost::multi_index_container;
using namespace boost::multi_index;

namespace scene
{

class TrustSessionContainer
{
public:
    TrustSessionContainer();
    virtual ~TrustSessionContainer() = default;

    void insert_trust_session(std::shared_ptr<frontend::TrustSession> const& trust_session);
    void remove_trust_session(std::shared_ptr<frontend::TrustSession> const& trust_session);

    bool insert_participant(frontend::TrustSession* trust_session, std::weak_ptr<frontend::Session> const& session);
    bool remove_participant(frontend::TrustSession* trust_session, std::weak_ptr<frontend::Session> const& session);

    void for_each_participant_for_trust_session(frontend::TrustSession* trust_session, std::function<void(std::weak_ptr<frontend::Session> const&)> f) const;
    void for_each_trust_session_for_participant(std::weak_ptr<frontend::Session> const& session, std::function<void(std::shared_ptr<frontend::TrustSession> const&)> f) const;

    bool insert_waiting_process(frontend::TrustSession* trust_session, pid_t process_id);
    void for_each_trust_session_for_waiting_process(pid_t process_id, std::function<void(std::shared_ptr<frontend::TrustSession> const&)> f) const;

private:
    std::mutex mutable mutex;

    std::unordered_map<frontend::TrustSession*, std::shared_ptr<frontend::TrustSession>> trust_sessions;

    typedef struct {
        frontend::TrustSession* trust_session;
        std::weak_ptr<frontend::Session> session;
        uint insert_order;

        frontend::Session* session_fun() const { return session.lock().get(); }
    } Participant;

    typedef multi_index_container<
        Participant,
        indexed_by<
            ordered_non_unique<
                composite_key<
                    Participant,
                    member<Participant, frontend::TrustSession*, &Participant::trust_session>,
                    member<Participant, uint, &Participant::insert_order>
                >
            >,
            ordered_unique<
                composite_key<
                    Participant,
                    const_mem_fun<Participant, frontend::Session*, &Participant::session_fun>,
                    member<Participant, frontend::TrustSession*, &Participant::trust_session>
                >
            >
        >
    > TrustSessionParticipants;

    typedef nth_index<TrustSessionParticipants,0>::type participant_by_trust_session;
    typedef nth_index<TrustSessionParticipants,1>::type participant_by_session;

    TrustSessionParticipants participant_map;
    participant_by_trust_session& trust_session_index;
    participant_by_session& participant_index;
    static uint insertion_order;

    typedef struct {
        frontend::TrustSession* trust_session;
        pid_t process_id;
    } WaitingProcess;

    typedef multi_index_container<
        WaitingProcess,
        indexed_by<
            ordered_non_unique<
                composite_key<
                    WaitingProcess,
                    member<WaitingProcess, frontend::TrustSession*, &WaitingProcess::trust_session>,
                    member<WaitingProcess, pid_t, &WaitingProcess::process_id>
                >
            >,
            ordered_non_unique<
                member<WaitingProcess, pid_t, &WaitingProcess::process_id>
            >
        >
    > WaitingTrustSessionsProcesses;

    typedef nth_index<WaitingTrustSessionsProcesses,0>::type process_by_trust_session;
    typedef nth_index<WaitingTrustSessionsProcesses,1>::type trust_session_by_process;

    WaitingTrustSessionsProcesses waiting_process_map;
    process_by_trust_session& waiting_process_trust_session_index;
    trust_session_by_process& waiting_process_index;
};

}
}

#endif // MIR_SCENE_TRUST_SESSION_PARTICIPANT_CONTAINER_H_
