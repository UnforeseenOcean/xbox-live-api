//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#pragma once
#include "xsapi/simple_stats.h"
#include "xsapi/system.h"
#include "system_internal.h"
#include "user_context.h"
#include <string>
#include <sstream>
#include <iostream>
#include "xsapi/mem.h"

namespace xbox { namespace services { namespace experimental { namespace stats { namespace manager { 

enum class svd_event_type
{
    unknown,
    stat_change,
    stat_context_change
};

struct stat_pending_state
{
    stat_pending_state() :
        statDataType(stat_data_type::undefined),
        statCompareType(stat_compare_type::always)
    {
        initialize_char_arr(statPendingName);
    }

    stat_data_type statDataType;
    stat_compare_type statCompareType;
    char_t statPendingName[STAT_PRESENCE_CHARS_NUM];
    stat_data statPendingData;
};

struct stat_context
{
    stat_context() { };

    string_t playerStateName;
    xbox::services::player_state::player_state_value playerStateValue;
};

// stat value document event
class svd_event
{
public:
    svd_event(_In_ stat_pending_state statPendingState);
    svd_event(_In_ xsapi_internal_vector(stat_context) statPendingState);

    svd_event_type event_type() const;
    const stat_pending_state& stat_info() const;
    const xsapi_internal_vector(stat_context)& context_list() const;

private:
    svd_event_type m_svdEventType;
    stat_pending_state m_statPendingState;
    xsapi_internal_vector(stat_context) m_statContextList;
};

/// internal class
/// The stat value document holds all of the stat information in an unordered map
class stats_value_document
{
public:
    void set_flush_function(
        _In_ const std::function<void()> flushFunction
        );

    xbox_live_result<std::shared_ptr<stat_value>> get_stat(
        _In_ const char_t* name
        ) const;

    xbox_live_result<void> set_stat(
        _In_ const char_t* statName,
        _In_ double statValue,
        _In_ stat_compare_type statCompareType = stat_compare_type::always
        );

    xbox_live_result<void> set_stat(
        _In_ const char_t* statName,
        _In_ const char_t* statValue
        );

    void get_stat_names(
        _Inout_ std::vector<string_t>& statNameList
        ) const;

    void get_stat_contexts(
        _Inout_ std::vector<stat_context>& statisticContextList
        ) const;

    void set_stat_contexts(
        _In_ const std::vector<stat_context>& statContextList
        );

    void clear_stat_contexts();

    void increment_client_version_number();

    web::json::value serialize() const;

    uint32_t client_version() const;
    uint32_t server_version() const;
    const xsapi_internal_string& client_id() const;

    uint32_t version() const;

    bool is_dirty() const;

    void clear_dirty_state();

    void do_work();

    static xbox_live_result<stats_value_document> _Deserialize(
        _In_ const web::json::value& data
        );

private:
    bool m_isDirty;
    uint32_t m_version;
    uint32_t m_clientVersion;
    uint32_t m_serverVersion;
    std::function<void()> m_fRequestFlush;
    xsapi_internal_string m_clientId;
    xsapi_internal_vector(svd_event) m_svdEventList;
    xsapi_internal_vector(stat_context) m_currentStatContexts;
    xsapi_internal_unordered_map(string_t, std::shared_ptr<stat_value>) m_statisticDocument;
};

/// internal class
/// The service which handles the http requests to the simplified stats service
class simplified_stats_service
{
public:
    simplified_stats_service();

    simplified_stats_service(
        _In_ std::shared_ptr<xbox::services::user_context> userContext,
        _In_ std::shared_ptr<xbox::services::xbox_live_context_settings> xboxLiveContextSettings,
        _In_ std::shared_ptr<xbox::services::xbox_live_app_config> appConfig
        );

    pplx::task<xbox_live_result<void>> update_stats_value_document(
        _In_ stats_value_document& statValuePostDocument
        );

    pplx::task<xbox_live_result<stats_value_document>> get_stats_value_document();

private:
    string_t pathandquery_simplified_stats_subpath(
        const string_t& xboxUserId,
        const string_t& serviceConfigurationId,
        bool useXuidTag
    ) const;

    std::shared_ptr<xbox::services::user_context> m_userContext;
    std::shared_ptr<xbox::services::xbox_live_context_settings> m_xboxLiveContextSettings;
    std::shared_ptr<xbox::services::xbox_live_app_config> m_appConfig;
};

struct stats_user_context
{
    stats_user_context() {}
    stats_user_context(
        _In_ stats_value_document _statValueDoc,
        _In_ std::shared_ptr<xbox_live_context_impl> _xboxLiveContextImpl,
        _In_ simplified_stats_service _simplifiedStatsService
        ) :
        statValueDocument(std::move(_statValueDoc)),
        xboxLiveContextImpl(std::move(_xboxLiveContextImpl)),
        simplifiedStatsService(std::move(_simplifiedStatsService))
    {
    }

    stats_value_document statValueDocument;
    std::shared_ptr<xbox_live_context_impl> xboxLiveContextImpl;
    simplified_stats_service simplifiedStatsService;
};

class stats_manager_impl : public std::enable_shared_from_this<stats_manager_impl>
{
public:
    stats_manager_impl();

    xbox_live_result<void> add_local_user(
        _In_ const xbox_live_user_t& user
        );

    xbox_live_result<void> remove_local_user(
        _In_ const xbox_live_user_t& user
        );

    xbox_live_result<void> request_flush_to_service(
        _In_ const xbox_live_user_t& user,
        _In_ bool isHighPriority = false
        );

    std::vector<stat_event> do_work();

    xbox_live_result<void> set_stat(
        _In_ const xbox_live_user_t& user,
        _In_ const string_t& name,
        _In_ double value,
        _In_ stat_compare_type statisticReplaceCompareType = stat_compare_type::always
        );

    xbox_live_result<void> set_stat(
        _In_ const xbox_live_user_t& user,
        _In_ const string_t& name,
        _In_ const char_t* value
        );

    xbox_live_result<void> get_stat_names(
        _In_ const xbox_live_user_t& user,
        _Inout_ std::vector<string_t>& statNameList
        );

    xbox_live_result<std::shared_ptr<stat_value>> get_stat(
        _In_ const xbox_live_user_t& user,
        _In_ const string_t& name
        );

    xbox_live_result<void> get_stat_contexts(
        _In_ const xbox_live_user_t& user,
        _Inout_ std::vector<stat_context>& statisticContextList
        );

    xbox_live_result<void> set_stat_contexts(
        _In_ const xbox_live_user_t& user,
        _In_ const std::vector<stat_context>& statContextList
        );

    xbox_live_result<void> clear_stat_contexts(
        _In_ const xbox_live_user_t& user
        );

    xbox_live_result<std::shared_ptr<xbox::services::leaderboard::leaderboard_result>> get_social_leaderboard(
        _In_ const xbox_live_user_t& user,
        _In_ const string_t& leaderboardOrStatName,
        _In_ const stat_context& statContext = stat_context(),
        _In_ const string_t& socialGroup = xbox::services::social::social_group_constants::people(),
        _In_ const leaderboard_query& leaderboardQuery = leaderboard_query(),
        _In_ bool isRealTime = false
        );

    xbox_live_result<std::shared_ptr<xbox::services::leaderboard::leaderboard_result>> get_global_leaderboard(
        _In_ const xbox_live_user_t& user,
        _In_ const string_t& leaderboardName,
        _In_ const stat_context& statContext = stat_context(),
        _In_ const leaderboard_query& leaderboardQuery = leaderboard_query(),
        _In_ bool shouldAutoRefresh = false
        );

    xbox_live_result<std::shared_ptr<user_statistic_view>> get_statistics_table(
        _In_ const xbox_live_user_t& user,
        _In_ const string_t& statName,
        _In_ const std::vector<string_t>& xboxUserIds,
        _In_ const stat_context& statContext = stat_context()
        );

    xbox_live_result<std::shared_ptr<user_statistic_view>> get_multiple_statistics_table(
        _In_ const xbox_live_user_t& user,
        _In_ const std::vector<string_t>& statName,
        _In_ const std::vector<string_t>& xboxUserIds,
        _In_ const stat_context& statContext = stat_context()
        );

    void write_offline();

private:
    static inline bool should_write_offline(xbox_live_result<void>& postResult)
    {
        return postResult.err() == xbox_live_error_condition::network || postResult.err() == xbox_live_error_condition::http_429_too_many_requests 
            || postResult.err() == xbox_live_error_code::http_status_500_internal_server_error || postResult.err() == xbox_live_error_code::http_status_502_bad_gateway
            || postResult.err() == xbox_live_error_code::http_status_503_service_unavailable || postResult.err() == xbox_live_error_code::http_status_504_gateway_timeout;
    }

    void flush_to_service(
        _In_ stats_user_context& statsUserContext,
        _In_ const xbox_live_user_t user
        );


    bool m_isOffline;
    std::vector<stat_event> m_statEventList;
    // TODO: change back to xsapi_internal_string
    xsapi_internal_unordered_map(string_t, stats_user_context) m_users;
    std::mutex m_statsServiceMutex;
};

}}}}}