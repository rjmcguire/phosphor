/* -*- Mode: C++; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *     Copyright 2016 Couchbase, Inc
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

#include "phosphor/trace_event.h"

#include "phosphor/platform/thread.h"
#include "utils/string_utils.h"

namespace phosphor {

    using namespace std::chrono;

    TraceEvent::TraceEvent(
        const tracepoint_info* _tpi,
        Type _type,
        uint32_t _thread_id,
        std::array<TraceArgument, arg_count>&& _args,
        std::array<TraceArgument::Type, arg_count>&& _arg_types)
        : tpi(_tpi),
          args(_args),
          time(
              duration_cast<nanoseconds>(steady_clock::now().time_since_epoch())
                  .count()),
          thread_id(_thread_id),
          arg_types(_arg_types),
          type(_type) {}

    std::string TraceEvent::to_string() const {
        typedef duration<
            int,
            std::ratio_multiply<hours::period, std::ratio<24> >::type>
            days;

        nanoseconds ttime(time);
        auto d = duration_cast<days>(ttime);
        ttime -= d;
        auto h = duration_cast<hours>(ttime);
        ttime -= h;
        auto m = duration_cast<minutes>(ttime);
        ttime -= m;
        auto s = duration_cast<seconds>(ttime);
        ttime -= s;
        auto us = duration_cast<nanoseconds>(ttime);

        return utils::format_string(
            "TraceEvent<%dd %02ld:%02ld:%02lld.%09lld, %s, %s, type=%s, "
            "thread_id=%d, arg1=%s, arg2=%s>",
            d.count(),
            h.count(),
            m.count(),
            s.count(),
            us.count(),
            getCategory(),
            getName(),
            typeToString(type),
            thread_id,
            args[0].to_string(arg_types[0]).c_str(),
            args[1].to_string(arg_types[1]).c_str());
    }

    std::string TraceEvent::to_json() const {
        std::string output;
        output += "{\"name\":" + utils::to_json(getName());
        output += ",\"cat\":" + utils::to_json(getCategory());

        auto type_converted = typeToJSON();
        output += ",\"ph\":\"" + std::string(type_converted.type) + "\"";
        output += type_converted.extras;

        output += ",\"ts\":" + std::to_string(time / 1000);
        output += ",\"pid\":" + std::to_string(platform::getCurrentProcessID()),
        output += ",\"tid\":" + std::to_string(thread_id);

        output += ",\"args\":{";
        for (int i = 0; i < arg_count; ++i) {
            if (arg_types[i] == TraceArgument::Type::is_none) {
                break;
            }
            if (i != 0) {
                output += ",";
            }

            output += utils::to_json(tpi->argument_names[i].load()) + ":";
            output += args[i].to_string(arg_types[i]);
        }
        output += "}";

        output += "}";
        return output;
    }

    const char* TraceEvent::typeToString(Type type) {
        switch (type) {
        case Type::AsyncStart:
            return "AsyncStart";
        case Type::AsyncEnd:
            return "AsyncEnd";
        case Type::SyncStart:
            return "SyncStart";
        case Type::SyncEnd:
            return "SyncEnd";
        case Type::Instant:
            return "Instant";
        case Type::GlobalInstant:
            return "GlobalInstant";
        }
        throw std::invalid_argument(
            "TraceEvent::typeToString: "
            "Invalid TraceEvent type");
    }

    const char* TraceEvent::getName() const {
        return tpi->name;
    }

    const char* TraceEvent::getCategory() const {
        return tpi->category;
    }

    TraceEvent::Type TraceEvent::getType() const {
        return type;
    }

    uint64_t TraceEvent::getThreadID() const {
        return thread_id;
    }

    const std::array<TraceArgument, arg_count>& TraceEvent::getArgs() const {
        return args;
    }

    const std::array<TraceArgument::Type, arg_count>& TraceEvent::getArgTypes() const {
        return arg_types;
    }

    const std::array<RelaxedAtomicCString, arg_count>&
            TraceEvent::getArgNames() const {
        return tpi->argument_names;
    }

    int64_t TraceEvent::getTime() const {
        return time;
    }

    TraceEvent::ToJsonResult TraceEvent::typeToJSON() const {
        TraceEvent::ToJsonResult res;

        switch (type) {
        case Type::AsyncStart:
            res.type = "b";
            res.extras =
                utils::format_string(",\"id\": \"0x%X\"", args[0].as_int);
            return res;
        case Type::AsyncEnd:
            res.type = "e";
            res.extras =
                utils::format_string(",\"id\": \"0x%X\"", args[0].as_int);
            return res;
        case Type::SyncStart:
            res.type = "B";
            res.extras = "";
            return res;
        case Type::SyncEnd:
            res.type = "E";
            res.extras = "";
            return res;
        case Type::Instant:
            res.type = "i";
            res.extras = ",\"s\":\"t\"";
            return res;
        case Type::GlobalInstant:
            res.type = "i";
            res.extras = ",\"s\":\"g\"";
            return res;
        }
        throw std::invalid_argument(
            "TraceEvent::typeToJSON: Invalid TraceArgument type");
    }

    std::ostream& operator<<(std::ostream& os, const TraceEvent& te) {
        os << te.to_string();
        return os;
    }
}
