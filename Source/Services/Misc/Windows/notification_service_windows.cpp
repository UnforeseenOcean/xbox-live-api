//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************
#include "pch.h"
#if !UNIT_TEST_SERVICES
#include "notification_service.h"
#include "xbox_system_factory.h"
#include "user_context.h"
#include "user_impl.h"

using namespace pplx;
using namespace xbox::services::system;

NAMESPACE_MICROSOFT_XBOX_SERVICES_NOTIFICATION_CPP_BEGIN

pplx::task<xbox_live_result<void>>
notification_service_windows::subscribe_to_notifications(
    _In_ std::shared_ptr<xbox::services::user_context> userContext,
    _In_ std::shared_ptr<xbox::services::xbox_live_context_settings> xboxLiveContextSettings,
    _In_ std::shared_ptr<xbox::services::xbox_live_app_config> appConfig
    )
{
    {
        std::lock_guard<std::mutex> guard(s_notificationSingletonLock);
        if (m_userContexts.find(userContext->xbox_user_id()) == m_userContexts.end())
        {
            m_userContexts.emplace(std::make_pair(userContext->xbox_user_id(), userContext));
        }
        else
        {
            return pplx::task<xbox_live_result<void>>();
        }
    }

    auto asyncOp = Windows::Networking::PushNotifications::PushNotificationChannelManager::CreatePushNotificationChannelForApplicationAsync();

    std::weak_ptr<notification_service_windows> thisShared = std::dynamic_pointer_cast<notification_service_windows>(shared_from_this());

    create_task(asyncOp)
    .then([thisShared, userContext, xboxLiveContextSettings, appConfig](task<Windows::Networking::PushNotifications::PushNotificationChannel^> t)
    {
        try
        {
            auto channel = t.get();
            channel->PushNotificationReceived += ref new Windows::Foundation::TypedEventHandler<Windows::Networking::PushNotifications::PushNotificationChannel ^, Windows::Networking::PushNotifications::PushNotificationReceivedEventArgs ^>(
            [thisShared](Windows::Networking::PushNotifications::PushNotificationChannel ^ channel, Windows::Networking::PushNotifications::PushNotificationReceivedEventArgs^ args)
            {
                std::shared_ptr<notification_service_windows> pThis(thisShared.lock());
                if (pThis != nullptr)
                {
                    try
                    {
                        pThis->on_push_notification_recieved(channel, args);
                    }
                    catch (...)
                    {
                        LOG_ERROR("on_push_notification_recieved error");
                    }
                }
            });
            
            std::shared_ptr<http_call> httpCall = xbox_system_factory::get_factory()->create_http_call(
                xboxLiveContextSettings,
                _T("POST"),
                L"https://notify.xboxlive.com/",
                L"system/notifications/endpoints",
                xbox_live_api::subscribe_to_notifications
                );

            auto applicationInstanceId = utils::create_guid(true);
            auto family = Windows::System::Profile::AnalyticsInfo::VersionInfo->DeviceFamily;
            auto form = Windows::System::Profile::AnalyticsInfo::DeviceForm;
            auto version = Windows::System::Profile::AnalyticsInfo::VersionInfo->DeviceFamilyVersion;
            string_t platform;
            if (family == "Windows.Mobile") 
            {
                platform = _T("WindowsOneCoreMobile");
            }
            else if (family == "Windows.Xbox")
            {
                platform = _T("Durango");
            }
            else
            {
                platform = _T("WindowsOneCore");
            }
            
            web::json::value payload;
            payload[_T("systemId")] = web::json::value::string(applicationInstanceId);
            payload[_T("endpointUri")] = web::json::value::string(channel->Uri->Data());
            payload[_T("platform")] = web::json::value::string(platform);
            payload[_T("platformVersion")] = web::json::value::string(_T("10"));
            payload[_T("locale")] = web::json::value::string(utils::get_locales());
            payload[_T("titleId")] = web::json::value::string(std::to_wstring(appConfig->title_id()));

            httpCall->set_request_body(payload.serialize());
            httpCall->get_response_with_auth(userContext).get();

        }
        catch (...)
        {
            LOG_ERROR("Failed to successfully register with notification service");
        }
    }, pplx::task_continuation_context::use_arbitrary());

    return pplx::task<xbox_live_result<void>>();
}

pplx::task<xbox_live_result<void>> notification_service_windows::subscribe_to_notifications()
{
    return pplx::task_from_result<xbox_live_result<void>>(xbox_live_result<void>(xbox_live_error_code::logic_error, "Not valid notification code path"));
}

void
notification_service_windows::on_push_notification_recieved(
    _In_ Windows::Networking::PushNotifications::PushNotificationChannel ^sender,
    _In_ Windows::Networking::PushNotifications::PushNotificationReceivedEventArgs ^args
    )
{
    std::error_code errc;
    if (args && args->RawNotification && args->RawNotification->Content)
    {
        auto parsedJson = web::json::value::parse(args->RawNotification->Content->Data(), errc);
        auto xboxLiveNotificationJson = utils::extract_json_field(parsedJson, _T("xboxLiveNotification"), errc, false);
        auto notificationTypeString = utils::extract_json_string(xboxLiveNotificationJson, _T("notificationType"), errc);
        auto xuid = utils::extract_json_string(xboxLiveNotificationJson, _T("userXuid"), errc);

        LOGS_INFO << "Received WNS notification, type: " << notificationTypeString << ", xuid: " << xuid;
        get_xsapi_singleton()->s_xboxServiceSettingsSingleton->_Raise_wns_event(xuid, notificationTypeString);

        if (!errc && utils::str_icmp(notificationTypeString, _T("spop")) == 0)
        {
            
            auto contextItor = m_userContexts.find(xuid);
            if (contextItor != m_userContexts.end() && contextItor->second != nullptr && contextItor->second->user() != nullptr)
            {
                contextItor->second->refresh_token();
            }
        }
    }
}

NAMESPACE_MICROSOFT_XBOX_SERVICES_NOTIFICATION_CPP_END
#endif